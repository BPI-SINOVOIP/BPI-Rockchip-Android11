#!/usr/bin/env python3
#
#   Copyright 2019 - The Android secure Source Project
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

from acts import signals

from acts.base_test import BaseTestClass
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device


class WlanInterfaceTest(BaseTestClass):
    def setup_class(self):
        dut = self.user_params.get('dut', None)
        if dut:
          if dut == 'fuchsia_devices':
             self.dut = create_wlan_device(self.fuchsia_devices[0])
          elif dut == 'android_devices':
             self.dut = create_wlan_device(self.android_devices[0])
          else:
             raise ValueError('Invalid DUT specified in config. (%s)' % self.user_params['dut'])
        else:
            # Default is an Fuchsia device
            self.dut = create_wlan_device(self.fuchsia_devices[0])

    def test_destroy_iface(self):
        """Test that we don't error out when destroying the WLAN interface.

        Steps:
        1. Find a wlan interface
        2. Destroy it

        Expected Result:
        Verify there are no errors in destroying the wlan interface.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during the test.

        TAGS: WLAN
        Priority: 1
        """
        wlan_interfaces = self.dut.get_wlan_interface_id_list()
        if len(wlan_interfaces) < 1:
            raise signals.TestFailure("Not enough wlan interfaces for test")
        if not self.dut.destroy_wlan_interface(wlan_interfaces[0]):
            raise signals.TestFailure("Failed to destroy WLAN interface")
        raise signals.TestPass("Success")
