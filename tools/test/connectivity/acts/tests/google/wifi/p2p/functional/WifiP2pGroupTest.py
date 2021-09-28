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


class WifiP2pGroupTest(WifiP2pBaseTest):
    """Tests for APIs in Android's WifiP2pManager class.

    Test Bed Requirement:
    * At least two Android devices
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)

    def setup_class(self):
        super().setup_class()
        if not "network_name" in self.user_params.keys():
            self.log.error("Missing mandatory user config \"network_name\"!")
        self.network_name = self.user_params["network_name"]
        if not "passphrase" in self.user_params.keys():
            self.log.error("Missing mandatory user config \"passphrase\"!")
        self.passphrase = self.user_params["passphrase"]
        if not "group_band" in self.user_params.keys():
            self.log.error("Missing mandatory user config \"group_band\"!")
        self.group_band = self.user_params["group_band"]

    def setup_test(self):
        super().setup_test()
        self.dut1.droid.wifiP2pRemoveGroup()
        self.dut2.droid.wifiP2pRemoveGroup()
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    def teardown_test(self):
        self.dut1.droid.wifiP2pRemoveGroup()
        self.dut2.droid.wifiP2pRemoveGroup()
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        super().teardown_test()

    def p2p_group_join(self, wps_type):
        """ General flow for p2p group join

        Steps:
        1. GO creates a group.
        2. GC joins the group.
        3. connection check via ping from GC to GO
        """
        go_dut = self.dut1
        gc_dut = self.dut2
        # Create a group
        wp2putils.p2p_create_group(go_dut)
        go_dut.ed.pop_event(p2pconsts.CONNECTED_EVENT,
                            p2pconsts.DEFAULT_TIMEOUT)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        # Request the connection
        wp2putils.p2p_connect(gc_dut,
                              go_dut,
                              False,
                              wps_type,
                              p2p_connect_type=p2pconsts.P2P_CONNECT_JOIN)

        go_ip = wp2putils.p2p_go_ip(gc_dut)
        wp2putils.p2p_connection_ping_test(gc_dut, go_ip)
        # trigger disconnect
        wp2putils.p2p_disconnect(go_dut)
        wp2putils.check_disconnect(gc_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    """Test Cases"""
    @test_tracker_info(uuid="c41f8293-5225-430d-917e-c294ddff7c2a")
    def test_p2p_group_join_via_pbc(self):
        """Verify the p2p creates a group and join this group via WPS PBC method.
        """
        self.p2p_group_join(WPS_PBC)

    @test_tracker_info(uuid="56eb339f-d7e4-44f0-9802-6094e9255957")
    def test_p2p_group_join_via_display(self):
        """Verify the p2p creates a group and join this group via WPS DISPLAY method.
        """
        self.p2p_group_join(WPS_DISPLAY)

    @test_tracker_info(uuid="27075cab-7859-49a7-afe9-b6cc6e8faddb")
    def test_p2p_group_with_config(self):
        """Verify the p2p creates a group and join an this group with config.

        Steps:
        1. GO creates a group with config.
        2. GC joins the group with config.
        3. connection check via ping from GC to GO
        """
        go_dut = self.dut1
        gc_dut = self.dut2
        # Create a group
        wp2putils.p2p_create_group_with_config(go_dut, self.network_name,
                                               self.passphrase,
                                               self.group_band)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        # Request the connection. Since config is known, this is reconnection.
        wp2putils.p2p_connect_with_config(gc_dut, go_dut, self.network_name,
                                          self.passphrase, self.group_band)

        go_ip = wp2putils.p2p_go_ip(gc_dut)
        wp2putils.p2p_connection_ping_test(gc_dut, go_ip)
        # trigger disconnect
        wp2putils.p2p_disconnect(go_dut)
        wp2putils.check_disconnect(gc_dut)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
