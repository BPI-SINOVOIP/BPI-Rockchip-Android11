#!/usr/bin/env python3.4
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

import time
import acts.test_utils.power.PowerBTBaseTest as PBtBT
import acts.test_utils.bt.bt_test_utils as btutils

SCREEN_OFF_WAIT_TIME = 2


class PowerBTidleTest(PBtBT.PowerBTBaseTest):
    def setup_class(self):

        super().setup_class()
        btutils.enable_bluetooth(self.dut.droid, self.dut.ed)

    # Test cases- Baseline
    def test_bt_on_unconnected_connectable(self):
        """BT turned on connectable mode.

        Page scan only.
        """
        self.dut.droid.bluetoothMakeConnectable()
        self.dut.droid.goToSleepNow()
        time.sleep(SCREEN_OFF_WAIT_TIME)
        self.measure_power_and_validate()

    def test_bt_on_unconnected_discoverable(self):
        """BT turned on discoverable mode.

        Page and inquiry scan.
        """
        self.dut.droid.bluetoothMakeConnectable()
        self.dut.droid.bluetoothMakeDiscoverable()
        self.dut.droid.goToSleepNow()
        time.sleep(SCREEN_OFF_WAIT_TIME)
        self.measure_power_and_validate()

    def test_bt_connected_idle(self):
        """BT idle after connecting to headset.

        """
        btutils.connect_phone_to_headset(self.dut, self.bt_device, 60)
        self.dut.droid.goToSleepNow()
        time.sleep(SCREEN_OFF_WAIT_TIME)
        self.measure_power_and_validate()
