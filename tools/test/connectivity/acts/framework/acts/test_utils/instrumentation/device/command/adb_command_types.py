#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

from acts.test_utils.instrumentation.device.command.intent_builder import \
    IntentBuilder


class DeviceState(object):
    """Class for adb commands for setting device properties to a value."""

    def __init__(self, base_cmd, on_val='1', off_val='0'):
        """Create a DeviceState.

        Args:
            base_cmd: The base adb command. Needs to accept an argument/value to
                generate the full command.
            on_val: Value used for the 'on' state
            off_val: Value used for the 'off' state
        """
        self._base_cmd = base_cmd
        self._on_val = on_val
        self._off_val = off_val

    def set_value(self, *values):
        """Returns the adb command with the given arguments/values.

        Args:
            values: The value(s) to run the command with
        """
        try:
            return self._base_cmd % values
        except TypeError:
            return str.strip(' '.join(
                [self._base_cmd] + [str(value) for value in values]))

    def toggle(self, enabled):
        """Returns the command corresponding to the desired state.

        Args:
            enabled: True for the 'on' state.
        """
        return self.set_value(self._on_val if enabled else self._off_val)


class DeviceSetprop(DeviceState):
    """Class for setprop commands."""

    def __init__(self, prop, on_val='1', off_val='0'):
        """Create a DeviceSetprop.

        Args:
            prop: Property name
            on_val: Value used for the 'on' state
            off_val: Value used for the 'off' state
        """
        super().__init__('setprop %s' % prop, on_val, off_val)


class DeviceSetting(DeviceState):
    """Class for commands to set a settings.db entry to a value."""

    def __init__(self, namespace, setting, on_val='1', off_val='0'):
        """Create a DeviceSetting.

        Args:
            namespace: Namespace of the setting
            setting: Setting name
            on_val: Value used for the 'on' state
            off_val: Value used for the 'off' state
        """
        super().__init__('settings put %s %s' % (namespace, setting),
                         on_val, off_val)


class DeviceGServices(DeviceState):
    """Class for overriding a GServices value."""

    OVERRIDE_GSERVICES_INTENT = ('com.google.gservices.intent.action.'
                                 'GSERVICES_OVERRIDE')

    def __init__(self, setting, on_val='true', off_val='false'):
        """Create a DeviceGServices.

        Args:
            setting: Name of the GServices setting
            on_val: Value used for the 'on' state
            off_val: Value used for the 'off' state
        """
        super().__init__(None, on_val, off_val)
        self._intent_builder = IntentBuilder('am broadcast')
        self._intent_builder.set_action(self.OVERRIDE_GSERVICES_INTENT)
        self._setting = setting

    def set_value(self, value):
        """Returns the adb command with the given value."""
        self._intent_builder.add_key_value_param(self._setting, value)
        return self._intent_builder.build()


class DeviceBinaryCommandSeries(object):
    """Class for toggling multiple settings at once."""

    def __init__(self, binary_commands):
        """Create a DeviceBinaryCommandSeries.

        Args:
            binary_commands: List of commands for setting toggleable options
        """
        self.cmd_list = binary_commands

    def toggle(self, enabled):
        """Returns the list of command corresponding to the desired state.

        Args:
            enabled: True for the 'on' state.
        """
        return [cmd.toggle(enabled) for cmd in self.cmd_list]
