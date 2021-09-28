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

from acts import asserts
from acts import utils

from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.p2p.WifiP2pBaseTest import WifiP2pBaseTest
from acts.test_utils.wifi.p2p import wifi_p2p_test_utils as wp2putils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts

WPS_PBC = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_PBC
WPS_DISPLAY = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_DISPLAY
WPS_KEYPAD = wp2putils.WifiP2PEnums.WpsInfo.WIFI_WPS_INFO_KEYPAD

WifiEnums = wutils.WifiEnums


class WifiP2pMultiPeersTest(WifiP2pBaseTest):
    """Tests for multiple clients.

    Test Bed Requirement:
    * 3 Android devices for each test in this class.
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)

    def setup_test(self):
        WifiP2pBaseTest.setup_test(self)
        asserts.skip_if(
            len(self.android_devices) < 3,
            "No enough android devices. Skip this test")

    def form_group(self, dut1, dut2, isReconnect=False, wpsType=WPS_PBC):
        # Request the connection to create a group
        wp2putils.p2p_connect(dut1, dut2, isReconnect, wpsType)

        if wp2putils.is_go(dut1):
            go_dut = dut1
            gc_dut = dut2
        elif wp2putils.is_go(dut2):
            go_dut = dut2
            gc_dut = dut1
        return (go_dut, gc_dut)

    def verify_group_connection(self, group_clients):
        for gc in group_clients:
            go_ip = wp2putils.p2p_go_ip(gc)
            wp2putils.p2p_connection_ping_test(gc, go_ip)

    def clear_all_events(self, duts):
        for dut in duts:
            dut.ed.clear_all_events()

    def check_disconnection(self, duts):
        for dut in duts:
            wp2putils.check_disconnect(dut)

    """Test Cases"""
    @test_tracker_info(uuid="20cd4f4d-fe7d-4ee2-a832-33caa5b9700b")
    def test_p2p_multi_clients_group_removal_behavior(self):
        """Verify the p2p group removal behavior

        Steps:
        1. form a group between 3 peers
        2. verify their connection
        3. disconnect
        4. form a group between 3 peers
        5. trigger disconnect from GO
        6. check the group is removed
        7. form a group between 3 peers
        8. trigger disconnect from a GC
        9. check the group is still alive
        10. disconnect
        """
        all_duts = [self.dut1, self.dut2, self.dut3]

        # the 1st round
        (go_dut, gc_dut) = self.form_group(self.dut1, self.dut2)
        gc2_dut = self.dut3
        wp2putils.p2p_connect(gc2_dut,
                              go_dut,
                              False,
                              WPS_PBC,
                              p2p_connect_type=p2pconsts.P2P_CONNECT_JOIN)

        self.verify_group_connection([gc_dut, gc2_dut])

        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        self.check_disconnection([gc_dut, gc2_dut])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # the 2nd round
        self.log.info("Reconnect test, triggered by GO")
        self.clear_all_events(all_duts)

        self.form_group(go_dut, gc_dut, isReconnect=True)
        wp2putils.p2p_connect(gc2_dut,
                              go_dut,
                              True,
                              WPS_PBC,
                              p2p_connect_type=p2pconsts.P2P_CONNECT_JOIN)

        # trigger disconnect from GO, the group is destroyed and all
        # client are disconnected.
        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        self.check_disconnection([gc_dut, gc2_dut])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

        # the 3rd round
        self.log.info("Reconnect test, triggered by GC")
        self.clear_all_events(all_duts)

        self.form_group(go_dut, gc_dut, isReconnect=True)
        wp2putils.p2p_connect(gc2_dut,
                              go_dut,
                              True,
                              WPS_PBC,
                              p2p_connect_type=p2pconsts.P2P_CONNECT_JOIN)

        # trigger disconnect from GC, the group is still there.
        gc_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(gc_dut)
        self.verify_group_connection([
            gc2_dut,
        ])

        # all clients are disconnected, the group is removed.
        wp2putils.p2p_disconnect(gc2_dut)
        self.check_disconnection([
            go_dut,
        ])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    @test_tracker_info(uuid="6ea6e802-df62-4ae2-aa15-44c3267fd99b")
    def test_p2p_connect_with_p2p_and_join_go(self):
        """Verify the invitation from GC

        Steps:
        1. form a group between 2 peers
        2. gc joins the group via go
        2. verify their connection
        3. disconnect
        """
        all_duts = [self.dut1, self.dut2, self.dut3]

        (go_dut, gc_dut) = self.form_group(self.dut1, self.dut2)
        gc2_dut = self.dut3
        wp2putils.p2p_connect(gc2_dut,
                              go_dut,
                              False,
                              WPS_PBC,
                              p2p_connect_type=p2pconsts.P2P_CONNECT_JOIN)

        self.verify_group_connection([gc_dut, gc2_dut])

        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        self.check_disconnection([gc_dut, gc2_dut])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    @test_tracker_info(uuid="e00469a4-93b7-44dd-8a5e-5d317e0e9333")
    def test_p2p_connect_with_p2p_and_legacy_client(self):
        """Verify the invitation from GC

        Steps:
        1. form a group between 2 peers
        2. gc joins the group via go
        2. verify their connection
        3. disconnect
        """
        all_duts = [self.dut1, self.dut2, self.dut3]

        (go_dut, gc_dut) = self.form_group(self.dut1, self.dut2)
        gc2_dut = self.dut3

        group = wp2putils.p2p_get_current_group(go_dut)
        network = {
            WifiEnums.SSID_KEY: group['NetworkName'],
            WifiEnums.PWD_KEY: group['Passphrase']
        }
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            gc2_dut, group['NetworkName'])
        wutils.wifi_connect(gc2_dut, network, num_of_tries=3)

        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    @test_tracker_info(uuid="bd53cc18-bcc7-4e27-b78b-1506f5c098c5")
    def test_p2p_connect_with_p2p_and_go_invite_peer(self):
        """Verify the invitation from GC

        Steps:
        1. form a group between 2 peers
        2. gc joins the group via go
        2. verify their connection
        3. disconnect
        """
        all_duts = [self.dut1, self.dut2, self.dut3]

        (go_dut, gc_dut) = self.form_group(self.dut1, self.dut2)
        gc2_dut = self.dut3
        wp2putils.p2p_connect(
            go_dut,
            gc2_dut,
            False,
            WPS_PBC,
            p2p_connect_type=p2pconsts.P2P_CONNECT_INVITATION,
            go_ad=go_dut)

        self.verify_group_connection([gc_dut, gc2_dut])

        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        self.check_disconnection([gc_dut, gc2_dut])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    @test_tracker_info(uuid="4d6e666d-dc48-4881-86c1-5d7cec5e2571")
    def test_p2p_connect_with_p2p_and_gc_invite_peer(self):
        """Verify the invitation from GC

        Steps:
        1. form a group between 2 peers
        2. gc joins the group via go
        2. verify their connection
        3. disconnect
        """
        all_duts = [self.dut1, self.dut2, self.dut3]

        (go_dut, gc_dut) = self.form_group(self.dut1, self.dut2)
        gc2_dut = self.dut3
        wp2putils.p2p_connect(
            gc_dut,
            gc2_dut,
            False,
            WPS_PBC,
            p2p_connect_type=p2pconsts.P2P_CONNECT_INVITATION,
            go_ad=go_dut)

        self.verify_group_connection([gc_dut, gc2_dut])

        go_dut.log.info("Trigger disconnection")
        wp2putils.p2p_disconnect(go_dut)
        self.check_disconnection([gc_dut, gc2_dut])
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
