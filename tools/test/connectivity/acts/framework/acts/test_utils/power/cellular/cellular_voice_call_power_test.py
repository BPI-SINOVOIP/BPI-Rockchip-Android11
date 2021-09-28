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

import time

from acts.controllers.anritsu_lib.md8475a import VirtualPhoneAutoAnswer

import acts.test_utils.power.cellular.cellular_power_base_test as PWCEL
from acts.test_utils.tel.tel_test_utils import initiate_call, hangup_call, set_phone_silent_mode


class PowerTelVoiceCallTest(PWCEL.PowerCellularLabBaseTest):
    """ Voice call power test.

    Inherits from PowerCellularLabBaseTest. Contains methods to initiate
    a voice call from the DUT and pick up from the callbox.

    """

    # Time for the callbox to pick up the call
    CALL_SETTLING_TIME = 10

    def setup_class(self):
        """ Executed only once when initializing the class.

        Configs the callbox to pick up the call automatically and
        sets the phone to silent mode.
        """

        super().setup_class()

        # Make the callbox pick up the call automatically
        self.virtualPhoneHandle = self.anritsu.get_VirtualPhone()
        self.virtualPhoneHandle.auto_answer = (VirtualPhoneAutoAnswer.ON, 0)

        # Set voice call volume to minimum
        set_phone_silent_mode(self.log, self.dut)

    def power_voice_call_test(self):
        """ Measures power during a voice call.

        Measurement step in this test. Starts the voice call and
        initiates power measurement. Pass or fail is decided with a
        threshold value.
        """

        # Initiate the voice call
        initiate_call(self.log, self.dut, "+11112223333")

        # Wait for the callbox to pick up
        time.sleep(self.CALL_SETTLING_TIME)

        # Mute the call
        self.dut.droid.telecomCallMute()

        # Turn of screen
        self.dut.droid.goToSleepNow()

        # Measure power
        result = self.collect_power_data()

        # End the call
        hangup_call(self.log, self.dut)

        # Check if power measurement is within the required values
        self.pass_fail_check(result.average_current)
