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

import math
import ntpath
import time
import acts.controllers.cellular_simulator as cc
from acts.test_utils.power.tel_simulations import LteSimulation
from acts.controllers.anritsu_lib import md8475a
from acts.controllers.anritsu_lib import _anritsu_utils as anritsu


class MD8475CellularSimulator(cc.AbstractCellularSimulator):

    MD8475_VERSION = 'A'

    # Indicates if it is able to use 256 QAM as the downlink modulation for LTE
    LTE_SUPPORTS_DL_256QAM = False

    # Indicates if it is able to use 64 QAM as the uplink modulation for LTE
    LTE_SUPPORTS_UL_64QAM = False

    # Indicates if 4x4 MIMO is supported for LTE
    LTE_SUPPORTS_4X4_MIMO = False

    # The maximum number of carriers that this simulator can support for LTE
    LTE_MAX_CARRIERS = 2

    # The maximum power that the equipment is able to transmit
    MAX_DL_POWER = -10

    # Simulation config files in the callbox computer.
    # These should be replaced in the future by setting up
    # the same configuration manually.
    LTE_BASIC_SIM_FILE = 'SIM_default_LTE.wnssp'
    LTE_BASIC_CELL_FILE = 'CELL_LTE_config.wnscp'
    LTE_CA_BASIC_SIM_FILE = 'SIM_LTE_CA.wnssp'
    LTE_CA_BASIC_CELL_FILE = 'CELL_LTE_CA_config.wnscp'

    # Filepath to the config files stored in the Anritsu callbox. Needs to be
    # formatted to replace {} with either A or B depending on the model.
    CALLBOX_CONFIG_PATH = 'C:\\Users\\MD8475A\\Documents\\DAN_configs\\'

    def __init__(self, ip_address):
        """ Initializes the cellular simulator.

        Args:
            ip_address: the ip address of the MD8475 instrument
        """
        super().__init__()

        try:
            self.anritsu = md8475a.MD8475A(ip_address,
                                           md8475_version=self.MD8475_VERSION)
        except anritsu.AnristuError:
            raise cc.CellularSimulatorError('Could not connect to MD8475.')

        self.bts = None

    def destroy(self):
        """ Sends finalization commands to the cellular equipment and closes
        the connection. """
        self.anritsu.stop_simulation()
        self.anritsu.disconnect()

    def setup_lte_scenario(self):
        """ Configures the equipment for an LTE simulation. """
        cell_file_name = self.LTE_BASIC_CELL_FILE
        sim_file_name = self.LTE_BASIC_SIM_FILE

        cell_file_path = ntpath.join(self.CALLBOX_CONFIG_PATH, cell_file_name)
        sim_file_path = ntpath.join(self.CALLBOX_CONFIG_PATH, sim_file_name)

        self.anritsu.load_simulation_paramfile(sim_file_path)
        self.anritsu.load_cell_paramfile(cell_file_path)
        self.anritsu.start_simulation()

        self.bts = [self.anritsu.get_BTS(md8475a.BtsNumber.BTS1)]

    def setup_lte_ca_scenario(self):
        """ Configures the equipment for an LTE with CA simulation. """
        cell_file_name = self.LTE_CA_BASIC_CELL_FILE
        sim_file_name = self.LTE_CA_BASIC_SIM_FILE

        cell_file_path = ntpath.join(self.CALLBOX_CONFIG_PATH, cell_file_name)
        sim_file_path = ntpath.join(self.CALLBOX_CONFIG_PATH, sim_file_name)

        self.anritsu.load_simulation_paramfile(sim_file_path)
        self.anritsu.load_cell_paramfile(cell_file_path)
        self.anritsu.start_simulation()

        self.bts = [
            self.anritsu.get_BTS(md8475a.BtsNumber.BTS1),
            self.anritsu.get_BTS(md8475a.BtsNumber.BTS2)
        ]

    def set_input_power(self, bts_index, input_power):
        """ Sets the input power for the indicated base station.

        Args:
            bts_index: the base station number
            input_power: the new input power
        """
        nrb_ul = int(self.bts[bts_index].nrb_ul)
        max_nrb_ul = self.bts[bts_index].max_nrb_ul
        input_level = str(
            round(input_power - 10 * math.log10(nrb_ul / max_nrb_ul), 1))
        if nrb_ul < max_nrb_ul:
            self.log.info('Number of UL RBs ({}) is less than the maximum RB '
                          'allocation ({}). Increasing UL reference power to '
                          '{} dbm to compensate'.format(
                              nrb_ul, max_nrb_ul, input_level))
        self.bts[bts_index].input_level = input_level

    def set_output_power(self, bts_index, output_power):
        """ Sets the output power for the indicated base station.

        Args:
            bts_index: the base station number
            output_power: the new output power
        """
        self.bts[bts_index].output_level = output_power

    def set_downlink_channel_number(self, bts_index, channel_number):
        """ Sets the downlink channel number for the indicated base station.

        Args:
            bts_index: the base station number
            channel_number: the new channel number
        """
        # Temporarily adding this line to workaround a bug in the
        # Anritsu callbox in which the channel number needs to be set
        # to a different value before setting it to the final one.
        self.bts[bts_index].dl_channel = str(channel_number + 1)
        time.sleep(8)
        self.bts[bts_index].dl_channel = str(channel_number)

    def set_enabled_for_ca(self, bts_index, enabled):
        """ Enables or disables the base station during carrier aggregation.

        Args:
            bts_index: the base station number
            enabled: whether the base station should be enabled for ca.
        """
        self.bts[bts_index].dl_cc_enabled = enabled

    def set_dl_modulation(self, bts_index, modulation):
        """ Sets the DL modulation for the indicated base station.

        Args:
            bts_index: the base station number
            modulation: the new DL modulation
        """
        self.bts[bts_index].lte_dl_modulation_order = modulation.value

    def set_ul_modulation(self, bts_index, modulation):
        """ Sets the UL modulation for the indicated base station.

        Args:
            bts_index: the base station number
            modulation: the new UL modulation
        """
        self.bts[bts_index].lte_ul_modulation_order = modulation.value

    def set_tbs_pattern_on(self, bts_index, tbs_pattern_on):
        """ Enables or disables TBS pattern in the indicated base station.

        Args:
            bts_index: the base station number
            tbs_pattern_on: the new TBS pattern setting
        """
        if tbs_pattern_on:
            self.bts[bts_index].tbs_pattern = 'FULLALLOCATION'
        else:
            self.bts[bts_index].tbs_pattern = 'OFF'

    def set_lte_rrc_state_change_timer(self, enabled, time=10):
        """ Configures the LTE RRC state change timer.

        Args:
            enabled: a boolean indicating if the timer should be on or off.
            time: time in seconds for the timer to expire
        """
        self.anritsu.set_lte_rrc_status_change(enabled)
        if enabled:
            self.anritsu.set_lte_rrc_status_change_timer(time)

    def set_cfi(self, bts_index, cfi):
        """ Sets the Channel Format Indicator for the indicated base station.

        Args:
            bts_index: the base station number
            cfi: the new CFI setting
        """
        self.bts[bts_index].cfi = cfi

    def set_paging_cycle(self, bts_index, cycle_duration):
        """ Sets the paging cycle duration for the indicated base station.

        Args:
            bts_index: the base station number
            cycle_duration: the new paging cycle duration in milliseconds
        """
        # TODO (b/146068532): implement.
        self.bts[bts_index].paging_duration = cycle_duration

    def set_phich_resource(self, bts_index, phich):
        """ Sets the PHICH Resource setting for the indicated base station.

        Args:
            bts_index: the base station number
            phich: the new PHICH resource setting
        """
        self.bts[bts_index].phich_resource = phich

    def set_band(self, bts_index, band):
        """ Sets the right duplex mode before switching to a new band.

        Args:
            bts_index: the base station number
            band: desired band
        """
        bts = self.bts[bts_index]

        # The callbox won't restore the band-dependent default values if the
        # request is to switch to the same band as the one the base station is
        # currently using. To ensure that default values are restored, go to a
        # different band before switching.
        if int(bts.band) == band:
            # Using bands 1 and 2 but it could be any others
            bts.band = '1' if band != 1 else '2'
            # Switching to config.band will be handled by the parent class
            # implementation of this method.

        bts.duplex_mode = self.get_duplex_mode(band).value
        bts.band = band
        time.sleep(5)  # It takes some time to propagate the new band

    def get_duplex_mode(self, band):
        """ Determines if the band uses FDD or TDD duplex mode

        Args:
            band: a band number
        Returns:
            an variable of class DuplexMode indicating if band is FDD or TDD
        """

        if 33 <= int(band) <= 46:
            return LteSimulation.DuplexMode.TDD
        else:
            return LteSimulation.DuplexMode.FDD

    def set_tdd_config(self, bts_index, config):
        """ Sets the frame structure for TDD bands.

        Args:
            bts_index: the base station number
            config: the desired frame structure. An int between 0 and 6.
        """

        if not 0 <= config <= 6:
            raise ValueError("The frame structure configuration has to be a "
                             "number between 0 and 6")

        self.bts[bts_index].uldl_configuration = config

        # Wait for the setting to propagate
        time.sleep(5)

    def set_ssf_config(self, bts_index, ssf_config):
        """ Sets the Special Sub-Frame config number for the indicated
        base station.

        Args:
            bts_index: the base station number
            ssf_config: the new ssf config number
        """
        # Cast to int in case it was passed as a string
        ssf_config = int(ssf_config)

        if not 0 <= ssf_config <= 9:
            raise ValueError('The Special Sub-Frame configuration has to be a '
                             'number between 0 and 9.')

        self.bts[bts_index].tdd_special_subframe = ssf_config

    def set_bandwidth(self, bts_index, bandwidth):
        """ Sets the LTE channel bandwidth (MHz)

        Args:
            bts_index: the base station number
            bandwidth: desired bandwidth (MHz)
        """
        bts = self.bts[bts_index]

        if bandwidth == 20:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_20MHz
        elif bandwidth == 15:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_15MHz
        elif bandwidth == 10:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_10MHz
        elif bandwidth == 5:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_5MHz
        elif bandwidth == 3:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_3MHz
        elif bandwidth == 1.4:
            bts.bandwidth = md8475a.BtsBandwidth.LTE_BANDWIDTH_1dot4MHz
        else:
            msg = "Bandwidth = {} MHz is not valid for LTE".format(bandwidth)
            self.log.error(msg)
            raise ValueError(msg)
        time.sleep(5)  # It takes some time to propagate the new settings

    def set_mimo_mode(self, bts_index, mimo):
        """ Sets the number of DL antennas for the desired MIMO mode.

        Args:
            bts_index: the base station number
            mimo: object of class MimoMode
        """

        bts = self.bts[bts_index]

        # If the requested mimo mode is not compatible with the current TM,
        # warn the user before changing the value.

        if mimo == LteSimulation.MimoMode.MIMO_1x1:
            if bts.transmode not in [
                    LteSimulation.TransmissionMode.TM1,
                    LteSimulation.TransmissionMode.TM7
            ]:
                self.log.warning(
                    "Using only 1 DL antennas is not allowed with "
                    "the current transmission mode. Changing the "
                    "number of DL antennas will override this "
                    "setting.")
            bts.dl_antenna = 1
        elif mimo == LteSimulation.MimoMode.MIMO_2x2:
            if bts.transmode not in [
                    LteSimulation.TransmissionMode.TM2,
                    LteSimulation.TransmissionMode.TM3,
                    LteSimulation.TransmissionMode.TM4,
                    LteSimulation.TransmissionMode.TM8,
                    LteSimulation.TransmissionMode.TM9
            ]:
                self.log.warning("Using two DL antennas is not allowed with "
                                 "the current transmission mode. Changing the "
                                 "number of DL antennas will override this "
                                 "setting.")
            bts.dl_antenna = 2
        elif mimo == LteSimulation.MimoMode.MIMO_4x4:
            if bts.transmode not in [
                    LteSimulation.TransmissionMode.TM2,
                    LteSimulation.TransmissionMode.TM3,
                    LteSimulation.TransmissionMode.TM4,
                    LteSimulation.TransmissionMode.TM9
            ]:
                self.log.warning("Using four DL antennas is not allowed with "
                                 "the current transmission mode. Changing the "
                                 "number of DL antennas will override this "
                                 "setting.")

            bts.dl_antenna = 4
        else:
            RuntimeError("The requested MIMO mode is not supported.")

    def set_scheduling_mode(self, bts_index, scheduling, mcs_dl, mcs_ul,
                            nrb_dl, nrb_ul):
        """ Sets the scheduling mode for LTE

        Args:
            bts_index: the base station number
            scheduling: DYNAMIC or STATIC scheduling (Enum list)
            mcs_dl: Downlink MCS (only for STATIC scheduling)
            mcs_ul: Uplink MCS (only for STATIC scheduling)
            nrb_dl: Number of RBs for downlink (only for STATIC scheduling)
            nrb_ul: Number of RBs for uplink (only for STATIC scheduling)
        """

        bts = self.bts[bts_index]
        bts.lte_scheduling_mode = scheduling.value

        if scheduling == LteSimulation.SchedulingMode.STATIC:

            if not all([nrb_dl, nrb_ul, mcs_dl, mcs_ul]):
                raise ValueError('When the scheduling mode is set to manual, '
                                 'the RB and MCS parameters are required.')

            bts.packet_rate = md8475a.BtsPacketRate.LTE_MANUAL
            bts.lte_mcs_dl = mcs_dl
            bts.lte_mcs_ul = mcs_ul
            bts.nrb_dl = nrb_dl
            bts.nrb_ul = nrb_ul

        time.sleep(5)  # It takes some time to propagate the new settings

    def lte_attach_secondary_carriers(self):
        """ Activates the secondary carriers for CA. Requires the DUT to be
        attached to the primary carrier first. """

        testcase = self.anritsu.get_AnritsuTestCases()
        # Setting the procedure to selection is needed because of a bug in the
        # instrument's software (b/139547391).
        testcase.procedure = md8475a.TestProcedure.PROCEDURE_SELECTION
        testcase.procedure = md8475a.TestProcedure.PROCEDURE_MULTICELL
        testcase.power_control = md8475a.TestPowerControl.POWER_CONTROL_DISABLE
        testcase.measurement_LTE = md8475a.TestMeasurement.MEASUREMENT_DISABLE

        self.anritsu.start_testcase()

        retry_counter = 0
        self.log.info("Waiting for the test case to start...")
        time.sleep(5)

        while self.anritsu.get_testcase_status() == "0":
            retry_counter += 1
            if retry_counter == 3:
                raise RuntimeError(
                    "The test case failed to start after {} "
                    "retries. The connection between the phone "
                    "and the base station might be unstable.".format(
                        retry_counter))
            time.sleep(10)

    def set_transmission_mode(self, bts_index, tmode):
        """ Sets the transmission mode for the LTE basetation

        Args:
            bts_index: the base station number
            tmode: Enum list from class 'TransmissionModeLTE'
        """

        bts = self.bts[bts_index]

        # If the selected transmission mode does not support the number of DL
        # antennas, throw an exception.
        if (tmode in [
                LteSimulation.TransmissionMode.TM1,
                LteSimulation.TransmissionMode.TM7
        ] and bts.dl_antenna != '1'):
            # TM1 and TM7 only support 1 DL antenna
            raise ValueError("{} allows only one DL antenna. Change the "
                             "number of DL antennas before setting the "
                             "transmission mode.".format(tmode.value))
        elif (tmode == LteSimulation.TransmissionMode.TM8
              and bts.dl_antenna != '2'):
            # TM8 requires 2 DL antennas
            raise ValueError("TM2 requires two DL antennas. Change the "
                             "number of DL antennas before setting the "
                             "transmission mode.")
        elif (tmode in [
                LteSimulation.TransmissionMode.TM2,
                LteSimulation.TransmissionMode.TM3,
                LteSimulation.TransmissionMode.TM4,
                LteSimulation.TransmissionMode.TM9
        ] and bts.dl_antenna == '1'):
            # TM2, TM3, TM4 and TM9 require 2 or 4 DL antennas
            raise ValueError("{} requires at least two DL atennas. Change the "
                             "number of DL antennas before setting the "
                             "transmission mode.".format(tmode.value))

        # The TM mode is allowed for the current number of DL antennas, so it
        # is safe to change this setting now
        bts.transmode = tmode.value

        time.sleep(5)  # It takes some time to propagate the new settings

    def wait_until_attached(self, timeout=120):
        """ Waits until the DUT is attached to the primary carrier.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        try:
            self.anritsu.wait_for_registration_state(time_to_wait=timeout)
        except anritsu.AnritsuError:
            raise cc.CellularSimulatorError('The phone did not attach before '
                                            'the timeout period ended.')

    def wait_until_communication_state(self, timeout=120):
        """ Waits until the DUT is in Communication state.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        try:
            self.anritsu.wait_for_communication_state(time_to_wait=timeout)
        except anritsu.AnritsuError:
            raise cc.CellularSimulatorError('The phone was not in '
                                            'Communication state before '
                                            'the timeout period ended.')

    def wait_until_idle_state(self, timeout=120):
        """ Waits until the DUT is in Idle state.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        try:
            self.anritsu.wait_for_idle_state(time_to_wait=timeout)
        except anritsu.AnritsuError:
            raise cc.CellularSimulatorError('The phone was not in Idle state '
                                            'before the time the timeout '
                                            'period ended.')

    def detach(self):
        """ Turns off all the base stations so the DUT loose connection."""
        self.anritsu.set_simulation_state_to_poweroff()

    def stop(self):
        """ Stops current simulation. After calling this method, the simulator
        will need to be set up again. """
        self.anritsu.stop_simulation()

    def start_data_traffic(self):
        """ Starts transmitting data from the instrument to the DUT. """
        try:
            self.anritsu.start_ip_traffic()
        except md8475a.AnritsuError as inst:
            # This typically happens when traffic is already running.
            # TODO (b/141962691): continue only if traffic is running
            self.log.warning(str(inst))
        time.sleep(4)

    def stop_data_traffic(self):
        """ Stops transmitting data from the instrument to the DUT. """
        try:
            self.anritsu.stop_ip_traffic()
        except md8475a.AnritsuError as inst:
            # This typically happens when traffic has already been stopped
            # TODO (b/141962691): continue only if traffic is stopped
            self.log.warning(str(inst))
        time.sleep(2)


class MD8475BCellularSimulator(MD8475CellularSimulator):

    MD8475_VERSION = 'B'

    # Indicates if it is able to use 256 QAM as the downlink modulation for LTE
    LTE_SUPPORTS_DL_256QAM = True

    # Indicates if it is able to use 64 QAM as the uplink modulation for LTE
    LTE_SUPPORTS_UL_64QAM = True

    # Indicates if 4x4 MIMO is supported for LTE
    LTE_SUPPORTS_4X4_MIMO = True

    # The maximum number of carriers that this simulator can support for LTE
    LTE_MAX_CARRIERS = 4

    # The maximum power that the equipment is able to transmit
    MAX_DL_POWER = -30

    # Simulation config files in the callbox computer.
    # These should be replaced in the future by setting up
    # the same configuration manually.
    LTE_BASIC_SIM_FILE = 'SIM_default_LTE.wnssp2'
    LTE_BASIC_CELL_FILE = 'CELL_LTE_config.wnscp2'
    LTE_CA_BASIC_SIM_FILE = 'SIM_LTE_CA.wnssp2'
    LTE_CA_BASIC_CELL_FILE = 'CELL_LTE_CA_config.wnscp2'

    # Filepath to the config files stored in the Anritsu callbox. Needs to be
    # formatted to replace {} with either A or B depending on the model.
    CALLBOX_CONFIG_PATH = 'C:\\Users\\MD8475B\\Documents\\DAN_configs\\'

    def setup_lte_ca_scenario(self):
        """ The B model can support up to five carriers. """

        super().setup_lte_ca_scenario()

        self.bts.extend([
            self.anritsu.get_BTS(md8475a.BtsNumber.BTS3),
            self.anritsu.get_BTS(md8475a.BtsNumber.BTS4),
            self.anritsu.get_BTS(md8475a.BtsNumber.BTS5)
        ])
