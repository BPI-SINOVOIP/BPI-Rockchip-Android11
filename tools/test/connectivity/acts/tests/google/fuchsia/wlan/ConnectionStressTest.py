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
Script for testing WiFi connection and disconnection in a loop

"""
from acts.base_test import BaseTestClass

import os
import uuid
import time

from acts import signals
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_security
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import associate
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import disconnect
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.fuchsia import utils
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.utils import rand_ascii_str



class ConnectionStressTest(BaseTestClass):
    # Default number of test iterations here.
    # Override using parameter in config file.
    # Eg: "connection_stress_test_iterations": "50"
    num_of_iterations = 10
    channel_2G = hostapd_constants.AP_DEFAULT_CHANNEL_2G
    channel_5G = hostapd_constants.AP_DEFAULT_CHANNEL_5G

    def setup_class(self):
        super().setup_class()
        self.ssid = rand_ascii_str(10)
        self.fd = self.fuchsia_devices[0]
        self.dut = create_wlan_device(self.fd)
        self.ap = self.access_points[0]
        self.num_of_iterations = int(
            self.user_params.get("connection_stress_test_iterations",
                                 self.num_of_iterations))
        self.log.info('iterations: %d' % self.num_of_iterations)

    def teardown_test(self):
        self.dut.reset_wifi()
        self.ap.stop_all_aps()

    def start_ap(self, profile, channel, security=None):
        """Starts an Access Point

        Args:
            profile: Profile name such as 'whirlwind'
            channel: Channel to operate on
        """
        self.log.info('Profile: %s, Channel: %d' % (profile, channel))
        setup_ap(
            access_point=self.ap,
            profile_name=profile,
            channel=channel,
            ssid=self.ssid,
            security=security)

    def connect_disconnect(self,
                           ap_config,
                           ssid=None,
                           password=None,
                           negative_test=False):
        """Helper to start an AP, connect DUT to it and disconnect

        Args:
            ap_config: Dictionary contaning profile name and channel
            ssid: ssid to connect to
            password: password for the ssid to connect to
        """
        # Start AP
        self.start_ap(ap_config['profile'],
                      ap_config['channel'],
                      ap_config['security'])

        failed = False
        # Connect and Disconnect several times
        for x in range(0, self.num_of_iterations):
            if not ssid:
                ssid = self.ssid
            if negative_test:
                if not associate(self.dut, ssid=ssid, password=password):
                    self.log.info('Attempt %d. Did not associate as expected.'
                                  % x)
                else:
                    self.log.error('Attempt %d. Negative test successfully '
                                   'associated. Fail.' % x)
                    failed = True
            else:
                # Connect
                if associate(self.dut, ssid=ssid, password=password):
                    self.log.info('Attempt %d. Successfully associated' % x)
                else:
                    self.log.error('Attempt %d. Failed to associate.' % x)
                    failed = True
                # Disconnect
                disconnect(self.dut)

            # Wait a second before trying again
            time.sleep(1)

        # Stop AP
        self.ap.stop_all_aps()
        if failed:
            raise signals.TestFailure('One or more association attempt failed.')

    def test_whirlwind_2g(self):
        self.connect_disconnect({
            'profile': 'whirlwind',
            'channel': self.channel_2G,
            'security': None
        })

    def test_whirlwind_5g(self):
        self.connect_disconnect({
            'profile': 'whirlwind',
            'channel': self.channel_5G,
            'security': None
        })

    def test_whirlwind_11ab_2g(self):
        self.connect_disconnect({
            'profile': 'whirlwind_11ab_legacy',
            'channel': self.channel_2G,
            'security': None
        })

    def test_whirlwind_11ab_5g(self):
        self.connect_disconnect({
            'profile': 'whirlwind_11ab_legacy',
            'channel': self.channel_5G,
            'security': None
        })

    def test_whirlwind_11ag_2g(self):
        self.connect_disconnect({
            'profile': 'whirlwind_11ag_legacy',
            'channel': self.channel_2G,
            'security': None
        })

    def test_whirlwind_11ag_5g(self):
        self.connect_disconnect({
            'profile': 'whirlwind_11ag_legacy',
            'channel': self.channel_5G,
            'security': None
        })

    def test_wrong_ssid_whirlwind_2g(self):
        self.connect_disconnect(
            {
                'profile': 'whirlwind',
                'channel': self.channel_2G,
                'security': None
            },
            ssid=rand_ascii_str(20),
            negative_test=True
        )

    def test_wrong_ssid_whirlwind_5g(self):
        self.connect_disconnect(
            {
                'profile': 'whirlwind',
                'channel': self.channel_5G,
                'security': None
            },
            ssid=rand_ascii_str(20),
            negative_test=True
        )

    def test_wrong_password_whirlwind_2g(self):
        self.connect_disconnect(
            {
                'profile': 'whirlwind',
                'channel': self.channel_2G,
                'security': hostapd_security.Security(
                    security_mode='wpa2',
                    password=rand_ascii_str(10))
            },
            password=rand_ascii_str(20),
            negative_test=True
        )

    def test_wrong_password_whirlwind_5g(self):
        self.connect_disconnect(
            {
                'profile': 'whirlwind',
                'channel': self.channel_5G,
                'security': hostapd_security.Security(
                    security_mode='wpa2',
                    password=rand_ascii_str(10))
            },
            password=rand_ascii_str(20),
            negative_test=True
        )