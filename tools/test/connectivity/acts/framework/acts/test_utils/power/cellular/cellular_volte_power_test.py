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

import acts.test_utils.tel.anritsu_utils as anritsu_utils
import acts.controllers.anritsu_lib.md8475a as md8475a

import acts.test_utils.power.cellular.cellular_power_base_test as PWCEL
from acts.test_utils.tel.tel_test_utils import initiate_call, hangup_call, set_phone_silent_mode


class PowerTelVoLTECallTest(PWCEL.PowerCellularLabBaseTest):
    """ VoLTE call power test.

    Inherits from PowerCellularLabBaseTest. Contains methods to initiate
    a voice call from the IMS server and pick up on the UE.

    """

    # Waiting time before trying to pick up from the phone
    CALL_INITIATING_TIME = 10

    def setup_class(self):
        """ Executed only once when initializing the class. """

        super().setup_class()

        # Set voice call volume to minimum
        set_phone_silent_mode(self.log, self.dut)

    def power_volte_call_test(self):
        """ Measures power during a VoLTE call.

        Measurement step in this test. Starts the voice call and
        initiates power measurement. Pass or fail is decided with a
        threshold value. """

        # Initiate the voice call
        self.anritsu.ims_cscf_call_action(
            anritsu_utils.DEFAULT_IMS_VIRTUAL_NETWORK_ID,
            md8475a.ImsCscfCall.MAKE.value)

        # Wait for the call to be started
        time.sleep(self.CALL_INITIATING_TIME)

        # Pickup the call
        self.dut.adb.shell('input keyevent KEYCODE_CALL')

        # Mute the call
        self.dut.droid.telecomCallMute()

        # Turn of screen
        self.dut.droid.goToSleepNow()

        # Measure power
        self.collect_power_data()

        # End the call
        hangup_call(self.log, self.dut)

        # Check if power measurement is within the required values
        self.pass_fail_check()
