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

SHORT_PRESS_WAIT_TIME = 0.5
MEDIUM_PRESS_WAIT_TIME = 3.0
LONG_PRESS_WAIT_TIME = 4.5
WAIT_FOR_EFFECT_TIME = 1


class Buttons(enum.Enum):
    NEXT = 'Next'
    PREVIOUS = "Previous"
    PLAY_PAUSE = 'Play_pause'
    VOLUME_UP = "Volume_up"
    VOLUME_DOWN = "Volume_down"


class EarstudioReceiver(BluetoothRelayDevice):

    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def power_on(self):
        """Power on the Earstudio device.

        BLUE LED blinks once when power is on. "power-on sound" plays when it is
        on. Automatically connects to a device that has been connected before.
        GREEN LED blinks once every 3 seconds after the "connection sound."
        Enters Discoverable Mode/Paring Mode when there is no device that has
        been connected before. GREEN LED blinks twice every 0.5 seconds.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(MEDIUM_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def power_off(self):
        """Power off the Earstudio device.

        RED LED blinks once right before power off. "power-off sound" plays when
        it is off.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(LONG_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_play_pause(self):
        """Toggle audio play state.

        GREEN LED slowly blinks once every 3 seconds during Bluetooth/USB
        playback.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_accept_call(self):
        """Receive incoming call.

        BLUE LED slowly blinks once every 3 seconds
        "Call-receiving sound" when received.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_reject_call(self):
        """Reject incoming call.

        "Call-rejection sound" when refused.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(MEDIUM_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_end_call(self):
        """End ongoing call.

        "Call-end sound" when ended.
        """
        self.relays[Buttons.PLAY_PAUSE.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_next(self):
        """Skip to the next track."""
        self.relays[Buttons.NEXT.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def toggle_ambient_mode(self):
        """Turn ambient mode on/off.

        Only available during playback.
        To use it, you must set 'Ambient Shortcut Key' to 'on' in the EarStudio
        app.
        """
        self.relays[Buttons.NEXT.value].set_nc_for(MEDIUM_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_previous(self):
        """Rewind to beginning of current or previous track."""
        self.relays[Buttons.PREVIOUS.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def enter_pairing_mode(self):
        """Enter BlueTooth pairing mode.

        GREEN LED blinks twice every 0.5 seconds after "enter paring-mode
        sound." Disconnects from the current connected device when entering
        this mode.
        """
        self.relays[Buttons.PREVIOUS.value].set_nc_for(MEDIUM_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_volume_up(self, press_duration=SHORT_PRESS_WAIT_TIME):
        """Turn up the volume.

        Volume increases by 0.5dB for each press.
        Press&holding the button increases the volume consistently up to 6dB.
        Args:
          press_duration (int|float): how long to hold button for.
        """
        self.relays[Buttons.VOLUME_UP.value].set_nc_for(press_duration)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_volume_down(self, press_duration=SHORT_PRESS_WAIT_TIME):
        """Turn down the volume.

        Volume decreases by 0.5dB for each press.
        Press&hold the button decreases the volume consistently down to -60dB.
        Pressing the button at the minimum volume turns to a mute level.
        Args:
          press_duration (int|float): how long to hold button for.
        """
        self.relays[Buttons.VOLUME_DOWN.value].set_nc_for(press_duration)
        time.sleep(WAIT_FOR_EFFECT_TIME)
