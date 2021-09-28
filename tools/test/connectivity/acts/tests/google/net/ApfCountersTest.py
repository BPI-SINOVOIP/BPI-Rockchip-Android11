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
from acts.test_decorators import test_tracker_info
from acts.test_utils.net.net_test_utils import start_tcpdump
from acts.test_utils.net.net_test_utils import stop_tcpdump
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.test_utils.wifi import wifi_test_utils as wutils

import acts.base_test
import acts.test_utils.wifi.wifi_test_utils as wutils

import copy
import os
import random
import time

WifiEnums = wutils.WifiEnums

RA_SCRIPT = 'sendra.py'
SCAPY = 'scapy-2.2.0.tar.gz'
SCAPY_INSTALL_COMMAND = 'sudo python setup.py install'
PROC_NET_SNMP6 = '/proc/net/snmp6'
LIFETIME_FRACTION = 6
LIFETIME = 180
INTERVAL = 2


class ApfCountersTest(WifiBaseTest):

    def __init__(self, controllers):
        WifiBaseTest.__init__(self, controllers)
        self.tests = ("test_IPv6_RA_packets",
                      "test_IPv6_RA_with_RTT", )

    def setup_class(self):
        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        req_params = []
        opt_param = ["reference_networks", ]

        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start()

        asserts.assert_true(
            len(self.reference_networks) > 0,
            "Need at least one reference network with psk.")
        wutils.wifi_toggle_state(self.dut, True)

        self.wpapsk_2g = self.reference_networks[0]["2g"]
        self.wpapsk_5g = self.reference_networks[0]["5g"]

        # install scapy
        current_dir = os.path.dirname(os.path.realpath(__file__))
        send_ra = os.path.join(current_dir, RA_SCRIPT)
        send_scapy = os.path.join(current_dir, SCAPY)
        self.access_points[0].install_scapy(send_scapy, send_ra)
        self.tcpdump_pid = None

    def setup_test(self):
        self.tcpdump_pid = start_tcpdump(self.dut, self.test_name)

    def teardown_test(self):
        stop_tcpdump(self.dut, self.tcpdump_pid, self.test_name)

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)
        self.dut.cat_adb_log(test_name, begin_time)

    def teardown_class(self):
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
        self.access_points[0].cleanup_scapy()
        wutils.reset_wifi(self.dut)

    """ Helper methods """

    def _get_icmp6intype134(self):
        """ Get ICMP6 Type 134 packet count on the DUT

        Returns:
            Number of ICMP6 type 134 packets
        """
        cmd = "grep Icmp6InType134 %s || true" % PROC_NET_SNMP6
        ra_count = self.dut.adb.shell(cmd)
        if not ra_count:
            return 0
        ra_count = int(ra_count.split()[-1].rstrip())
        self.dut.log.info("RA Count %s:" % ra_count)
        return ra_count

    """ Tests """

    @test_tracker_info(uuid="bc8d3f27-582a-464a-be30-556f07b77ee1")
    def test_IPv6_RA_packets(self):
        """ Test if the device filters the IPv6 packets

        Steps:
          1. Send a RA packet to DUT. DUT should accept this
          2. Send duplicate RA packets. The RA packets should be filtered
             for the next 30 seconds (1/6th of RA lifetime)
          3. The next RA packets should be accepted
        """
        # get mac address of the dut
        ap = self.access_points[0]
        wifi_network = copy.deepcopy(self.wpapsk_5g)
        wifi_network['meteredOverride'] = 1
        wutils.connect_to_wifi_network(self.dut, wifi_network)
        mac_addr = self.dut.droid.wifiGetConnectionInfo()['mac_address']
        self.log.info("mac_addr %s" % mac_addr)
        time.sleep(30) # wait 30 sec before sending RAs

        # get the current ra count
        ra_count = self._get_icmp6intype134()

        # Start scapy to send RA to the phone's MAC
        ap.send_ra('wlan1', mac_addr, 0, 1)

        # get the latest ra count
        ra_count_latest = self._get_icmp6intype134()
        asserts.assert_true(ra_count_latest == ra_count + 1,
                            "Device dropped the first RA in sequence")

        # Generate and send 'x' number of duplicate RAs, for 1/6th of the the
        # lifetime of the original RA. Test assumes that the original RA has a
        # lifetime of 180s. Hence, all RAs received within the next 30s of the
        # original RA should be filtered.
        ra_count = ra_count_latest
        count = LIFETIME / LIFETIME_FRACTION / INTERVAL
        ap.send_ra('wlan1', mac_addr, interval=INTERVAL, count=count)

        # Fail test if at least 90% of RAs were not dropped.
        ra_count_latest = self._get_icmp6intype134()
        pkt_loss = count - (ra_count_latest - ra_count)
        percentage_loss = float(pkt_loss) / count * 100
        asserts.assert_true(percentage_loss >= 90, "Device did not filter "
                            "duplicate RAs correctly. %d Percent of duplicate"
                            " RAs were accepted" % (100 - percentage_loss))

        # Any new RA after this should be accepted.
        ap.send_ra('wlan1', mac_addr, interval=INTERVAL, count=1)
        ra_count_latest = self._get_icmp6intype134()
        asserts.assert_true(ra_count_latest == ra_count + 1,
                            "Device did not accept new RA after 1/6th time "
                            "interval. Device dropped a valid RA in sequence.")

    @test_tracker_info(uuid="d2a0aff0-048c-475f-9bba-d90d8d9ebae3")
    def test_IPv6_RA_with_RTT(self):
        """Test if the device filters IPv6 RA packets with different re-trans time

        Steps:
          1. Get the current RA count
          2. Send 400 packets with different re-trans time
          3. Verify that RA count increased by 400
          4. Verify internet connectivity
        """
        pkt_num = 400
        rtt_list = random.sample(range(10, 10000), pkt_num)

        # get mac address of the dut
        ap = self.access_points[0]
        wutils.connect_to_wifi_network(self.dut, self.wpapsk_5g)
        mac_addr = self.dut.droid.wifiGetConnectionInfo()['mac_address']
        self.log.info("mac_addr %s" % mac_addr)
        time.sleep(30) # wait 30 sec before sending RAs

        # get the current ra count
        ra_count = self._get_icmp6intype134()

        # send RA with differnt re-trans time
        for rtt in rtt_list:
            ap.send_ra('wlan1', mac_addr, 0, 1, rtt=rtt)

        # get the new RA count
        new_ra_count = self._get_icmp6intype134()
        asserts.assert_true(new_ra_count == ra_count + pkt_num,
                            "Device did not accept all RAs")

        # verify if internet connectivity works after sending RA packets
        wutils.validate_connection(self.dut)
