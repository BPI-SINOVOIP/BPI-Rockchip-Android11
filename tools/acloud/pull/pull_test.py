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
"""Tests for pull."""
import unittest

import os
import tempfile
import mock

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import ssh
from acloud.internal.lib import utils
from acloud.list import list as list_instances
from acloud.public import config
from acloud.pull import pull


class PullTest(driver_test_lib.BaseDriverTest):
    """Test pull."""

    # pylint: disable=no-member
    def testPullFileFromInstance(self):
        """test PullFileFromInstance."""
        cfg = mock.MagicMock()
        cfg.ssh_private_key_path = "fake_ssh_path"
        cfg.extra_args_ssh_tunnel = ""
        instance = mock.MagicMock()
        instance.ip = "1.1.1.1"
        # Multiple selected files case.
        selected_files = ["file1.log", "file2.log"]
        self.Patch(pull, "SelectLogFileToPull", return_value=selected_files)
        self.Patch(pull, "GetDownloadLogFolder", return_value="fake_folder")
        self.Patch(pull, "PullLogs")
        self.Patch(pull, "DisplayLog")
        pull.PullFileFromInstance(cfg, instance)
        self.assertEqual(pull.DisplayLog.call_count, 0)

        # Only one file selected case.
        selected_files = ["file1.log"]
        self.Patch(pull, "SelectLogFileToPull", return_value=selected_files)
        pull.PullFileFromInstance(cfg, instance)
        self.assertEqual(pull.DisplayLog.call_count, 1)

    # pylint: disable=no-member
    def testPullLogs(self):
        """test PullLogs."""
        _ssh = mock.MagicMock()
        self.Patch(utils, "PrintColorString")
        log_files = ["file1.log", "file2.log"]
        download_folder = "/fake_folder"
        pull.PullLogs(_ssh, log_files, download_folder)
        self.assertEqual(_ssh.ScpPullFile.call_count, 2)
        utils.PrintColorString.assert_called_once()

    @mock.patch.object(ssh.Ssh, "Run")
    def testDisplayLog(self, mock_ssh_run):
        """Test DisplayLog."""
        fake_ip = ssh.IP(external="1.1.1.1", internal="10.1.1.1")
        _ssh = ssh.Ssh(ip=fake_ip,
                       user=constants.GCE_USER,
                       ssh_private_key_path="/fake/acloud_rea")
        self.Patch(utils, "GetUserAnswerYes", return_value="Y")
        log_file = "file1.log"
        pull.DisplayLog(_ssh, log_file)
        expected_cmd = "tail -f -n +1 %s" % log_file
        mock_ssh_run.assert_has_calls([
            mock.call(expected_cmd, show_output=True)])

    def testGetDownloadLogFolder(self):
        """test GetDownloadLogFolder."""
        self.Patch(tempfile, "gettempdir", return_value="/tmp")
        self.Patch(os.path, "exists", return_value=True)
        instance = "instance"
        expected_path = "/tmp/instance"
        self.assertEqual(pull.GetDownloadLogFolder(instance), expected_path)

    def testSelectLogFileToPull(self):
        """test choose log files from the remote instance."""
        _ssh = mock.MagicMock()

        # Test only one log file case
        log_files = ["file1.log"]
        self.Patch(pull, "GetAllLogFilePaths", return_value=log_files)
        expected_result = ["file1.log"]
        self.assertEqual(pull.SelectLogFileToPull(_ssh), expected_result)

        # Test no log files case
        self.Patch(pull, "GetAllLogFilePaths", return_value=[])
        with self.assertRaises(errors.CheckPathError):
            pull.SelectLogFileToPull(_ssh)

        # Test two log files case.
        log_files = ["file1.log", "file2.log"]
        choose_log = ["file2.log"]
        self.Patch(pull, "GetAllLogFilePaths", return_value=log_files)
        self.Patch(utils, "GetAnswerFromList", return_value=choose_log)
        expected_result = ["file2.log"]
        self.assertEqual(pull.SelectLogFileToPull(_ssh), expected_result)

        # Test user provided file name exist.
        log_files = ["/home/vsoc-01/cuttlefish_runtime/file1.log",
                     "/home/vsoc-01/cuttlefish_runtime/file2.log"]
        input_file = "file1.log"
        self.Patch(pull, "GetAllLogFilePaths", return_value=log_files)
        expected_result = ["/home/vsoc-01/cuttlefish_runtime/file1.log"]
        self.assertEqual(pull.SelectLogFileToPull(_ssh, input_file), expected_result)

        # Test user provided file name not exist.
        log_files = ["/home/vsoc-01/cuttlefish_runtime/file1.log",
                     "/home/vsoc-01/cuttlefish_runtime/file2.log"]
        input_file = "not_exist.log"
        self.Patch(pull, "GetAllLogFilePaths", return_value=log_files)
        with self.assertRaises(errors.CheckPathError):
            pull.SelectLogFileToPull(_ssh, input_file)

    def testFilterLogfiles(self):
        """test filer log file from black list."""
        # Filter out file name is "kernel".
        files = ["kernel.log", "logcat", "kernel"]
        expected_result = ["kernel.log", "logcat"]
        self.assertEqual(pull.FilterLogfiles(files), expected_result)

        # Filter out file extension is ".img".
        files = ["kernel.log", "system.img", "userdata.img", "launcher.log"]
        expected_result = ["kernel.log", "launcher.log"]
        self.assertEqual(pull.FilterLogfiles(files), expected_result)

    @mock.patch.object(pull, "PullFileFromInstance")
    def testRun(self, mock_pull_file):
        """test Run."""
        cfg = mock.MagicMock()
        args = mock.MagicMock()
        instance_obj = mock.MagicMock()
        # Test case with provided instance name.
        args.instance_name = "instance_1"
        args.file_name = "file1.log"
        args.no_prompt = True
        self.Patch(config, "GetAcloudConfig", return_value=cfg)
        self.Patch(list_instances, "GetInstancesFromInstanceNames",
                   return_value=[instance_obj])
        pull.Run(args)
        mock_pull_file.assert_has_calls([
            mock.call(cfg, instance_obj, args.file_name, args.no_prompt)])

        # Test case for user select one instance to pull log.
        selected_instance = mock.MagicMock()
        self.Patch(list_instances, "ChooseOneRemoteInstance",
                   return_value=selected_instance)
        args.instance_name = None
        pull.Run(args)
        mock_pull_file.assert_has_calls([
            mock.call(cfg, selected_instance, args.file_name, args.no_prompt)])


if __name__ == '__main__':
    unittest.main()
