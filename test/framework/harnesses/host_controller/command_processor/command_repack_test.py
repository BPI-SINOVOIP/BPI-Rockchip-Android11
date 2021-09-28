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
from host_controller.command_processor import command_repack

_SAMPLE_COMMAND_REPACK = "repack --dest"


class CommandRepackTest(unittest.TestCase):
    """Tests for repack command processor"""

    @mock.patch("host_controller.console.Console")
    def testGetDestURLWithoutGSI(self, mock_console):
        mock_console.detailed_fetch_info = {
            "device": {
                "branch": "git_device_branch",
                "target": "device_userdebug",
                "build_id": "1234567"
            }
        }
        command = command_repack.CommandRepack()
        command._SetUp(mock_console)
        ret = command.GetDestURL("gs://bucket/base/url")

        self.assertEqual(ret,
                         "gs://bucket/base/url/device_branch/device_userdebug"
                         "/device_branch_device_userdebug_1234567.zip")

    @mock.patch("host_controller.console.Console")
    def testGetDestURLWithGSI(self, mock_console):
        mock_console.device_image_info = {common.GSI_ZIPFILE: ""}
        mock_console.detailed_fetch_info = {
            "device": {
                "branch": "git_device_branch",
                "target": "device-userdebug",
                "build_id": "1234567"
            },
            "gsi": {
                "branch": "git_aosp_gsi_branch",
                "target": "gsi-userdebug",
                "build_id": "2345678"
            }
        }

        command = command_repack.CommandRepack()
        command._SetUp(mock_console)
        ret = command.GetDestURL("gs://bucket/base/url")

        self.assertEqual(
            ret, "gs://bucket/base/url/device_branch/device-userdebug/"
            "aosp_gsi_branch/gsi-userdebug/device_branch_device-userdebug"
            "_1234567_aosp_gsi_branch_gsi-userdebug_2345678.zip")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_repack.logging")
    def testRepackFullDeviceImageAbsent(self, mock_logger, mock_console):
        mock_console.device_image_info = {}
        command = command_repack.CommandRepack()
        command._SetUp(mock_console)
        ret = command._Run("--dest=gs://bucket/path/")

        self.assertFalse(ret)
        mock_logger.error.assert_called_with(
            "please execute this command after fetching at least"
            " one device img set.")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_repack.zipfile")
    @mock.patch("host_controller.command_processor.command_repack.os")
    @mock.patch("host_controller.command_processor.command_repack.tempfile")
    @mock.patch("host_controller.command_processor.command_repack.shutil")
    def testRepackWithFullDeviceImage(self, mock_shutil, mock_tempfile,
                                      mock_os, mock_zipfile, mock_console):
        mock_zip_ref = mock.Mock()
        mock_zip_ref.__enter__ = mock.Mock(return_value=mock_zip_ref)
        mock_zip_ref.__exit__ = mock.Mock(return_value=None)
        mock_zipfile.ZipFile.return_value = mock_zip_ref
        mock_console.device_image_info = {
            common.FULL_ZIPFILE: "/path/to/full-zipfile.zip",
            "system.img": "/path/to/full-zipfile.zip.dir/system.img",
            "odm.img": "/path/to/full-zipfile.zip.dir/odm.img"
        }
        mock_os.getcwd.return_value = "/abs/path/to/current/dir"
        mock_os.path.split = os.path.split
        command = command_repack.CommandRepack()
        command.GetDestURL = mock.Mock(
            return_value="gs://dest/gs/url/file.zip")
        command._SetUp(mock_console)
        ret = command._Run("--dest=gs://bucket/path/")

        self.assertIsNone(ret)
        mock_zip_ref.extract.assert_called_with("android-info.txt",
                                                common.FULL_ZIPFILE_DIR)
        mock_zip_ref.write.assert_any_call(
            "/path/to/full-zipfile.zip.dir/system.img",
            "system.img",
            compress_type=mock.ANY)
        mock_zip_ref.write.assert_any_call(
            "/path/to/full-zipfile.zip.dir/odm.img",
            "odm.img",
            compress_type=mock.ANY)

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_repack.zipfile")
    @mock.patch("host_controller.command_processor.command_repack.os")
    @mock.patch("host_controller.command_processor.command_repack.tempfile")
    @mock.patch("host_controller.command_processor.command_repack.shutil")
    def testRepackWithAdditionalFiles(self, mock_shutil, mock_tempfile,
                                      mock_os, mock_zipfile, mock_console):
        mock_zip_ref = mock.Mock()
        mock_zip_ref.__enter__ = mock.Mock(return_value=mock_zip_ref)
        mock_zip_ref.__exit__ = mock.Mock(return_value=None)
        mock_zipfile.ZipFile.return_value = mock_zip_ref
        mock_console.device_image_info = {
            common.FULL_ZIPFILE: "/path/to/full-zipfile.zip",
            "system.img": "/path/to/full-zipfile.zip.dir/system.img",
            "odm.img": "/path/to/full-zipfile.zip.dir/odm.img"
        }
        mock_os.getcwd.return_value = "/abs/path/to/current/dir"
        mock_os.path.split = os.path.split
        mock_os.path.join = os.path.join
        mock_os.path.basename = os.path.basename
        command = command_repack.CommandRepack()
        command.GetDestURL = mock.Mock(
            return_value="gs://dest/gs/url/file.zip")
        command._SetUp(mock_console)
        ret = command._Run(
            "--dest=gs://bucket/path/ --additional_files /path/to/tmp/af1"
            " /path/to/tmp/af2")

        self.assertIsNone(ret)
        mock_zip_ref.extract.assert_called_with("android-info.txt",
                                                common.FULL_ZIPFILE_DIR)
        mock_zip_ref.write.assert_any_call(
            "/path/to/full-zipfile.zip.dir/system.img",
            "system.img",
            compress_type=mock.ANY)
        mock_zip_ref.write.assert_any_call(
            "/path/to/full-zipfile.zip.dir/odm.img",
            "odm.img",
            compress_type=mock.ANY)
        mock_zip_ref.write.assert_any_call(
            "/path/to/tmp/af1", "additional_file/af1", compress_type=mock.ANY)
        mock_zip_ref.write.assert_any_call(
            "/path/to/tmp/af2", "additional_file/af2", compress_type=mock.ANY)


if __name__ == "__main__":
    unittest.main()
