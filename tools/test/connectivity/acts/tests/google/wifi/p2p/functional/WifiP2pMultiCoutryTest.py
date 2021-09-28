#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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
import acts.controllers.packet_capture as packet_capture
import re
import logging

from acts import asserts
from acts import utils

from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.p2p.WifiP2pBaseTest import WifiP2pBaseTest
from acts.test_utils.wifi.p2p import wifi_p2p_test_utils as wp2putils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts
from acts.controllers.ap_lib.hostapd_constants import CHANNEL_MAP

WPS_PBC = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
WPS_DISPLAY = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_DISPLAY
WPS_KEYPAD = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_KEYPAD

WifiEnums = wutils.WifiEnums

class WifiP2pMultiCoutryTest(WifiP2pBaseTest):
    """Tests for APIs in Android's WifiP2pManager class.

    Test Bed Requirement:
    * At least two Android devices
    * 3 Android devices for WifiP2pMultiPeersTest.py
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)
        self.basetest_name = (
            "test_p2p_connect_via_pbc_and_ping_and_reconnect_multicountry",
            "test_p2p_connect_via_display_and_ping_and_reconnect_multicountry",
            )

        self.generate_tests()

    def generate_testcase(self, basetest_name, country):
        """Generates a single test case from the given data.

        Args:
            basetest_name: The name of the base test case.
            country: The information about the country code under test.
        """
        base_test = getattr(self, basetest_name)
        test_tracker_uuid = ""

        testcase_name = 'test_%s_%s' % (basetest_name, country)
        test_case = test_tracker_info(uuid=test_tracker_uuid)(
            lambda: base_test(country))
        setattr(self, testcase_name, test_case)
        self.tests.append(testcase_name)

    def generate_tests(self):
        for country in self.user_params['wifi_country_code']:
                for basetest_name in self.basetest_name:
                    self.generate_testcase(basetest_name, country)

    def setup_class(self):
        super().setup_class()
        if hasattr(self, 'packet_capture'):
            self.packet_capture = self.packet_capture[0]
        self.channel_list_2g = WifiEnums.ALL_2G_FREQUENCIES
        self.channel_list_5g = WifiEnums.ALL_5G_FREQUENCIES

    def conf_packet_capture(self, band, channel):
        """Configure packet capture on necessary channels."""
        freq_to_chan = wutils.WifiEnums.freq_to_channel[int(channel)]
        logging.info("Capturing packets from "
                     "frequency:{}, Channel:{}".format(channel, freq_to_chan))
        result = self.packet_capture.configure_monitor_mode(band, freq_to_chan)
        if not result:
            logging.error("Failed to configure channel "
                          "for {} band".format(band))
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, band, self.test_name)
        time.sleep(5)

    def get_p2p0_freq(self, dut):
        """ get P2P0 interface status"""
        get_p2p0 = "timeout 3 logcat |grep P2P-GROUP-STARTED |grep p2p-p2p0-* |grep freq="
        out_p2p01 = dut.adb.shell(get_p2p0)
        out_p2p0 = re.findall("freq=(\d+)", out_p2p01)
        return out_p2p0



    """Test Cases"""

    @test_tracker_info(uuid="f7d98b9e-494e-4e60-ae29-8418e270d2d8")
    def test_p2p_connect_via_pbc_and_ping_and_reconnect_multicountry(self, country):
        """Verify the p2p connect via pbc functionality

        Steps:
        1. Setting country code for each device
        2. Request the connection which include discover the target device
        3. check which dut is GO and which dut is GC
        4. connection check via ping from GC to GO
        5. disconnect
        6. Trigger connect again from GO for reconnect test.
        7. GO trigger disconnect
        8. Trigger connect again from GC for reconnect test.
        9. GC trigger disconnect
        """
        # Request the connection

        wutils.set_wifi_country_code(self.dut1, country)
        wutils.set_wifi_country_code(self.dut2, country)
        wp2putils.p2p_connect(self.dut1, self.dut2, False, WPS_PBC)
        p2pfreg = int(self.get_p2p0_freq(self.dut1)[0])
        p2p_channel = str(CHANNEL_MAP[p2pfreg])
        n = int(p2p_channel)
        if n in range(len(self.channel_list_2g)):
            sp2p_band = '2g'
        else:
            sp2p_band = '5g'
        self.dut1.log.info('p2p frequency : {}'.format(p2pfreg))
        self.dut1.log.info('p2p channel : {}'.format(p2p_channel))
        self.dut1.log.info('p2p band : {}'.format(sp2p_band))
        if hasattr(self, 'packet_capture'):
            self.conf_packet_capture(sp2p_band, p2pfreg)
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
        wp2putils.check_disconnect(
            go_dut, timeout=p2pconsts.DEFAULT_GROUP_CLIENT_LOST_TIME)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        time.sleep(10)

    @test_tracker_info(uuid="56b9745a-06ec-49fc-91d5-383f2dac082a")
    def test_p2p_connect_via_display_and_ping_and_reconnect_multicountry(self, country):
        """Verify the p2p connect via display functionality

        Steps:
        1. Setting country code for each device
        2. Request the connection which include discover the target device
        3. check which dut is GO and which dut is GC
        4. connection check via ping from GC to GO
        5. disconnect
        6. Trigger connect again from GO for reconnect test.
        7. GO trigger disconnect
        8. Trigger connect again from GC for reconnect test.
        9. GC trigger disconnect
        """
        # Request the connection
        wutils.set_wifi_country_code(self.dut1, country)
        wutils.set_wifi_country_code(self.dut2, country)
        wp2putils.p2p_connect(self.dut1, self.dut2, False, WPS_DISPLAY)
        p2pfreg = int(self.get_p2p0_freq(self.dut1)[0])
        p2p_channel = str(CHANNEL_MAP[p2pfreg])
        n = int(p2p_channel)
        if n in range(len(self.channel_list_2g)):
            sp2p_band = '2g'
        else:
            sp2p_band = '5g'
        self.dut1.log.info('p2p frequency : {}'.format(p2pfreg))
        self.dut1.log.info('p2p channel : {}'.format(p2p_channel))
        self.dut1.log.info('p2p band : {}'.format(sp2p_band))
        if hasattr(self, 'packet_capture'):
            self.conf_packet_capture(sp2p_band, p2pfreg)
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
        wp2putils.p2p_connect(go_dut, gc_dut, True, WPS_DISPLAY)
        wp2putils.p2p_disconnect(go_dut)
        wp2putils.check_disconnect(gc_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # trigger reconnect from GC
        self.log.info("Reconnect test, triggered by GC")
        go_dut.ed.clear_all_events()
        gc_dut.ed.clear_all_events()
        wp2putils.p2p_connect(gc_dut, go_dut, True, WPS_DISPLAY)
        wp2putils.p2p_disconnect(gc_dut)
        wp2putils.check_disconnect(
            go_dut, timeout=p2pconsts.DEFAULT_GROUP_CLIENT_LOST_TIME)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        time.sleep(10)

