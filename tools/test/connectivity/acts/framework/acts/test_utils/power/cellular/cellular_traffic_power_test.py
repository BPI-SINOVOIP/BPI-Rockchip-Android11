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

import scapy.all as scapy

from acts import asserts
from acts.metrics.loggers.blackbox import BlackboxMetricLogger
from acts.test_utils.power import IperfHelper as IPH
import acts.test_utils.power.cellular.cellular_power_base_test as PWCEL
from acts.test_utils.wifi import wifi_power_test_utils as wputils


class PowerTelTrafficTest(PWCEL.PowerCellularLabBaseTest):
    """ Cellular traffic power test.

    Inherits from PowerCellularLabBaseTest. Parses config specific
    to this kind of test. Contains methods to start data traffic
    between a local instance of iPerf and one running in the dut.

    """

    # Keywords for test name parameters
    PARAM_DIRECTION = 'direction'
    PARAM_DIRECTION_UL = 'ul'
    PARAM_DIRECTION_DL = 'dl'
    PARAM_DIRECTION_DL_UL = 'dlul'
    PARAM_BANDWIDTH_LIMIT = 'blimit'

    # Iperf waiting time
    IPERF_MARGIN = 10

    # Constant used to calculate the tcp window size from total throughput
    TCP_WINDOW_FRACTION = 40

    def __init__(self, controllers):
        """ Class initialization.

        Sets test parameters to initial values.
        """

        super().__init__(controllers)

        # These variables are passed to iPerf when starting data
        # traffic with the -b parameter to limit throughput on
        # the application layer.
        self.bandwidth_limit_dl = None
        self.bandwidth_limit_ul = None

        # Throughput obtained from iPerf
        self.iperf_results = {}

        # Blackbox metrics loggers

        self.dl_tput_logger = BlackboxMetricLogger.for_test_case(
            metric_name='avg_dl_tput')
        self.ul_tput_logger = BlackboxMetricLogger.for_test_case(
            metric_name='avg_ul_tput')

    def setup_class(self):
        super().setup_class()

        # Verify that at least one PacketSender controller has been initialized
        if not hasattr(self, 'packet_senders'):
            raise RuntimeError('At least one packet sender controller needs '
                               'to be defined in the test config files.')

    def setup_test(self):
        """ Executed before every test case.

        Parses test configuration from the test name and prepares
        the simulation for measurement.
        """

        # Reset results at the start of the test
        self.iperf_results = {}

        # Call parent method first to setup simulation
        if not super().setup_test():
            return False

        # Traffic direction

        values = self.consume_parameter(self.PARAM_DIRECTION, 1)

        if not values:
            self.log.warning("The keyword {} was not included in the testname "
                             "parameters. Setting to {} by default.".format(
                                 self.PARAM_DIRECTION,
                                 self.PARAM_DIRECTION_DL_UL))
            self.traffic_direction = self.PARAM_DIRECTION_DL_UL
        elif values[1] in [
                self.PARAM_DIRECTION_DL, self.PARAM_DIRECTION_UL,
                self.PARAM_DIRECTION_DL_UL
        ]:
            self.traffic_direction = values[1]
        else:
            self.log.error("The test name has to include parameter {} "
                           "followed by {}/{}/{}.".format(
                               self.PARAM_DIRECTION, self.PARAM_DIRECTION_UL,
                               self.PARAM_DIRECTION_DL,
                               self.PARAM_DIRECTION_DL_UL))
            return False

        # Bandwidth limit

        values = self.consume_parameter(self.PARAM_BANDWIDTH_LIMIT, 2)

        if values:
            self.bandwidth_limit_dl = values[1]
            self.bandwidth_limit_ul = values[2]
        else:
            self.bandwidth_limit_dl = 0
            self.bandwidth_limit_ul = 0
            self.log.error(
                "No bandwidth limit was indicated in the test parameters. "
                "Setting to default value of 0 (no limit to bandwidth). To set "
                "a different value include parameter '{}' followed by two "
                "strings indicating downlink and uplink bandwidth limits for "
                "iPerf.".format(self.PARAM_BANDWIDTH_LIMIT))

        # No errors when parsing parameters
        return True

    def teardown_test(self):
        """Tear down necessary objects after test case is finished.

        """

        super().teardown_test()

        # Log the throughput values to Blackbox
        self.dl_tput_logger.metric_value = self.iperf_results.get('DL', 0)
        self.ul_tput_logger.metric_value = self.iperf_results.get('UL', 0)

        # Log the throughput values to Spanner
        self.power_logger.set_dl_tput(self.iperf_results.get('DL', 0))
        self.power_logger.set_ul_tput(self.iperf_results.get('UL', 0))

        for ips in self.iperf_servers:
            ips.stop()

    def power_tel_traffic_test(self):
        """ Measures power and throughput during data transmission.

        Measurement step in this test. Starts iPerf client in the DUT and then
        initiates power measurement. After that, DUT is connected again and
        the result from iPerf is collected. Pass or fail is decided with a
        threshold value.
        """

        # Start data traffic
        iperf_helpers = self.start_tel_traffic(self.dut)

        # Measure power
        result = self.collect_power_data()

        # Wait for iPerf to finish
        time.sleep(self.IPERF_MARGIN + 2)

        # Collect throughput measurement
        self.iperf_results = self.get_iperf_results(self.dut, iperf_helpers)

        # Check if power measurement is below the required value
        self.pass_fail_check(result.average_current)

        return result.average_current, self.iperf_results

    def get_iperf_results(self, device, iperf_helpers):
        """ Pulls iperf results from the device.

        Args:
            device: the device from which iperf results need to be pulled.

        Returns:
            a dictionary containing DL/UL throughput in Mbit/s.
        """

        throughput = {}

        for iph in iperf_helpers:

            self.log.info("Getting {} throughput results.".format(
                iph.traffic_direction))

            iperf_result = iph.process_iperf_results(device, self.log,
                                                     self.iperf_servers,
                                                     self.test_name)

            throughput[iph.traffic_direction] = iperf_result

        return throughput

    def pass_fail_check(self, average_current=None):
        """ Checks power consumption and throughput.

        Uses the base class method to check power consumption. Also, compares
        the obtained throughput with the expected value provided by the
        simulation class.

        """

        for direction, throughput in self.iperf_results.items():
            try:
                if direction == "UL":
                    expected_t = self.simulation.maximum_uplink_throughput()
                elif direction == "DL":
                    expected_t = self.simulation.maximum_downlink_throughput()
                else:
                    raise RuntimeError("Unexpected traffic direction value.")
            except NotImplementedError:
                # Some simulation classes might not have implemented the max
                # throughput calculation yet.
                self.log.debug("Expected throughput is not available for the "
                               "current simulation class.")
            else:

                self.log.info(
                    "The expected {} throughput is {} Mbit/s.".format(
                        direction, expected_t))
                asserts.assert_true(
                    0.90 < throughput / expected_t < 1.10,
                    "{} throughput differed more than 10% from the expected "
                    "value! ({}/{} = {})".format(
                        direction, round(throughput, 3), round(expected_t, 3),
                        round(throughput / expected_t, 3)))

        super().pass_fail_check(average_current)

    def start_tel_traffic(self, client_host):
        """ Starts iPerf in the indicated device and initiates traffic.

        Starts the required iperf clients and servers according to the traffic
        pattern config in the current test.

        Args:
            client_host: device handler in which to start the iperf client.

        Returns:
            A list of iperf helpers.
        """
        # The iPerf server is hosted in this computer
        self.iperf_server_address = scapy.get_if_addr(
            self.packet_senders[0].interface)

        # Start iPerf traffic
        iperf_helpers = []

        # Calculate TCP windows as a fraction of the expected throughput
        # Some simulation classes don't implement this method yed
        try:
            dl_max_throughput = self.simulation.maximum_downlink_throughput()
            ul_max_throughput = self.simulation.maximum_uplink_throughput()
        except NotImplementedError:
            self.log.error("Maximum downlink/uplink throughput method not "
                           "implemented for %s." %
                           type(self.simulation).__name__)
            ul_tcp_window = None
            dl_tcp_window = None
        else:
            # Calculate the TCP window only if dl/ul max throughput was
            # obtained. Use tcp_window_fraction if given in parameters. If
            # tcp_window_fraction is false then send None.
            if hasattr(self, 'tcp_window_fraction'):
                if not self.tcp_window_fraction:
                    ul_tcp_window = None
                    dl_tcp_window = None
                elif self.tcp_window_fraction > 0.0:
                    dl_tcp_window = dl_max_throughput / self.tcp_window_fraction
                    ul_tcp_window = ul_max_throughput / self.tcp_window_fraction
                else:
                    self.log.warning("tcp_window_fraction should be positive "
                                     "int or 'false'. Disabling window")
                    ul_tcp_window = None
                    dl_tcp_window = None
            else:
                dl_tcp_window = dl_max_throughput / self.TCP_WINDOW_FRACTION
                ul_tcp_window = ul_max_throughput / self.TCP_WINDOW_FRACTION

        if self.traffic_direction in [
                self.PARAM_DIRECTION_DL, self.PARAM_DIRECTION_DL_UL
        ]:
            # Downlink traffic
            iperf_helpers.append(
                self.start_iperf_traffic(client_host,
                                         server_idx=len(iperf_helpers),
                                         traffic_direction='DL',
                                         window=dl_tcp_window,
                                         bandwidth=self.bandwidth_limit_dl))

        if self.traffic_direction in [
                self.PARAM_DIRECTION_UL, self.PARAM_DIRECTION_DL_UL
        ]:
            # Uplink traffic
            iperf_helpers.append(
                self.start_iperf_traffic(client_host,
                                         server_idx=len(iperf_helpers),
                                         traffic_direction='UL',
                                         window=ul_tcp_window,
                                         bandwidth=self.bandwidth_limit_ul))

        return iperf_helpers

    def start_iperf_traffic(self,
                            client_host,
                            server_idx,
                            traffic_direction,
                            bandwidth=0,
                            window=None):
        """Starts iPerf data traffic.

        Starts an iperf client in an android device and a server locally.

        Args:
            client_host: device handler in which to start the iperf client
            server_idx: id of the iperf server to connect to
            traffic_direction: has to be either 'UL' or 'DL'
            bandwidth: bandwidth limit for data traffic
            window: the tcp window. if None, no window will be passed to iperf

        Returns:
            An IperfHelper object for the started client/server pair.
        """

        config = {
            'traffic_type': 'TCP',
            'duration':
            self.mon_duration + self.mon_offset + self.IPERF_MARGIN,
            'start_meas_time': 4,
            'server_idx': server_idx,
            'port': self.iperf_servers[server_idx].port,
            'traffic_direction': traffic_direction,
            'window': window
        }

        # If bandwidth is equal to zero then no bandwith requirements are set
        if bandwidth > 0:
            config['bandwidth'] = bandwidth

        iph = IPH.IperfHelper(config)

        # Start the server locally
        self.iperf_servers[server_idx].start()

        # Start the client in the android device
        wputils.run_iperf_client_nonblocking(client_host,
                                             self.iperf_server_address,
                                             iph.iperf_args)

        return iph


