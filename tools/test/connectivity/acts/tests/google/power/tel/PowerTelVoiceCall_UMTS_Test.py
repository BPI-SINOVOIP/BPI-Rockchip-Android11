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

import acts.test_utils.power.cellular.cellular_voice_call_power_test as cvcpt


class PowerTelVoiceCall_UMTS_Test(cvcpt.PowerTelVoiceCallTest):
    def test_umts_voice_r_8_band_1_pul_low_1(self):
        self.power_voice_call_test()

    def test_umts_voice_r_8_band_4_pul_max_2(self):
        self.power_voice_call_test()

    def test_umts_voice_r_7_band_5_pul_low_3(self):
        self.power_voice_call_test()

    def test_umts_voice_r_7_band_4_pul_max_4(self):
        self.power_voice_call_test()

    def test_umts_voice_r_99_band_1_pul_low_5(self):
        self.power_voice_call_test()

    def test_umts_voice_r_99_band_5_pul_max_6(self):
        self.power_voice_call_test()

