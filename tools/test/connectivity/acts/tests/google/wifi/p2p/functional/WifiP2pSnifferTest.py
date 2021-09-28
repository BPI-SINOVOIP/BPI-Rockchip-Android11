#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.utils
import time
import re

from acts import asserts
from acts import utils

from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.p2p.WifiP2pBaseTest import WifiP2pBaseTest
from acts.test_utils.wifi.p2p import wifi_p2p_test_utils as wp2putils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts
from acts.controllers.ap_lib.hostapd_constants import BAND_2G
from scapy.all import *

WPS_PBC = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
WPS_DISPLAY = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_DISPLAY
WPS_KEYPAD = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_KEYPAD
DEFAULT_TIMEOUT = 10


class WifiP2pSnifferTest(WifiP2pBaseTest):
    """Tests factory MAC is not leaked for p2p discovery and associated cases.

    Test Bed Requirement:
    * At least two Android devices
    * An access point as sniffer
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)

    def setup_class(self):
        super(WifiP2pSnifferTest, self).setup_class()
        wp2putils.wifi_p2p_set_channels_for_current_group(self.dut1, 6, 6)
        wp2putils.wifi_p2p_set_channels_for_current_group(self.dut2, 6, 6)
        self.configure_packet_capture()

    def setup_test(self):
        super(WifiP2pSnifferTest, self).setup_test()
        self.pcap_procs = wutils.start_pcap(self.packet_capture, '2g',
                                            self.test_name)

    def teardown_test(self):
        if self.pcap_procs:
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        super(WifiP2pSnifferTest, self).teardown_test()

    def configure_packet_capture(self):
        """Configure packet capture on the social channels."""
        self.packet_capture = self.packet_capture[0]
        result = self.packet_capture.configure_monitor_mode(BAND_2G, 6)
        if not result:
            raise ValueError("Failed to configure channel for 2G band")

    def verify_mac_no_leakage(self):
        time.sleep(DEFAULT_TIMEOUT)
        self.log.info("Stopping packet capture")
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        # Verify factory MAC is not leaked in 2G pcaps
        pcap_fname = '%s_%s.pcap' % (self.pcap_procs[BAND_2G][1],
                                     BAND_2G.upper())
        self.pcap_procs = None
        packets = rdpcap(pcap_fname)
        wutils.verify_mac_not_found_in_pcap(self.dut1, self.dut1_mac, packets)
        wutils.verify_mac_not_found_in_pcap(self.dut2, self.dut2_mac, packets)

    """Test Cases"""
    @test_tracker_info(uuid="d04e62dc-e1ef-4cea-86e6-39f0dd08fb6b")
    def test_p2p_discovery_sniffer(self):
        """Verify the p2p discovery functionality
        Steps:
        1. Discover the target device
        2. Check the target device in peer list
        """
        self.log.info("Device discovery")
        wp2putils.find_p2p_device(self.dut1, self.dut2)
        wp2putils.find_p2p_device(self.dut2, self.dut1)
        self.verify_mac_no_leakage()

    @test_tracker_info(uuid="6a02be84-912d-4b5b-8dfa-fd80d2554c55")
    def test_p2p_connect_via_pbc_and_ping_and_reconnect_sniffer(self):
        """Verify the p2p connect via pbc functionality

        Steps:
        1. Request the connection which include discover the target device
        2. check which dut is GO and which dut is GC
        3. connection check via ping from GC to GO
        4. disconnect
        5. Trigger connect again from GO for reconnect test.
        6. GO trigger disconnect
        7. Trigger connect again from GC for reconnect test.
        8. GC trigger disconnect
        """
        # Request the connection
        wp2putils.p2p_connect(self.dut1, self.dut2, False, WPS_PBC)

        if wp2putils.is_go(self.dut1):
            go_dut = self.dut1
            gc_dut = self.dut2
        elif wp2putils.is_go(self.dut2):
            go_dut = self.dut2
            gc_dut = self.dut1

        go_ip = wp2putils.p2p_go_ip(gc_dut)
        wp2putils.p2p_connection_ping_test(gc_dut, go_ip)

        # trigger disconnect
        wp2putils.p2p_disconnect(self.dut1)
        wp2putils.check_disconnect(self.dut2)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        self.log.info("Reconnect test, triggered by GO")
        # trigger reconnect from GO
        go_dut.ed.clear_all_events()
        gc_dut.ed.clear_all_events()
        wp2putils.p2p_connect(go_dut, gc_dut, True, WPS_PBC)
        wp2putils.p2p_disconnect(go_dut)
        wp2putils.check_disconnect(gc_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # trigger reconnect from GC
        self.log.info("Reconnect test, triggered by GC")
        go_dut.ed.clear_all_events()
        gc_dut.ed.clear_all_events()
        wp2putils.p2p_connect(gc_dut, go_dut, True, WPS_PBC)
        wp2putils.p2p_disconnect(gc_dut)
        wp2putils.check_disconnect(go_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # teardown
        self.verify_mac_no_leakage()
