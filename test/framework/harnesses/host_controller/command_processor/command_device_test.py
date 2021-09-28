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

import re
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from host_controller import common
from host_controller.command_processor import command_device


def cmd_util_side_effect(value, timeout=0, callback_on_timeout=None, *args):
    ret = ("", "", 0)
    if value == "adb devices":
        ret = ("List of devices attached\ndevice1\tdevice\ndevice2\tdevice",
               "", 0)
    elif value == "fastboot devices":
        ret = ("device3\tfastboot\n", "", 0)
    elif re.match("fastboot -s .* getvar product", value):
        ret = ("", "product: somefish", 0)
    return ret


class CommandDeviceTest(unittest.TestCase):
    """Tests for device command processor"""

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_device.cmd_utils")
    def testUpdateDevice(self, mock_cmd_utils, mock_console):
        command = command_device.CommandDevice()
        command._SetUp(mock_console)
        mock_host = mock.Mock()
        mock_host.hostname = "vtslab-001"
        mock_cmd_utils.ExecuteOneShellCommand.side_effect = cmd_util_side_effect
        command.UpdateDevice("vti", mock_host, False)
        mock_console._vti_endpoint_client.UploadDeviceInfo.assert_called_with(
            "vtslab-001", [{
                "status": mock.ANY,
                "serial": "device3",
                "product": "somefish"
            }, {
                "status": mock.ANY,
                "serial": "device1",
                "product": "somefish"
            }, {
                "status": mock.ANY,
                "serial": "device2",
                "product": "somefish"
            }])

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_device.cmd_utils")
    def testUpdateDeviceLeaseJob(self, mock_cmd_utils, mock_console):
        command = command_device.CommandDevice()
        command._SetUp(mock_console)
        mock_host = mock.Mock()
        mock_host.hostname = "vtslab-001"
        mock_cmd_utils.ExecuteOneShellCommand.side_effect = cmd_util_side_effect
        command.UpdateDevice("vti", mock_host, True)
        mock_console._job_in_queue.put.assert_called_with("lease")

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_device.cmd_utils")
    def testUpdateDeviceFromJobPool(self, mock_cmd_utils, mock_console):
        mock_console.GetSerials.return_value = ["device1", "device4"]
        command = command_device.CommandDevice()
        command._SetUp(mock_console)
        mock_host = mock.Mock()
        mock_host.hostname = "vtslab-001"
        mock_cmd_utils.ExecuteOneShellCommand.side_effect = cmd_util_side_effect
        command.UpdateDevice("vti", mock_host, False, from_job_pool=True)
        mock_console._vti_endpoint_client.UploadDeviceInfo.assert_called_with(
            "vtslab-001",
            [{
                'status': common._DEVICE_STATUS_DICT["no-response"],
                'serial': 'device4',
                'product': 'error'
            }, {
                'status': common._DEVICE_STATUS_DICT["online"],
                'serial': 'device1',
                'product': mock.ANY
            }])

    @mock.patch("host_controller.console.Console")
    def testCommandDeviceUpdateSingle(self, mock_console):
        command = command_device.CommandDevice()
        command.UpdateDevice = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("--update=single")
        self.assertIsNone(ret)
        command.UpdateDevice.assert_called_with("vti", mock.ANY, False, True,
                                                False)

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_device.threading")
    def testCommandDeviceUpdateSetSerial(self, mock_threading, mock_console):
        mock_thread = mock.Mock()
        mock_threading.Thread.return_value = mock_thread
        command = command_device.CommandDevice()
        command.UpdateDevice = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("--set_serial=device1,device2,device3")
        self.assertIsNone(ret)
        mock_console.SetSerials.assert_called_with(
            ["device1", "device2", "device3"])
        mock_threading.Thread.assert_called_with(
            args=("vti", mock.ANY, False, 30, True, False),
            target=command.UpdateDeviceRepeat)
        mock_thread.start.assert_called_with()

    @mock.patch("host_controller.console.Console")
    @mock.patch("host_controller.command_processor.command_device.threading")
    def testCommandDeviceUpdateStop(self, mock_threading, mock_console):
        mock_thread = mock.Mock()
        mock_threading.Thread.return_value = mock_thread
        command = command_device.CommandDevice()
        command.UpdateDevice = mock.Mock()
        command._SetUp(mock_console)
        ret = command._Run("")
        self.assertIsNone(ret)
        mock_threading.Thread.assert_called_with(
            args=("vti", mock.ANY, False, 30, True, False),
            target=command.UpdateDeviceRepeat)
        mock_thread.start.assert_called_with()
        ret = command._Run("--update=stop")
        self.assertIsNone(ret)
        self.assertFalse(mock_thread.keep_running)


if __name__ == "__main__":
    unittest.main()
