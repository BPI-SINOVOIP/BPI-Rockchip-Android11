#!/usr/bin/env python3
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

import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.utils
import time

from acts import asserts
from acts import utils

from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.p2p.WifiP2pBaseTest import WifiP2pBaseTest
from acts.test_utils.wifi.p2p import wifi_p2p_test_utils as wp2putils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts

WPS_PBC = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
WPS_DISPLAY = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_DISPLAY
WPS_KEYPAD = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_KEYPAD


class WifiP2pManagerTest(WifiP2pBaseTest):
    """Tests for APIs in Android's WifiP2pManager class.

    Test Bed Requirement:
    * At least two Android devices
    * 3 Android devices for WifiP2pMultiPeersTest.py
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)

    """Test Cases"""
    @test_tracker_info(uuid="28ddb16c-2ce4-44da-92f9-701d0dacc321")
    def test_p2p_discovery(self):
        """Verify the p2p discovery functionality
        Steps:
        1. Discover the target device
        2. Check the target device in peer list
        """
        self.log.info("Device discovery")
        wp2putils.find_p2p_device(self.dut1, self.dut2)
        wp2putils.find_p2p_device(self.dut2, self.dut1)

    @test_tracker_info(uuid="0016e6db-9b46-44fb-a53e-10a81eee955e")
    def test_p2p_connect_via_pbc_and_ping_and_reconnect(self):
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
        wp2putils.check_disconnect(
            go_dut, timeout=p2pconsts.DEFAULT_GROUP_CLIENT_LOST_TIME)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    @test_tracker_info(uuid="12bbe73a-5a6c-4307-9797-c77c7efdc4b5")
    def test_p2p_connect_via_display_and_ping_and_reconnect(self):
        """Verify the p2p connect via display functionality

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
        wp2putils.p2p_connect(self.dut1, self.dut2, False, WPS_DISPLAY)

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

    @test_tracker_info(uuid="efe88f57-5a08-4195-9592-2f6945a9d18a")
    def test_p2p_connect_via_keypad_and_ping_and_reconnect(self):
        """Verify the p2p connect via keypad functionality

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
        wp2putils.p2p_connect(self.dut1, self.dut2, False, WPS_KEYPAD)

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
        wp2putils.p2p_connect(go_dut, gc_dut, True, WPS_KEYPAD)
        wp2putils.p2p_disconnect(go_dut)
        wp2putils.check_disconnect(gc_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # trigger reconnect from GC
        self.log.info("Reconnect test, triggered by GC")
        go_dut.ed.clear_all_events()
        gc_dut.ed.clear_all_events()
        wp2putils.p2p_connect(gc_dut, go_dut, True, WPS_KEYPAD)
        wp2putils.p2p_disconnect(gc_dut)
        wp2putils.check_disconnect(
            go_dut, timeout=p2pconsts.DEFAULT_GROUP_CLIENT_LOST_TIME)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
