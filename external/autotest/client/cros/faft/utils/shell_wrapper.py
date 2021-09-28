# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A module to abstract the shell execution environment on DUT."""

import subprocess

import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils


class UnsupportedSuccessToken(Exception):
    """Unsupported character found."""
    pass


class LocalShell(object):
    """An object to wrap the local shell environment."""

    def __init__(self, os_if):
        """Initialize the LocalShell object."""
        self._os_if = os_if

    def _run_command(self, cmd, block=True):
        """Helper function of run_command() methods.

        Return the subprocess.Popen() instance to provide access to console
        output in case command succeeded.  If block=False, will not wait for
        process to return before returning.
        """
        self._os_if.log('Executing: %s' % cmd)
        process = subprocess.Popen(
                cmd,
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
        if block:
            process.wait()
        return process

    def run_command(self, cmd, block=True):
        """Run a shell command.

        In case of the command returning an error print its stdout and stderr
        outputs on the console and dump them into the log. Otherwise suppress
        all output.

        @param block: if True (default), wait for command to finish
        @raise error.CmdError: if block is True and command fails (rc!=0)
        """
        start_time = time.time()
        process = self._run_command(cmd, block)
        if block and process.returncode:
            # Grab output only if an error occurred
            returncode = process.returncode
            stdout = process.stdout.read()
            stderr = process.stderr.read()
            duration = time.time() - start_time
            result = utils.CmdResult(cmd, stdout, stderr, returncode, duration)
            self._os_if.log('Command failed.\n%s' % result)
            raise error.CmdError(cmd, result)

    def run_command_get_result(self, cmd, ignore_status=False):
        """Run a shell command, and get the result (output and returncode).

        @param ignore_status: if True, do not raise CmdError, even if rc != 0.
        @raise error.CmdError: if command fails (rc!=0) and not ignore_result
        @return the result of the command
        @rtype: utils.CmdResult
        """
        start_time = time.time()

        process = self._run_command(cmd, block=True)

        returncode = process.returncode
        stdout = process.stdout.read()
        stderr = process.stderr.read()
        duration = time.time() - start_time
        result = utils.CmdResult(cmd, stdout, stderr, returncode, duration)

        if returncode and not ignore_status:
            self._os_if.log('Command failed:\n%s' % result)
            raise error.CmdError(cmd, result)

        self._os_if.log('Command result:\n%s' % result)
        return result

    def run_command_check_output(self, cmd, success_token):
        """Run a command and check whether standard output contains some string.

        The sucess token is assumed to not contain newlines.

        @param cmd: A string of the command to make a blocking call with.
        @param success_token: A string to search the standard output of the
                command for.

        @returns a Boolean indicating whthere the success_token was in the
                stdout of the cmd.

        @raises UnsupportedSuccessToken if a newline is found in the
                success_token.
        """
        # The run_command_get_outuput method strips newlines from stdout.
        if '\n' in success_token:
            raise UnsupportedSuccessToken()
        cmd_stdout = ''.join(self.run_command_get_output(cmd))
        self._os_if.log('Checking for %s in %s' % (success_token, cmd_stdout))
        return success_token in cmd_stdout

    def run_command_get_status(self, cmd):
        """Run a shell command and return its return code.

        The return code of the command is returned, in case of any error.
        """
        process = self._run_command(cmd)
        return process.returncode

    def run_command_get_output(self, cmd, include_stderr=False):
        """Run shell command and return stdout (and possibly stderr) to the caller.

        The output is returned as a list of strings stripped of the newline
        characters.
        """
        process = self._run_command(cmd)
        text = [x.rstrip() for x in process.stdout.readlines()]
        if include_stderr:
            text.extend([x.rstrip() for x in process.stderr.readlines()])
        return text

    def read_file(self, path):
        """Read the content of the file."""
        with open(path) as f:
            return f.read()

    def write_file(self, path, data):
        """Write the data to the file."""
        with open(path, 'w') as f:
            f.write(data)

    def append_file(self, path, data):
        """Append the data to the file."""
        with open(path, 'a') as f:
            f.write(data)
