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

import ntpath

import time
from acts.controllers.anritsu_lib.md8475a import BtsGprsMode
from acts.controllers.anritsu_lib.md8475a import BtsNumber
from acts.controllers.anritsu_lib import md8475_cellular_simulator as anritsusim
from acts.test_utils.power.tel_simulations.BaseSimulation import BaseSimulation
from acts.test_utils.tel.anritsu_utils import GSM_BAND_DCS1800
from acts.test_utils.tel.anritsu_utils import GSM_BAND_EGSM900
from acts.test_utils.tel.anritsu_utils import GSM_BAND_GSM850
from acts.test_utils.tel.anritsu_utils import GSM_BAND_RGSM900
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_ONLY


class GsmSimulation(BaseSimulation):
    """ Single base station GSM. """

    # Simulation config files in the callbox computer.
    # These should be replaced in the future by setting up
    # the same configuration manually.

    GSM_BASIC_SIM_FILE = 'SIM_default_GSM.wnssp'

    GSM_CELL_FILE = 'CELL_GSM_config.wnscp'

    # Test name parameters

    PARAM_BAND = "band"
    PARAM_GPRS = "gprs"
    PARAM_EGPRS = "edge"
    PARAM_NO_GPRS = "nogprs"
    PARAM_SLOTS = "slots"

    bands_parameter_mapping = {
        '850': GSM_BAND_GSM850,
        '900': GSM_BAND_EGSM900,
        '1800': GSM_BAND_DCS1800,
        '1900': GSM_BAND_RGSM900
    }

    def __init__(self, simulator, log, dut, test_config, calibration_table):
        """ Initializes the simulator for a single-carrier GSM simulation.

        Loads a simple LTE simulation enviroment with 1 basestation. It also
        creates the BTS handle so we can change the parameters as desired.

        Args:
            simulator: a cellular simulator controller
            log: a logger handle
            dut: the android device handler
            test_config: test configuration obtained from the config file
            calibration_table: a dictionary containing path losses for
                different bands.

        """
        # The GSM simulation relies on the cellular simulator to be a MD8475
        if not isinstance(self.simulator, anritsusim.MD8475CellularSimulator):
            raise ValueError('The GSM simulation relies on the simulator to '
                             'be an Anritsu MD8475 A/B instrument.')

        # The Anritsu controller needs to be unwrapped before calling
        # super().__init__ because setup_simulator() requires self.anritsu and
        # will be called during the parent class initialization.
        self.anritsu = self.simulator.anritsu
        self.bts1 = self.anritsu.get_BTS(BtsNumber.BTS1)

        super().__init__(simulator, log, dut, test_config, calibration_table)

        if not dut.droid.telephonySetPreferredNetworkTypesForSubscription(
                NETWORK_MODE_GSM_ONLY,
                dut.droid.subscriptionGetDefaultSubId()):
            log.error("Coold not set preferred network type.")
        else:
            log.info("Preferred network type set.")

    def setup_simulator(self):
        """ Do initial configuration in the simulator. """

        # Load callbox config files
        callbox_config_path = self.CALLBOX_PATH_FORMAT_STR.format(
            self.anritsu._md8475_version)

        self.anritsu.load_simulation_paramfile(
            ntpath.join(callbox_config_path, self.GSM_BASIC_SIM_FILE))
        self.anritsu.load_cell_paramfile(
            ntpath.join(callbox_config_path, self.GSM_CELL_FILE))

        # Start simulation if it wasn't started
        self.anritsu.start_simulation()

    def parse_parameters(self, parameters):
        """ Configs a GSM simulation using a list of parameters.

        Calls the parent method first, then consumes parameters specific to GSM.

        Args:
            parameters: list of parameters
        """

        # Setup band

        values = self.consume_parameter(parameters, self.PARAM_BAND, 1)

        if not values:
            raise ValueError(
                "The test name needs to include parameter '{}' followed by "
                "the required band number.".format(self.PARAM_BAND))

        self.set_band(self.bts1, values[1])
        self.load_pathloss_if_required()

        # Setup GPRS mode

        if self.consume_parameter(parameters, self.PARAM_GPRS):
            self.bts1.gsm_gprs_mode = BtsGprsMode.GPRS
        elif self.consume_parameter(parameters, self.PARAM_EGPRS):
            self.bts1.gsm_gprs_mode = BtsGprsMode.EGPRS
        elif self.consume_parameter(parameters, self.PARAM_NO_GPRS):
            self.bts1.gsm_gprs_mode = BtsGprsMode.NO_GPRS
        else:
            raise ValueError(
                "GPRS mode needs to be indicated in the test name with either "
                "{}, {} or {}.".format(self.PARAM_GPRS, self.PARAM_EGPRS,
                                       self.PARAM_NO_GPRS))

        # Setup slot allocation

        values = self.consume_parameter(parameters, self.PARAM_SLOTS, 2)

        if not values:
            raise ValueError(
                "The test name needs to include parameter {} followed by two "
                "int values indicating DL and UL slots.".format(
                    self.PARAM_SLOTS))

        self.bts1.gsm_slots = (int(values[1]), int(values[2]))

    def set_band(self, bts, band):
        """ Sets the band used for communication.

        Args:
            bts: basestation handle
            band: desired band
        """

        bts.band = band
        time.sleep(5)  # It takes some time to propagate the new band
