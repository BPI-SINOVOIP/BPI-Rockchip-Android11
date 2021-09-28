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

import time

from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.p2p.WifiP2pBaseTest import WifiP2pBaseTest
from acts.test_utils.wifi.p2p import wifi_p2p_test_utils as wp2putils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts


class WifiP2pLocalServiceTest(WifiP2pBaseTest):
    """Tests for APIs in Android's WifiP2pManager and p2p local service class.

    Test Bed Requirement:
    * At least two Android devices
    * 3 Android devices for WifiP2pMultiPeersTest.py
    """
    def __init__(self, controllers):
        WifiP2pBaseTest.__init__(self, controllers)

    """Test Cases"""
    @test_tracker_info(uuid="ba879c8d-0fbd-41fb-805c-5cd1cd312090")
    def test_p2p_upnp_service(self):
        """Verify the p2p discovery functionality
        Steps:
        1. dut1 add local Upnp service
        2. dut2 register Upnp Service listener
        3. Check dut2 peer list if it only included dut1
        4. Setup p2p upnp local service request with different query string
        5. Check p2p upnp local servier query result is expect or not
        6. Test different query string and check query result
        Note: Step 2 - Step 5 should reference function requestServiceAndCheckResult
        """
        self.log.info("Add local Upnp Service")
        wp2putils.createP2pLocalService(self.dut1,
                                        p2pconsts.P2P_LOCAL_SERVICE_UPNP)

        wp2putils.requestServiceAndCheckResult(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_UPNP, None, None)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        wp2putils.requestServiceAndCheckResult(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_UPNP, "ssdp:all", None)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        wp2putils.requestServiceAndCheckResult(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_UPNP, "upnp:rootdevice", None)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)

    """Test Cases"""
    @test_tracker_info(uuid="470306fa-5c46-4258-ade2-9c0834bb04f9")
    def test_p2p_bonjour_service(self):
        """Verify the p2p discovery functionality
        Steps:
        1. dut1 add local bonjour service - IPP and AFP
        2. dut2 register bonjour Service listener - dnssd and dnssd_txrecord
        3. Check dut2 peer list if it only included dut1
        4. Setup p2p bonjour local service request with different query string
        5. Check p2p bonjour local servier query result is expect or not
        6. Test different query string and check query result
        Note: Step 2 - Step 5 should reference function requestServiceAndCheckResult
        """
        self.log.info("Add local bonjour service to %s" % (self.dut1.name))
        wp2putils.createP2pLocalService(self.dut1,
                                        p2pconsts.P2P_LOCAL_SERVICE_IPP)
        wp2putils.createP2pLocalService(self.dut1,
                                        p2pconsts.P2P_LOCAL_SERVICE_AFP)

        wp2putils.requestServiceAndCheckResultWithRetry(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_BONJOUR, None, None)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        wp2putils.requestServiceAndCheckResultWithRetry(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_BONJOUR, "_ipp._tcp", None)
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        wp2putils.requestServiceAndCheckResultWithRetry(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_BONJOUR, "_ipp._tcp", "MyPrinter")
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        wp2putils.requestServiceAndCheckResultWithRetry(
            self.dut1, self.dut2, wp2putils.WifiP2PEnums.WifiP2pServiceInfo.
            WIFI_P2P_SERVICE_TYPE_BONJOUR, "_afpovertcp._tcp", "Example")
