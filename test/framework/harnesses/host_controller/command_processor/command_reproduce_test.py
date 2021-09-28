#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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
#

import os
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from host_controller import common
from host_controller.command_processor import command_reproduce


def emit_fetch_commands(**kwargs):
    gsi = "gsi_branch" in kwargs
    return [], gsi


def emit_flash_commands(gsi, **kwargs):
    return []


class CommandReproduceTest(unittest.TestCase):
    """Tests for reproduce command processor"""

    def setUp(self):
        mock_console = mock.Mock()
        self._command = command_reproduce.CommandReproduce()
        self._command._SetUp(mock_console)

    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testGenerateSetupCommandsNoFetchInfo(self, mock_logging):
        mock_msg = mock.Mock()
        mock_msg.schedule_config.manifest_branch = ""
        ret = self._command.GenerateSetupCommands(mock_msg,
                                                  ["serial1", "serial2"])
        self.assertEqual(ret, [])
        mock_logging.error.assert_called_with(
            "Report contains no fetch information. "
            "Aborting pre-test setups on the device(s).")

    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testGenerateSetupCommandsNoSerial(self, mock_logging):
        mock_msg = mock.Mock()
        mock_msg.schedule_config.manifest_branch = "some_branch"
        ret = self._command.GenerateSetupCommands(mock_msg, [])
        self.assertEqual(ret, [])
        mock_logging.error.assert_called_with(
            "Device serial number(s) not given. "
            "Aborting pre-test setups on the device(s).")

    @mock.patch("host_controller.command_processor.command_reproduce.imp")
    def testGenerateSetupCommands(self, mock_imp):
        mock_campiagn = mock.Mock()
        mock_campiagn.EmitFetchCommands.side_effect = emit_fetch_commands
        mock_campiagn.EmitFlashCommands.side_effect = emit_flash_commands
        mock_imp.load_source.return_value = mock_campiagn
        mock_msg = mock.Mock()
        mock_msg.suite_name = "vts"
        mock_msg.suite_plan = "vts"
        mock_msg.vendor_build_id = "1234567"
        mock_msg.gsi_build_id = "2345678"
        mock_msg.build_id = "3456789"
        mock_msg.branch = "git_whatever-release"
        mock_msg.target = "test_suites_bitness"
        mock_msg.schedule_config.manifest_branch = "git_whatever-release"
        mock_msg.schedule_config.pab_account_id = common._DEFAULT_ACCOUNT_ID_INTERNAL
        mock_msg.schedule_config.build_target = []
        mock_build_target_msg = mock.Mock()
        mock_build_target_msg.name = "somefish-userdebug"
        mock_build_target_msg.test_schedule = []
        mock_build_target_msg.require_signed_device_build = False
        mock_build_target_msg.has_bootloader_img = True
        mock_build_target_msg.has_radio_img = True
        mock_test_schedule_msg = mock.Mock()
        mock_test_schedule_msg.gsi_branch = "git_whatever-gsi"
        mock_test_schedule_msg.gsi_build_target = "aosp_bitness-userdebug"
        mock_test_schedule_msg.gsi_pab_account_id = common._DEFAULT_ACCOUNT_ID
        mock_test_schedule_msg.test_pab_account_id = common._DEFAULT_ACCOUNT_ID
        mock_build_target_msg.test_schedule.append(mock_test_schedule_msg)
        mock_msg.schedule_config.build_target.append(mock_build_target_msg)
        self._command.GenerateSetupCommands(mock_msg, ["serial1", "serial2"])
        mock_campiagn.EmitFetchCommands.assert_called_with(
            build_id="1234567",
            build_storage_type=1,
            build_target="somefish-userdebug",
            gsi_branch="git_whatever-gsi",
            gsi_build_id="2345678",
            gsi_build_target="aosp_bitness-userdebug",
            gsi_pab_account_id="543365459",
            gsi_storage_type=1,
            gsi_vendor_version=mock.ANY,
            has_bootloader_img=True,
            has_radio_img=True,
            manifest_branch="git_whatever-release",
            pab_account_id="541462473",
            require_signed_device_build=False,
            serial=["serial1", "serial2"],
            shards="2",
            test_branch="git_whatever-release",
            test_build_id="3456789",
            test_build_target="test_suites_bitness",
            test_name="vts/vts",
            test_pab_account_id="543365459")
        mock_campiagn.EmitFlashCommands.assert_called_with(
            True,
            build_id="1234567",
            build_storage_type=1,
            build_target="somefish-userdebug",
            gsi_branch="git_whatever-gsi",
            gsi_build_id="2345678",
            gsi_build_target="aosp_bitness-userdebug",
            gsi_pab_account_id="543365459",
            gsi_storage_type=1,
            gsi_vendor_version=mock.ANY,
            has_bootloader_img=True,
            has_radio_img=True,
            manifest_branch="git_whatever-release",
            pab_account_id="541462473",
            require_signed_device_build=False,
            serial=["serial1", "serial2"],
            shards="2",
            test_branch="git_whatever-release",
            test_build_id="3456789",
            test_build_target="test_suites_bitness",
            test_name="vts/vts",
            test_pab_account_id="543365459")

    def testGenerateTestSuiteFetchCommandGCS(self):
        report_msg = mock.Mock()
        report_msg.branch = "gs://bucket/path/to/vts/release"
        report_msg.target = "android-vts.zip"
        report_msg.suite_name = "VTS"
        ret = self._command.GenerateTestSuiteFetchCommand(report_msg)
        self.assertEqual(
            ret, "fetch --type=gcs "
            "--path=gs://bucket/path/to/vts/release/android-vts.zip "
            "--set_suite_as=vts")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.SchedCfgMsg")
    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testGenerateTestSuiteFetchCommandIndexError(self, mock_logging,
                                                    mock_sched_cfg_msg):
        report_msg = mock.Mock()
        report_msg.branch = "git_whatever-release"
        report_msg.target = "test_suites_bitness"
        report_msg.build_id = "1234567"
        report_msg.suite_name = "VTS"
        report_msg.schedule_config.build_target = []
        mock_test_schedule_msg = mock.Mock()
        mock_test_schedule_msg.test_pab_account_id = "1234567898765"
        mock_sched_cfg_msg.TestScheduleConfigMessage.return_value = (
            mock_test_schedule_msg)
        ret = self._command.GenerateTestSuiteFetchCommand(report_msg)
        mock_logging.exception.assert_called()
        self.assertEqual(ret, "fetch --type=pab --branch=git_whatever-release "
                         "--target=test_suites_bitness --build_id=1234567 "
                         "--artifact_name=android-vts.zip "
                         "--account_id=1234567898765")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.SchedCfgMsg")
    def testGenerateTestSuiteFetchCommandPAB(self, mock_sched_cfg_msg):
        report_msg = mock.Mock()
        report_msg.branch = "git_whatever-release"
        report_msg.target = "test_suites_bitness"
        report_msg.build_id = "1234567"
        report_msg.suite_name = "VTS"
        mock_build_target_msg = mock.Mock()
        mock_test_schedule_msg = mock.Mock()
        mock_test_schedule_msg.test_pab_account_id = "987654321"
        mock_build_target_msg.test_schedule = []
        mock_build_target_msg.test_schedule.append(mock_test_schedule_msg)
        report_msg.schedule_config.build_target = []
        report_msg.schedule_config.build_target.append(mock_build_target_msg)
        mock_sched_cfg_msg.TestScheduleConfigMessage.return_value = (
            mock_test_schedule_msg)

        ret = self._command.GenerateTestSuiteFetchCommand(report_msg)
        self.assertEqual(ret, "fetch --type=pab --branch=git_whatever-release "
                         "--target=test_suites_bitness --build_id=1234567 "
                         "--artifact_name=android-vts.zip "
                         "--account_id=987654321")

    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testGetResultFromGCSNoTestSuite(self, mock_logging):
        report_msg = mock.Mock()
        report_msg.result_path = "gs://bucket/path/to/log/files"
        self._command.console.test_suite_info = {}
        ret = self._command.GetResultFromGCS("/mock_bin/gsutil", report_msg,
                                             "vts")
        self.assertFalse(ret)
        mock_logging.exception.assert_called()

    @mock.patch("host_controller.command_processor.command_reproduce.os")
    def testGetResultFromGCSMkdirResults(self, mock_os):
        report_msg = mock.Mock()
        report_msg.result_path = "gs://bucket/path/to/log/files"
        self._command.console.test_suite_info = {
            "vts": "tmp/android-vts/tools/vts-tradefed"
        }
        mock_os.path.exists.return_value = False
        mock_os.path.join = os.path.join
        mock_os.path.dirname = os.path.dirname
        self._command.GetResultFromGCS("/mock_bin/gsutil", report_msg, "vts")
        mock_os.mkdir.assert_called_with("tmp/android-vts/tools/../results")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch("host_controller.command_processor.command_reproduce.os")
    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testGetResultFromGCSNoResult(self, mock_logging, mock_os,
                                     mock_gcs_util):
        report_msg = mock.Mock()
        report_msg.result_path = "gs://bucket/path/to/log/files"
        self._command.console.test_suite_info = {
            "vts": "tmp/android-vts/tools/vts-tradefed"
        }
        mock_gcs_util.List.return_value = [
            "some_log1.zip", "some_log2.zip", "not_a_result.zip"
        ]
        ret = self._command.GetResultFromGCS("/mock_bin/gsutil", report_msg,
                                             "vts")
        self.assertFalse(ret)

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch("host_controller.command_processor.command_reproduce.os")
    @mock.patch("host_controller.command_processor.command_reproduce.zipfile")
    def testGetResultFromGCS(self, mock_zipfile, mock_os, mock_gcs_util):
        report_msg = mock.Mock()
        report_msg.result_path = "gs://bucket/path/to/log/files"
        self._command.console.test_suite_info = {
            "vts": "tmp/android-vts/tools/vts-tradefed"
        }
        mock_zip_ref = mock.Mock()
        mock_zip_ref.__enter__ = mock.Mock(return_value=mock_zip_ref)
        mock_zip_ref.__exit__ = mock.Mock(return_value=None)
        mock_zipfile.ZipFile.return_value = mock_zip_ref
        mock_gcs_util.List.return_value = [
            "some_log1.zip", "some_log2.zip", "results_some_hash.zip"
        ]
        mock_gcs_util.Copy.return_value = True
        mock_os.path.join = os.path.join
        mock_os.path.exists.return_value = False
        mock_os.path.dirname = os.path.dirname
        ret = self._command.GetResultFromGCS("/mock_bin/gsutil", report_msg,
                                             "vts")
        self.assertTrue(ret)
        mock_zip_ref.extractall.assert_called_with(
            "tmp/android-vts/tools/../results")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testCommandReproduceGsutilAbsent(self, mock_logging, mock_gcs_util):
        mock_gcs_util.GetGsutilPath.return_value = ""
        ret = self._command._Run(
            "--report_path=gs://bucket/path/to/report/file")
        self.assertFalse(ret)
        mock_logging.error.assert_called_with(
            "Please check whether gsutil is installed and on your PATH")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch("host_controller.command_processor.command_reproduce.logging")
    def testCommandReproduceInvalidURL(self, mock_logging, mock_gcs_util):
        mock_gcs_util.GetGsutilPath.return_value = "/mock_bin/gsutil"
        ret = self._command._Run("--report_path=/some/path/to/report/file")
        self.assertFalse(ret)
        mock_logging.error.assert_called_with("%s is not a valid GCS path.",
                                              "/some/path/to/report/file")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch(
        "host_controller.command_processor.command_reproduce.open",
        new_callable=mock.mock_open)
    @mock.patch("host_controller.command_processor.command_reproduce.imp")
    def testCommandReproduceAutomatedRetry(self, mock_imp, mock_open,
                                           mock_gcs_util):
        mock_gcs_util.GetGsutilPath.return_value = "/mock_bin/gsutil"
        mock_gcs_util.IsGcsFile.return_value = True
        mock_gcs_util.Copy = mock.Mock(return_value=True)
        command_reproduce.SuiteResMsg.ParseFromString = mock.Mock(
            return_value={})
        self._command.console.test_suite_info = {
            "vts": "tmp/android-vts/tools/vts-tradefed"
        }
        mock_campiagn = mock.Mock()
        mock_imp.load_source.return_value = mock_campiagn
        self._command.ReplaceVars = mock.Mock(return_value="tmp")
        self._command.GetResultFromGCS = mock.Mock(return_value=True)
        self._command.GenerateSetupCommands = mock.Mock(return_value=[])
        self._command.GenerateTestSuiteFetchCommand = mock.Mock(
            return_value=[])
        ret = self._command._Run("--report_path=gs://some/path/to/report/file"
                                 " --serial=device1 --automated_retry")
        self.assertIsNone(ret)
        self._command.console.onecmd.assert_called_with(
            "retry --suite vts  --serial device1")

    @mock.patch(
        "host_controller.command_processor.command_reproduce.gcs_utils")
    @mock.patch(
        "host_controller.command_processor.command_reproduce.open",
        new_callable=mock.mock_open)
    @mock.patch("host_controller.command_processor.command_reproduce.imp")
    def testCommandReproduceAutomatedRetryShardOption(
            self, mock_imp, mock_open, mock_gcs_util):
        mock_gcs_util.GetGsutilPath.return_value = "/mock_bin/gsutil"
        mock_gcs_util.IsGcsFile.return_value = True
        mock_gcs_util.Copy = mock.Mock(return_value=True)
        command_reproduce.SuiteResMsg.ParseFromString = mock.Mock()
        self._command.console.test_suite_info = {
            "vts": "tmp/android-vts/tools/vts-tradefed"
        }
        mock_campiagn = mock.Mock()
        mock_campiagn.GetVersion.return_value = 8.0
        mock_imp.load_source.return_value = mock_campiagn
        self._command.ReplaceVars = mock.Mock(return_value="tmp")
        self._command.GetResultFromGCS = mock.Mock(return_value=True)
        self._command.GenerateSetupCommands = mock.Mock(return_value=[])
        self._command.GenerateTestSuiteFetchCommand = mock.Mock(
            return_value=[])
        ret = self._command._Run(
            "--suite=vts --report_path=gs://some/path/to/report/file"
            " --serial=device1,device2 --automated_retry")
        self.assertIsNone(ret)
        self._command.console.onecmd.assert_called_with(
            "retry --suite vts  --shards 2 "
            "--serial device1 --serial device2")


if __name__ == "__main__":
    unittest.main()
