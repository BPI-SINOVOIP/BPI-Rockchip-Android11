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

import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from host_controller import common
from host_controller.command_processor import command_dut


class CommandDUTTest(unittest.TestCase):
    """Tests for DUT command processor"""

    def setUp(self):
        """Creates CommandSheet."""
        self._command = command_dut.CommandDUT()
        mock_console = mock.Mock()
        mock_console.device_status = {}
        self._command._SetUp(mock_console)

    def testSetOtherVolumesWithoutVersionInfo(self):
        mock_adb_proxy = mock.Mock()
        self._command.SetOtherVolumes(mock_adb_proxy, 5)
        self.assertEqual(mock_adb_proxy.shell.mock_calls, [
            mock.call("service call audio 3 i32 0 i32 5 i32 1"),
            mock.call("service call audio 3 i32 2 i32 5 i32 1"),
            mock.call("service call audio 3 i32 3 i32 5 i32 1"),
            mock.call("service call audio 3 i32 4 i32 5 i32 1"),
        ])

    def testSetOtherVolumesWithVersionInfo(self):
        mock_adb_proxy = mock.Mock()
        self._command.SetOtherVolumes(mock_adb_proxy, 0, 8.1)
        self.assertEqual(mock_adb_proxy.shell.mock_calls, [
            mock.call("service call audio 3 i32 0 i32 0 i32 1"),
            mock.call("service call audio 3 i32 2 i32 0 i32 1"),
            mock.call("service call audio 3 i32 3 i32 0 i32 1"),
            mock.call("service call audio 3 i32 4 i32 0 i32 1"),
        ])
        mock_adb_proxy.shell.mock_calls = []
        self._command.SetOtherVolumes(mock_adb_proxy, 10, 9.0)
        self.assertEqual(mock_adb_proxy.shell.mock_calls, [
            mock.call("service call audio 7 i32 0 i32 10 i32 1"),
            mock.call("service call audio 7 i32 2 i32 10 i32 1"),
            mock.call("service call audio 7 i32 3 i32 10 i32 1"),
            mock.call("service call audio 7 i32 4 i32 10 i32 1"),
        ])

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.logging")
    def testCommandDUTBootupFail(self, mock_logging, mock_android_device):
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = False
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run("--serial device1 --operation wifi_on")
        self.assertFalse(ret)
        mock_logging.error.assert_called_with("Device %s failed to bootup.",
                                              "device1")

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.adb")
    def testCommandDUTWifiOnWithoutAP(self, mock_adb, mock_android_device):
        mock_adb_proxy = mock.Mock()
        mock_adb.AdbProxy.return_value = mock_adb_proxy
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = True
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run("--serial device1 --operation wifi_on")
        self.assertIsNone(ret)
        mock_adb_proxy.root.assert_called_once()
        mock_adb_proxy.shell.assert_called_with("svc wifi enable")
        mock_adb_proxy.install.assert_not_called()

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.adb")
    def testCommandDUTWifiOn(self, mock_adb, mock_android_device):
        mock_adb_proxy = mock.Mock()
        mock_adb.AdbProxy.return_value = mock_adb_proxy
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = True
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run("--serial device1 --operation wifi_on --ap %s"
                                 % common._DEFAULT_WIFI_AP)
        self.assertIsNone(ret)
        mock_adb_proxy.root.assert_called_once()
        mock_adb_proxy.shell.assert_any_call("svc wifi enable")
        mock_adb_proxy.install.assert_called_with(
            "../testcases/DATA/app/WifiUtil/WifiUtil.apk")
        mock_adb_proxy.shell.assert_called_with(
            "am instrument -e method \"connectToNetwork\" -e ssid GoogleGuest "
            "-w com.android.tradefed.utils.wifi/.WifiUtil")

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.adb")
    def testCommandDUTWifiOff(self, mock_adb, mock_android_device):
        mock_adb_proxy = mock.Mock()
        mock_adb.AdbProxy.return_value = mock_adb_proxy
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = True
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run("--serial device1 --operation wifi_off")
        self.assertIsNone(ret)
        mock_adb_proxy.root.assert_called_once()
        mock_adb_proxy.shell.assert_any_call("svc wifi disable")

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.adb")
    def testCommandDUTVolumeMute(self, mock_adb, mock_android_device):
        self._command.SetOtherVolumes = mock.Mock()
        mock_adb_proxy = mock.Mock()
        mock_adb.AdbProxy.return_value = mock_adb_proxy
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = True
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run("--serial device1 --operation volume_mute")
        self.assertIsNone(ret)
        mock_adb_proxy.root.assert_called_once()
        self.assertEqual(mock_adb_proxy.shell.mock_calls,
                         [mock.call("input keyevent 25")] * 30)
        self._command.SetOtherVolumes.assert_called_with(mock.ANY, 0, 8.0)

    @mock.patch("host_controller.command_processor.command_dut.android_device")
    @mock.patch("host_controller.command_processor.command_dut.adb")
    def testCommandDUTVolumeMax(self, mock_adb, mock_android_device):
        self._command.SetOtherVolumes = mock.Mock()
        mock_adb_proxy = mock.Mock()
        mock_adb.AdbProxy.return_value = mock_adb_proxy
        mock_device = mock.Mock()
        mock_device.waitForBootCompletion.return_value = True
        mock_android_device.AndroidDevice.return_value = mock_device
        ret = self._command._Run(
            "--serial device1 --operation volume_max --version 9.0")
        self.assertIsNone(ret)
        mock_adb_proxy.root.assert_called_once()
        self.assertEqual(mock_adb_proxy.shell.mock_calls,
                         [mock.call("input keyevent 24")] * 30)
        self._command.SetOtherVolumes.assert_called_with(mock.ANY, 30, 9.0)


if __name__ == "__main__":
    unittest.main()
