#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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
Script for testing WiFi recovery after rebooting the AP.

Override default number of iterations using the following
parameter in the test config file.

"reboot_ap_stress_test_iterations": "10"
"""

import os
import uuid
import time

from acts import asserts
from acts import signals
from acts.base_test import BaseTestClass
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import disconnect
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import is_connected
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap_and_associate
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.fuchsia import utils
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.utils import rand_ascii_str


class RebootAPStressTest(BaseTestClass):
    # Default number of test iterations here.
    # Override using parameter in config file.
    # Eg: "reboot_ap_stress_test_iterations": "10"
    num_of_iterations = 3

    # Default wait time in seconds for the device
    # to connect back after AP reboot.
    # Override using parameter in config file.
    # Eg: "wait_to_connect_after_ap_reboot_s": "60"
    wait_to_connect_after_ap_reboot_s = 30

    # Time to wait for device to disconnect
    # after AP reboot.
    wait_after_ap_reboot_s = 1

    def setup_class(self):
        super().setup_class()
        self.ssid = rand_ascii_str(10)
        self.wlan_device = create_wlan_device(self.fuchsia_devices[0])
        self.ap = self.access_points[0]
        self.num_of_iterations = int(
            self.user_params.get("reboot_ap_stress_test_iterations",
                                 self.num_of_iterations))
        self.wait_to_connect_after_ap_reboot_s = int(
            self.user_params.get("wait_to_connect_after_ap_reboot_s",
                                 self.wait_to_connect_after_ap_reboot_s))
    def teardown_test(self):
        disconnect(self.wlan_device)
        self.wlan_device.reset_wifi()
        self.ap.stop_all_aps()

    def setup_ap(self):
        setup_ap(access_point=self.ap,
            profile_name='whirlwind',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_reboot_AP_stress(self):
        setup_ap_and_associate(
            access_point=self.ap,
            client=self.wlan_device,
            profile_name='whirlwind',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

        asserts.assert_true(is_connected(self.wlan_device),
                        'Failed to connect.')

        for _ in range(0, self.num_of_iterations):
            # Stop AP
            self.ap.stop_all_aps()
            time.sleep(self.wait_after_ap_reboot_s)

            # Did we disconnect from AP?
            asserts.assert_false(is_connected(self.wlan_device),
                        'Failed to disconnect.')

            # Start AP
            self.setup_ap()

            # Give the device time to connect back
            time.sleep(self.wait_to_connect_after_ap_reboot_s)

            # Did we connect back to WiFi?
            asserts.assert_true(is_connected(self.wlan_device),
                        'Failed to connect back.')

        return True
