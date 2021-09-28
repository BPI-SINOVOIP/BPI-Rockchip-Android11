#
#   Copyright 2017 - The Android Open Source Project
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

import pprint
import random
import time
from acts import context
from scapy.all import *

from acts import asserts
from acts import base_test
from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

WifiEnums = wutils.WifiEnums
DEF_ATTN = 60
MAX_ATTN = 95
ROAM_DBM = -75
WAIT_AFTER_ATTN = 12
ATTN_STEP = 5

class WifiRoamingTest(WifiBaseTest):

    def setup_class(self):
        """Setup required dependencies from config file and configure
           the required networks for testing roaming.

        Returns:
            True if successfully configured the requirements for testing.
        """
        super().setup_class()

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        req_params = ["roaming_attn", "roam_interval", "ping_addr",
                      "max_bugreports"]
        opt_param = ["open_network", "reference_networks",]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(ap_count=2)

        asserts.assert_true(
            len(self.reference_networks) > 1,
            "Need at least two psk networks for roaming.")
        asserts.assert_true(
            len(self.open_network) > 1,
            "Need at least two open networks for roaming")


        self.configure_packet_capture()

    def teardown_class(self):
        self.dut.ed.clear_all_events()
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    def setup_test(self):
        super().setup_test()
        self.dut.ed.clear_all_events()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)
        for a in self.attenuators:
            a.set_atten(0)

    def roaming_from_AP1_and_AP2(self, AP1_network, AP2_network):
        """Test roaming between two APs.

        Args:
            AP1_network: AP-1's network information.
            AP2_network: AP-2's network information.

        Steps:
        1. Make AP1 visible, AP2 not visible.
        2. Connect to AP1's ssid.
        3. Make AP1 not visible, AP2 visible.
        4. Expect DUT to roam to AP2.
        5. Validate connection information and ping.
        """
        wutils.set_attns(self.attenuators, "AP1_on_AP2_off")
        wifi_config = AP1_network.copy()
        wifi_config.pop("bssid")
        wutils.connect_to_wifi_network(self.dut, wifi_config)
        self.log.info("Roaming from %s to %s", AP1_network, AP2_network)
        wutils.trigger_roaming_and_validate(
            self.dut, self.attenuators, "AP1_off_AP2_on", AP2_network)

    def get_rssi(self, pcap_file, expected_bssid):
        """Get signal strength of the wifi network attenuated.

        Args:
            pcap_file: PCAP file path.
            expected_bssid: BSSID of the wifi network attenuated.
        """
        packets = []
        try:
            packets = rdpcap(pcap_file)
        except Scapy_Exception:
            self.log.error("Failed to read pcap file")
        if not packets:
            return 0

        dbm = -100
        for pkt in packets:
            if pkt and hasattr(pkt, 'type') and pkt.type == 0 and \
                pkt.subtype == 8 and hasattr(pkt, 'info'):
                  bssid = pkt.addr3
                  if expected_bssid == bssid:
                      dbm = int(pkt.dBm_AntSignal)
        self.log.info("RSSI: %s" % dbm)
        return dbm

    def trigger_roaming_and_verify_attenuation(self, network):
        """Trigger roaming and verify signal strength is below roaming limit.

        Args:
            network: Wifi network that is being attenuated.
        """
        wutils.set_attns_steps(self.attenuators, "AP1_off_AP2_on")
        band = '5G' if network['SSID'].startswith('5g_') else '2G'
        attn = DEF_ATTN + ATTN_STEP
        while attn <= MAX_ATTN:
            self.pcap_procs = wutils.start_pcap(
                self.packet_capture, 'dual', self.test_name)
            time.sleep(WAIT_AFTER_ATTN/3)
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
            pcap_file = os.path.join(
                context.get_current_context().get_full_output_path(),
                'PacketCapture',
                '%s_%s.pcap' % (self.test_name, band))

            rssi = self.get_rssi(pcap_file, network["bssid"])
            if rssi == 0:
                self.log.error("Failed to verify signal strength")
                break
            if self.get_rssi(pcap_file, network["bssid"]) < ROAM_DBM:
                break

            self.attenuators[0].set_atten(attn)
            self.attenuators[1].set_atten(attn)
            time.sleep(WAIT_AFTER_ATTN) # allow some time for attenuation
            attn += 5

    def validate_roaming(self, expected_con):
        """Validate roaming.

        Args:
            expected_con: Expected wifi network after roaming.
        """
        expected_con = {
            WifiEnums.SSID_KEY: expected_con[WifiEnums.SSID_KEY],
            WifiEnums.BSSID_KEY: expected_con["bssid"],
        }
        curr_con = self.dut.droid.wifiGetConnectionInfo()
        for key in expected_con:
            if expected_con[key] != curr_con[key]:
                asserts.fail("Expected '%s' to be %s, actual is %s." %
                             (key, expected_con[key], curr_con[key]))
        self.log.info("Roamed to %s successfully",
                      expected_con[WifiEnums.BSSID_KEY])
        if not wutils.validate_connection(self.dut):
            raise signals.TestFailure("Fail to connect to internet on %s" %
                                      expected_con[WifiEnums.BSSID_KEY])

    """ Tests Begin.

        The following tests are designed to test inter-SSID Roaming only.

        """
    @test_tracker_info(uuid="db8a46f9-713f-4b98-8d9f-d36319905b0a")
    def test_roaming_between_AP1_to_AP2_open_2g(self):
        AP1_network = self.open_network[0]["2g"]
        AP2_network = self.open_network[1]["2g"]
        self.roaming_from_AP1_and_AP2(AP1_network, AP2_network)

    @test_tracker_info(uuid="0db67d9b-6ea9-4f40-acf2-155c4ecf9dc5")
    def test_roaming_between_AP1_to_AP2_open_5g(self):
        AP1_network = self.open_network[0]["5g"]
        AP2_network = self.open_network[1]["5g"]
        self.roaming_from_AP1_and_AP2(AP1_network, AP2_network)

    @test_tracker_info(uuid="eabc7319-d962-4bef-b679-725e9ff00420")
    def test_roaming_between_AP1_to_AP2_psk_2g(self):
        AP1_network = self.reference_networks[0]["2g"]
        AP2_network = self.reference_networks[1]["2g"]
        self.roaming_from_AP1_and_AP2(AP1_network, AP2_network)

    @test_tracker_info(uuid="1cf9c681-4ff0-45c1-9719-f01629f6a7f7")
    def test_roaming_between_AP1_to_AP2_psk_5g(self):
        AP1_network = self.reference_networks[0]["5g"]
        AP2_network = self.reference_networks[1]["5g"]
        self.roaming_from_AP1_and_AP2(AP1_network, AP2_network)

    @test_tracker_info(uuid="3114d625-5cdd-4205-bb46-5a9d057dc80d")
    def test_roaming_fail_psk_2g(self):
        network = {'SSID':'test_roaming_fail', 'password':'roam123456@'}
        # AP2 network with incorrect password.
        network_fail = {'SSID':'test_roaming_fail', 'password':'roam123456@#$%^'}
        # Setup AP1 with the correct password.
        wutils.ap_setup(self, 0, self.access_points[0], network)
        network_bssid = self.access_points[0].get_bssid_from_ssid(
                network["SSID"], '2g')
        # Setup AP2 with the incorrect password.
        wutils.ap_setup(self, 1, self.access_points[1], network_fail)
        network_fail_bssid = self.access_points[1].get_bssid_from_ssid(
                network_fail["SSID"], '2g')
        network['bssid'] = network_bssid
        network_fail['bssid'] = network_fail_bssid
        try:
            # Initiate roaming with AP2 configured with incorrect password.
            self.roaming_from_AP1_and_AP2(network, network_fail)
        except:
            self.log.info("Roaming failed to AP2 with incorrect password.")
            # Re-configure AP2 after roaming failed, with correct password.
            self.log.info("Re-configuring AP2 with correct password.")
            wutils.ap_setup(self, 1, self.access_points[1], network)
        self.roaming_from_AP1_and_AP2(network, network_fail)

    """ Tests End """
