#!/usr/bin/env python3.4
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

import acts.test_utils.power.PowerBaseTest as PBT
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi import wifi_power_test_utils as wputils

IPERF_DURATION = 'iperf_duration'
INITIAL_ATTEN = [0, 0, 90, 90]


class PowerWiFiBaseTest(PBT.PowerBaseTest):
    """Base class for WiFi power related tests.

    Inherited from the PowerBaseTest class
    """

    def setup_class(self):

        super().setup_class()
        if hasattr(self, 'access_points'):
            self.access_point = self.access_points[0]
            self.access_point_main = self.access_points[0]
            if len(self.access_points) > 1:
                self.access_point_aux = self.access_points[1]
        if hasattr(self, 'attenuators'):
            self.set_attenuation(INITIAL_ATTEN)
        if hasattr(self, 'network_file'):
            self.networks = self.unpack_custom_file(self.network_file, False)
            self.main_network = self.networks['main_network']
            self.aux_network = self.networks['aux_network']
        if hasattr(self, 'packet_senders'):
            self.pkt_sender = self.packet_senders[0]
        if hasattr(self, 'iperf_servers'):
            self.iperf_server = self.iperf_servers[0]
        if self.iperf_duration:
            self.mon_duration = self.iperf_duration - 10
            self.create_monsoon_info()

    def teardown_test(self):
        """Tear down necessary objects after test case is finished.

        Bring down the AP interface, delete the bridge interface, stop the
        packet sender, and reset the ethernet interface for the packet sender
        """
        super().teardown_test()
        if hasattr(self, 'pkt_sender'):
            self.pkt_sender.stop_sending(ignore_status=True)
        if hasattr(self, 'brconfigs'):
            self.access_point.bridge.teardown(self.brconfigs)
            delattr(self, 'brconfigs')
        if hasattr(self, 'brconfigs_main'):
            self.access_point_main.bridge.teardown(self.brconfigs_main)
            delattr(self, 'brconfigs_main')
        if hasattr(self, 'brconfigs_aux'):
            self.access_point_aux.bridge.teardown(self.brconfigs_aux)
            delattr(self, 'brconfigs_aux')
        if hasattr(self, 'access_points'):
            for ap in self.access_points:
                ap.close()
        if hasattr(self, 'pkt_sender'):
            wputils.reset_host_interface(self.pkt_sender.interface)
        if hasattr(self, 'iperf_server'):
            self.iperf_server.stop()

    def teardown_class(self):
        """Clean up the test class after tests finish running

        """
        super().teardown_class()
        if hasattr(self, 'access_points'):
            for ap in self.access_points:
                ap.close()

    def setup_ap_connection(self, network, bandwidth=80, connect=True,
                            ap=None):
        """Setup AP and connect DUT to it.

        Args:
            network: the network config for the AP to be setup
            bandwidth: bandwidth of the WiFi network to be setup
            connect: indicator of if connect dut to the network after setup
            ap: access point object, default is None to find the main AP
        Returns:
            self.brconfigs: dict for bridge interface configs
        """
        wutils.wifi_toggle_state(self.dut, True)
        if not ap:
            if hasattr(self, 'access_points'):
                self.brconfigs = wputils.ap_setup(
                    self.access_point, network, bandwidth=bandwidth)
        else:
            self.brconfigs = wputils.ap_setup(ap, network, bandwidth=bandwidth)
        if connect:
            wutils.wifi_connect(self.dut, network, num_of_tries=3)

        if ap or (not ap and hasattr(self, 'access_points')):
            return self.brconfigs

    def collect_power_data(self):
        """Measure power, plot and check pass/fail.

        If IPERF is run, need to pull iperf results and attach it to the plot.
        """
        result = super().collect_power_data()
        tag = ''
        if self.iperf_duration:
            throughput = self.process_iperf_results()
            tag = '_RSSI_{0:d}dBm_Throughput_{1:.2f}Mbps'.format(
                self.RSSI, throughput)
            wputils.monsoon_data_plot(self.mon_info, result, tag=tag)
        return result
