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
import time
import enum

from acts.controllers.relay_lib.relay import SynchronizeRelays
from acts.controllers.relay_lib.devices.bluetooth_relay_device import BluetoothRelayDevice

PAIRING_MODE_WAIT_TIME = 5.2


class Buttons(enum.Enum):
    HOME = 'Home'
    BACK = 'Back'
    PLAY_PAUSE = 'Play'


class FuguRemote(BluetoothRelayDevice):
    """A Nexus Player (Fugu) Remote.

    Wraps the button presses, as well as the special features like pairing.
    """

    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def setup(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.setup(self)
        # If the Fugu remote does have a power relay attached, turn it on.
        power = 'Power'
        if power in self.relays:
            self.relays[power].set_nc()

    def clean_up(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.clean_up(self)

    def enter_pairing_mode(self):
        """Enters pairing mode. Blocks the thread until pairing mode is set.

        Holds down the 'Home' and 'Back' buttons for a little over 5 seconds.
        """
        with SynchronizeRelays():
            self.hold_down(Buttons.HOME.value)
            self.hold_down(Buttons.BACK.value)

        time.sleep(PAIRING_MODE_WAIT_TIME)

        with SynchronizeRelays():
            self.release(Buttons.HOME.value)
            self.release(Buttons.BACK.value)

    def press_play_pause(self):
        """Briefly presses the Play/Pause button."""
        self.press(Buttons.PLAY_PAUSE.value)

    def press_home(self):
        """Briefly presses the Home button."""
        self.press(Buttons.HOME.value)

    def press_back(self):
        """Briefly presses the Back button."""
        self.press(Buttons.BACK.value)
