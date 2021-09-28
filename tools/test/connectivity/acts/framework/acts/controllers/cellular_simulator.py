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
from acts import logger
from acts.test_utils.power import tel_simulations as sims


class AbstractCellularSimulator:
    """ A generic cellular simulator controller class that can be derived to
    implement equipment specific classes and allows the tests to be implemented
    without depending on a singular instrument model.

    This class defines the interface that every cellular simulator controller
    needs to implement and shouldn't be instantiated by itself. """

    # Indicates if it is able to use 256 QAM as the downlink modulation for LTE
    LTE_SUPPORTS_DL_256QAM = None

    # Indicates if it is able to use 64 QAM as the uplink modulation for LTE
    LTE_SUPPORTS_UL_64QAM = None

    # Indicates if 4x4 MIMO is supported for LTE
    LTE_SUPPORTS_4X4_MIMO = None

    # The maximum number of carriers that this simulator can support for LTE
    LTE_MAX_CARRIERS = None

    # The maximum power that the equipment is able to transmit
    MAX_DL_POWER = None

    def __init__(self):
        """ Initializes the cellular simulator. """
        self.log = logger.create_tagged_trace_logger('CellularSimulator')

    def destroy(self):
        """ Sends finalization commands to the cellular equipment and closes
        the connection. """
        raise NotImplementedError()

    def setup_lte_scenario(self):
        """ Configures the equipment for an LTE simulation. """
        raise NotImplementedError()

    def setup_lte_ca_scenario(self):
        """ Configures the equipment for an LTE with CA simulation. """
        raise NotImplementedError()

    def configure_bts(self, config, bts_index=0):
        """ Commands the equipment to setup a base station with the required
        configuration. This method applies configurations that are common to all
        RATs.

        Args:
            config: a BaseSimulation.BtsConfig object.
            bts_index: the base station number.
        """

        if config.output_power:
            self.set_output_power(bts_index, config.output_power)

        if config.input_power:
            self.set_input_power(bts_index, config.input_power)

        if isinstance(config, sims.LteSimulation.LteSimulation.BtsConfig):
            self.configure_lte_bts(config, bts_index)

    def configure_lte_bts(self, config, bts_index=0):
        """ Commands the equipment to setup an LTE base station with the
        required configuration.

        Args:
            config: an LteSimulation.BtsConfig object.
            bts_index: the base station number.
        """
        if config.band:
            self.set_band(bts_index, config.band)

        if config.dlul_config:
            self.set_tdd_config(bts_index, config.dlul_config)

        if config.ssf_config:
            self.set_ssf_config(bts_index, config.ssf_config)

        if config.bandwidth:
            self.set_bandwidth(bts_index, config.bandwidth)

        if config.dl_channel:
            self.set_downlink_channel_number(bts_index, config.dl_channel)

        if config.mimo_mode:
            self.set_mimo_mode(bts_index, config.mimo_mode)

        if config.transmission_mode:
            self.set_transmission_mode(bts_index, config.transmission_mode)

        # Modulation order should be set before set_scheduling_mode being
        # called.
        if config.dl_modulation_order:
            self.set_dl_modulation(bts_index, config.dl_modulation_order)

        if config.ul_modulation_order:
            self.set_ul_modulation(bts_index, config.ul_modulation_order)

        if config.scheduling_mode:

            if (config.scheduling_mode ==
                    sims.LteSimulation.SchedulingMode.STATIC
                    and not (config.dl_rbs and config.ul_rbs and config.dl_mcs
                             and config.ul_mcs)):
                raise ValueError('When the scheduling mode is set to manual, '
                                 'the RB and MCS parameters are required.')

            # If scheduling mode is set to Dynamic, the RB and MCS parameters
            # will be ignored by set_scheduling_mode.
            self.set_scheduling_mode(bts_index, config.scheduling_mode,
                                     config.dl_mcs, config.ul_mcs,
                                     config.dl_rbs, config.ul_rbs)

        # This variable stores a boolean value so the following is needed to
        # differentiate False from None
        if config.dl_cc_enabled is not None:
            self.set_enabled_for_ca(bts_index, config.dl_cc_enabled)

        # This variable stores a boolean value so the following is needed to
        # differentiate False from None
        if config.tbs_pattern_on is not None:
            self.set_tbs_pattern_on(bts_index, config.tbs_pattern_on)

        if config.cfi:
            self.set_cfi(bts_index, config.cfi)

        if config.paging_cycle:
            self.set_paging_cycle(bts_index, config.paging_cycle)

        if config.phich:
            self.set_phich_resource(bts_index, config.phich)

    def set_lte_rrc_state_change_timer(self, enabled, time=10):
        """ Configures the LTE RRC state change timer.

        Args:
            enabled: a boolean indicating if the timer should be on or off.
            time: time in seconds for the timer to expire
        """
        raise NotImplementedError()

    def set_band(self, bts_index, band):
        """ Sets the band for the indicated base station.

        Args:
            bts_index: the base station number
            band: the new band
        """
        raise NotImplementedError()

    def set_input_power(self, bts_index, input_power):
        """ Sets the input power for the indicated base station.

        Args:
            bts_index: the base station number
            input_power: the new input power
        """
        raise NotImplementedError()

    def set_output_power(self, bts_index, output_power):
        """ Sets the output power for the indicated base station.

        Args:
            bts_index: the base station number
            output_power: the new output power
        """
        raise NotImplementedError()

    def set_tdd_config(self, bts_index, tdd_config):
        """ Sets the tdd configuration number for the indicated base station.

        Args:
            bts_index: the base station number
            tdd_config: the new tdd configuration number
        """
        raise NotImplementedError()

    def set_ssf_config(self, bts_index, ssf_config):
        """ Sets the Special Sub-Frame config number for the indicated
        base station.

        Args:
            bts_index: the base station number
            ssf_config: the new ssf config number
        """
        raise NotImplementedError()

    def set_bandwidth(self, bts_index, bandwidth):
        """ Sets the bandwidth for the indicated base station.

        Args:
            bts_index: the base station number
            bandwidth: the new bandwidth
        """
        raise NotImplementedError()

    def set_downlink_channel_number(self, bts_index, channel_number):
        """ Sets the downlink channel number for the indicated base station.

        Args:
            bts_index: the base station number
            channel_number: the new channel number
        """
        raise NotImplementedError()

    def set_mimo_mode(self, bts_index, mimo_mode):
        """ Sets the mimo mode for the indicated base station.

        Args:
            bts_index: the base station number
            mimo_mode: the new mimo mode
        """
        raise NotImplementedError()

    def set_transmission_mode(self, bts_index, transmission_mode):
        """ Sets the transmission mode for the indicated base station.

        Args:
            bts_index: the base station number
            transmission_mode: the new transmission mode
        """
        raise NotImplementedError()

    def set_scheduling_mode(self, bts_index, scheduling_mode, mcs_dl, mcs_ul,
                            nrb_dl, nrb_ul):
        """ Sets the scheduling mode for the indicated base station.

        Args:
            bts_index: the base station number
            scheduling_mode: the new scheduling mode
            mcs_dl: Downlink MCS (only for STATIC scheduling)
            mcs_ul: Uplink MCS (only for STATIC scheduling)
            nrb_dl: Number of RBs for downlink (only for STATIC scheduling)
            nrb_ul: Number of RBs for uplink (only for STATIC scheduling)
        """
        raise NotImplementedError()

    def set_enabled_for_ca(self, bts_index, enabled):
        """ Enables or disables the base station during carrier aggregation.

        Args:
            bts_index: the base station number
            enabled: whether the base station should be enabled for ca.
        """
        raise NotImplementedError()

    def set_dl_modulation(self, bts_index, modulation):
        """ Sets the DL modulation for the indicated base station.

        Args:
            bts_index: the base station number
            modulation: the new DL modulation
        """
        raise NotImplementedError()

    def set_ul_modulation(self, bts_index, modulation):
        """ Sets the UL modulation for the indicated base station.

        Args:
            bts_index: the base station number
            modulation: the new UL modulation
        """
        raise NotImplementedError()

    def set_tbs_pattern_on(self, bts_index, tbs_pattern_on):
        """ Enables or disables TBS pattern in the indicated base station.

        Args:
            bts_index: the base station number
            tbs_pattern_on: the new TBS pattern setting
        """
        raise NotImplementedError()

    def set_cfi(self, bts_index, cfi):
        """ Sets the Channel Format Indicator for the indicated base station.

        Args:
            bts_index: the base station number
            cfi: the new CFI setting
        """
        raise NotImplementedError()

    def set_paging_cycle(self, bts_index, cycle_duration):
        """ Sets the paging cycle duration for the indicated base station.

        Args:
            bts_index: the base station number
            cycle_duration: the new paging cycle duration in milliseconds
        """
        raise NotImplementedError()

    def set_phich_resource(self, bts_index, phich):
        """ Sets the PHICH Resource setting for the indicated base station.

        Args:
            bts_index: the base station number
            phich: the new PHICH resource setting
        """
        raise NotImplementedError()

    def lte_attach_secondary_carriers(self):
        """ Activates the secondary carriers for CA. Requires the DUT to be
        attached to the primary carrier first. """
        raise NotImplementedError()

    def wait_until_attached(self, timeout=120):
        """ Waits until the DUT is attached to the primary carrier.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        raise NotImplementedError()

    def wait_until_communication_state(self, timeout=120):
        """ Waits until the DUT is in Communication state.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        raise NotImplementedError()

    def wait_until_idle_state(self, timeout=120):
        """ Waits until the DUT is in Idle state.

        Args:
            timeout: after this amount of time the method will raise a
                CellularSimulatorError exception. Default is 120 seconds.
        """
        raise NotImplementedError()

    def detach(self):
        """ Turns off all the base stations so the DUT loose connection."""
        raise NotImplementedError()

    def stop(self):
        """ Stops current simulation. After calling this method, the simulator
        will need to be set up again. """
        raise NotImplementedError()

    def start_data_traffic(self):
        """ Starts transmitting data from the instrument to the DUT. """
        raise NotImplementedError()

    def stop_data_traffic(self):
        """ Stops transmitting data from the instrument to the DUT. """
        raise NotImplementedError()


class CellularSimulatorError(Exception):
    """ Exceptions thrown when the cellular equipment is unreachable or it
    returns an error after receiving a command. """
    pass
