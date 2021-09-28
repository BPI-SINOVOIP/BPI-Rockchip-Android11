#!/usr/bin/env python
#
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

"""Tests for acloud.internal.lib.ssh."""

import subprocess
import unittest
import mock

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import ssh


class SshTest(driver_test_lib.BaseDriverTest):
    """Test ssh class."""

    FAKE_SSH_PRIVATE_KEY_PATH = "/fake/acloud_rea"
    FAKE_SSH_USER = "fake_user"
    FAKE_IP = ssh.IP(external="1.1.1.1", internal="10.1.1.1")
    FAKE_EXTRA_ARGS_SSH = "-o ProxyCommand='ssh fake_user@2.2.2.2 Server 22'"
    FAKE_REPORT_INTERNAL_IP = True

    def setUp(self):
        """Set up the test."""
        super(SshTest, self).setUp()
        self.created_subprocess = mock.MagicMock()
        self.created_subprocess.stdout = mock.MagicMock()
        self.created_subprocess.stdout.readline = mock.MagicMock(return_value='')
        self.created_subprocess.poll = mock.MagicMock(return_value=0)
        self.created_subprocess.returncode = 0
        self.created_subprocess.communicate = mock.MagicMock(return_value=
                                                             ('', ''))

    def testSSHExecuteWithRetry(self):
        """test SSHExecuteWithRetry method."""
        self.Patch(subprocess, "Popen",
                   side_effect=subprocess.CalledProcessError(
                       None, "ssh command fail."))
        self.assertRaises(subprocess.CalledProcessError,
                          ssh.ShellCmdWithRetry,
                          "fake cmd")

    def testGetBaseCmdWithInternalIP(self):
        """Test get base command with internal ip."""
        ssh_object = ssh.Ssh(ip=self.FAKE_IP,
                             user=self.FAKE_SSH_USER,
                             ssh_private_key_path=self.FAKE_SSH_PRIVATE_KEY_PATH,
                             report_internal_ip=self.FAKE_REPORT_INTERNAL_IP)
        expected_ssh_cmd = ("/usr/bin/ssh -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                            "-o StrictHostKeyChecking=no -l fake_user 10.1.1.1")
        self.assertEqual(ssh_object.GetBaseCmd(constants.SSH_BIN), expected_ssh_cmd)

    def testGetBaseCmd(self):
        """Test get base command."""
        ssh_object = ssh.Ssh(self.FAKE_IP, self.FAKE_SSH_USER, self.FAKE_SSH_PRIVATE_KEY_PATH)
        expected_ssh_cmd = ("/usr/bin/ssh -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                            "-o StrictHostKeyChecking=no -l fake_user 1.1.1.1")
        self.assertEqual(ssh_object.GetBaseCmd(constants.SSH_BIN), expected_ssh_cmd)

        expected_scp_cmd = ("/usr/bin/scp -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                            "-o StrictHostKeyChecking=no")
        self.assertEqual(ssh_object.GetBaseCmd(constants.SCP_BIN), expected_scp_cmd)

    # pylint: disable=no-member
    def testSshRunCmd(self):
        """Test ssh run command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP, self.FAKE_SSH_USER, self.FAKE_SSH_PRIVATE_KEY_PATH)
        ssh_object.Run("command")
        expected_cmd = ("exec /usr/bin/ssh -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no -l fake_user 1.1.1.1 command")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    def testSshRunCmdwithExtraArgs(self):
        """test ssh rum command with extra command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP,
                             self.FAKE_SSH_USER,
                             self.FAKE_SSH_PRIVATE_KEY_PATH,
                             self.FAKE_EXTRA_ARGS_SSH)
        ssh_object.Run("command")
        expected_cmd = ("exec /usr/bin/ssh -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no "
                        "-o ProxyCommand='ssh fake_user@2.2.2.2 Server 22' "
                        "-l fake_user 1.1.1.1 command")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    def testScpPullFileCmd(self):
        """Test scp pull file command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP, self.FAKE_SSH_USER, self.FAKE_SSH_PRIVATE_KEY_PATH)
        ssh_object.ScpPullFile("/tmp/test", "/tmp/test_1.log")
        expected_cmd = ("exec /usr/bin/scp -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no fake_user@1.1.1.1:/tmp/test /tmp/test_1.log")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    def testScpPullFileCmdwithExtraArgs(self):
        """Test scp pull file command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP,
                             self.FAKE_SSH_USER,
                             self.FAKE_SSH_PRIVATE_KEY_PATH,
                             self.FAKE_EXTRA_ARGS_SSH)
        ssh_object.ScpPullFile("/tmp/test", "/tmp/test_1.log")
        expected_cmd = ("exec /usr/bin/scp -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no "
                        "-o ProxyCommand='ssh fake_user@2.2.2.2 Server 22' "
                        "fake_user@1.1.1.1:/tmp/test /tmp/test_1.log")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    def testScpPushFileCmd(self):
        """Test scp push file command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP, self.FAKE_SSH_USER, self.FAKE_SSH_PRIVATE_KEY_PATH)
        ssh_object.ScpPushFile("/tmp/test", "/tmp/test_1.log")
        expected_cmd = ("exec /usr/bin/scp -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no /tmp/test fake_user@1.1.1.1:/tmp/test_1.log")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    def testScpPushFileCmdwithExtraArgs(self):
        """Test scp pull file command."""
        self.Patch(subprocess, "Popen", return_value=self.created_subprocess)
        ssh_object = ssh.Ssh(self.FAKE_IP,
                             self.FAKE_SSH_USER,
                             self.FAKE_SSH_PRIVATE_KEY_PATH,
                             self.FAKE_EXTRA_ARGS_SSH)
        ssh_object.ScpPushFile("/tmp/test", "/tmp/test_1.log")
        expected_cmd = ("exec /usr/bin/scp -i /fake/acloud_rea -q -o UserKnownHostsFile=/dev/null "
                        "-o StrictHostKeyChecking=no "
                        "-o ProxyCommand='ssh fake_user@2.2.2.2 Server 22' "
                        "/tmp/test fake_user@1.1.1.1:/tmp/test_1.log")
        subprocess.Popen.assert_called_with(expected_cmd,
                                            shell=True,
                                            stderr=-2,
                                            stdin=None,
                                            stdout=-1)

    # pylint: disable=protected-access
    def testIPAddress(self):
        """Test IP class to get ip address."""
        # Internal ip case.
        ssh_object = ssh.Ssh(ip=ssh.IP(external="1.1.1.1", internal="10.1.1.1"),
                             user=self.FAKE_SSH_USER,
                             ssh_private_key_path=self.FAKE_SSH_PRIVATE_KEY_PATH,
                             report_internal_ip=True)
        expected_ip = "10.1.1.1"
        self.assertEqual(ssh_object._ip, expected_ip)

        # External ip case.
        ssh_object = ssh.Ssh(ip=ssh.IP(external="1.1.1.1", internal="10.1.1.1"),
                             user=self.FAKE_SSH_USER,
                             ssh_private_key_path=self.FAKE_SSH_PRIVATE_KEY_PATH)
        expected_ip = "1.1.1.1"
        self.assertEqual(ssh_object._ip, expected_ip)

        # Only one ip case.
        ssh_object = ssh.Ssh(ip=ssh.IP(ip="1.1.1.1"),
                             user=self.FAKE_SSH_USER,
                             ssh_private_key_path=self.FAKE_SSH_PRIVATE_KEY_PATH)
        expected_ip = "1.1.1.1"
        self.assertEqual(ssh_object._ip, expected_ip)

    def testWaitForSsh(self):
        """Test WaitForSsh."""
        ssh_object = ssh.Ssh(ip=self.FAKE_IP,
                             user=self.FAKE_SSH_USER,
                             ssh_private_key_path=self.FAKE_SSH_PRIVATE_KEY_PATH,
                             report_internal_ip=self.FAKE_REPORT_INTERNAL_IP)
        self.Patch(ssh, "_SshCall", return_value=-1)
        self.assertRaises(errors.DeviceConnectionError,
                          ssh_object.WaitForSsh,
                          timeout=1,
                          sleep_for_retry=1,
                          max_retry=1)


if __name__ == "__main__":
    unittest.main()
