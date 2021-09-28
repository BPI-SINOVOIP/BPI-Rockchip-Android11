#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import acts.test_utils.power.cellular.cellular_traffic_power_test as ctpt


class PowerTelTraffic_UMTS_Test(ctpt.PowerTelTrafficTest):
    def test_umts_traffic_r_8_band_1_pul_max_direction_ul_pattern_0_100_1(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_1_pul_high_direction_ul_pattern_0_100_2(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_1_pul_medium_direction_ul_pattern_0_100_3(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_1_pul_low_direction_ul_pattern_0_100_4(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_7_band_1_pul_low_direction_ul_pattern_0_100_5(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_99_band_1_pul_low_direction_ul_pattern_0_100_6(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_4_pul_low_direction_ul_pattern_0_100_7(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_5_pul_low_direction_ul_pattern_0_100_8(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_5_pul_low_direction_dl_pattern_100_0_9(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_5_pul_low_direction_dlul_pattern_90_10_10(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_5_pul_low_direction_dlul_pattern_75_25_11(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_8_band_5_pul_low_direction_dlul_pattern_50_50_12(self):
        self.power_tel_traffic_test()

    def test_umts_traffic_r_7_band_4_pul_max_direction_dl_pattern_100_0_13(self):
        self.power_tel_traffic_test()

    #def test_umts_traffic_r_99_band_4_pul_max_direction_dl_pattern_100_0_14(self):
    #    self.power_tel_traffic_test()

    def test_umts_traffic_r_7_band_4_pul_max_direction_ul_pattern_0_100_15(self):
        self.power_tel_traffic_test()

    #def test_umts_traffic_r_99_band_4_pul_max_direction_ul_pattern_0_100_16(self):
    #    self.power_tel_traffic_test()