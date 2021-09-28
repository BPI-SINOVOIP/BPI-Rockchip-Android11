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
import acts.test_utils.bt.BleEnum as bleenum
import acts.test_utils.bt.bt_power_test_utils as btputils
import acts.test_utils.power.PowerBTBaseTest as PBtBT

BLE_LOCATION_SCAN_ENABLE = 'settings put secure location_mode 3'
EXTRA_SCAN_TIME = 10
MONSOON_TAIL_CUT = 5


class PowerBLEscanTest(PBtBT.PowerBTBaseTest):
    def __init__(self, configs):
        super().__init__(configs)
        req_params = ['scan_modes', 'scan_duration']
        self.unpack_userparams(req_params)

        for scan_mode in self.scan_modes:
            self.generate_test_case_no_devices_around(scan_mode,
                                                      self.scan_duration)

    def setup_class(self):

        super().setup_class()
        self.dut.adb.shell(BLE_LOCATION_SCAN_ENABLE)
        # Make sure during power measurement, scan is always on
        self.mon_info.duration = (self.scan_duration - self.mon_offset -
                                  EXTRA_SCAN_TIME - MONSOON_TAIL_CUT)

    def generate_test_case_no_devices_around(self, scan_mode, scan_duration):
        def test_case_fn():

            self.measure_ble_scan_power(scan_mode, scan_duration)

        test_case_name = ('test_BLE_{}_no_advertisers'.format(
            bleenum.ScanSettingsScanMode(scan_mode).name))
        setattr(self, test_case_name, test_case_fn)

    def measure_ble_scan_power(self, scan_mode, scan_duration):

        btputils.start_apk_ble_scan(self.dut, scan_mode, scan_duration)
        time.sleep(EXTRA_SCAN_TIME)
        self.measure_power_and_validate()
