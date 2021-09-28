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

"beacon_loss_test_iterations": "5"
"""

import os
import uuid
import time

from acts import asserts
from acts import signals
from acts import utils
from acts.base_test import BaseTestClass
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import disconnect
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import is_connected
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import associate
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.utils import rand_ascii_str


class BeaconLossTest(BaseTestClass):
    # Default number of test iterations here.
    # Override using parameter in config file.
    # Eg: "beacon_loss_test_iterations": "10"
    num_of_iterations = 5

    # Time to wait for AP to startup
    wait_ap_startup_s = 15

    # Default wait time in seconds for the AP radio to turn back on
    wait_to_connect_after_ap_txon_s = 5

    # Time to wait for device to disconnect after AP radio of
    wait_after_ap_txoff_s = 15

    # Time to wait for device to complete connection setup after
    # given an associate command
    wait_client_connection_setup_s = 15

    def setup_class(self):
        super().setup_class()
        self.ssid = rand_ascii_str(10)
        self.wlan_device = create_wlan_device(self.fuchsia_devices[0])
        self.ap = self.access_points[0]
        self.num_of_iterations = int(
            self.user_params.get("beacon_loss_test_iterations",
                                 self.num_of_iterations))
        self.in_use_interface = None

    def teardown_test(self):
        disconnect(self.wlan_device)
        self.wlan_device.reset_wifi()
        # ensure radio is on, in case the test failed while the radio was off
        self.ap.iwconfig.ap_iwconfig(self.in_use_interface, "txpower on")
        self.ap.stop_all_aps()

    def beacon_loss(self, channel):
        setup_ap(
            access_point=self.ap,
            profile_name='whirlwind',
            channel=channel,
            ssid=self.ssid)
        time.sleep(self.wait_ap_startup_s)
        if channel > 14:
            self.in_use_interface = self.ap.wlan_5g
        else:
            self.in_use_interface = self.ap.wlan_2g

        # TODO(b/144505723): [ACTS] update BeaconLossTest.py to handle client
        # roaming, saved networks, etc.
        self.log.info("sending associate command for ssid %s", self.ssid)
        associate(client=self.wlan_device, ssid=self.ssid)

        asserts.assert_true(
            is_connected(self.wlan_device), 'Failed to connect.')

        time.sleep(self.wait_client_connection_setup_s)

        for _ in range(0, self.num_of_iterations):
            # Turn off AP radio
            self.log.info("turning off radio")
            self.ap.iwconfig.ap_iwconfig(self.in_use_interface, "txpower off")
            time.sleep(self.wait_after_ap_txoff_s)

            # Did we disconnect from AP?
            asserts.assert_false(
                is_connected(self.wlan_device), 'Failed to disconnect.')

            # Turn on AP radio
            self.log.info("turning on radio")
            self.ap.iwconfig.ap_iwconfig(self.in_use_interface, "txpower on")
            time.sleep(self.wait_to_connect_after_ap_txon_s)

            # Tell the client to connect
            self.log.info("sending associate command for ssid %s" % self.ssid)
            associate(client=self.wlan_device, ssid=self.ssid)
            time.sleep(self.wait_client_connection_setup_s)

            # Did we connect back to WiFi?
            asserts.assert_true(
                is_connected(self.wlan_device), 'Failed to connect back.')

        return True

    def test_beacon_loss_2g(self):
        self.beacon_loss(channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G)

    def test_beacon_loss_5g(self):
        self.beacon_loss(channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G)
