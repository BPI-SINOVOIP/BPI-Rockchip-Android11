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


class PowerTelTraffic_LTE_Test(ctpt.PowerTelTrafficTest):
    def test_lte_traffic_band_12_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_1(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_12_pul_max_bw_5_tm_1_mimo_1x1_pattern_0_100_2(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_12_pul_low_bw_14_tm_1_mimo_1x1_pattern_0_100_3(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_20_pul_low_bw_5_tm_3_mimo_2x2_pattern_100_0_4(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_13_pul_low_bw_5_tm_1_mimo_1x1_pattern_75_25_5(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_13_pul_max_bw_10_tm_1_mimo_1x1_pattern_100_100_6(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_5_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_7(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_1_pul_medium_bw_20_tm_3_mimo_4x4_pattern_100_0_8(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_1_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_9(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_3_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_10(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_3_pul_max_bw_10_tm_1_mimo_1x1_pattern_100_100_11(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_2_pul_low_bw_3_tm_1_mimo_1x1_pattern_0_100_12(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_2_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_13(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_2_pul_medium_bw_20_tm_3_mimo_4x4_pattern_100_0_14(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_15(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_low_bw_5_tm_3_mimo_4x4_pattern_100_0_16(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_max_bw_5_tm_3_mimo_4x4_pattern_100_100_17(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_medium_bw_10_tm_3_mimo_4x4_pattern_100_0_18(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_medium_bw_20_tm_3_mimo_4x4_pattern_100_0_19(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_4_pul_max_bw_20_tm_1_mimo_1x1_pattern_100_100_20(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_high_bw_15_tm_1_mimo_1x1_pattern_0_100_21(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_high_bw_20_tm_1_mimo_1x1_pattern_0_100_22(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_max_bw_10_tm_1_mimo_1x1_pattern_100_100_23(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_max_bw_20_tm_1_mimo_1x1_pattern_100_100_24(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_low_bw_10_tm_4_mimo_2x2_pattern_75_25_25(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_medium_bw_10_tm_3_mimo_4x4_pattern_100_0_26(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_7_pul_medium_bw_20_tm_3_mimo_4x4_pattern_100_0_27(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_38_pul_medium_bw_20_tm_3_mimo_4x4_tddconfig_2_28(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_38_pul_max_bw_10_tm_1_mimo_1x1_tddconfig_1_29(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_38_pul_high_bw_5_tm_1_mimo_1x1_tddconfig_5_30(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_40_pul_low_bw_20_tm_4_mimo_2x2_tddconfig_2_31(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_40_pul_max_bw_10_tm_1_mimo_1x1_tddconfig_5_32(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_41_pul_medium_bw_20_tm_3_mimo_4x4_tddconfig_2_33(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_41_pul_high_bw_15_tm_1_mimo_1x1_tddconfig_1_34(self):
        self.power_tel_traffic_test()

    def test_lte_traffic_band_42_pul_low_bw_20_tm_4_mimo_2x2_tddconfig_2_35(self):
        self.power_tel_traffic_test()