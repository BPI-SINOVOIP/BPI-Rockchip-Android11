# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Various utility functions."""

import errno
import functools
import os
import signal
import subprocess
import sys
import tempfile
import time

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# pylint: disable=wrong-import-position
import rh.shell
import rh.signals


def timedelta_str(delta):
    """A less noisy timedelta.__str__.

    The default timedelta stringification contains a lot of leading zeros and
    uses microsecond resolution.  This makes for noisy output.
    """
    total = delta.total_seconds()
    hours, rem = divmod(total, 3600)
    mins, secs = divmod(rem, 60)
    ret = '%i.%03is' % (secs, delta.microseconds // 1000)
    if mins:
        ret = '%im%s' % (mins, ret)
    if hours:
        ret = '%ih%s' % (hours, ret)
    return ret


class CompletedProcess(getattr(subprocess, 'CompletedProcess', object)):
    """An object to store various attributes of a child process.

    This is akin to subprocess.CompletedProcess.
    """

    # The linter is confused by the getattr usage above.
    # TODO(vapier): Drop this once we're Python 3-only and we drop getattr.
    # pylint: disable=bad-option-value,super-on-old-class
    def __init__(self, args=None, returncode=None, stdout=None, stderr=None):
        if sys.version_info.major < 3:
            self.args = args
            self.stdout = stdout
            self.stderr = stderr
            self.returncode = returncode
        else:
            super(CompletedProcess, self).__init__(
                args=args, returncode=returncode, stdout=stdout, stderr=stderr)

    @property
    def cmd(self):
        """Alias to self.args to better match other subprocess APIs."""
        return self.args

    @property
    def cmdstr(self):
        """Return self.cmd as a nicely formatted string (useful for logs)."""
        return rh.shell.cmd_to_str(self.cmd)


class CalledProcessError(subprocess.CalledProcessError):
    """Error caught in run() function.

    This is akin to subprocess.CalledProcessError.  We do not support |output|,
    only |stdout|.

    Attributes:
      returncode: The exit code of the process.
      cmd: The command that triggered this exception.
      msg: Short explanation of the error.
      exception: The underlying Exception if available.
    """

    def __init__(self, returncode, cmd, stdout=None, stderr=None, msg=None,
                 exception=None):
        if exception is not None and not isinstance(exception, Exception):
            raise TypeError('exception must be an exception instance; got %r'
                            % (exception,))

        super(CalledProcessError, self).__init__(returncode, cmd, stdout)
        # The parent class will set |output|, so delete it.
        del self.output
        # TODO(vapier): When we're Python 3-only, delete this assignment as the
        # parent handles it for us.
        self.stdout = stdout
        # TODO(vapier): When we're Python 3-only, move stderr to the init above.
        self.stderr = stderr
        self.msg = msg
        self.exception = exception

    @property
    def cmdstr(self):
        """Return self.cmd as a well shell-quoted string for debugging."""
        return '' if self.cmd is None else rh.shell.cmd_to_str(self.cmd)

    def stringify(self, stdout=True, stderr=True):
        """Custom method for controlling what is included in stringifying this.

        Args:
          stdout: Whether to include captured stdout in the return value.
          stderr: Whether to include captured stderr in the return value.

        Returns:
          A summary string for this result.
        """
        items = [
            'return code: %s; command: %s' % (self.returncode, self.cmdstr),
        ]
        if stderr and self.stderr:
            items.append(self.stderr)
        if stdout and self.stdout:
            items.append(self.stdout)
        if self.msg:
            items.append(self.msg)
        return '\n'.join(items)

    def __str__(self):
        return self.stringify()


class TerminateCalledProcessError(CalledProcessError):
    """We were signaled to shutdown while running a command.

    Client code shouldn't generally know, nor care about this class.  It's
    used internally to suppress retry attempts when we're signaled to die.
    """


def _kill_child_process(proc, int_timeout, kill_timeout, cmd, original_handler,
                        signum, frame):
    """Used as a signal handler by RunCommand.

    This is internal to Runcommand.  No other code should use this.
    """
    if signum:
        # If we've been invoked because of a signal, ignore delivery of that
        # signal from this point forward.  The invoking context of this func
        # restores signal delivery to what it was prior; we suppress future
        # delivery till then since this code handles SIGINT/SIGTERM fully
        # including delivering the signal to the original handler on the way
        # out.
        signal.signal(signum, signal.SIG_IGN)

    # Do not trust Popen's returncode alone; we can be invoked from contexts
    # where the Popen instance was created, but no process was generated.
    if proc.returncode is None and proc.pid is not None:
        try:
            while proc.poll_lock_breaker() is None and int_timeout >= 0:
                time.sleep(0.1)
                int_timeout -= 0.1

            proc.terminate()
            while proc.poll_lock_breaker() is None and kill_timeout >= 0:
                time.sleep(0.1)
                kill_timeout -= 0.1

            if proc.poll_lock_breaker() is None:
                # Still doesn't want to die.  Too bad, so sad, time to die.
                proc.kill()
        except EnvironmentError as e:
            print('Ignoring unhandled exception in _kill_child_process: %s' % e,
                  file=sys.stderr)

        # Ensure our child process has been reaped, but don't wait forever.
        proc.wait_lock_breaker(timeout=60)

    if not rh.signals.relay_signal(original_handler, signum, frame):
        # Mock up our own, matching exit code for signaling.
        raise TerminateCalledProcessError(
            signum << 8, cmd, msg='Received signal %i' % signum)


class _Popen(subprocess.Popen):
    """subprocess.Popen derivative customized for our usage.

    Specifically, we fix terminate/send_signal/kill to work if the child process
    was a setuid binary; on vanilla kernels, the parent can wax the child
    regardless, on goobuntu this apparently isn't allowed, thus we fall back
    to the sudo machinery we have.

    While we're overriding send_signal, we also suppress ESRCH being raised
    if the process has exited, and suppress signaling all together if the
    process has knowingly been waitpid'd already.
    """

    # pylint: disable=arguments-differ
    def send_signal(self, signum):
        if self.returncode is not None:
            # The original implementation in Popen allows signaling whatever
            # process now occupies this pid, even if the Popen object had
            # waitpid'd.  Since we can escalate to sudo kill, we do not want
            # to allow that.  Fixing this addresses that angle, and makes the
            # API less sucky in the process.
            return

        try:
            os.kill(self.pid, signum)
        except EnvironmentError as e:
            if e.errno == errno.ESRCH:
                # Since we know the process is dead, reap it now.
                # Normally Popen would throw this error- we suppress it since
                # frankly that's a misfeature and we're already overriding
                # this method.
                self.poll()
            else:
                raise

    def _lock_breaker(self, func, *args, **kwargs):
        """Helper to manage the waitpid lock.

        Workaround https://bugs.python.org/issue25960.
        """
        # If the lock doesn't exist, or is not locked, call the func directly.
        lock = getattr(self, '_waitpid_lock', None)
        if lock is not None and lock.locked():
            try:
                lock.release()
                return func(*args, **kwargs)
            finally:
                if not lock.locked():
                    lock.acquire()
        else:
            return func(*args, **kwargs)

    def poll_lock_breaker(self, *args, **kwargs):
        """Wrapper around poll() to break locks if needed."""
        return self._lock_breaker(self.poll, *args, **kwargs)

    def wait_lock_breaker(self, *args, **kwargs):
        """Wrapper around wait() to break locks if needed."""
        return self._lock_breaker(self.wait, *args, **kwargs)


# We use the keyword arg |input| which trips up pylint checks.
# pylint: disable=redefined-builtin,input-builtin
def run(cmd, redirect_stdout=False, redirect_stderr=False, cwd=None, input=None,
        shell=False, env=None, extra_env=None, combine_stdout_stderr=False,
        check=True, int_timeout=1, kill_timeout=1, capture_output=False,
        close_fds=True):
    """Runs a command.

    Args:
      cmd: cmd to run.  Should be input to subprocess.Popen.  If a string, shell
          must be true.  Otherwise the command must be an array of arguments,
          and shell must be false.
      redirect_stdout: Returns the stdout.
      redirect_stderr: Holds stderr output until input is communicated.
      cwd: The working directory to run this cmd.
      input: The data to pipe into this command through stdin.  If a file object
          or file descriptor, stdin will be connected directly to that.
      shell: Controls whether we add a shell as a command interpreter.  See cmd
          since it has to agree as to the type.
      env: If non-None, this is the environment for the new process.
      extra_env: If set, this is added to the environment for the new process.
          This dictionary is not used to clear any entries though.
      combine_stdout_stderr: Combines stdout and stderr streams into stdout.
      check: Whether to raise an exception when command returns a non-zero exit
          code, or return the CompletedProcess object containing the exit code.
          Note: will still raise an exception if the cmd file does not exist.
      int_timeout: If we're interrupted, how long (in seconds) should we give
          the invoked process to clean up before we send a SIGTERM.
      kill_timeout: If we're interrupted, how long (in seconds) should we give
          the invoked process to shutdown from a SIGTERM before we SIGKILL it.
      capture_output: Set |redirect_stdout| and |redirect_stderr| to True.
      close_fds: Whether to close all fds before running |cmd|.

    Returns:
      A CompletedProcess object.

    Raises:
      CalledProcessError: Raises exception on error.
    """
    if capture_output:
        redirect_stdout, redirect_stderr = True, True

    # Set default for variables.
    popen_stdout = None
    popen_stderr = None
    stdin = None
    result = CompletedProcess()

    # Force the timeout to float; in the process, if it's not convertible,
    # a self-explanatory exception will be thrown.
    kill_timeout = float(kill_timeout)

    def _get_tempfile():
        try:
            return tempfile.TemporaryFile(buffering=0)
        except EnvironmentError as e:
            if e.errno != errno.ENOENT:
                raise
            # This can occur if we were pointed at a specific location for our
            # TMP, but that location has since been deleted.  Suppress that
            # issue in this particular case since our usage gurantees deletion,
            # and since this is primarily triggered during hard cgroups
            # shutdown.
            return tempfile.TemporaryFile(dir='/tmp', buffering=0)

    # Modify defaults based on parameters.
    # Note that tempfiles must be unbuffered else attempts to read
    # what a separate process did to that file can result in a bad
    # view of the file.
    # The Popen API accepts either an int or a file handle for stdout/stderr.
    # pylint: disable=redefined-variable-type
    if redirect_stdout:
        popen_stdout = _get_tempfile()

    if combine_stdout_stderr:
        popen_stderr = subprocess.STDOUT
    elif redirect_stderr:
        popen_stderr = _get_tempfile()
    # pylint: enable=redefined-variable-type

    # If subprocesses have direct access to stdout or stderr, they can bypass
    # our buffers, so we need to flush to ensure that output is not interleaved.
    if popen_stdout is None or popen_stderr is None:
        sys.stdout.flush()
        sys.stderr.flush()

    # If input is a string, we'll create a pipe and send it through that.
    # Otherwise we assume it's a file object that can be read from directly.
    if isinstance(input, str):
        stdin = subprocess.PIPE
        input = input.encode('utf-8')
    elif input is not None:
        stdin = input
        input = None

    if isinstance(cmd, str):
        if not shell:
            raise Exception('Cannot run a string command without a shell')
        cmd = ['/bin/bash', '-c', cmd]
        shell = False
    elif shell:
        raise Exception('Cannot run an array command with a shell')

    # If we are using enter_chroot we need to use enterchroot pass env through
    # to the final command.
    env = env.copy() if env is not None else os.environ.copy()
    env.update(extra_env if extra_env else {})

    result.args = cmd

    proc = None
    try:
        proc = _Popen(cmd, cwd=cwd, stdin=stdin, stdout=popen_stdout,
                      stderr=popen_stderr, shell=False, env=env,
                      close_fds=close_fds)

        old_sigint = signal.getsignal(signal.SIGINT)
        handler = functools.partial(_kill_child_process, proc, int_timeout,
                                    kill_timeout, cmd, old_sigint)
        signal.signal(signal.SIGINT, handler)

        old_sigterm = signal.getsignal(signal.SIGTERM)
        handler = functools.partial(_kill_child_process, proc, int_timeout,
                                    kill_timeout, cmd, old_sigterm)
        signal.signal(signal.SIGTERM, handler)

        try:
            (result.stdout, result.stderr) = proc.communicate(input)
        finally:
            signal.signal(signal.SIGINT, old_sigint)
            signal.signal(signal.SIGTERM, old_sigterm)

            if popen_stdout:
                # The linter is confused by how stdout is a file & an int.
                # pylint: disable=maybe-no-member,no-member
                popen_stdout.seek(0)
                result.stdout = popen_stdout.read()
                popen_stdout.close()

            if popen_stderr and popen_stderr != subprocess.STDOUT:
                # The linter is confused by how stderr is a file & an int.
                # pylint: disable=maybe-no-member,no-member
                popen_stderr.seek(0)
                result.stderr = popen_stderr.read()
                popen_stderr.close()

        result.returncode = proc.returncode

        if check and proc.returncode:
            msg = 'cwd=%s' % cwd
            if extra_env:
                msg += ', extra env=%s' % extra_env
            raise CalledProcessError(
                result.returncode, result.cmd, stdout=result.stdout,
                stderr=result.stderr, msg=msg)
    except OSError as e:
        estr = str(e)
        if e.errno == errno.EACCES:
            estr += '; does the program need `chmod a+x`?'
        if not check:
            result = CompletedProcess(
                args=cmd, stderr=estr.encode('utf-8'), returncode=255)
        else:
            raise CalledProcessError(
                result.returncode, result.cmd, stdout=result.stdout,
                stderr=result.stderr, msg=estr, exception=e)
    finally:
        if proc is not None:
            # Ensure the process is dead.
            # Some pylint3 versions are confused here.
            # pylint: disable=too-many-function-args
            _kill_child_process(proc, int_timeout, kill_timeout, cmd, None,
                                None, None)

    # Make sure output is returned as a string rather than bytes.
    if result.stdout is not None:
        result.stdout = result.stdout.decode('utf-8', 'replace')
    if result.stderr is not None:
        result.stderr = result.stderr.decode('utf-8', 'replace')

    return result
# pylint: enable=redefined-builtin,input-builtin