class PowerTelRvRTest(PowerTelTrafficTest):
    """ Gets Range vs Rate curves while measuring power consumption.

    Uses PowerTelTrafficTest as a base class.
    """

    # Test name configuration keywords
    PARAM_SWEEP = "sweep"
    PARAM_SWEEP_UPLINK = "uplink"
    PARAM_SWEEP_DOWNLINK = "downlink"

    # Sweep values. Need to be set before starting test by test
    # function or child class.
    downlink_power_sweep = None
    uplink_power_sweep = None

    def setup_test(self):
        """ Executed before every test case.

        Parses test configuration from the test name and prepares
        the simulation for measurement.
        """

        # Call parent method first to setup simulation
        if not super().setup_test():
            return False

        # Get which power value to sweep from config

        try:
            values = self.consume_parameter(self.PARAM_SWEEP, 1)

            if values[1] == self.PARAM_SWEEP_UPLINK:
                self.sweep = self.PARAM_SWEEP_UPLINK
            elif values[1] == self.PARAM_SWEEP_DOWNLINK:
                self.sweep = self.PARAM_SWEEP_DOWNLINK
            else:
                raise ValueError()
        except:
            self.log.error(
                "The test name has to include parameter {} followed by "
                "either {} or {}.".format(self.PARAM_SWEEP,
                                          self.PARAM_SWEEP_DOWNLINK,
                                          self.PARAM_SWEEP_UPLINK))
            return False

        return True

    def power_tel_rvr_test(self):
        """ Main function for the RvR test.

        Produces the RvR curve according to the indicated sweep values.
        """

        if self.sweep == self.PARAM_SWEEP_DOWNLINK:
            sweep_range = self.downlink_power_sweep
        elif self.sweep == self.PARAM_SWEEP_UPLINK:
            sweep_range = self.uplink_power_sweep

        current = []
        throughput = []

        for pw in sweep_range:

            if self.sweep == self.PARAM_SWEEP_DOWNLINK:
                self.simulation.set_downlink_rx_power(self.simulation.bts1, pw)
            elif self.sweep == self.PARAM_SWEEP_UPLINK:
                self.simulation.set_uplink_tx_power(self.simulation.bts1, pw)

            i, t = self.power_tel_traffic_test()
            self.log.info("---------------------")
            self.log.info("{} -- {} --".format(self.sweep, pw))
            self.log.info("{} ----- {}".format(i, t[0]))
            self.log.info("---------------------")

            current.append(i)
            throughput.append(t[0])

        print(sweep_range)
        print(current)
        print(throughput)
