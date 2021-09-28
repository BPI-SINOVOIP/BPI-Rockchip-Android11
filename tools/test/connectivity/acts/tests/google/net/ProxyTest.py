#
#   Copyright 2018 - The Android Open Source Project
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

from acts import asserts
from acts import base_test
from acts.test_decorators import test_tracker_info
from acts.test_utils.net.net_test_utils import start_tcpdump
from acts.test_utils.net.net_test_utils import stop_tcpdump
from acts.test_utils.wifi import wifi_test_utils as wutils


from scapy.all import IP
from scapy.all import TCP
from scapy.all import UDP
from scapy.all import Raw
from scapy.all import rdpcap
from scapy.all import Scapy_Exception


class ProxyTest(base_test.BaseTestClass):
    """ Network proxy tests """

    def setup_class(self):
        """ Setup devices for tests and unpack params """
        self.dut = self.android_devices[0]
        req_params = ("proxy_pac", "proxy_server",
                      "proxy_port", "bypass_host", "non_bypass_host",
                      "wifi_network")
        self.unpack_userparams(req_param_names=req_params,)
        wutils.wifi_test_device_init(self.dut)
        wutils.wifi_toggle_state(self.dut, True)
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, self.wifi_network["SSID"])
        wutils.wifi_connect(self.dut, self.wifi_network)
        self.tcpdump_pid = None
        self.proxy_port = int(self.proxy_port)

    def teardown_test(self):
        self.dut.droid.connectivityResetGlobalProxy()
        global_proxy = self.dut.droid.connectivityGetGlobalProxy()
        if global_proxy:
            self.log.error("Failed to reset global proxy settings")

    def teardown_class(self):
        wutils.reset_wifi(self.dut)

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)

    """ Helper methods """

    def _verify_http_request(self, ad):
        """ Send http requests to hosts

        Steps:
            1. Send http requests to hosts
                a. Host that is bypassed by proxy server
                b. Host that goes through proxy server
            2. Verify that both return valid responses

        Args:
            1. ad: dut to run http requests
        """
        for host in [self.bypass_host, self.non_bypass_host]:
            host = "https://%s" % host
            result = ad.droid.httpRequestString(host)
            asserts.assert_true(result, "Http request failed for %s" % host)

    def _verify_proxy_server(self, pcap_file, bypass_host, hostname):
        """ Verify that http requests are going through proxy server

        Args:
            1. tcpdump: pcap file
            2. bypass_host: boolean value if the request goes through proxy
            3. hostname: hostname requested

        Returns:
            True/False if the bypass condition met
        """
        self.log.info("Checking proxy server for query to: %s" % hostname)
        try:
            packets = rdpcap(pcap_file)
        except Scapy_Exception:
            asserts.fail("Not a valid pcap file")

        dns_query = False
        http_query = False
        for pkt in packets:
            summary = "%s" % pkt.summary()
            if UDP in pkt and pkt[UDP].dport == 53 and hostname in summary:
                dns_query = True
                break
            if TCP in pkt and pkt[TCP].dport == self.proxy_port and Raw in pkt\
                and hostname in str(pkt[Raw]):
                  http_query = True

        self.log.info("Bypass hostname set to: %s" % bypass_host)
        self.log.info("Found DNS query for host: %s" % dns_query)
        self.log.info("Found HTTP query for host: %s" % http_query)
        if bypass_host and http_query and not dns_query or \
            not bypass_host and not http_query and dns_query:
              return False
        return True

    def _test_proxy(self):
        """ Test pac piroxy and manual proxy settings

        Steps:
            1. Start tcpdump
            2. Run http requests
            3. Stop tcpdump
            4. Verify the packets from tcpdump have valid queries
        """

        # start tcpdump on the device
        self.tcpdump_pid = start_tcpdump(self.dut, self.test_name)

        # verify http requests
        self._verify_http_request(self.dut)

        # stop tcpdump on the device
        pcap_file = stop_tcpdump(self.dut, self.tcpdump_pid, self.test_name)

        # verify proxy server
        result = self._verify_proxy_server(pcap_file, True, self.bypass_host)
        asserts.assert_true(result, "Proxy failed for %s" % self.bypass_host)
        result = self._verify_proxy_server(pcap_file, False, self.non_bypass_host)
        asserts.assert_true(result, "Proxy failed for %s" % self.non_bypass_host)

    """ Test Cases """

    @test_tracker_info(uuid="16881315-1a50-48ce-bd36-7b0d2f21b734")
    def test_pac_proxy_over_wifi(self):
        """ Test proxy with auto config over wifi

        Steps:
            1. Connect to a wifi network
            2. Set a global proxy with auto config
            3. Do a http request on the hostnames
            4. Verify that no DNS packets seen for non bypassed hostnames
            5. Verify that DNS packets seen for bypassed hostnames
        """
        # set global pac proxy
        self.log.info("Setting global proxy to: %s" % self.proxy_pac)
        self.dut.droid.connectivitySetGlobalPacProxy(self.proxy_pac)
        global_proxy = self.dut.droid.connectivityGetGlobalProxy()
        asserts.assert_true(global_proxy['PacUrl'] == self.proxy_pac,
                            "Failed to set pac proxy")

        # test proxy
        self._test_proxy()

    @test_tracker_info(uuid="4d3361f6-866d-423c-9ed7-5a6943575fe9")
    def test_manual_proxy_over_wifi(self):
        """ Test manual proxy over wifi

        Steps:
            1. Connect to a wifi network
            2. Set a global manual proxy with proxy server, port & bypass URLs
            3. Do a http request on the hostnames
            4. Verify that no DNS packets are seen for non bypassed hostnames
            5. Verify that DNS packets seen for bypassed hostnames
        """
        # set global manual proxy
        self.log.info("Setting global proxy to: %s %s %s" %
                      (self.proxy_server, self.proxy_port, self.bypass_host))
        self.dut.droid.connectivitySetGlobalProxy(self.proxy_server,
                                                  self.proxy_port,
                                                  self.bypass_host)
        global_proxy = self.dut.droid.connectivityGetGlobalProxy()
        asserts.assert_true(global_proxy['Hostname'] == self.proxy_server,
                            "Failed to set manual proxy")

        # test proxy
        self._test_proxy()
