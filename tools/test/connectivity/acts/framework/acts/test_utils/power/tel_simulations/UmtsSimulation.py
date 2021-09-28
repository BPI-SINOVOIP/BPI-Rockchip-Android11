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

from acts.controllers.anritsu_lib import md8475_cellular_simulator as anritsusim
from acts.controllers.anritsu_lib.md8475a import BtsNumber
from acts.controllers.anritsu_lib.md8475a import BtsPacketRate
from acts.test_utils.power.tel_simulations.BaseSimulation import BaseSimulation
from acts.test_utils.tel.tel_defines import NETWORK_MODE_WCDMA_ONLY


class UmtsSimulation(BaseSimulation):
    """ Single base station simulation. """

    # Simulation config files in the callbox computer.
    # These should be replaced in the future by setting up
    # the same configuration manually.

    UMTS_BASIC_SIM_FILE = 'SIM_default_WCDMA.wnssp'

    UMTS_R99_CELL_FILE = 'CELL_WCDMA_R99_config.wnscp'

    UMTS_R7_CELL_FILE = 'CELL_WCDMA_R7_config.wnscp'

    UMTS_R8_CELL_FILE = 'CELL_WCDMA_R8_config.wnscp'

    # Test name parameters
    PARAM_RELEASE_VERSION = "r"
    PARAM_RELEASE_VERSION_99 = "99"
    PARAM_RELEASE_VERSION_8 = "8"
    PARAM_RELEASE_VERSION_7 = "7"
    PARAM_UL_PW = 'pul'
    PARAM_DL_PW = 'pdl'
    PARAM_BAND = "band"
    PARAM_RRC_STATUS_CHANGE_TIMER = "rrcstatuschangetimer"

    # Units in which signal level is defined in DOWNLINK_SIGNAL_LEVEL_DICTIONARY
    DOWNLINK_SIGNAL_LEVEL_UNITS = "RSCP"

    # RSCP signal levels thresholds (as reported by Android). Units are dBm
    # Using LTE thresholds + 24 dB to have equivalent SPD
    # 24 dB comes from 10 * log10(3.84 MHz / 15 KHz)

    DOWNLINK_SIGNAL_LEVEL_DICTIONARY = {
        'excellent': -51,
        'high': -76,
        'medium': -86,
        'weak': -96
    }

    # Transmitted output power for the phone
    # Stronger Tx power means that the signal received by the BTS is weaker
    # Units are dBm

    UPLINK_SIGNAL_LEVEL_DICTIONARY = {
        'low': -20,
        'medium': 8,
        'high': 15,
        'max': 23
    }

    # Converts packet rate to the throughput that can be actually obtained in
    # Mbits/s

    packet_rate_to_dl_throughput = {
        BtsPacketRate.WCDMA_DL384K_UL64K: 0.362,
        BtsPacketRate.WCDMA_DL21_6M_UL5_76M: 18.5,
        BtsPacketRate.WCDMA_DL43_2M_UL5_76M: 36.9
    }

    packet_rate_to_ul_throughput = {
        BtsPacketRate.WCDMA_DL384K_UL64K: 0.0601,
        BtsPacketRate.WCDMA_DL21_6M_UL5_76M: 5.25,
        BtsPacketRate.WCDMA_DL43_2M_UL5_76M: 5.25
    }

    def __init__(self, simulator, log, dut, test_config, calibration_table):
        """ Initializes the cellular simulator for a UMTS simulation.

        Loads a simple UMTS simulation enviroment with 1 basestation. It also
        creates the BTS handle so we can change the parameters as desired.

        Args:
            simulator: a cellular simulator controller
            log: a logger handle
            dut: the android device handler
            test_config: test configuration obtained from the config file
            calibration_table: a dictionary containing path losses for
                different bands.

        """
        # The UMTS simulation relies on the cellular simulator to be a MD8475
        if not isinstance(self.simulator, anritsusim.MD8475CellularSimulator):
            raise ValueError('The UMTS simulation relies on the simulator to '
                             'be an Anritsu MD8475 A/B instrument.')

        # The Anritsu controller needs to be unwrapped before calling
        # super().__init__ because setup_simulator() requires self.anritsu and
        # will be called during the parent class initialization.
        self.anritsu = self.simulator.anritsu
        self.bts1 = self.anritsu.get_BTS(BtsNumber.BTS1)

        super().__init__(simulator, log, dut, test_config, calibration_table)

        if not dut.droid.telephonySetPreferredNetworkTypesForSubscription(
                NETWORK_MODE_WCDMA_ONLY,
                dut.droid.subscriptionGetDefaultSubId()):
            log.error("Coold not set preferred network type.")
        else:
            log.info("Preferred network type set.")

        self.release_version = None
        self.packet_rate = None

    def setup_simulator(self):
        """ Do initial configuration in the simulator. """

        # Load callbox config files
        callbox_config_path = self.CALLBOX_PATH_FORMAT_STR.format(
            self.anritsu._md8475_version)

        self.anritsu.load_simulation_paramfile(
            ntpath.join(callbox_config_path, self.UMTS_BASIC_SIM_FILE))

        # Start simulation if it wasn't started
        self.anritsu.start_simulation()

    def parse_parameters(self, parameters):
        """ Configs an UMTS simulation using a list of parameters.

        Calls the parent method and consumes parameters specific to UMTS.

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

        # Setup release version

        values = self.consume_parameter(parameters, self.PARAM_RELEASE_VERSION,
                                        1)

        if not values or values[1] not in [
                self.PARAM_RELEASE_VERSION_7, self.PARAM_RELEASE_VERSION_8,
                self.PARAM_RELEASE_VERSION_99
        ]:
            raise ValueError(
                "The test name needs to include the parameter {} followed by a "
                "valid release version.".format(self.PARAM_RELEASE_VERSION))

        self.set_release_version(self.bts1, values[1])

        # Setup W-CDMA RRC status change and CELL_DCH timer for idle test case

        values = self.consume_parameter(parameters,
                                        self.PARAM_RRC_STATUS_CHANGE_TIMER, 1)
        if not values:
            self.log.info(
                "The test name does not include the '{}' parameter. Disabled "
                "by default.".format(self.PARAM_RRC_STATUS_CHANGE_TIMER))
            self.anritsu.set_umts_rrc_status_change(False)
        else:
            self.rrc_sc_timer = int(values[1])
            self.anritsu.set_umts_rrc_status_change(True)
            self.anritsu.set_umts_dch_stat_timer(self.rrc_sc_timer)

        # Setup uplink power

        ul_power = self.get_uplink_power_from_parameters(parameters)

        # Power is not set on the callbox until after the simulation is
        # started. Saving this value in a variable for later
        self.sim_ul_power = ul_power

        # Setup downlink power

        dl_power = self.get_downlink_power_from_parameters(parameters)

        # Power is not set on the callbox until after the simulation is
        # started. Saving this value in a variable for later
        self.sim_dl_power = dl_power

    def set_release_version(self, bts, release_version):
        """ Sets the release version.

        Loads the cell parameter file matching the requested release version.
        Does nothing is release version is already the one requested.

        """

        if release_version == self.release_version:
            self.log.info(
                "Release version is already {}.".format(release_version))
            return
        if release_version == self.PARAM_RELEASE_VERSION_99:

            cell_parameter_file = self.UMTS_R99_CELL_FILE
            self.packet_rate = BtsPacketRate.WCDMA_DL384K_UL64K

        elif release_version == self.PARAM_RELEASE_VERSION_7:

            cell_parameter_file = self.UMTS_R7_CELL_FILE
            self.packet_rate = BtsPacketRate.WCDMA_DL21_6M_UL5_76M

        elif release_version == self.PARAM_RELEASE_VERSION_8:

            cell_parameter_file = self.UMTS_R8_CELL_FILE
            self.packet_rate = BtsPacketRate.WCDMA_DL43_2M_UL5_76M

        else:
            raise ValueError("Invalid UMTS release version number.")

        self.anritsu.load_cell_paramfile(
            ntpath.join(self.callbox_config_path, cell_parameter_file))

        self.release_version = release_version

        # Loading a cell parameter file stops the simulation
        self.start()

        bts.packet_rate = self.packet_rate

    def maximum_downlink_throughput(self):
        """ Calculates maximum achievable downlink throughput in the current
            simulation state.

        Returns:
            Maximum throughput in mbps.

        """

        if self.packet_rate not in self.packet_rate_to_dl_throughput:
            raise NotImplementedError("Packet rate not contained in the "
                                      "throughput dictionary.")
        return self.packet_rate_to_dl_throughput[self.packet_rate]

    def maximum_uplink_throughput(self):
        """ Calculates maximum achievable uplink throughput in the current
            simulation state.

        Returns:
            Maximum throughput in mbps.

        """

        if self.packet_rate not in self.packet_rate_to_ul_throughput:
            raise NotImplementedError("Packet rate not contained in the "
                                      "throughput dictionary.")
        return self.packet_rate_to_ul_throughput[self.packet_rate]

    def set_downlink_rx_power(self, bts, signal_level):
        """ Starts IP data traffic while setting downlink power.

        This is only necessary for UMTS for unclear reasons. b/139026916 """

        # Starts IP traffic while changing this setting to force the UE to be
        # in Communication state, as UL power cannot be set in Idle state
        self.start_traffic_for_calibration()

        # Wait until it goes to communication state
        self.anritsu.wait_for_communication_state()

        super().set_downlink_rx_power(bts, signal_level)

        # Stop IP traffic after setting the signal level
        self.stop_traffic_for_calibration()

    def set_band(self, bts, band):
        """ Sets the band used for communication.

        Args:
            bts: basestation handle
            band: desired band
        """

        bts.band = band
        time.sleep(5)  # It takes some time to propagate the new band
