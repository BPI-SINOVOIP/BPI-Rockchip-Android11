#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import unittest

from acts.test_utils.instrumentation.device.command.adb_command_types import \
    DeviceBinaryCommandSeries
from acts.test_utils.instrumentation.device.command.adb_command_types import \
    DeviceGServices
from acts.test_utils.instrumentation.device.command.adb_command_types import \
    DeviceSetprop
from acts.test_utils.instrumentation.device.command.adb_command_types import \
    DeviceSetting
from acts.test_utils.instrumentation.device.command.adb_command_types import \
    DeviceState


class AdbCommandTypesTest(unittest.TestCase):

    def test_device_state(self):
        """Tests that DeviceState returns the correct ADB command with
        set_value.
        """
        base_cmd = 'run command with vals'
        val1 = 15
        val2 = 24
        device_state = DeviceState(base_cmd)
        self.assertEqual(device_state.set_value(val1, val2),
                         'run command with vals 15 24')

    def test_device_state_with_base_cmd_as_format_string(self):
        """Tests that DeviceState returns the correct ADB command if the base
        command is given as a format string.
        """
        base_cmd = 'echo %s > /test/data'
        val = 23
        device_state = DeviceState(base_cmd)
        self.assertEqual(device_state.set_value(val), 'echo 23 > /test/data')

    def test_device_binary_state(self):
        """Tests that DeviceState returns the correct ADB commands with toggle.
        """
        on_cmd = 'enable this service'
        off_cmd = 'disable the service'
        device_binary_state = DeviceState('', on_cmd, off_cmd)
        self.assertEqual(device_binary_state.toggle(True), on_cmd)
        self.assertEqual(device_binary_state.toggle(False), off_cmd)

    def test_device_setprop(self):
        """Tests that DeviceSetprop returns the correct ADB command with
        set_value.
        """
        prop = 'some.property'
        val = 3
        device_setprop = DeviceSetprop(prop)
        self.assertEqual(device_setprop.set_value(val),
                         'setprop some.property 3')

    def test_device_binary_setprop(self):
        """Tests that DeviceSetprop returns the correct ADB commands with
        toggle.
        """
        prop = 'some.other.property'
        on_val = True
        off_val = False
        device_binary_setprop = DeviceSetprop(prop, on_val, off_val)
        self.assertEqual(device_binary_setprop.toggle(True),
                         'setprop some.other.property True')
        self.assertEqual(device_binary_setprop.toggle(False),
                         'setprop some.other.property False')

    def test_device_setting(self):
        """Tests that DeviceSetting returns the correct ADB command with
        set_value.
        """
        namespace = 'global'
        setting = 'some_new_setting'
        val = 10
        device_setting = DeviceSetting(namespace, setting)
        self.assertEqual(device_setting.set_value(val),
                         'settings put global some_new_setting 10')

    def test_device_binary_setting(self):
        """Tests that DeviceSetting returns the correct ADB commands with
        toggle.
        """
        namespace = 'system'
        setting = 'some_other_setting'
        on_val = 'on'
        off_val = 'off'
        device_binary_setting = DeviceSetting(
            namespace, setting, on_val, off_val)
        self.assertEqual(
            device_binary_setting.toggle(True),
            'settings put system some_other_setting on')
        self.assertEqual(
            device_binary_setting.toggle(False),
            'settings put system some_other_setting off')

    def test_device_gservices(self):
        """Tests that DeviceGServices returns the correct ADB command with
        set_value.
        """
        setting = 'some_gservice'
        val = 22
        device_gservices = DeviceGServices(setting)
        self.assertEqual(
            device_gservices.set_value(val),
            'am broadcast -a '
            'com.google.gservices.intent.action.GSERVICES_OVERRIDE '
            '--ei some_gservice 22')

    def test_device_binary_command_series(self):
        """Tests that DeviceBinaryCommandSuite returns the correct ADB
        commands.
        """
        on_cmds = [
            'settings put global test_setting on',
            'setprop test.prop 1',
            'svc test_svc enable'
        ]
        off_cmds = [
            'settings put global test_setting off',
            'setprop test.prop 0',
            'svc test_svc disable'
        ]
        device_binary_command_series = DeviceBinaryCommandSeries(
            [
                DeviceSetting('global', 'test_setting', 'on', 'off'),
                DeviceSetprop('test.prop'),
                DeviceState('svc test_svc', 'enable', 'disable')
            ]
        )
        self.assertEqual(device_binary_command_series.toggle(True), on_cmds)
        self.assertEqual(device_binary_command_series.toggle(False), off_cmds)


if __name__ == "__main__":
    unittest.main()
