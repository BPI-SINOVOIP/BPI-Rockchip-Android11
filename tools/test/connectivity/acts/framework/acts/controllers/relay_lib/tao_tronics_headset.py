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

import enum
import time

from acts.controllers.relay_lib.devices.bluetooth_relay_device import BluetoothRelayDevice

WAIT_TIME = 0.05


class Buttons(enum.Enum):
    NEXT = 'Next'
    PREVIOUS = "Previous"
    PLAY_PAUSE = 'Play_pause'
    VOLUME_UP = "Volume_up"
    VOLUME_DOWN = "Volume_down"


class TaoTronicsCarkit(BluetoothRelayDevice):

    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def setup(self):
        """Sets all relays to their default state (off)."""
        BluetoothRelayDevice.setup(self)

    def press_play_pause(self):
        """
        Sets relay to
            Play state : if there is no A2DP_streaming.
            Pause state : if there is A2DP_streaming.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_no_for(WAIT_TIME)

    def press_next(self):
        """Skips to next song from relay_device."""
        self.relays[Buttons.NEXT.value].set_no_for(WAIT_TIME)

    def press_previous(self):
        """Skips to previous song from relay_device."""
        self.relays[Buttons.PREVIOUS.value].set_no_for(WAIT_TIME)

    def press_volume_up(self):
        """Increases volume from relay_device."""
        self.relays[Buttons.VOLUME_UP.value].set_no_for(WAIT_TIME)

    def press_volume_down(self):
        """Decreases volume from relay_device."""
        self.relays[Buttons.VOLUME_DOWN.value].set_no_for(WAIT_TIME)

    def press_initiate_call(self):
        """Initiate call from relay device."""
        for i in range(0, 2):
            self.press(Buttons.PLAY_PAUSE.value)
            time.sleep(0.2)
        return True

    def press_accept_call(self):
        """Accepts call from relay device."""
        self.press(Buttons.PLAY_PAUSE.value)
        return True
