# Copyright 2019 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Ssh Utilities."""
from __future__ import print_function
import logging

import subprocess
import sys
import threading

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import utils

logger = logging.getLogger(__name__)

_SSH_CMD = ("-i %(rsa_key_file)s "
            "-q -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no")
_SSH_IDENTITY = "-l %(login_user)s %(ip_addr)s"
_SSH_CMD_MAX_RETRY = 5
_SSH_CMD_RETRY_SLEEP = 3
_WAIT_FOR_SSH_MAX_TIMEOUT = 60


def _SshCallWait(cmd, timeout=None):
    """Runs a single SSH command.

    - SSH returns code 0 for "Successful execution".
    - Use wait() until the process is complete without receiving any output.

    Args:
        cmd: String of the full SSH command to run, including the SSH binary
             and its arguments.
        timeout: Optional integer, number of seconds to give

    Returns:
        An exit status of 0 indicates that it ran successfully.
    """
    logger.info("Running command \"%s\"", cmd)
    process = subprocess.Popen(cmd, shell=True, stdin=None,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if timeout:
        # TODO: if process is killed, out error message to log.
        timer = threading.Timer(timeout, process.kill)
        timer.start()
    process.wait()
    if timeout:
        timer.cancel()
    return process.returncode


def _SshCall(cmd, timeout=None):
    """Runs a single SSH command.

    - SSH returns code 0 for "Successful execution".
    - Use communicate() until the process and the child thread are complete.

    Args:
        cmd: String of the full SSH command to run, including the SSH binary
             and its arguments.
        timeout: Optional integer, number of seconds to give

    Returns:
        An exit status of 0 indicates that it ran successfully.
    """
    logger.info("Running command \"%s\"", cmd)
    process = subprocess.Popen(cmd, shell=True, stdin=None,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if timeout:
        # TODO: if process is killed, out error message to log.
        timer = threading.Timer(timeout, process.kill)
        timer.start()
    process.communicate()
    if timeout:
        timer.cancel()
    return process.returncode


def _SshLogOutput(cmd, timeout=None, show_output=False):
    """Runs a single SSH command while logging its output and processes its return code.

    Output is streamed to the log at the debug level for more interactive debugging.
    SSH returns error code 255 for "failed to connect", so this is interpreted as a failure in
    SSH rather than a failure on the target device and this is converted to a different exception
    type.

    Args:
        cmd: String of the full SSH command to run, including the SSH binary and its arguments.
        timeout: Optional integer, number of seconds to give.
        show_output: Boolean, True to show command output in screen.

    Raises:
        errors.DeviceConnectionError: Failed to connect to the GCE instance.
        subprocess.CalledProc: The process exited with an error on the instance.
    """
    # Use "exec" to let cmd to inherit the shell process, instead of having the
    # shell launch a child process which does not get killed.
    cmd = "exec " + cmd
    logger.info("Running command \"%s\"", cmd)
    process = subprocess.Popen(cmd, shell=True, stdin=None,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if timeout:
        # TODO: if process is killed, out error message to log.
        timer = threading.Timer(timeout, process.kill)
        timer.start()
    stdout, _ = process.communicate()
    if stdout:
        if show_output or process.returncode != 0:
            print(stdout.strip(), file=sys.stderr)
        else:
            # fetch_cvd and launch_cvd can be noisy, so left at debug
            logger.debug(stdout.strip())
    if timeout:
        timer.cancel()
    if process.returncode == 255:
        raise errors.DeviceConnectionError(
            "Failed to send command to instance (%s)" % cmd)
    elif process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, cmd)


def ShellCmdWithRetry(cmd, timeout=None, show_output=False):
    """Runs a shell command on remote device.

    If the network is unstable and causes SSH connect fail, it will retry. When
    it retry in a short time, you may encounter unstable network. We will use
    the mechanism of RETRY_BACKOFF_FACTOR. The retry time for each failure is
    times * retries.

    Args:
        cmd: String of the full SSH command to run, including the SSH binary and its arguments.
        timeout: Optional integer, number of seconds to give.
        show_output: Boolean, True to show command output in screen.

    Raises:
        errors.DeviceConnectionError: For any non-zero return code of
                                      remote_cmd.
    """
    utils.RetryExceptionType(
        exception_types=errors.DeviceConnectionError,
        max_retries=_SSH_CMD_MAX_RETRY,
        functor=_SshLogOutput,
        sleep_multiplier=_SSH_CMD_RETRY_SLEEP,
        retry_backoff_factor=utils.DEFAULT_RETRY_BACKOFF_FACTOR,
        cmd=cmd,
        timeout=timeout,
        show_output=show_output)


class IP(object):
    """ A class that control the IP address."""
    def __init__(self, external=None, internal=None, ip=None):
        """Init for IP.
            Args:
                external: String, external ip.
                internal: String, internal ip.
                ip: String, default ip to set for either external and internal
                if neither is set.
        """
        self.external = external or ip
        self.internal = internal or ip


class Ssh(object):
    """A class that control the remote instance via the IP address.

    Attributes:
        _ip: an IP object.
        _user: String of user login into the instance.
        _ssh_private_key_path: Path to the private key file.
        _extra_args_ssh_tunnel: String, extra args for ssh or scp.
    """
    def __init__(self, ip, user, ssh_private_key_path,
                 extra_args_ssh_tunnel=None, report_internal_ip=False):
        self._ip = ip.internal if report_internal_ip else ip.external
        self._user = user
        self._ssh_private_key_path = ssh_private_key_path
        self._extra_args_ssh_tunnel = extra_args_ssh_tunnel

    def Run(self, target_command, timeout=None, show_output=False):
        """Run a shell command over SSH on a remote instance.

        Example:
            ssh:
                base_cmd_list is ["ssh", "-i", "~/private_key_path" ,"-l" , "user", "1.1.1.1"]
                target_command is "remote command"
            scp:
                base_cmd_list is ["scp", "-i", "~/private_key_path"]
                target_command is "{src_file} {dst_file}"

        Args:
            target_command: String, text of command to run on the remote instance.
            timeout: Integer, the maximum time to wait for the command to respond.
            show_output: Boolean, True to show command output in screen.
        """
        ShellCmdWithRetry(self.GetBaseCmd(constants.SSH_BIN) + " " + target_command,
                          timeout,
                          show_output)

    def GetBaseCmd(self, execute_bin):
        """Get a base command over SSH on a remote instance.

        Example:
            execute bin is ssh:
                ssh -i ~/private_key_path $extra_args -l user 1.1.1.1
            execute bin is scp:
                scp -i ~/private_key_path $extra_args

        Args:
            execute_bin: String, execute type, e.g. ssh or scp.

        Returns:
            Strings of base connection command.

        Raises:
            errors.UnknownType: Don't support the execute bin.
        """
        base_cmd = [utils.FindExecutable(execute_bin)]
        base_cmd.append(_SSH_CMD % {"rsa_key_file": self._ssh_private_key_path})
        if self._extra_args_ssh_tunnel:
            base_cmd.append(self._extra_args_ssh_tunnel)

        if execute_bin == constants.SSH_BIN:
            base_cmd.append(_SSH_IDENTITY %
                            {"login_user":self._user, "ip_addr":self._ip})
            return " ".join(base_cmd)
        if execute_bin == constants.SCP_BIN:
            return " ".join(base_cmd)

        raise errors.UnknownType("Don't support the execute bin %s." % execute_bin)

    def CheckSshConnection(self, timeout):
        """Run remote 'uptime' ssh command to check ssh connection.

        Args:
            timeout: Integer, the maximum time to wait for the command to respond.

        Raises:
            errors.DeviceConnectionError: Ssh isn't ready in the remote instance.
        """
        remote_cmd = [self.GetBaseCmd(constants.SSH_BIN)]
        remote_cmd.append("uptime")

        if _SshCallWait(" ".join(remote_cmd), timeout) == 0:
            return
        raise errors.DeviceConnectionError(
            "Ssh isn't ready in the remote instance.")

    @utils.TimeExecute(function_description="Waiting for SSH server")
    def WaitForSsh(self, timeout=None, sleep_for_retry=_SSH_CMD_RETRY_SLEEP,
                   max_retry=_SSH_CMD_MAX_RETRY):
        """Wait until the remote instance is ready to accept commands over SSH.

        Args:
            timeout: Integer, the maximum time in seconds to wait for the
                     command to respond.
            sleep_for_retry: Integer, the sleep time in seconds for retry.
            max_retry: Integer, the maximum number of retry.

        Raises:
            errors.DeviceConnectionError: Ssh isn't ready in the remote instance.
        """
        timeout_one_round = timeout / max_retry if timeout else None
        utils.RetryExceptionType(
            exception_types=errors.DeviceConnectionError,
            max_retries=max_retry,
            functor=self.CheckSshConnection,
            sleep_multiplier=sleep_for_retry,
            retry_backoff_factor=utils.DEFAULT_RETRY_BACKOFF_FACTOR,
            timeout=timeout_one_round or _WAIT_FOR_SSH_MAX_TIMEOUT)

    def ScpPushFile(self, src_file, dst_file):
        """Scp push file to remote.

        Args:
            src_file: The source file path to be pulled.
            dst_file: The destination file path the file is pulled to.
        """
        scp_command = [self.GetBaseCmd(constants.SCP_BIN)]
        scp_command.append(src_file)
        scp_command.append("%s@%s:%s" %(self._user, self._ip, dst_file))
        ShellCmdWithRetry(" ".join(scp_command))

    def ScpPullFile(self, src_file, dst_file):
        """Scp pull file from remote.

        Args:
            src_file: The source file path to be pulled.
            dst_file: The destination file path the file is pulled to.
        """
        scp_command = [self.GetBaseCmd(constants.SCP_BIN)]
        scp_command.append("%s@%s:%s" %(self._user, self._ip, src_file))
        scp_command.append(dst_file)
        ShellCmdWithRetry(" ".join(scp_command))
