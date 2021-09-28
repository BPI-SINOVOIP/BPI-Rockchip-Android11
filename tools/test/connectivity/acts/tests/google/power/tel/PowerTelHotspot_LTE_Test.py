#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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

import acts.test_utils.power.cellular.cellular_hotspot_traffic_power_test as chtpw


class PowerTelHotspot_LTE_Test(chtpw.PowerTelHotspotTest):

    def test_lte_hotspot_band_13_pul_medium_bw_10_tm_4_mimo_2x2_direction_dlul_pattern_75_25_wifiband_5g_1(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_13_pul_medium_bw_10_tm_4_mimo_2x2_direction_dlul_pattern_75_25_wifiband_2g_2(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_13_pul_low_bw_10_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_2g_3(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_13_pul_low_bw_10_tm_4_mimo_2x2_direction_dl_pattern_100_0_wifiband_5g_4(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_12_pul_max_bw_10_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_2g_5(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_12_pul_max_bw_10_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_5g_6(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_2_pul_low_bw_5_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_5g_7(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_4_pul_low_bw_20_tm_3_mimo_4x4_direction_dl_pattern_100_0_wifiband_2g_8(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_low_bw_20_tm_4_mimo_2x2_direction_dl_pattern_100_0_wifiband_2g_9(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_low_bw_20_tm_4_mimo_2x2_direction_dl_pattern_100_0_wifiband_5g_10(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_low_bw_10_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_2g_11(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_low_bw_20_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_2g_12(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_low_bw_20_tm_3_mimo_4x4_direction_dl_pattern_100_0_wifiband_5g_13(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_3_pul_medium_bw_20_tm_4_mimo_2x2_direction_dlul_pattern_75_25_wifiband_5g_14(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_4_pul_low_bw_20_tm_3_mimo_4x4_direction_dl_pattern_100_0_wifiband_2g_15(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_4_pul_low_bw_20_tm_3_mimo_4x4_direction_dl_pattern_100_0_wifiband_5g_16(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_4_pul_max_bw_20_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_2g_17(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_4_pul_max_bw_20_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_5g_18(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_low_bw_20_tm_4_mimo_2x2_direction_dl_pattern_100_0_wifiband_2g_19(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_low_bw_20_tm_4_mimo_2x2_direction_dl_pattern_100_0_wifiband_5g_20(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_low_bw_10_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_2g_21(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_low_bw_20_tm_1_mimo_1x1_direction_dl_pattern_100_0_wifiband_2g_22(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_low_bw_20_tm_3_mimo_4x4_direction_dl_pattern_100_0_wifiband_5g_23(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_medium_bw_20_tm_4_mimo_2x2_direction_dlul_pattern_75_25_wifiband_5g_24(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_max_bw_20_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_2g_25(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_7_pul_max_bw_20_tm_1_mimo_1x1_direction_ul_pattern_100_100_wifiband_5g_26(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_38_pul_low_bw_10_tm_1_mimo_1x1_direction_dlul_tddconfig_2_wifiband_2g_27(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_38_pul_low_bw_10_tm_4_mimo_2x2_direction_dlul_tddconfig_2_wifiband_5g_28(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_40_pul_low_bw_20_tm_4_mimo_2x2_direction_dlul_tddconfig_2_wifiband_5g_29(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_41_pul_low_bw_20_tm_3_mimo_4x4_direction_dlul_tddconfig_2_wifiband_5g_30(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_41_pul_max_bw_20_tm_1_mimo_1x1_direction_dlul_tddconfig_2_wifiband_2g_31(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_41_pul_max_bw_20_tm_1_mimo_1x1_direction_dlul_tddconfig_2_wifiband_5g_32(self):
        self.power_tel_tethering_test()

    def test_lte_hotspot_band_41_pul_medium_bw_20_tm_4_mimo_2x2_direction_dlul_tddconfig_2_wifiband_5g_33(self):
        self.power_tel_tethering_test()
