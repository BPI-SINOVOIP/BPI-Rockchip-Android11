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

import time
from acts.test_utils.tel.tel_test_utils \
              import sms_send_receive_verify, multithread_func
from acts.utils import rand_ascii_str
from acts.test_utils.tel.tel_subscription_utils \
              import get_subid_from_slot_index, set_subid_for_message
from acts.test_utils.tel.tel_defines \
              import MULTI_SIM_CONFIG, WAIT_TIME_ANDROID_STATE_SETTLING
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_voice_utils \
              import phone_setup_voice_general_for_slot


class TelLiveMSIMSmsTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.numer_of_slots = 2
        self.sim_config = { "config": MULTI_SIM_CONFIG,
                            "number_of_sims": 2 }
        self.message_lengths = (50, 160, 180)

    def _sms_test_dsds(self, ads):
        """Test SMS between two phones on multiple sub_id

        Returns:
            True if success.
            False if failed.
        """
        for slot in range(self.sim_config["number_of_sims"]):
            set_subid_for_message(
                ads[0], get_subid_from_slot_index(self.log,ads[0],slot))
            set_subid_for_message(
                ads[1], get_subid_from_slot_index(self.log,ads[1],slot))
            for length in self.message_lengths:
                message_array = [rand_ascii_str(length)]
                ads[0].log.info("SMS Test - length %s from slot %s to slot %s",
                                 length, slot, slot)
                if not sms_send_receive_verify(self.log, ads[0], ads[1],
                                              message_array, slot_id_rx = slot):
                    ads[0].log.warning(
                        "SMS of length %s test failed from slot %s to slot %s",
                         length, slot, slot)
                    return False
                ads[0].log.info("SMS Test - length %s from slot %s to slot %s",
                                length, slot, 1-slot)
                if not sms_send_receive_verify(self.log, ads[0], ads[1],
                                            message_array, slot_id_rx = 1-slot):
                    ads[0].log.warning(
                        "SMS of length %s test failed from slot %s to slot %s",
                        length, slot, 1-slot)
                    return False
                else:
                    ads[0].log.info("SMS of length %s test succeeded", length)
            self.log.info("SMS test of length %s characters succeeded.",
                          self.message_lengths)
        return True

    @test_tracker_info(uuid="5c0b60d0-a963-4ccf-8c0a-3d968b99d3cd")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_sms_general(self):
        """Test SMS basic function between two phone. Phones in any network.

        Airplane mode is off.
        Send SMS from PhoneA (sub_id 0 and 1) to PhoneB (sub_id 0 and 1)
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_general_for_slot, (self.log, ads[0],0)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[1],0)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[0],1)),
                 (phone_setup_voice_general_for_slot, (self.log, ads[1],1))
                 ]

        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_dsds(ads)

