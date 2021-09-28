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
from host_controller.command_processor import command_upload


def side_effect(value):
    return value


class CommandUploadTest(unittest.TestCase):
    """Tests for upload command processor"""

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.open")
    @mock.patch("host_controller.command_processor.command_upload.cmd_utils")
    @mock.patch("host_controller.command_processor.command_upload.SuiteResMsg")
    @mock.patch("host_controller.command_processor.command_upload.SchedCfgMsg")
    def testUploadReportBootupErr(self, mock_sched_config_msg,
                                  mock_suite_res_msg, mock_cmd_util, mock_open,
                                  mock_console):
        mock_open.__enter__ = mock.Mock(return_value=mock_open)
        mock_open.__exit__ = mock.Mock(return_value=None)
        mock_cmd_util.ExecuteOneShellCommand = mock.Mock(
            return_value=("", "", 0))
        mock_console.vti_endpoint_client.CheckBootUpStatus.return_value = False
        mock_console.FormatString.side_effect = side_effect
        mock_console.fetch_info = {
            "branch": "git_device_branch",
            "target": "device-userdebug",
            "build_id": "1234567",
            "account_id": common._DEFAULT_ACCOUNT_ID,
        }
        mock_console.detailed_fetch_info = {
            "device": {
                "branch": "git_device_branch",
                "target": "device-userdebug",
                "build_id": "1234567",
                "account_id": common._DEFAULT_ACCOUNT_ID,
                "fetch_signed_build": False,
            },
            "gsi": {
                "branch": "git_aosp_gsi_branch",
                "target": "gsi-userdebug",
                "build_id": "2345678",
                "account_id": common._DEFAULT_ACCOUNT_ID_INTERNAL,
            }
        }
        mock_console.tmp_logdir = "tmp/log"
        mock_console.device_image_info = {
            "bootloader.img": "path/to/bootloader.img"
        }
        mock_pb2 = mock.Mock()
        mock_pb2.repacked_image_path = []
        mock_pb2.schedule_config.build_target = []
        mock_build_sched_config_pb2 = mock.Mock()
        mock_build_sched_config_pb2.test_schedule = []
        mock_test_sched_config_pb2 = mock.Mock()
        mock_suite_res_msg.TestSuiteResultMessage.return_value = mock_pb2
        mock_sched_config_msg.BuildScheduleConfigMessage.return_value = (
            mock_build_sched_config_pb2)
        mock_sched_config_msg.TestScheduleConfigMessage.return_value = (
            mock_test_sched_config_pb2)
        command = command_upload.CommandUpload()
        command._SetUp(mock_console)
        command.UploadReport("/path/to/bin/gsutil", "gs://report-bucket/",
                             "tmp/console.log", "tmp/result.log", "vts",
                             "some_plan")
        self.assertEqual(mock_pb2.build_id, "1234567")
        self.assertEqual(mock_pb2.suite_name, "vts")
        self.assertEqual(mock_pb2.suite_plan, "some_plan")
        self.assertEqual(mock_pb2.suite_build_number, mock_pb2.build_id)
        self.assertEqual(mock_pb2.build_system_fingerprint,
                         "git_aosp_gsi_branch/gsi-userdebug/2345678")
        self.assertEqual(mock_pb2.build_vendor_fingerprint,
                         "git_device_branch/device-userdebug/1234567")
        self.assertEqual(mock_pb2.repacked_image_path, ["{repack_path}"])
        self.assertEqual(mock_pb2.schedule_config.manifest_branch,
                         "git_device_branch")
        self.assertEqual(mock_pb2.schedule_config.pab_account_id,
                         common._DEFAULT_ACCOUNT_ID)

        self.assertTrue(mock_build_sched_config_pb2.has_bootloader_img)
        self.assertFalse(mock_build_sched_config_pb2.has_radio_img)

        self.assertEqual(mock_test_sched_config_pb2.gsi_branch,
                         "git_aosp_gsi_branch")
        self.assertEqual(mock_test_sched_config_pb2.gsi_build_target,
                         "gsi-userdebug")
        self.assertEqual(mock_test_sched_config_pb2.gsi_pab_account_id,
                         common._DEFAULT_ACCOUNT_ID_INTERNAL)
        mock_cmd_util.ExecuteOneShellCommand.assert_called_with(
            "/path/to/bin/gsutil cp tmp/log/{timestamp_time}.bin "
            "gs://report-bucket/{timestamp_time}.bin")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.open")
    @mock.patch("host_controller.command_processor.command_upload.cmd_utils")
    @mock.patch("host_controller.command_processor.command_upload.SuiteResMsg")
    @mock.patch("host_controller.command_processor.command_upload.os")
    @mock.patch("host_controller.command_processor.command_upload.xml_utils")
    @mock.patch("host_controller.command_processor.command_upload.SchedCfgMsg")
    def testUploadReportBootupOk(self, mock_sched_config_msg, mock_xml_util,
                                 mock_os, mock_suite_res_msg, mock_cmd_util,
                                 mock_open, mock_console):
        mock_open.__enter__ = mock.Mock(return_value=mock_open)
        mock_open.__exit__ = mock.Mock(return_value=None)
        mock_cmd_util.ExecuteOneShellCommand = mock.Mock(
            return_value=("", "", 0))
        mock_console.vti_endpoint_client.CheckBootUpStatus.return_value = True
        mock_console.FormatString.side_effect = side_effect
        mock_console.tmp_logdir = "tmp/log"
        mock_pb2 = mock.Mock()
        mock_pb2.repacked_image_path = []
        mock_pb2.schedule_config.build_target = []
        mock_build_sched_config_pb2 = mock.Mock()
        mock_build_sched_config_pb2.test_schedule = []
        mock_test_sched_config_pb2 = mock.Mock()
        mock_suite_res_msg.TestSuiteResultMessage.return_value = mock_pb2
        mock_sched_config_msg.BuildScheduleConfigMessage.return_value = (
            mock_build_sched_config_pb2)
        mock_sched_config_msg.TestScheduleConfigMessage.return_value = (
            mock_test_sched_config_pb2)
        mock_os.listdir.return_value = ["1", "2", "3"]
        mock_os.path.isdir.return_value = True
        mock_os.path.islink.return_value = False
        mock_os.path.join = os.path.join
        mock_os.path.basename = os.path.basename
        mock_xml_util.GetAttributes.return_value = {
            common._SUITE_NAME_ATTR_KEY:
            "vts",
            common._SUITE_PLAN_ATTR_KEY:
            "some_plan",
            common._SUITE_VERSION_ATTR_KEY:
            "8.0_R1",
            common._SUITE_BUILD_NUM_ATTR_KEY:
            "1234567",
            common._START_TIME_ATTR_KEY:
            "0",
            common._END_TIME_ATTR_KEY:
            "1",
            common._HOST_NAME_ATTR_KEY:
            "this-host",
            common._FINGERPRINT_ATTR_KEY:
            "git_device_branch/device-userdebug/1234567",
            common._SYSTEM_FINGERPRINT_ATTR_KEY:
            "git_aosp_gsi_branch/gsi-userdebug/2345678",
            common._VENDOR_FINGERPRINT_ATTR_KEY:
            "git_device_branch/device-userdebug/1234567",
            common._PASSED_ATTR_KEY:
            "1265",
            common._FAILED_ATTR_KEY:
            "43",
            common._MODULES_TOTAL_ATTR_KEY:
            "100",
            common._MODULES_DONE_ATTR_KEY:
            "98",
        }
        command = command_upload.CommandUpload()
        command._SetUp(mock_console)
        command.UploadReport("/path/to/bin/gsutil", "gs://report-bucket/",
                             "tmp/console.log", "tmp/vts/results", "vts",
                             "some_plan")
        self.assertEqual(mock_pb2.build_id, "1234567")
        self.assertEqual(mock_pb2.suite_name, "vts")
        self.assertEqual(mock_pb2.suite_plan, "some_plan")
        self.assertEqual(mock_pb2.suite_version, "8.0_R1")
        self.assertEqual(mock_pb2.suite_build_number, mock_pb2.build_id)
        self.assertEqual(mock_pb2.start_time, 0)
        self.assertEqual(mock_pb2.end_time, 1)
        self.assertEqual(mock_pb2.host_name, "this-host")
        self.assertEqual(mock_pb2.build_system_fingerprint,
                         "git_aosp_gsi_branch/gsi-userdebug/2345678")
        self.assertEqual(mock_pb2.build_vendor_fingerprint,
                         "git_device_branch/device-userdebug/1234567")
        self.assertEqual(mock_pb2.passed_test_case_count, 1265)
        self.assertEqual(mock_pb2.failed_test_case_count, 43)
        self.assertEqual(mock_pb2.modules_total, 100)
        self.assertEqual(mock_pb2.modules_done, 98)
        self.assertEqual(mock_pb2.repacked_image_path, ["{repack_path}"])
        mock_cmd_util.ExecuteOneShellCommand.assert_called_with(
            "/path/to/bin/gsutil cp tmp/log/{timestamp_time}.bin "
            "gs://report-bucket/{timestamp_time}.bin")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.os")
    @mock.patch("host_controller.command_processor.command_upload.logging")
    def testUploadReportResultDirAbsent(self, mock_logger, mock_os,
                                        mock_console):
        mock_console.vti_endpoint_client.CheckBootUpStatus.return_value = True
        mock_console.FormatString.side_effect = side_effect
        mock_console.tmp_logdir = "tmp/log"
        mock_os.listdir.return_value = []
        command = command_upload.CommandUpload()
        command._SetUp(mock_console)
        ret = command.UploadReport("/path/to/bin/gsutil",
                                   "gs://report-bucket/", "tmp/console.log",
                                   "tmp/result.log", "vts", "some_plan")
        self.assertFalse(ret)
        mock_logger.error.assert_called_with("No test result found.")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.gcs_utils")
    @mock.patch("host_controller.command_processor.command_upload.logging")
    def testCommandUploadGsutilAbsent(self, mock_logger, mock_gcs_util,
                                      mock_console):
        mock_gcs_util.GetGsutilPath.return_value = ""
        command = command_upload.CommandUpload()
        command.UploadReport = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("--src=tmp/result.log --dest=gs://report-bucket/")
        self.assertFalse(ret)
        mock_logger.error.assert_called_with(
            "Please check gsutil is installed and on your PATH")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.gcs_utils")
    @mock.patch("host_controller.command_processor.command_upload.logging")
    def testCommandUploadLatestSrc(self, mock_logger, mock_gcs_util,
                                   mock_console):
        mock_gcs_util.GetGsutilPath.return_value = "/path/to/bin/gsutil"
        command = command_upload.CommandUpload()
        command.UploadReport = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run(
            "--src=latest-something.img --dest=gs://report-bucket/")
        self.assertFalse(ret)
        mock_logger.error.assert_called_with(
            "Unable to find something.img in device_image_info")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.gcs_utils")
    @mock.patch("host_controller.command_processor.command_upload.os")
    def testCommandUploadLatestLegitSrc(self, mock_os, mock_gcs_util,
                                        mock_console):
        mock_os.path.isfile.return_value = True
        mock_console.device_image_info = {"system.img": "path/to/system.img"}
        mock_console.FormatString.side_effect = side_effect
        mock_gcs_util.GetGsutilPath.return_value = "/path/to/bin/gsutil"
        command = command_upload.CommandUpload()
        command.UploadReport = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run(
            "--src=latest-system.img --dest=gs://report-bucket/dir "
            "--clear_dest")
        self.assertIsNone(ret)
        mock_gcs_util.Remove.assert_called_with(
            "/path/to/bin/gsutil", "gs://report-bucket/dir", recursive=True)
        mock_gcs_util.Copy.assert_called_with('/path/to/bin/gsutil',
                                              'path/to/system.img',
                                              'gs://report-bucket/dir')

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.gcs_utils")
    @mock.patch("host_controller.command_processor.command_upload.os")
    @mock.patch("host_controller.command_processor.command_upload.logging")
    def testCommandUploadFalseDest(self, mock_logger, mock_os, mock_gcs_util,
                                   mock_console):
        mock_os.path.isfile.return_value = True
        mock_console.device_image_info = {"system.img": "path/to/system.img"}
        mock_console.FormatString.side_effect = side_effect
        mock_gcs_util.GetGsutilPath.return_value = "/path/to/bin/gsutil"
        command = command_upload.CommandUpload()
        command.UploadReport = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("--src=latest-system.img --dest=/report-bucket/")
        self.assertFalse(ret)
        mock_logger.error.assert_called_with(
            "/report-bucket/ is not correct GCS url.")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_upload.gcs_utils")
    @mock.patch("host_controller.command_processor.command_upload.os")
    def testCommandUploadMultipleFiles(self, mock_os, mock_gcs_util,
                                       mock_console):
        mock_os.path.isfile.return_value = True
        mock_console.FormatString.side_effect = side_effect
        mock_gcs_util.GetGsutilPath.return_value = "/path/to/bin/gsutil"
        command = command_upload.CommandUpload()
        command.UploadReport = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("--src=result.zip --dest=gs://report-bucket/")
        self.assertIsNone(ret)
        mock_gcs_util.Copy.assert_called_with(
            '/path/to/bin/gsutil', 'result.zip', 'gs://report-bucket/')


if __name__ == "__main__":
    unittest.main()
