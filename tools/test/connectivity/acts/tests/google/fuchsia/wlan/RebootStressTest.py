#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""
Script for testing WiFi recovery after device reboot

"""
from acts.base_test import BaseTestClass

import os
import uuid

from acts import asserts
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import is_connected
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap_and_associate
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import disconnect
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.fuchsia import utils
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.utils import rand_ascii_str

class RebootStressTest(BaseTestClass):
    # Default number of test iterations here.
    # Override using parameter in config file.
    # Eg: "reboot_stress_test_iterations": "10"
    num_of_iterations = 3

    def setup_class(self):
        super().setup_class()
        self.ssid = rand_ascii_str(10)
        self.fd = self.fuchsia_devices[0]
        self.wlan_device = create_wlan_device(self.fd)
        self.ap = self.access_points[0]
        self.num_of_iterations = int(
            self.user_params.get("reboot_stress_test_iterations",
                                 self.num_of_iterations))

        setup_ap_and_associate(
            access_point=self.ap,
            client=self.wlan_device,
            profile_name='whirlwind',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def teardown_test(self):
        disconnect(self.wlan_device)
        self.wlan_device.reset_wifi()
        self.ap.stop_all_aps()

    def test_reboot_stress(self):
        for x in range(0, self.num_of_iterations):
            # Reboot device
            self.fd.reboot()

            # Did we connect back to WiFi?
            asserts.assert_true(
                is_connected(self.wlan_device), 'Failed to connect.')
        return True
