#!/usr/bin/env python3
#
#   Copyright 2017 - The Android Open Source Project
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
import enum
import time

from acts.controllers.relay_lib.devices.bluetooth_relay_device import BluetoothRelayDevice

# Necessary timeout in between commands
CMD_TIMEOUT = 1.2
# Pairing mode activation wait time
PAIRING_MODE_WAIT_TIME = 4.5
SINGLE_ACTION_SHORT_WAIT_TIME = 0.6
SINGLE_ACTION_LONG_WAIT_TIME = 2.0
MISSING_RELAY_MSG = 'Relay config for Three button "%s" missing relay "%s".'


class Buttons(enum.Enum):
    ACTION = 'Action'
    NEXT = 'Next'
    PREVIOUS = 'Previous'


class SingleButtonDongle(BluetoothRelayDevice):
    """A Bluetooth dongle with one generic button Normally action.

    Wraps the button presses, as well as the special features like pairing.
    """

    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relay(Buttons.ACTION.value)

    def enter_pairing_mode(self):
        """Enters pairing mode. Blocks the thread until pairing mode is set.

        Holds down the 'ACTION' buttons for PAIRING_MODE_WAIT_TIME seconds.
        """
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=PAIRING_MODE_WAIT_TIME)

    def press_play_pause(self):
        """Briefly presses the Action button."""
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=SINGLE_ACTION_SHORT_WAIT_TIME)

    def press_vr_mode(self):
        """Long press the Action button."""
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=SINGLE_ACTION_LONG_WAIT_TIME)

    def setup(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.setup(self)

    def clean_up(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.clean_up(self)


class ThreeButtonDongle(BluetoothRelayDevice):
    """A Bluetooth dongle with three generic buttons Normally action, next, and
     previous.

    Wraps the button presses, as well as the special features like pairing.
    """

    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def setup(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.setup(self)

    def clean_up(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.clean_up(self)

    def enter_pairing_mode(self):
        """Enters pairing mode. Blocks the thread until pairing mode is set.

        Holds down the 'ACTION' buttons for a little over 5 seconds.
        """
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=PAIRING_MODE_WAIT_TIME)

    def press_play_pause(self):
        """Briefly presses the Action button."""
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=SINGLE_ACTION_SHORT_WAIT_TIME)
        time.sleep(CMD_TIMEOUT)

    def press_vr_mode(self):
        """Long press the Action button."""
        self.relays[Buttons.ACTION.value].set_nc_for(
            seconds=SINGLE_ACTION_LONG_WAIT_TIME)
        time.sleep(CMD_TIMEOUT)

    def press_next(self):
        """Briefly presses the Next button."""
        self.relays[Buttons.NEXT.value].set_nc_for(
            seconds=SINGLE_ACTION_SHORT_WAIT_TIME)
        time.sleep(CMD_TIMEOUT)

    def press_previous(self):
        """Briefly presses the Previous button."""
        self.relays[Buttons.PREVIOUS.value].set_nc_for(
            seconds=SINGLE_ACTION_SHORT_WAIT_TIME)
        time.sleep(CMD_TIMEOUT)
