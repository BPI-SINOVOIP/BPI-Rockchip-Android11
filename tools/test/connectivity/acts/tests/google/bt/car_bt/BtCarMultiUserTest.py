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
Automates test for testing bluetooth integrity with multi-user accounts

"""
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt import bt_test_utils
from acts.test_utils.users import users
import time
import random
import re


class BtCarMultiUserTest(BluetoothBaseTest):
    OWNER_ID = 0

    def setup_class(self):
        self.droid_ad = self.android_devices[0]
        for ad in self.android_devices:
            bt_test_utils.clear_bonded_devices(ad)

        self.userid_1 = users.create_new_user(self.droid_ad, "testUser")
        self.userid_2 = users.create_new_user(self.droid_ad, "testUser2")

    def setup_test(self):
        if not bt_test_utils.bluetooth_enabled_check(self.droid_ad):
            return False

    def teardown_class(self):
        users.switch_user(self.droid_ad, self.OWNER_ID)
        for user_id in [self.userid_1, self.userid_2]:
            users.remove_user(self.droid_ad, user_id)

    def on_fail(self, test_name, begin_time):
        bt_test_utils.take_btsnoop_logs(self.android_devices, self, test_name)

    def delay(self, delay):
        self.log.debug("Delay interval {}".format(0.01 * delay))

        start_time = time.time()
        while (start_time + 0.01 * delay) > time.time():
            time.sleep(0.1)

    @test_tracker_info(uuid='859b6207-b9bc-47a3-a949-cc415f0536c2')
    @BluetoothBaseTest.bt_test_wrap
    def test_multi_user_switch(self):
        """Test continuously switches users.

        Steps:
        1. Switch to user a
        2. Wait
        3. Switch to user b
        4. Check for bluetooth crashes
        5. Repeat

        Returns:
            Pass if no bluetooth crashes
            Fail if bluetooth crashed once
        """
        laps = 10
        initial_bt_crashes = bt_test_utils.get_bluetooth_crash_count(self.droid_ad)
        crashes_since_start = initial_bt_crashes

        for count in range(laps):
            delay_interval = random.randint(0, 1000)

            self.delay(delay_interval)

            if not users.switch_user(self.droid_ad, self.userid_1):
                return False

            self.delay(delay_interval)

            if not users.switch_user(self.droid_ad, self.userid_2):
                return False

            crashes_since_start = abs(initial_bt_crashes - bt_test_utils.get_bluetooth_crash_count(self.droid_ad))
            self.log.info("Current bluetooth crashes: {} over {} laps".format(crashes_since_start, count))

        if crashes_since_start != 0:
            self.log.error("Bluetooth stack crashed {} times over {} laps".format(crashes_since_start, laps))
            return False
        return True
