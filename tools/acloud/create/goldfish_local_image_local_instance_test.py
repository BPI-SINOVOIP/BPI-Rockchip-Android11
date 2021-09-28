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
"""Tests for GoldfishLocalImageLocalInstance."""

import os
import shutil
import tempfile
import unittest
import mock

from acloud import errors
import acloud.create.goldfish_local_image_local_instance as instance_module


class GoldfishLocalImageLocalInstance(unittest.TestCase):
    """Test GoldfishLocalImageLocalInstance methods."""

    _EXPECTED_DEVICES_IN_REPORT = [
        {
            "instance_name": "local-goldfish-instance",
            "ip": "127.0.0.1:5555",
            "adb_port": 5555
        }
    ]

    def setUp(self):
        self._goldfish = instance_module.GoldfishLocalImageLocalInstance()
        self._temp_dir = tempfile.mkdtemp()
        self._image_dir = os.path.join(self._temp_dir, "images")
        self._tool_dir = os.path.join(self._temp_dir, "tool")
        self._instance_dir = os.path.join(self._temp_dir, "instance")
        self._emulator_is_running = False
        self._mock_proc = mock.Mock()
        self._mock_proc.poll.side_effect = (
            lambda: None if self._emulator_is_running else 0)

        os.mkdir(self._image_dir)
        os.mkdir(self._tool_dir)

        # Create emulator binary
        self._emulator_path = os.path.join(self._tool_dir, "emulator",
                                           "emulator")
        self._CreateEmptyFile(self._emulator_path)

    def tearDown(self):
        shutil.rmtree(self._temp_dir, ignore_errors=True)

    @staticmethod
    def _CreateEmptyFile(path):
        parent_dir = os.path.dirname(path)
        if not os.path.exists(parent_dir):
            os.makedirs(parent_dir)
        with open(path, "w") as _:
            pass

    def _MockPopen(self, *_args, **_kwargs):
        self._emulator_is_running = True
        return self._mock_proc

    def _MockEmuCommand(self, *args):
        if not self._emulator_is_running:
            # Connection refused
            return 1

        if args == ("kill",):
            self._emulator_is_running = False
            return 0

        if args == ():
            return 0

        raise ValueError("Unexpected arguments " + str(args))

    def _SetUpMocks(self, mock_popen, mock_adb_tools, mock_utils,
                    mock_instance):
        mock_utils.IsSupportedPlatform.return_value = True

        mock_instance_object = mock.Mock(ip="127.0.0.1",
                                         adb_port=5555,
                                         console_port="5554",
                                         device_serial="unittest",
                                         instance_dir=self._instance_dir)
        # name is a positional argument of Mock().
        mock_instance_object.name = "local-goldfish-instance"
        mock_instance.return_value = mock_instance_object

        mock_adb_tools_object = mock.Mock()
        mock_adb_tools_object.EmuCommand.side_effect = self._MockEmuCommand
        mock_adb_tools.return_value = mock_adb_tools_object

        mock_popen.side_effect = self._MockPopen

    def _GetExpectedEmulatorArgs(self, *extra_args):
        cmd = [
            self._emulator_path, "-verbose", "-show-kernel", "-read-only",
            "-ports", "5554,5555",
            "-logcat-output",
            os.path.join(self._instance_dir, "logcat.txt"),
            "-stdouterr-file",
            os.path.join(self._instance_dir, "stdouterr.txt")
        ]
        cmd.extend(extra_args)
        return cmd

    # pylint: disable=protected-access
    @mock.patch("acloud.create.goldfish_local_image_local_instance.instance."
                "LocalGoldfishInstance")
    @mock.patch("acloud.create.goldfish_local_image_local_instance.utils")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "adb_tools.AdbTools")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "subprocess.Popen")
    def testCreateAVDInBuildEnvironment(self, mock_popen, mock_adb_tools,
                                        mock_utils, mock_instance):
        """Test _CreateAVD with build environment variables and files."""
        self._SetUpMocks(mock_popen, mock_adb_tools, mock_utils, mock_instance)

        self._CreateEmptyFile(os.path.join(self._image_dir,
                                           "system-qemu.img"))
        self._CreateEmptyFile(os.path.join(self._image_dir, "system",
                                           "build.prop"))

        mock_environ = {"ANDROID_EMULATOR_PREBUILTS":
                        os.path.join(self._tool_dir, "emulator")}

        mock_avd_spec = mock.Mock(flavor="phone",
                                  boot_timeout_secs=100,
                                  gpu=None,
                                  autoconnect=True,
                                  local_instance_id=1,
                                  local_image_dir=self._image_dir,
                                  local_system_image_dir=None,
                                  local_tool_dirs=[])

        # Test deleting an existing instance.
        self._emulator_is_running = True

        with mock.patch.dict("acloud.create."
                             "goldfish_local_image_local_instance.os.environ",
                             mock_environ, clear=True):
            report = self._goldfish._CreateAVD(mock_avd_spec, no_prompts=False)

        self.assertEqual(report.data.get("devices"),
                         self._EXPECTED_DEVICES_IN_REPORT)

        mock_instance.assert_called_once_with(1, avd_flavor="phone")

        self.assertTrue(os.path.isdir(self._instance_dir))

        mock_popen.assert_called_once()
        self.assertEqual(mock_popen.call_args[0][0],
                         self._GetExpectedEmulatorArgs())
        self._mock_proc.poll.assert_called()

    # pylint: disable=protected-access
    @mock.patch("acloud.create.goldfish_local_image_local_instance.instance."
                "LocalGoldfishInstance")
    @mock.patch("acloud.create.goldfish_local_image_local_instance.utils")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "adb_tools.AdbTools")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "subprocess.Popen")
    def testCreateAVDFromSdkRepository(self, mock_popen, mock_adb_tools,
                                       mock_utils, mock_instance):
        """Test _CreateAVD with SDK repository files."""
        self._SetUpMocks(mock_popen, mock_adb_tools, mock_utils, mock_instance)

        self._CreateEmptyFile(os.path.join(self._image_dir, "system.img"))
        self._CreateEmptyFile(os.path.join(self._image_dir, "build.prop"))

        mock_avd_spec = mock.Mock(flavor="phone",
                                  boot_timeout_secs=None,
                                  gpu=None,
                                  autoconnect=True,
                                  local_instance_id=2,
                                  local_image_dir=self._image_dir,
                                  local_system_image_dir=None,
                                  local_tool_dirs=[self._tool_dir])

        with mock.patch.dict("acloud.create."
                             "goldfish_local_image_local_instance.os.environ",
                             dict(), clear=True):
            report = self._goldfish._CreateAVD(mock_avd_spec, no_prompts=True)

        self.assertEqual(report.data.get("devices"),
                         self._EXPECTED_DEVICES_IN_REPORT)

        mock_instance.assert_called_once_with(2, avd_flavor="phone")

        self.assertTrue(os.path.isdir(self._instance_dir))

        mock_popen.assert_called_once()
        self.assertEqual(mock_popen.call_args[0][0],
                         self._GetExpectedEmulatorArgs())
        self._mock_proc.poll.assert_called()

        self.assertTrue(os.path.isfile(
            os.path.join(self._image_dir, "system", "build.prop")))

    # pylint: disable=protected-access
    @mock.patch("acloud.create.goldfish_local_image_local_instance.instance."
                "LocalGoldfishInstance")
    @mock.patch("acloud.create.goldfish_local_image_local_instance.utils")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "adb_tools.AdbTools")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "subprocess.Popen")
    def testCreateAVDTimeout(self, mock_popen, mock_adb_tools,
                             mock_utils, mock_instance):
        """Test _CreateAVD with SDK repository files and timeout error."""
        self._SetUpMocks(mock_popen, mock_adb_tools, mock_utils, mock_instance)
        mock_utils.PollAndWait.side_effect = errors.DeviceBootTimeoutError(
            "timeout")

        self._CreateEmptyFile(os.path.join(self._image_dir, "system.img"))
        self._CreateEmptyFile(os.path.join(self._image_dir, "build.prop"))

        mock_avd_spec = mock.Mock(flavor="phone",
                                  boot_timeout_secs=None,
                                  gpu=None,
                                  autoconnect=True,
                                  local_instance_id=2,
                                  local_image_dir=self._image_dir,
                                  local_system_image_dir=None,
                                  local_tool_dirs=[self._tool_dir])

        with mock.patch.dict("acloud.create."
                             "goldfish_local_image_local_instance.os.environ",
                             dict(), clear=True):
            report = self._goldfish._CreateAVD(mock_avd_spec, no_prompts=True)

        self.assertEqual(report.data.get("devices_failing_boot"),
                         self._EXPECTED_DEVICES_IN_REPORT)
        self.assertEqual(report.errors, ["timeout"])

    # pylint: disable=protected-access
    @mock.patch("acloud.create.goldfish_local_image_local_instance.instance."
                "LocalGoldfishInstance")
    @mock.patch("acloud.create.goldfish_local_image_local_instance.utils")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "adb_tools.AdbTools")
    @mock.patch("acloud.create.goldfish_local_image_local_instance."
                "subprocess.Popen")
    @mock.patch("acloud.create.goldfish_local_image_local_instance.ota_tools")
    def testCreateAVDWithMixedImages(self, mock_ota_tools, mock_popen,
                                     mock_adb_tools, mock_utils,
                                     mock_instance):
        """Test _CreateAVD with mixed images in build environment."""
        mock_ota_tools.FindOtaTools.return_value = self._tool_dir
        mock_ota_tools_object = mock.Mock()
        mock_ota_tools.OtaTools.return_value = mock_ota_tools_object
        mock_ota_tools_object.MkCombinedImg.side_effect = (
            lambda out_path, _conf, _get_img: self._CreateEmptyFile(out_path))

        self._SetUpMocks(mock_popen, mock_adb_tools, mock_utils, mock_instance)

        self._CreateEmptyFile(os.path.join(self._image_dir,
                                           "system-qemu.img"))
        self._CreateEmptyFile(os.path.join(self._image_dir, "system",
                                           "build.prop"))

        mock_environ = {"ANDROID_EMULATOR_PREBUILTS":
                        os.path.join(self._tool_dir, "emulator")}

        mock_utils.GetBuildEnvironmentVariable.side_effect = (
            lambda key: mock_environ[key])

        mock_avd_spec = mock.Mock(flavor="phone",
                                  boot_timeout_secs=None,
                                  gpu="auto",
                                  autoconnect=False,
                                  local_instance_id=3,
                                  local_image_dir=self._image_dir,
                                  local_system_image_dir="/unit/test",
                                  local_tool_dirs=[])

        with mock.patch.dict("acloud.create."
                             "goldfish_local_image_local_instance.os.environ",
                             mock_environ, clear=True):
            report = self._goldfish._CreateAVD(mock_avd_spec, no_prompts=True)

        self.assertEqual(report.data.get("devices"),
                         self._EXPECTED_DEVICES_IN_REPORT)

        mock_instance.assert_called_once_with(3, avd_flavor="phone")

        self.assertTrue(os.path.isdir(self._instance_dir))

        mock_ota_tools.FindOtaTools.assert_called_once()
        mock_ota_tools.OtaTools.assert_called_with(self._tool_dir)

        mock_ota_tools_object.BuildSuperImage.assert_called_once()
        self.assertEqual(mock_ota_tools_object.BuildSuperImage.call_args[0][1],
                         os.path.join(self._image_dir, "misc_info.txt"))

        mock_ota_tools_object.MakeDisabledVbmetaImage.assert_called_once()

        mock_ota_tools_object.MkCombinedImg.assert_called_once()
        self.assertEqual(
            mock_ota_tools_object.MkCombinedImg.call_args[0][1],
            os.path.join(self._image_dir, "system-qemu-config.txt"))

        mock_popen.assert_called_once()
        self.assertEqual(
            mock_popen.call_args[0][0],
            self._GetExpectedEmulatorArgs(
                "-gpu", "auto", "-no-window", "-qemu", "-append",
                "androidboot.verifiedbootstate=orange"))
        self._mock_proc.poll.assert_called()


if __name__ == "__main__":
    unittest.main()
