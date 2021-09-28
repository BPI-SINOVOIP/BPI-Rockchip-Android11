#!/usr/bin/env python3
#
# Copyright (C) 2016 The Android Open Source Project
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
This test is used to test basic functionality of bluetooth adapter by turning it ON/OFF.
"""

from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt import bt_test_utils

import random
import time


class BtCarToggleTest(BluetoothBaseTest):

    def setup_class(self):
        self.droid_ad = self.android_devices[0]
        for ad in self.android_devices:
            bt_test_utils.clear_bonded_devices(ad)

    def setup_test(self):
        self.droid_ad.ed.clear_all_events()
        if not bt_test_utils.bluetooth_enabled_check(self.droid_ad):
            return False

    def on_fail(self, test_name, begin_time):
        bt_test_utils.take_btsnoop_logs(self.android_devices, self, test_name)

    def delay(self, delay_interval):
        self.log.debug("Toggle delay interval {}".format(0.01 * delay_interval))

        start_time = time.time()
        while (start_time + 0.01 * delay_interval) > time.time():
            time.sleep(0.1)

    @test_tracker_info(uuid='290eb41f-6e66-4dc1-8f3e-55783901d116')
    @BluetoothBaseTest.bt_test_wrap
    def test_bluetooth_reset(self):
        """Test resetting bluetooth.

        Test the integrity of resetting bluetooth on Android.

        Steps:
        1. Toggle bluetooth off.
        2. Toggle bluetooth on.

        Returns:
          Pass if True
          Fail if False

        """
        return bt_test_utils.reset_bluetooth([self.droid_ad])

    @test_tracker_info(uuid='5ff9dc37-67ff-4367-9803-4a58a22de610')
    @BluetoothBaseTest.bt_test_wrap
    def test_bluetooth_continuous_reset(self):
        """Test resets bluetooth continuously with delay intervals .

            Test the integrity of resetting bluetooth on Android.

            Steps:
                1. Toggle bluetooth off.
                2. Toggle bluetooth on.
                3. Repeat.

            Returns:
              Pass if True
              Fail if False

          """

        for count in range(50):

            delay_off_interval = random.randint(0, 1000)
            delay_on_interval = random.randint(0, 1000)

            passed = True

            self.delay(delay_off_interval)

            passed = self.droid_ad.droid.bluetoothToggleState(False)
            self.delay(delay_on_interval)
            passed = passed and self.droid_ad.droid.bluetoothToggleState(True)

            if not passed:
                self.log.error("Failed to reset bluetooth: Iteration count {}".format(count))
                return False
            return True
