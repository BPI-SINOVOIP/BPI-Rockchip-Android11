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
from acts.test_utils.power.tel_simulations.LteSimulation import LteSimulation
import acts.test_utils.tel.anritsu_utils as anritsu_utils
import acts.controllers.anritsu_lib.md8475a as md8475a


class LteImsSimulation(LteSimulation):

    LTE_BASIC_SIM_FILE = 'VoLTE_ATT_Sim.wnssp'
    LTE_BASIC_CELL_FILE = 'VoLTE_ATT_Cell.wnscp'

    def attach(self):
        """ After attaching verify the UE has registered with the IMS server.

        Returns:
            True if the phone was able to attach, False if not.
        """

        if not super().attach():
            return False

        # The phone should have registered with the IMS server before attaching.
        # Make sure the IMS registration was successful by verifying the CSCF
        # status is SIP IDLE.
        if not anritsu_utils.wait_for_ims_cscf_status(
                self.log, self.anritsu,
                anritsu_utils.DEFAULT_IMS_VIRTUAL_NETWORK_ID,
                md8475a.ImsCscfStatus.SIPIDLE.value):
            self.log.error('UE failed to register with the IMS server.')
            return False

        return True
