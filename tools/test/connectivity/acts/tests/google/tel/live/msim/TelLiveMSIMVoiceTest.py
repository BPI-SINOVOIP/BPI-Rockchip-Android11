#!/usr/bin/env python3.4
#
#   Copyright 2018 - Google
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

from acts.test_utils.tel.tel_voice_utils \
        import two_phone_call_msim_short_seq, phone_setup_voice_general_for_slot
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_defines import MULTI_SIM_CONFIG


class TelLiveMSIMVoiceTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.sim_config = {
                            "config":MULTI_SIM_CONFIG,
                            "number_of_sims":2
                           }

    @TelephonyBaseTest.tel_test_wrap
    @test_tracker_info(uuid="3639cd85-7dba-4a81-8723-4f554e3ddcf8")
    def test_msim_voice_general(self):
        """ DSDS voice to voice call.

        1. Make Sure PhoneA attached to voice network.
        2. Make Sure PhoneB attached to voice network.
        3. Call from PhoneA to PhoneB, accept on PhoneB, hang up on PhoneA.
        4. Call from PhoneA to PhoneB, accept on PhoneB, hang up on PhoneB.
        5. Perform steps 3 and 4 with both sub_ids

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_general_for_slot, (self.log, ads[0], 0)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[1], 0)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[0], 1)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[1], 1))
                 ]

        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False

        return two_phone_call_msim_short_seq(self.log, ads[0], None, None,
                                             ads[1], None, None, None)
