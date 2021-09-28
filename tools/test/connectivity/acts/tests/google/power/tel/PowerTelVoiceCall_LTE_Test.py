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

import acts.test_utils.power.cellular.cellular_volte_power_test as cvltept


class PowerTelVoiceCall_LTE_Test(cvltept.PowerTelVoLTECallTest):
    def test_lteims_voice_band_12_pul_low_bw_10_tm_1_mimo_1x1_1(self):
        self.power_volte_call_test()

    def test_lteims_voice_band_4_pul_low_bw_10_tm_1_mimo_1x1_2(self):
        self.power_volte_call_test()

    def test_lteims_voice_band_30_pul_low_bw_10_tm_1_mimo_1x1_3(self):
        self.power_volte_call_test()

    def test_lteims_voice_band_4_pul_low_bw_20_tm_3_mimo_2x2_4(self):
        self.power_volte_call_test()

