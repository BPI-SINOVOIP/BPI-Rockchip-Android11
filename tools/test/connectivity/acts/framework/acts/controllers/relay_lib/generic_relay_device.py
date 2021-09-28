#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

from acts.controllers.relay_lib.errors import RelayConfigError
from acts.controllers.relay_lib.relay import SynchronizeRelays
from acts.controllers.relay_lib.relay_device import RelayDevice

MISSING_RELAY_MSG = 'Relay config for %s device "%s" missing relay "%s".'


class GenericRelayDevice(RelayDevice):
    """A default, all-encompassing implementation of RelayDevice.

    This class allows for quick access to getting relay switches through the
    subscript ([]) operator. Note that it does not allow for re-assignment or
    additions to the relays dictionary.
    """

    def __init__(self, config, relay_rig):
        RelayDevice.__init__(self, config, relay_rig)

    def _ensure_config_contains_relays(self, relay_names):
        for relay_name in relay_names:
            self._ensure_config_contains_relay(relay_name)

    def _ensure_config_contains_relay(self, relay_name):
        """Throws an error if the relay does not exist."""
        if relay_name not in self.relays:
            raise RelayConfigError(MISSING_RELAY_MSG % (self.__class__.__name__,
                                                        self.name, relay_name))

    def get_button_names(self):
        """Returns the list of all button names."""
        return list(self.relays.keys())

    def setup(self):
        """Sets all relays to their default state (off)."""
        with SynchronizeRelays():
            for relay in self.relays.values():
                relay.set_no()

    def clean_up(self):
        """Sets all relays to their default state (off)."""
        with SynchronizeRelays():
            for relay in self.relays.values():
                if relay.is_dirty():
                    relay.set_no()

    def press(self, button_name):
        """Presses the button for a short period of time.

        Args:
            button_name: the name of the button to press.
        """
        self.relays[button_name].set_nc_for()

    def hold_down(self, button_name):
        """Holds down the button until release is called.

        If the button is already being held, the state does not change.

        Args:
            button_name: the name of the button to hold down.
        """
        self.relays[button_name].set_nc()

    def release(self, button_name):
        """Releases the held down button with name 'button_name'.

        If the button is already depressed, the state does not change.

        Args:
            button_name: the name of the button to release.
        """
        self.relays[button_name].set_no()
