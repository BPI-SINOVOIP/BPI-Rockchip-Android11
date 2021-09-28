#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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
"""
Device Details:
https://www.logitech.com/en-in/product/bluetooth-audio-adapter#specification-tabular
"""
import enum

from acts.controllers.relay_lib.devices.bluetooth_relay_device import BluetoothRelayDevice

PAIRING_MODE_WAIT_TIME = 5
WAIT_TIME = 0.1


class Buttons(enum.Enum):
    POWER = 'Power'
    PAIR = 'Pair'


class LogitechAudioReceiver(BluetoothRelayDevice):
    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def setup(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.setup(self)

    def clean_up(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.clean_up(self)

    def power_on(self):
        """Power on relay."""
        self.relays[Buttons.POWER.value].set_nc()

    def enter_pairing_mode(self):
        """Sets relay in paring mode."""
        self.relays[Buttons.PAIR.value].set_nc()
