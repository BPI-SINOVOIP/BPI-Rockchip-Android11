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
This test script exercises test scenarios for connections to Bluetooth headphones.

This test script was designed with this setup in mind:
Shield box one: Android Device and 3 headset.
"""

import json
import os
import time
import sys

from acts import logger
from acts.asserts import assert_false
from acts.asserts import assert_true
from acts.keys import Config
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.bt_constants import ble_scan_settings_modes
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_test_utils import bluetooth_enabled_check
from acts.test_utils.bt.bt_test_utils import clear_bonded_devices
from acts.test_utils.bt.bt_test_utils import disable_bluetooth
from acts.test_utils.bt.bt_test_utils import reset_bluetooth
from acts.controllers.relay_lib.sony_xb2_speaker import SonyXB2Speaker



class HeadphoneTest(BluetoothBaseTest):

    WAIT_TIME = 10
    iterations = 10


    def _discover_and_pair(self, headphone):
        self.ad.droid.bluetoothStartDiscovery()
        time.sleep(10)
        self.ad.droid.bluetoothCancelDiscovery()
        for device in self.ad.droid.bluetoothGetDiscoveredDevices():
            if device['address'] == headphone.mac_address:
                self.ad.droid.bluetoothDiscoverAndBond(
                    headphone.mac_address)
                end_time = time.time() + 20
                self.log.info("Found the mac address")
                self.log.info("Verifying devices are bonded")
                while time.time() < end_time:
                    bonded_devices = self.ad.droid.bluetoothGetBondedDevices()
                    for d in bonded_devices:
                        if d['address'] == headphone.mac_address:
                            self.log.info("Successfully bonded to device.")
                            self.log.info(
                                'Bonded devices:\n%s', bonded_devices)
                            return True
        return False

    def setup_class(self):
        self.ad.droid.bluetoothFactoryReset()
        # Factory reset requires a short delay to take effect
        time.sleep(3)

        self.ad.log.info("Making sure BT phone is enabled here during setup")
        if not bluetooth_enabled_check(self.ad):
            self.log.error("Failed to turn Bluetooth on DUT")
        # Give a breathing time of short delay to take effect
        time.sleep(3)

        reset_bluetooth([self.ad])

        # Determine if we have a relay-based device
        if hasattr(self, 'relay_devices'):
            for headphone in self.relay_devices:
                headphone.setup()


        '''
        # Turn of the Power Supply for the headphones.
        if hasattr(self, 'relay_devices'):
            power_supply = self.relay_devices[0]
            power_supply.power_off()
        '''
        super(HeadphoneTest, self).setup_class()
        return clear_bonded_devices(self.ad)

    def teardown_class(self):
        if self.headphone_list is not None:
            for headphone in self.headphone_list:
                headphone.power_off()
                headphone.clean_up()
        '''
        power_supply = self.relay_devices[0]
        power_supply.power_off()
        '''

        return clear_bonded_devices(self.ad)

    @property
    def ad(self):
        return self.android_devices[0]

    @property
    def headphone_list(self):
        return self.relay_devices

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='157c1fa1-3d6f-4cfc-8f86-ad267746af71')
    def test_pair_and_unpair_bt_device(self):
        """ Test Pairing and Unpairing Bluetooth Headphones to the Android device.

             Steps:
              1. Pair and Connect Bluetooth headset.
              2. Disconnect Bleutooth Headset.

              Expected Result: Bluetooth pairing and unpairing should succeed.

              Returns:
                 Pass if True
                 Fail if False

              TAGS: Classic.
              Priority: 0
        """
        for headphone in self.headphone_list:
            self.log.info("Start testing on  " + headphone.name )

            if self._discover_and_pair(headphone) is False:
                # The device is probably not in pairing mode, put in pairing mode.

                headphone.turn_power_on_and_enter_pairing_mode()
                time.sleep(6)
                if self._discover_and_pair(headphone) is False:
                    # Device must be on, but not in pairing  mode.
                    headphone.power_off()
                    headphone.turn_power_on_and_enter_pairing_mode()
                    time.sleep(6)
                    msg = "Unable to pair to %s", headphone.name
                    assert_true(self._discover_and_pair(headphone), msg)

            if len(self.ad.droid.bluetoothGetConnectedDevices()) == 0:
                self.log.error("Not connected to relay based device.")
                return False
            clear_bonded_devices(self.ad)



    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='7ba73c39-2a69-4a72-b708-d603ce658740')
    def test_pair_unpair_stress(self):
        """ Stress Test for Pairing and Unpairing of Bluetooth Headphones.

             Steps:
              1. Pairing and Connect Bluetooth headset.
              2. Disconnect Bleutooth Headset.
              3. Repeat step 1-2 for 10 iterations.

              Expected Result: Bluetooth pairing and unpairing should succeed.

              Returns:
                 Pass if True
                 Fail if False

              TAGS: Classic, Stress
              Priority: 1
        """
        for n in range(self.iterations):
            self.log.info("Pair headphone iteration %s.", (n + 1))

            for headphone in self.headphone_list:
                if self._discover_and_pair(headphone) is False:
                    # The device is probably not in pairing mode, put in pairing mode.
                    headphone.turn_power_on_and_enter_pairing_mode()
                    time.sleep(6)
                    if self._discover_and_pair(headphone) is False:
                        # Device must be on, but not in pairing  mode.
                        headphone.power_off()
                        headphone.turn_power_on_and_enter_pairing_mode()
                        time.sleep(6)
                        msg = "Unable to pair to %s", headphone.name
                        assert_true(self._discover_and_pair(headphone), msg)

                if len(self.ad.droid.bluetoothGetConnectedDevices()) == 0:
                    self.log.error("Not connected to relay based device.")
                    return False
                clear_bonded_devices(self.ad)

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='e2e5fe90-16f1-4830-9b81-6632ae5aab52')
    def test_multi_a2dp_test(self):
        """ Test for Pairing and Unpairing mutliple A2dp devices.

             Steps:
              1. Pairing and Connect multiple Bluetooth headsets.
              2. Disconnect all Bleutooth Headset.

              Expected Result: All headphones should be connected simultaneously.

              Returns:
                 Pass if True
                 Fail if False

              TAGS: Classic, Multi-A2dp.
              Priority: 1
        """

        for n, headphone in enumerate(self.headphone_list):
            if self._discover_and_pair(headphone) is False:
                # The device is probably not in pairing mode, put in pairing mode.
                headphone.turn_power_on_and_enter_pairing_mode()
                time.sleep(6)
                if self._discover_and_pair(headphone) is False:
                    # Device must be on, but not in pairing  mode.
                    headphone.power_off()
                    headphone.turn_power_on_and_enter_pairing_mode()
                    time.sleep(6)
                    msg = "Unable to pair to %s", headphone.name
                    assert_true(self._discover_and_pair(headphone), msg)

            if len(self.ad.droid.bluetoothGetConnectedDevices()) != n+1:
                self.log.error("Not connected to  %s", headphone.name)
                return False
