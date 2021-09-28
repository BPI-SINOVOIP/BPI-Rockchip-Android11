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


class PowerTelTraffic_GSM_Test(ctpt.PowerTelTrafficTest):
    def test_gsm_traffic_band_1900_gprs_pul_edge_direction_ul_pattern_0_100_slots_1_4_1(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_1900_gprs_pul_weak_direction_ul_pattern_0_100_slots_1_4_2(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_1900_gprs_pul_medium_direction_ul_pattern_0_100_slots_1_4_3(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_1900_gprs_pul_excellent_direction_ul_pattern_0_100_slots_1_4_4(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_1900_edge_pul_excellent_direction_ul_pattern_0_100_slots_1_4_5(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_850_edge_pul_excellent_direction_ul_pattern_0_100_slots_1_4_6(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_900_edge_pul_excellent_direction_ul_pattern_0_100_slots_1_4_7(self):
        self.power_tel_traffic_test()

    def test_gsm_traffic_band_1800_edge_pul_excellent_direction_ul_pattern_0_100_slots_1_4_8(self):
        self.power_tel_traffic_test()