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
from acts.controllers.relay_lib.generic_relay_device import GenericRelayDevice
from acts.controllers.relay_lib.helpers import validate_key


class BluetoothRelayDevice(GenericRelayDevice):
    """A base class for bluetooth devices.

    This base class is similar to GenericRelayDevice, but requires a mac_address
    to be set from within the config taken in. This helps with type checking
    for use of relays against bluetooth utils.
    """
    def __init__(self, config, relay_rig):
        GenericRelayDevice.__init__(self, config, relay_rig)

        self.mac_address = validate_key('mac_address', config, str,
                                        self.__class__.__name__)

    def get_mac_address(self):
        """Returns the mac address of this device."""
        return self.mac_address

