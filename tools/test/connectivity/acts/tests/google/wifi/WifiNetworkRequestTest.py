#!/usr/bin/env python3.4
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

import itertools
import pprint
import queue
import time

import acts.base_test
import acts.signals as signals
import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.utils

from acts import asserts
from acts.controllers.android_device import SL4A_APK_NAME
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.test_utils.wifi import wifi_constants

WifiEnums = wutils.WifiEnums

# Network request timeout to use.
NETWORK_REQUEST_TIMEOUT_MS = 60 * 1000
# Timeout to wait for instant failure.
NETWORK_REQUEST_INSTANT_FAILURE_TIMEOUT_SEC = 5

class WifiNetworkRequestTest(WifiBaseTest):
    """Tests for NetworkRequest with WifiNetworkSpecifier API surface.

    Test Bed Requirement:
    * one Android device
    * Several Wi-Fi networks visible to the device, including an open Wi-Fi
      network.
    """
    def __init__(self, configs):
        super().__init__(configs)
        self.enable_packet_log = True

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        req_params = []
        opt_param = [
            "open_network", "reference_networks"
        ]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(wpa_network=True,
                                               wep_network=True)
        elif "OpenWrtAP" in self.user_params:
            self.configure_openwrt_ap_and_start(open_network=True,
                                                wpa_network=True,
                                                wep_network=True)

        asserts.assert_true(
            len(self.reference_networks) > 0,
            "Need at least one reference network with psk.")
        self.wpa_psk_2g = self.reference_networks[0]["2g"]
        self.wpa_psk_5g = self.reference_networks[0]["5g"]
        self.open_2g = self.open_network[0]["2g"]
        self.open_5g = self.open_network[0]["5g"]

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        self.remove_approvals()
        self.clear_deleted_ephemeral_networks()
        wutils.wifi_toggle_state(self.dut, True)
        self.dut.ed.clear_all_events()

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        self.dut.droid.wifiReleaseNetworkAll()
        self.dut.droid.wifiDisconnect()
        wutils.reset_wifi(self.dut)
        # Ensure we disconnected from the current network before the next test.
        if self.dut.droid.wifiGetConnectionInfo()["supplicant_state"] != "disconnected":
            wutils.wait_for_disconnect(self.dut)
        wutils.wifi_toggle_state(self.dut, False)
        self.dut.ed.clear_all_events()

    def teardown_class(self):
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    """Helper Functions"""
    def wait_for_network_lost(self):
        """
        Wait for network lost callback from connectivity service (wifi
        disconnect).

        Args:
            ad: Android device object.
        """
        try:
            self.dut.droid.wifiStartTrackingStateChange()
            event = self.dut.ed.pop_event(
                wifi_constants.WIFI_NETWORK_CB_ON_LOST, 10)
            self.dut.droid.wifiStopTrackingStateChange()
        except queue.Empty:
            raise signals.TestFailure(
                "Device did not disconnect from the network")

    def remove_approvals(self):
        self.dut.log.debug("Removing all approvals from sl4a app")
        self.dut.adb.shell(
            "cmd wifi network-requests-remove-user-approved-access-points"
            + " " + SL4A_APK_NAME)

    def clear_deleted_ephemeral_networks(self):
        self.dut.log.debug("Clearing deleted ephemeral networks")
        self.dut.adb.shell(
            "cmd wifi clear-deleted-ephemeral-networks")

    @test_tracker_info(uuid="d70c8380-61ba-48a3-b76c-a0b55ce4eabf")
    def test_connect_to_wpa_psk_2g_with_ssid(self):
        """
        Initiates a connection to network via network request with specific SSID

        Steps:
        1. Send a network specifier with the specific SSID/credentials of
           WPA-PSK 2G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        """
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_2g,
                                                  self.wpa_psk_2g)

    @test_tracker_info(uuid="44f2bf40-a282-4413-b8f2-3abb3caa49ee")
    def test_connect_to_open_5g_with_ssid(self):
        """
        Initiates a connection to network via network request with specific SSID

        Steps:
        1. Send a network specifier with the specific SSID of Open 5G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        """
        wutils.wifi_connect_using_network_request(self.dut, self.open_5g,
                                                  self.open_5g)

    @test_tracker_info(uuid="09d1823e-4f85-42f8-8c20-de7637f6d4be")
    def test_connect_to_wpa_psk_5g_with_ssid_pattern(self):
        """
        Initiates a connection to network via network request with SSID pattern

        Steps:
        1. Send a network specifier with the SSID pattern/credentials of
           WPA-PSK 5G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        """
        network_specifier = self.wpa_psk_5g.copy();
        # Remove ssid & replace with ssid pattern.
        network_ssid = network_specifier.pop(WifiEnums.SSID_KEY)
        # Remove the last element of ssid & replace with .* to create a matching
        # pattern.
        network_specifier[WifiEnums.SSID_PATTERN_KEY] = network_ssid[:-1] + ".*"
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_5g,
                                                  network_specifier)

    @test_tracker_info(uuid="52216329-06f1-45ef-8d5f-de8b02d9f975")
    def test_connect_to_open_5g_after_connecting_to_wpa_psk_2g(self):
        """
        Initiates a connection to network via network request with SSID pattern

        Steps:
        1. Send a network specifier with the specific SSID of Open 5G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        5. Release the network request.
        6. Send another network specifier with the specific SSID & credentials
           of WPA-PSK 2G network.
        7. Ensure we disconnect from the previous network.
        8. Wait for platform to scan and find matching networks.
        9. Simulate user selecting the new network.
        10. Ensure that the device connects to the new network.
        """
        # Complete flow for the first request.
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_2g,
                                                  self.wpa_psk_2g)
        # Release the request.
        self.dut.droid.wifiReleaseNetwork(self.wpa_psk_2g)
        # Ensure we disconnected from the previous network.
        wutils.wait_for_disconnect(self.dut)
        self.dut.log.info("Disconnected from network %s", self.wpa_psk_2g)
        self.dut.ed.clear_all_events()
        # Complete flow for the second request.
        wutils.wifi_connect_using_network_request(self.dut, self.open_5g,
                                                  self.open_5g)

    @test_tracker_info(uuid="f28b5dc9-771f-42ef-8178-e55e9a16f5c7")
    def test_connect_to_wpa_psk_5g_while_connecting_to_open_2g(self):
        """
        Initiates a connection to network via network request with specific SSID

        Steps:
        1. Send a network specifier with the specific SSID & credentials of
           WPA-PSK 5G network.
        2. Send another network specifier with the specific SSID of Open 2G
           network.
        3. Ensure we disconnect from the previous network.
        4. Wait for platform to scan and find matching networks.
        5. Simulate user selecting the new network.
        6. Ensure that the device connects to the new network.
        """
        # Make the first request.
        self.dut.droid.wifiRequestNetworkWithSpecifier(self.open_2g)
        self.dut.log.info("Sent network request with %s", self.open_2g)
        # Complete flow for the second request.
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_5g,
                                                  self.wpa_psk_5g)

    @test_tracker_info(uuid="2ab82a98-37da-4b27-abb6-578bedebccdc")
    def test_connect_to_open_5g_while_connected_to_wpa_psk_2g(self):
        """
        Initiates a connection to network via network request with specific SSID

        Steps:
        1. Send a network specifier with the specific SSID of Open 5G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        5. Send another network specifier with the specific SSID & credentials
           of WPA-PSK 2G network.
        6. Ensure we disconnect from the previous network.
        7. Wait for platform to scan and find matching networks.
        8. Simulate user selecting the new network.
        9. Ensure that the device connects to the new network.
        """
        # Complete flow for the first request.
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_2g,
                                                  self.wpa_psk_2g)
        # Send the second request.
        self.dut.droid.wifiRequestNetworkWithSpecifier(self.open_5g)
        self.dut.log.info("Sent network request with %s", self.open_5g)
        # Ensure we do not disconnect from the previous network until the user
        # approves the new request.
        self.dut.ed.clear_all_events()
        wutils.ensure_no_disconnect(self.dut)

        # Now complete the flow and ensure we connected to second request.
        wutils.wait_for_wifi_connect_after_network_request(self.dut,
                                                           self.open_5g)

    @test_tracker_info(uuid="f0bb2213-b3d1-4fb8-bbdc-ad55c4fb05ed")
    def test_connect_to_wpa_psk_2g_which_is_already_approved(self):
        """
        Initiates a connection to network via network request with specific SSID
        bypassing user approval.

        Steps:
        1. Send a network specifier with the specific SSID/credentials of
           WPA-PSK 2G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        5. Ensure we disconnect from the previous network.
        6. Send another network specifier with the specific
           SSID/BSSID/credentials of WPA-PSK 2G network.
        7. Ensure that the device bypasses user approval & connects to the
           same network.
        """
        # Complete flow for the first request.
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_2g,
                                                  self.wpa_psk_2g)
        # Release the request.
        self.dut.droid.wifiReleaseNetwork(self.wpa_psk_2g)
        # Ensure we disconnected from the network.
        wutils.wait_for_disconnect(self.dut)
        self.dut.log.info("Disconnected from network %s", self.wpa_psk_2g)
        self.dut.ed.clear_all_events()

        # Find bssid for the WPA-PSK 2G network.
        scan_results = self.dut.droid.wifiGetScanResults()
        match_results = wutils.match_networks(
            {WifiEnums.SSID_KEY: self.wpa_psk_2g[WifiEnums.SSID_KEY]},
            scan_results)
        asserts.assert_equal(len(match_results), 1,
                             "Cannot find bssid for WPA-PSK 2G network")
        bssid = match_results[0][WifiEnums.BSSID_KEY]
        # Send the second request with bssid.
        network_specifier_with_bssid = self.wpa_psk_2g.copy();
        network_specifier_with_bssid[WifiEnums.BSSID_KEY] = bssid
        self.dut.droid.wifiRequestNetworkWithSpecifier(
            network_specifier_with_bssid)
        self.dut.log.info("Sent network request with %r",
                          network_specifier_with_bssid)

        # Ensure we connected to second request without user approval.
        wutils.wait_for_connect(self.dut, self.wpa_psk_2g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="fcf84d94-5f6e-4bd6-9f76-40a0228d4ebe")
    def test_connect_to_wpa_psk_2g_which_is_already_approved_but_then_forgot(self):
        """
        Initiates a connection to network via network request with specific SSID
        with user approval.

        Steps:
        1. Send a network specifier with the specific SSID/credentials of
           WPA-PSK 2G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user selecting the network.
        4. Ensure that the device connects to the network.
        4. Simulate user fogetting the network from the UI.
        6. Ensure we disconnect from the previous network.
        7. Send another network specifier with the specific
           SSID/BSSID/credentials of WPA-PSK 2G network.
        8. Ensure that the device does not bypass user approval & connects to the
           same network with user approval. (This should also clear the blacklist)
        9. Send the same network specifier with the specific
           SSID/BSSID/credentials of WPA-PSK 2G network.
        10.Ensure that the device bypasses user approval now & connects to the
           same network.
        """
        # Complete flow for the first request.
        wutils.wifi_connect_using_network_request(self.dut, self.wpa_psk_2g,
                                                  self.wpa_psk_2g)

        # Simulate user forgeting the ephemeral network.
        self.dut.droid.wifiUserDisconnectNetwork(self.wpa_psk_2g[WifiEnums.SSID_KEY])
        # Ensure we disconnected from the network.
        wutils.wait_for_disconnect(self.dut)
        self.dut.log.info("Disconnected from network %s", self.wpa_psk_2g)
        self.dut.ed.clear_all_events()
        # Release the first request.
        self.dut.droid.wifiReleaseNetwork(self.wpa_psk_2g)

        # Find bssid for the WPA-PSK 2G network.
        scan_results = self.dut.droid.wifiGetScanResults()
        match_results = wutils.match_networks(
            {WifiEnums.SSID_KEY: self.wpa_psk_2g[WifiEnums.SSID_KEY]},
            scan_results)
        asserts.assert_equal(len(match_results), 1,
                             "Cannot find bssid for WPA-PSK 2G network")
        bssid = match_results[0][WifiEnums.BSSID_KEY]
        # Send the second request with bssid.
        network_specifier_with_bssid = self.wpa_psk_2g.copy();
        network_specifier_with_bssid[WifiEnums.BSSID_KEY] = bssid
        self.dut.droid.wifiRequestNetworkWithSpecifier(
            network_specifier_with_bssid)
        self.dut.log.info("Sent network request with %r",
                          network_specifier_with_bssid)

        # Ensure that we did not connect bypassing user approval.
        assert_msg = "Device should not connect without user approval"
        asserts.assert_false(
            wutils.wait_for_connect(self.dut,
                                    self.wpa_psk_2g[WifiEnums.SSID_KEY],
                                    assert_on_fail=False),
            assert_msg)

        # Now complete the flow and ensure we connected to second request.
        wutils.wait_for_wifi_connect_after_network_request(self.dut,
                                                           self.wpa_psk_2g)

        # Now make the same request again & ensure that we connect without user
        # approval.
        self.dut.droid.wifiRequestNetworkWithSpecifier(
            network_specifier_with_bssid)
        self.dut.log.info("Sent network request with %r",
                          network_specifier_with_bssid)
        wutils.wait_for_connect(self.dut, self.wpa_psk_2g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="2f90a266-f04d-4932-bb5b-d075bedfd56d")
    def test_match_failure_with_invalid_ssid_pattern(self):
        """
        Initiates a connection to network via network request with SSID pattern
        that does not match any networks.

        Steps:
        1. Trigger a connect to one of the networks (as a saved network).
        2. Send a network specifier with the non-matching SSID pattern.
        3. Ensure that the platform does not return any matching networks.
        4. Wait for the request to timeout.
        """
        network = self.wpa_psk_5g

        # Trigger a connection to a network as a saved network before the
        # request and ensure that this does not change the behavior.
        wutils.connect_to_wifi_network(self.dut, network, check_connectivity=False)

        network_specifier = self.wpa_psk_5g.copy();
        # Remove ssid & replace with invalid ssid pattern.
        network_ssid = network_specifier.pop(WifiEnums.SSID_KEY)
        network_specifier[WifiEnums.SSID_PATTERN_KEY] = \
            network_ssid + "blah" + ".*"

        self.dut.droid.wifiStartTrackingStateChange()
        expected_ssid = network[WifiEnums.SSID_KEY]

        self.dut.droid.wifiRequestNetworkWithSpecifierWithTimeout(
              network_specifier, NETWORK_REQUEST_TIMEOUT_MS)
        self.dut.log.info("Sent network request with invalid specifier %s",
                    network_specifier)
        time.sleep(wifi_constants.NETWORK_REQUEST_CB_REGISTER_DELAY_SEC)
        self.dut.droid.wifiRegisterNetworkRequestMatchCallback()
        # Wait for the request to timeout.
        timeout_secs = NETWORK_REQUEST_TIMEOUT_MS * 2 / 1000
        try:
            on_unavailable_event = self.dut.ed.pop_event(
                wifi_constants.WIFI_NETWORK_CB_ON_UNAVAILABLE, timeout_secs)
            asserts.assert_true(on_unavailable_event, "Network request did not timeout")
        except queue.Empty:
            asserts.fail("No events returned")
        finally:
            self.dut.droid.wifiStopTrackingStateChange()

    @test_tracker_info(uuid="caa96f57-840e-4997-9280-655edd3b76ee")
    def test_connect_failure_user_rejected(self):
        """
        Initiates a connection to network via network request with specific SSID
        which the user rejected.

        Steps:
        1. Send a network specifier with the specific SSID/credentials of
           WPA-PSK 5G network.
        2. Wait for platform to scan and find matching networks.
        3. Simulate user rejecting the network.
        4. Ensure that we get an instant onUnavailable callback.
        5. Simulate user fogetting the network from the UI.
        """
        network = self.wpa_psk_5g
        expected_ssid = network[WifiEnums.SSID_KEY]

        self.dut.droid.wifiStartTrackingStateChange()

        self.dut.droid.wifiRequestNetworkWithSpecifierWithTimeout(
              network, NETWORK_REQUEST_TIMEOUT_MS)
        self.dut.log.info("Sent network request with specifier %s", network)
        time.sleep(wifi_constants.NETWORK_REQUEST_CB_REGISTER_DELAY_SEC)
        self.dut.droid.wifiRegisterNetworkRequestMatchCallback()

        # Wait for the platform to scan and return a list of networks
        # matching the request
        try:
            matched_network = None
            for _ in [0,  3]:
                on_match_event = self.dut.ed.pop_event(
                    wifi_constants.WIFI_NETWORK_REQUEST_MATCH_CB_ON_MATCH, 30)
                asserts.assert_true(on_match_event,
                                    "Network request on match not received.")
                matched_scan_results = on_match_event["data"]
                self.dut.log.debug("Network request on match results %s",
                                   matched_scan_results)
                matched_network = wutils.match_networks(
                    {WifiEnums.SSID_KEY: network[WifiEnums.SSID_KEY]},
                     matched_scan_results)
                if matched_network:
                    break;
            asserts.assert_true(
                matched_network, "Target network %s not found" % network)

            # Send user rejection.
            self.dut.droid.wifiSendUserRejectionForNetworkRequestMatch()
            self.dut.log.info("Sent user rejection for network request %s",
                              expected_ssid)

            # Wait for the platform to raise unavailable callback
            # instantaneously.
            on_unavailable_event = self.dut.ed.pop_event(
                wifi_constants.WIFI_NETWORK_CB_ON_UNAVAILABLE,
                NETWORK_REQUEST_INSTANT_FAILURE_TIMEOUT_SEC)
            asserts.assert_true(on_unavailable_event,
                                "Network request on available not received.")
        except queue.Empty:
            asserts.fail("Expected events not returned")
        finally:
            self.dut.droid.wifiStopTrackingStateChange()
