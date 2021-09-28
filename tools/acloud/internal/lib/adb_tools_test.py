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
"""Tests for AdbTools."""

import subprocess
import unittest
import mock

from acloud import errors
from acloud.internal.lib import adb_tools
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import utils


class AdbToolsTest(driver_test_lib.BaseDriverTest):
    """Test adb functions."""
    DEVICE_ALIVE = ("List of devices attached\n"
                    "127.0.0.1:48451 device product:aosp_cf_x86_phone "
                    "model:Cuttlefish_x86_phone device:vsoc_x86 "
                    "transport_id:98")
    DEVICE_OFFLINE = ("List of devices attached\n"
                      "127.0.0.1:48451 offline")
    DEVICE_NONE = ("List of devices attached")

    # pylint: disable=no-member
    def testGetAdbConnectionStatus(self):
        """Test get adb connection status."""
        fake_adb_port = "48451"
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_ALIVE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.GetAdbConnectionStatus(), "device")

        self.Patch(subprocess, "check_output", return_value=self.DEVICE_OFFLINE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.GetAdbConnectionStatus(), "offline")

        self.Patch(subprocess, "check_output", return_value=self.DEVICE_NONE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.GetAdbConnectionStatus(), None)

    def testGetAdbConnectionStatusFail(self):
        """Test adb connect status fail."""
        fake_adb_port = None
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_NONE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.GetAdbConnectionStatus(), None)

    def testGetAdbInformation(self):
        """Test get adb information."""
        fake_adb_port = "48451"
        dict_device = {'product': 'aosp_cf_x86_phone',
                       'usb': None,
                       'adb_status': 'device',
                       'device': 'vsoc_x86',
                       'model': 'Cuttlefish_x86_phone',
                       'transport_id': '98'}
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_ALIVE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.device_information, dict_device)

        dict_office = {'product': None,
                       'usb': None,
                       'adb_status': 'offline',
                       'device': None,
                       'model': None,
                       'transport_id': None}
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_OFFLINE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.device_information, dict_office)

        dict_none = {'product': None,
                     'usb': None,
                     'adb_status': None,
                     'device': None,
                     'model': None,
                     'transport_id': None}
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_NONE)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        self.assertEqual(adb_cmd.device_information, dict_none)

    # pylint: disable=no-member,protected-access
    def testConnectAdb(self):
        """Test connect adb."""
        fake_adb_port = "48451"
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_ALIVE)
        self.Patch(subprocess, "check_call", return_value=True)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        adb_cmd.ConnectAdb()
        self.assertEqual(adb_cmd.IsAdbConnectionAlive(), True)
        subprocess.check_call.assert_not_called()

        self.Patch(subprocess, "check_output", return_value=self.DEVICE_OFFLINE)
        self.Patch(subprocess, "check_call", return_value=True)
        subprocess.check_call.call_count = 0
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        adb_cmd.ConnectAdb()
        self.assertEqual(adb_cmd.IsAdbConnectionAlive(), False)
        subprocess.check_call.assert_called_with([adb_cmd._adb_command,
                                                  adb_tools._ADB_CONNECT,
                                                  adb_cmd._device_serial])

    # pylint: disable=no-member,protected-access
    def testDisconnectAdb(self):
        """Test disconnect adb."""
        fake_adb_port = "48451"
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_ALIVE)
        self.Patch(subprocess, "check_call", return_value=True)
        adb_cmd = adb_tools.AdbTools(fake_adb_port)

        self.assertEqual(adb_cmd.IsAdbConnected(), True)
        subprocess.check_call.assert_not_called()

        self.Patch(subprocess, "check_output", side_effect=[self.DEVICE_OFFLINE,
                                                            self.DEVICE_NONE])
        self.Patch(subprocess, "check_call", return_value=True)
        subprocess.check_call.call_count = 0
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        adb_cmd.DisconnectAdb()
        self.assertEqual(adb_cmd.IsAdbConnected(), False)
        subprocess.check_call.assert_called_with([adb_cmd._adb_command,
                                                  adb_tools._ADB_DISCONNECT,
                                                  adb_cmd._device_serial])

        self.Patch(subprocess, "check_output", return_value=self.DEVICE_NONE)
        self.Patch(subprocess, "check_call", return_value=True)
        subprocess.check_call.call_count = 0
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        adb_cmd.DisconnectAdb()
        self.assertEqual(adb_cmd.IsAdbConnected(), False)
        subprocess.check_call.assert_not_called()

        # test raise error if adb still alive after disconnect
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_OFFLINE)
        self.Patch(subprocess, "check_call", return_value=True)
        subprocess.check_call.call_count = 0
        adb_cmd = adb_tools.AdbTools(fake_adb_port)
        with self.assertRaises(errors.AdbDisconnectFailed):
            adb_cmd.DisconnectAdb()

    def testEmuCommand(self):
        """Test emu command."""
        fake_adb_port = "48451"
        fake_device_serial = "fake_device_serial"
        self.Patch(utils, "FindExecutable", return_value="path/adb")
        self.Patch(subprocess, "check_output", return_value=self.DEVICE_NONE)

        mock_popen_obj = mock.Mock(returncode=1)
        self.Patch(subprocess, "Popen", return_value=mock_popen_obj)

        adb_cmd = adb_tools.AdbTools(adb_port=fake_adb_port,
                                     device_serial=fake_device_serial)
        returncode = adb_cmd.EmuCommand("unit", "test")
        self.assertEqual(returncode, 1)
        subprocess.Popen.assert_called_once_with(
            ["path/adb", "-s", "fake_device_serial", "emu", "unit", "test"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        mock_popen_obj.communicate.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
