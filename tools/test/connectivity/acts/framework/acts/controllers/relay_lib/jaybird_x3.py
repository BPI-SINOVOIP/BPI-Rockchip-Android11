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
MED_PRESS_WAIT_TIME = 1.5
POWER_ON_WAIT_TIME = 2.5
LONG_PRESS_WAIT_TIME = 4.5

WAIT_FOR_EFFECT_TIME = 2.5


class Buttons(enum.Enum):
    VOLUME_UP = "Volume_up"
    VOLUME_DOWN = "Volume_down"
    POWER = "Power"


class JaybirdX3Earbuds(BluetoothRelayDevice):
    """Jaybird X3 earbuds model

    A relay device class for Jaybird X3 earbuds that provides basic Bluetooth
    """
    def __init__(self, config, relay_rig):
        BluetoothRelayDevice.__init__(self, config, relay_rig)
        self._ensure_config_contains_relays(button.value for button in Buttons)

    def power_off(self):
        """If the device powers off, the LED will flash red before it
        powers off. A voice prompt will say "POWER_OFF".
        """
        self.relays[Buttons.POWER.value].set_nc_for(LONG_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def power_on(self):
        """If the device powers on, the LED will flash green.
        A voice prompt will say "POWER ON".
        """
        self.relays[Buttons.POWER.value].set_nc_for(POWER_ON_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def enter_pairing_mode(self):
        """The Jaybird can only enter pairing mode from an OFF state.
        """
        self.power_on()
        self.power_off()
        self.relays[Buttons.POWER.value].set_nc_for(LONG_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_play_pause(self):
        """Toggles the audio play state."""
        self.relays[Buttons.POWER.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def activate_voice_commands(self):
        """Activates voice commands during music streaming."""
        self.relays[Buttons.POWER.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_accept_call(self):
        """Receives an incoming call."""
        self.relays[Buttons.POWER.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_reject_call(self):
        """Rejects an incoming call."""
        self.relays[Buttons.POWER.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_next(self):
        """Skips to the next track."""
        self.relays[Buttons.VOLUME_UP.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_previous(self):
        """Rewinds to beginning of current or previous track."""
        self.relays[Buttons.VOLUME_DOWN.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_volume_up(self):
        """Turns up the volume."""
        self.relays[Buttons.VOLUME_UP.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def press_volume_down(self):
        """Turns down the volume."""
        self.relays[Buttons.VOLUME_DOWN.value].set_nc_for(SHORT_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def toggle_hands_free(self):
        """Switches call audio between the phone and X3 buds."""
        self.relays[Buttons.VOLUME_UP.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)

    def mute_phone_call(self):
        """Mutes phone call audio."""
        self.relays[Buttons.VOLUME_DOWN.value].set_nc_for(MED_PRESS_WAIT_TIME)
        time.sleep(WAIT_FOR_EFFECT_TIME)