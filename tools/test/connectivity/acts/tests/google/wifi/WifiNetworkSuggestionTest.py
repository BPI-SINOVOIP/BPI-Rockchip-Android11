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
# EAP Macros
EAP = WifiEnums.Eap
EapPhase2 = WifiEnums.EapPhase2
# Enterprise Config Macros
Ent = WifiEnums.Enterprise
ATT = 2
# Suggestion network Macros
Untrusted = "untrusted"
AutoJoin = "enableAutojoin"
# Network request Macros
ClearCapabilities = "ClearCapabilities"
TransportType = "TransportType"


# Default timeout used for reboot, toggle WiFi and Airplane mode,
# for the system to settle down after the operation.
DEFAULT_TIMEOUT = 10
PASSPOINT_TIMEOUT = 30


class WifiNetworkSuggestionTest(WifiBaseTest):
    """Tests for WifiNetworkSuggestion API surface.

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
        opt_param = [
            "open_network", "reference_networks", "hidden_networks", "radius_conf_2g",
            "radius_conf_5g", "ca_cert", "eap_identity", "eap_password", "passpoint_networks",
            "domain_suffix_match"]
        self.unpack_userparams(opt_param_names=opt_param,)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(
                wpa_network=True, ent_network=True,
                radius_conf_2g=self.radius_conf_2g,
                radius_conf_5g=self.radius_conf_5g,)
        elif "OpenWrtAP" in self.user_params:
            self.configure_openwrt_ap_and_start(open_network=True,
                                                wpa_network=True,)
        if hasattr(self, "reference_networks") and \
            isinstance(self.reference_networks, list):
              self.wpa_psk_2g = self.reference_networks[0]["2g"]
              self.wpa_psk_5g = self.reference_networks[0]["5g"]
        if hasattr(self, "open_network") and isinstance(self.open_network,list):
            self.open_2g = self.open_network[0]["2g"]
            self.open_5g = self.open_network[0]["5g"]
        if hasattr(self, "hidden_networks") and \
            isinstance(self.hidden_networks, list):
              self.hidden_network = self.hidden_networks[0]
        if hasattr(self, "passpoint_networks"):
            self.passpoint_network = self.passpoint_networks[ATT]
            self.passpoint_network[WifiEnums.SSID_KEY] = \
                self.passpoint_networks[ATT][WifiEnums.SSID_KEY][0]
        self.dut.droid.wifiRemoveNetworkSuggestions([])
        self.dut.adb.shell(
            "pm disable com.google.android.apps.carrier.carrierwifi")

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        self.dut.unlock_screen()
        self.clear_user_disabled_networks()
        wutils.wifi_toggle_state(self.dut, True)
        self.dut.ed.clear_all_events()
        self.clear_carrier_approved(str(self.dut.droid.telephonyGetSimCarrierId()))
        if "_ent_" in self.test_name:
            if "OpenWrtAP" in self.user_params:
                self.access_points[0].close()
                self.configure_openwrt_ap_and_start(
                    ent_network=True,
                    radius_conf_2g=self.radius_conf_2g,
                    radius_conf_5g=self.radius_conf_5g,)
            self.ent_network_2g = self.ent_networks[0]["2g"]
            self.ent_network_5g = self.ent_networks[0]["5g"]

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        self.dut.droid.wifiRemoveNetworkSuggestions([])
        self.dut.droid.wifiDisconnect()
        wutils.reset_wifi(self.dut)
        wutils.wifi_toggle_state(self.dut, False)
        self.dut.ed.clear_all_events()
        self.clear_carrier_approved(str(self.dut.droid.telephonyGetSimCarrierId()))

    def teardown_class(self):
        self.dut.adb.shell(
            "pm enable com.google.android.apps.carrier.carrierwifi")
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    """Helper Functions"""
    def set_approved(self, approved):
        self.dut.log.debug("Setting suggestions from sl4a app "
                           + "approved" if approved else "not approved")
        self.dut.adb.shell("cmd wifi network-suggestions-set-user-approved"
                           + " " + SL4A_APK_NAME
                           + " " + ("yes" if approved else "no"))

    def is_approved(self):
        is_approved_str = self.dut.adb.shell(
            "cmd wifi network-suggestions-has-user-approved"
            + " " + SL4A_APK_NAME)
        return True if (is_approved_str == "yes") else False

    def set_carrier_approved(self, carrier_id, approved):
        self.dut.log.debug(("Setting IMSI protection exemption for carrier: " + carrier_id
                                + "approved" if approved else "not approved"))
        self.dut.adb.shell("cmd wifi imsi-protection-exemption-set-user-approved-for-carrier"
                           + " " + carrier_id
                           + " " + ("yes" if approved else "no"))

    def is_carrier_approved(self, carrier_id):
        is_approved_str = self.dut.adb.shell(
            "cmd wifi imsi-protection-exemption-has-user-approved-for-carrier"
            + " " + carrier_id)
        return True if (is_approved_str == "yes") else False

    def clear_carrier_approved(self, carrier_id):
        self.dut.adb.shell(
            "cmd wifi imsi-protection-exemption-clear-user-approved-for-carrier"
            + " " + carrier_id)

    def clear_user_disabled_networks(self):
        self.dut.log.debug("Clearing user disabled networks")
        self.dut.adb.shell(
            "cmd wifi clear-user-disabled-networks")

    def add_suggestions_and_ensure_connection(self, network_suggestions,
                                              expected_ssid,
                                              expect_post_connection_broadcast):
        if expect_post_connection_broadcast is not None:
            self.dut.droid.wifiStartTrackingNetworkSuggestionStateChange()

        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions(network_suggestions),
            "Failed to add suggestions")
        # Enable suggestions by the app.
        self.dut.log.debug("Enabling suggestions from test")
        self.set_approved(True)
        wutils.start_wifi_connection_scan_and_return_status(self.dut)
        # if suggestion is passpoint wait longer for connection.
        if "profile" in network_suggestions:
            time.sleep(PASSPOINT_TIMEOUT)
        wutils.wait_for_connect(self.dut, expected_ssid)

        if expect_post_connection_broadcast is None:
            return

        # Check if we expected to get the broadcast.
        try:
            event = self.dut.ed.pop_event(
                wifi_constants.WIFI_NETWORK_SUGGESTION_POST_CONNECTION, 60)
        except queue.Empty:
            if expect_post_connection_broadcast:
                raise signals.TestFailure(
                    "Did not receive post connection broadcast")
        else:
            if not expect_post_connection_broadcast:
                raise signals.TestFailure(
                    "Received post connection broadcast")
        finally:
            self.dut.droid.wifiStopTrackingNetworkSuggestionStateChange()
        self.dut.ed.clear_all_events()

    def remove_suggestions_disconnect_and_ensure_no_connection_back(self,
                                                                    network_suggestions,
                                                                    expected_ssid):
        # Remove suggestion trigger disconnect and wait for the disconnect.
        self.dut.log.info("Removing network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiRemoveNetworkSuggestions(network_suggestions),
            "Failed to remove suggestions")
        wutils.wait_for_disconnect(self.dut)
        self.dut.ed.clear_all_events()

        # Now ensure that we didn't connect back.
        asserts.assert_false(
            wutils.wait_for_connect(self.dut, expected_ssid, assert_on_fail=False),
            "Device should not connect back")

    def _test_connect_to_wifi_network_reboot_config_store(self,
                                                          network_suggestions,
                                                          wifi_network):
        """ Test network suggestion with reboot config store

        Args:
        1. network_suggestions: network suggestions in list to add to the device.
        2. wifi_network: expected wifi network to connect to
        """

        self.add_suggestions_and_ensure_connection(
            network_suggestions, wifi_network[WifiEnums.SSID_KEY], None)

        # Reboot and wait for connection back to the same suggestion.
        self.dut.reboot()
        time.sleep(DEFAULT_TIMEOUT)

        wutils.wait_for_connect(self.dut, wifi_network[WifiEnums.SSID_KEY])

        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            network_suggestions, wifi_network[WifiEnums.SSID_KEY])

        # Reboot with empty suggestion, verify user approval is kept.
        self.dut.reboot()
        time.sleep(DEFAULT_TIMEOUT)
        asserts.assert_true(self.is_approved(), "User approval should be kept")

    @test_tracker_info(uuid="bda8ed20-4382-4380-831a-64cf77eca108")
    def test_connect_to_wpa_psk_2g(self):
        """ Adds a network suggestion and ensure that the device connected.

        Steps:
        1. Send a network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        4. Remove the suggestions and ensure the device does not connect back.
        """
        self.add_suggestions_and_ensure_connection(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY],
            False)

        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="b2df6ebe-9c5b-4e84-906a-e76f96fcef56")
    def test_connect_to_wpa_psk_2g_with_screen_off(self):
        """ Adds a network suggestion and ensure that the device connected
        when the screen is off.

        Steps:
        1. Send an invalid suggestion to the device (Needed for PNO scan to start).
        2. Toggle screen off.
        3. Send a valid network suggestion to the device.
        4. Wait for the device to connect to it.
        5. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        6. Remove the suggestions and ensure the device does not connect back.
        """
        invalid_suggestion = self.wpa_psk_5g
        network_ssid = invalid_suggestion.pop(WifiEnums.SSID_KEY)
        invalid_suggestion[WifiEnums.SSID_KEY] = network_ssid + "blah"

        self.dut.log.info("Adding invalid suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([invalid_suggestion]),
            "Failed to add suggestions")

        # Approve suggestions by the app.
        self.set_approved(True)

        # Turn screen off to ensure PNO kicks-in.
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        time.sleep(10)

        # Add valid suggestions & ensure we restart PNO and connect to it.
        self.add_suggestions_and_ensure_connection(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY],
            False)

        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="f18bf994-ef3b-45d6-aba0-dd6338b07979")
    def test_connect_to_wpa_psk_2g_modify_meteredness(self):
        """ Adds a network suggestion and ensure that the device connected.
        Change the meteredness of the network after the connection.

        Steps:
        1. Send a network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        4. Mark the network suggestion metered.
        5. Ensure that the device disconnected and reconnected back to the
           suggestion.
        6. Mark the network suggestion unmetered.
        7. Ensure that the device did not disconnect.
        8. Remove the suggestions and ensure the device does not connect back.
        """
        self.add_suggestions_and_ensure_connection(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY],
            False)

        mod_suggestion = self.wpa_psk_2g

        # Mark the network metered.
        self.dut.log.debug("Marking suggestion as metered")
        mod_suggestion[WifiEnums.IS_SUGGESTION_METERED] = True
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([mod_suggestion]),
            "Failed to add suggestions")
        # Wait for disconnect.
        wutils.wait_for_disconnect(self.dut)
        self.dut.log.info("Disconnected from network %s", mod_suggestion)
        self.dut.ed.clear_all_events()
        # Wait for reconnect.
        wutils.wait_for_connect(self.dut, mod_suggestion[WifiEnums.SSID_KEY])

        # Mark the network unmetered.
        self.dut.log.debug("Marking suggestion as unmetered")
        mod_suggestion[WifiEnums.IS_SUGGESTION_METERED] = False
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([mod_suggestion]),
            "Failed to add suggestions")
        # Ensure there is no disconnect.
        wutils.ensure_no_disconnect(self.dut)
        self.dut.ed.clear_all_events()

        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [mod_suggestion], mod_suggestion[WifiEnums.SSID_KEY])


    @test_tracker_info(uuid="f54bc250-d9e9-4f00-8b5b-b866e8550b43")
    def test_connect_to_highest_priority(self):
        """
        Adds network suggestions and ensures that device connects to
        the suggestion with the highest priority.

        Steps:
        1. Send 2 network suggestions to the device (with different priorities).
        2. Wait for the device to connect to the network with the highest
           priority.
        3. In-place modify network suggestions with priorities reversed
        4. Restart wifi, wait for the device to connect to the network with the highest
           priority.
        5. Re-add the suggestions with the priorities reversed again.
        6. Again wait for the device to connect to the network with the highest
           priority.
        """
        network_suggestion_2g = self.wpa_psk_2g
        network_suggestion_5g = self.wpa_psk_5g

        # Add suggestions & wait for the connection event.
        network_suggestion_2g[WifiEnums.PRIORITY] = 5
        network_suggestion_5g[WifiEnums.PRIORITY] = 2
        self.add_suggestions_and_ensure_connection(
            [network_suggestion_2g, network_suggestion_5g],
            self.wpa_psk_2g[WifiEnums.SSID_KEY],
            None)

        # In-place modify Reverse the priority, should be no disconnect
        network_suggestion_2g[WifiEnums.PRIORITY] = 2
        network_suggestion_5g[WifiEnums.PRIORITY] = 5
        self.dut.log.info("Modifying network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([network_suggestion_2g,
                                                      network_suggestion_5g]),
            "Failed to add suggestions")
        wutils.ensure_no_disconnect(self.dut)

        # Disable and re-enable wifi, should connect to higher priority
        wutils.wifi_toggle_state(self.dut, False)
        time.sleep(DEFAULT_TIMEOUT)
        wutils.wifi_toggle_state(self.dut, True)
        wutils.start_wifi_connection_scan_and_return_status(self.dut)
        wutils.wait_for_connect(self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])

        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [], self.wpa_psk_5g[WifiEnums.SSID_KEY])

        # Reverse the priority.
        # Add suggestions & wait for the connection event.
        network_suggestion_2g[WifiEnums.PRIORITY] = 5
        network_suggestion_5g[WifiEnums.PRIORITY] = 2
        self.add_suggestions_and_ensure_connection(
            [network_suggestion_2g, network_suggestion_5g],
            self.wpa_psk_2g[WifiEnums.SSID_KEY],
            None)

    @test_tracker_info(uuid="b1d27eea-23c8-4c4f-b944-ef118e4cc35f")
    def test_connect_to_wpa_psk_2g_with_post_connection_broadcast(self):
        """ Adds a network suggestion and ensure that the device connected.

        Steps:
        1. Send a network suggestion to the device with
           isAppInteractionRequired set.
        2. Wait for the device to connect to it.
        3. Ensure that we did receive the post connection broadcast
           (isAppInteractionRequired = True).
        4. Remove the suggestions and ensure the device does not connect back.
        """
        network_suggestion = self.wpa_psk_2g
        network_suggestion[WifiEnums.IS_APP_INTERACTION_REQUIRED] = True
        self.add_suggestions_and_ensure_connection(
            [network_suggestion], self.wpa_psk_2g[WifiEnums.SSID_KEY],
            True)
        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [self.wpa_psk_2g], self.wpa_psk_2g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="a036a24d-29c0-456d-ae6a-afdde34da710")
    def test_connect_to_wpa_psk_5g_reboot_config_store(self):
        """
        Adds a network suggestion and ensure that the device connects to it
        after reboot.

        Steps:
        1. Send a network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        4. Reboot the device.
        5. Wait for the device to connect to back to it.
        6. Remove the suggestions and ensure the device does not connect back.
        7. Reboot the device again, ensure user approval is kept
        """
        self._test_connect_to_wifi_network_reboot_config_store(
            [self.wpa_psk_5g], self.wpa_psk_5g)

    @test_tracker_info(uuid="61649a2b-0f00-4272-9b9b-40ad5944da31")
    def test_connect_to_wpa_ent_config_aka_reboot_config_store(self):
        """
        Adds a network suggestion and ensure that the device connects to it
        after reboot.

        Steps:
        1. Send a Enterprise AKA network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast.
        4. Reboot the device.
        5. Wait for the device to connect to the wifi network.
        6. Remove suggestions and ensure device doesn't connect back to it.
        7. Reboot the device again, ensure user approval is kept
        """
        self.config_aka = {
            Ent.EAP: int(EAP.AKA),
            WifiEnums.SSID_KEY: self.ent_network_2g[WifiEnums.SSID_KEY],
            "carrierId": str(self.dut.droid.telephonyGetSimCarrierId()),
        }
        if "carrierId" in self.config_aka:
            self.set_carrier_approved(self.config_aka["carrierId"], True)
        self._test_connect_to_wifi_network_reboot_config_store(
            [self.config_aka], self.ent_network_2g)
        if "carrierId" in self.config_aka:
            self.clear_carrier_approved(self.config_aka["carrierId"])

    @test_tracker_info(uuid="98b2d40a-acb4-4a2f-aba1-b069e2a1d09d")
    def test_connect_to_wpa_ent_config_ttls_pap_reboot_config_store(self):
        """
        Adds a network suggestion and ensure that the device connects to it
        after reboot.

        Steps:
        1. Send a Enterprise TTLS PAP network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast.
        4. Reboot the device.
        5. Wait for the device to connect to the wifi network.
        6. Remove suggestions and ensure device doesn't connect back to it.
        7. Reboot the device again, ensure user approval is kept
        """
        self.config_ttls = {
            Ent.EAP: int(EAP.TTLS),
            Ent.CA_CERT: self.ca_cert,
            Ent.IDENTITY: self.eap_identity,
            Ent.PASSWORD: self.eap_password,
            Ent.PHASE2: int(EapPhase2.MSCHAPV2),
            WifiEnums.SSID_KEY: self.ent_network_2g[WifiEnums.SSID_KEY],
            Ent.DOM_SUFFIX_MATCH: self.domain_suffix_match,
        }
        config = dict(self.config_ttls)
        config[WifiEnums.Enterprise.PHASE2] = WifiEnums.EapPhase2.PAP.value

        self._test_connect_to_wifi_network_reboot_config_store(
            [config], self.ent_network_2g)

    @test_tracker_info(uuid="554b5861-22d0-4922-a5f4-712b4cf564eb")
    def test_fail_to_connect_to_wpa_psk_5g_when_not_approved(self):
        """
        Adds a network suggestion and ensure that the device does not
        connect to it until we approve the app.

        Steps:
        1. Send a network suggestion to the device with the app not approved.
        2. Ensure the network is present in scan results, but we don't connect
           to it.
        3. Now approve the app.
        4. Wait for the device to connect to it.
        """
        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([self.wpa_psk_5g]),
            "Failed to add suggestions")

        # Disable suggestions by the app.
        self.set_approved(False)

        # Ensure the app is not approved.
        asserts.assert_false(
            self.is_approved(),
            "Suggestions should be disabled")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])

        # Ensure we don't connect to the network.
        asserts.assert_false(
            wutils.wait_for_connect(
                self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY], assert_on_fail=False),
            "Should not connect to network suggestions from unapproved app")

        self.dut.log.info("Enabling suggestions from test")
        # Now Enable suggestions by the app & ensure we connect to the network.
        self.set_approved(True)

        # Ensure the app is approved.
        asserts.assert_true(
            self.is_approved(),
            "Suggestions should be enabled")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])

        wutils.wait_for_connect(self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="98400dea-776e-4a0a-9024-18845b27331c")
    def test_fail_to_connect_to_wpa_psk_2g_after_user_forgot_network(self):
        """
        Adds a network suggestion and ensures that the device does not
        connect to it after the user forgot the network previously.

        Steps:
        1. Send a network suggestion to the device with
           isAppInteractionRequired set.
        2. Wait for the device to connect to it.
        3. Ensure that we did receive the post connection broadcast
           (isAppInteractionRequired = True).
        4. Simulate user forgetting the network and the device does not
           connecting back even though the suggestion is active from the app.
        """
        network_suggestion = self.wpa_psk_2g
        network_suggestion[WifiEnums.IS_APP_INTERACTION_REQUIRED] = True
        self.add_suggestions_and_ensure_connection(
            [network_suggestion], self.wpa_psk_2g[WifiEnums.SSID_KEY],
            True)

        # Simulate user disconnect the network.
        self.dut.droid.wifiUserDisconnectNetwork(
            self.wpa_psk_2g[WifiEnums.SSID_KEY])
        wutils.wait_for_disconnect(self.dut)
        self.dut.log.info("Disconnected from network %s", self.wpa_psk_2g)
        self.dut.ed.clear_all_events()

        # Now ensure that we don't connect back even though the suggestion
        # is still active.
        asserts.assert_false(
            wutils.wait_for_connect(self.dut,
                                    self.wpa_psk_2g[WifiEnums.SSID_KEY],
                                    assert_on_fail=False),
            "Device should not connect back")

    @test_tracker_info(uuid="93c86b05-fa56-4d79-ad27-009a16f691b1")
    def test_connect_to_hidden_network(self):
        """
        Adds a network suggestion with hidden SSID config, ensure device can scan
        and connect to this network.

        Steps:
        1. Send a hidden network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        4. Remove the suggestions and ensure the device does not connect back.
        """
        asserts.skip_if(not hasattr(self, "hidden_networks"), "No hidden networks, skip this test")

        network_suggestion = self.hidden_network
        self.add_suggestions_and_ensure_connection(
            [network_suggestion], network_suggestion[WifiEnums.SSID_KEY], False)
        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [network_suggestion], network_suggestion[WifiEnums.SSID_KEY])

    @test_tracker_info(uuid="806dff14-7543-482b-bd0a-598de59374b3")
    @WifiBaseTest.wifi_test_wrap
    def test_connect_to_passpoint_network_with_post_connection_broadcast(self):
        """ Adds a passpoint network suggestion and ensure that the device connected.

        Steps:
        1. Send a network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did receive the post connection broadcast
               (isAppInteractionRequired = true).
        4. Remove the suggestions and ensure the device does not connect back.
        """
        asserts.skip_if(not hasattr(self, "passpoint_networks"),
                        "No passpoint networks, skip this test")
        passpoint_config = self.passpoint_network
        passpoint_config[WifiEnums.IS_APP_INTERACTION_REQUIRED] = True
        if "carrierId" in passpoint_config:
            self.set_carrier_approved(passpoint_config["carrierId"], True)
        self.add_suggestions_and_ensure_connection([passpoint_config],
                                                   passpoint_config[WifiEnums.SSID_KEY], True)
        self.remove_suggestions_disconnect_and_ensure_no_connection_back(
            [passpoint_config], passpoint_config[WifiEnums.SSID_KEY])
        if "carrierId" in passpoint_config:
            self.clear_carrier_approved(passpoint_config["carrierId"])

    @test_tracker_info(uuid="159b8b8c-fb00-4d4e-a29f-606881dcbf44")
    @WifiBaseTest.wifi_test_wrap
    def test_connect_to_passpoint_network_reboot_config_store(self):
        """
        Adds a passpoint network suggestion and ensure that the device connects to it
        after reboot.

        Steps:
        1. Send a network suggestion to the device.
        2. Wait for the device to connect to it.
        3. Ensure that we did not receive the post connection broadcast
           (isAppInteractionRequired = False).
        4. Reboot the device.
        5. Wait for the device to connect to back to it.
        6. Remove the suggestions and ensure the device does not connect back.
        7. Reboot the device again, ensure user approval is kept
        """
        asserts.skip_if(not hasattr(self, "passpoint_networks"),
                        "No passpoint networks, skip this test")
        passpoint_config = self.passpoint_network
        if "carrierId" in passpoint_config:
            self.set_carrier_approved(passpoint_config["carrierId"], True)
        self._test_connect_to_wifi_network_reboot_config_store([passpoint_config],
                                                               passpoint_config)
        if "carrierId" in passpoint_config:
            self.clear_carrier_approved(passpoint_config["carrierId"])

    @test_tracker_info(uuid="34f3d28a-bedf-43fe-a12d-2cfadf6bc6eb")
    @WifiBaseTest.wifi_test_wrap
    def test_fail_to_connect_to_passpoint_network_when_not_approved(self):
        """
        Adds a passpoint network suggestion and ensure that the device does not
        connect to it until we approve the app.

        Steps:
        1. Send a network suggestion to the device with the app not approved.
        2. Ensure the network is present in scan results, but we don't connect
           to it.
        3. Now approve the app.
        4. Wait for the device to connect to it.
        """
        asserts.skip_if(not hasattr(self, "passpoint_networks"),
                        "No passpoint networks, skip this test")
        passpoint_config = self.passpoint_network
        if "carrierId" in passpoint_config:
            self.set_carrier_approved(passpoint_config["carrierId"], True)
        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([passpoint_config]),
            "Failed to add suggestions")

        # Disable suggestions by the app.
        self.set_approved(False)

        # Ensure the app is not approved.
        asserts.assert_false(
            self.is_approved(),
            "Suggestions should be disabled")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, passpoint_config[WifiEnums.SSID_KEY])

        # Ensure we don't connect to the network.
        asserts.assert_false(
            wutils.wait_for_connect(
                self.dut, passpoint_config[WifiEnums.SSID_KEY], assert_on_fail=False),
            "Should not connect to network suggestions from unapproved app")

        self.dut.log.info("Enabling suggestions from test")
        # Now Enable suggestions by the app & ensure we connect to the network.
        self.set_approved(True)

        # Ensure the app is approved.
        asserts.assert_true(
            self.is_approved(),
            "Suggestions should be enabled")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, passpoint_config[WifiEnums.SSID_KEY])
        time.sleep(PASSPOINT_TIMEOUT)
        wutils.wait_for_connect(self.dut, passpoint_config[WifiEnums.SSID_KEY])
        if "carrierId" in passpoint_config:
            self.clear_carrier_approved(passpoint_config["carrierId"])

    @test_tracker_info(uuid="cf624cda-4d25-42f1-80eb-6c717fb08338")
    @WifiBaseTest.wifi_test_wrap
    def test_fail_to_connect_to_passpoint_network_when_imsi_protection_exemption_not_approved(self):
        """
        Adds a passpoint network suggestion using SIM credential without IMSI privacy protection.
        Before user approves the exemption, ensure that the device does noconnect to it until we
        approve the carrier exemption.

        Steps:
        1. Send a network suggestion to the device with IMSI protection exemption not approved.
        2. Ensure the network is present in scan results, but we don't connect
           to it.
        3. Now approve the carrier.
        4. Wait for the device to connect to it.
        """
        asserts.skip_if(not hasattr(self, "passpoint_networks"),
                        "No passpoint networks, skip this test")
        passpoint_config = self.passpoint_network
        asserts.skip_if("carrierId" not in passpoint_config,
                        "Not a SIM based passpoint network, skip this test")

        # Ensure the carrier is not approved.
        asserts.assert_false(
            self.is_carrier_approved(passpoint_config["carrierId"]),
            "Carrier shouldn't be approved")

        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([passpoint_config]),
            "Failed to add suggestions")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, passpoint_config[WifiEnums.SSID_KEY])

        # Ensure we don't connect to the network.
        asserts.assert_false(
            wutils.wait_for_connect(
                self.dut, passpoint_config[WifiEnums.SSID_KEY], assert_on_fail=False),
            "Should not connect to network suggestions from unapproved app")

        self.dut.log.info("Enabling suggestions from test")
        # Now approve IMSI protection exemption by carrier & ensure we connect to the network.
        self.set_carrier_approved(passpoint_config["carrierId"], True)

        # Ensure the carrier is approved.
        asserts.assert_true(
            self.is_carrier_approved(passpoint_config["carrierId"]),
            "Carrier should be approved")

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, passpoint_config[WifiEnums.SSID_KEY])
        time.sleep(PASSPOINT_TIMEOUT)
        wutils.wait_for_connect(self.dut, passpoint_config[WifiEnums.SSID_KEY])
        self.clear_carrier_approved(passpoint_config["carrierId"])

    @test_tracker_info(uuid="e35f99c8-78a4-4b96-9258-f9834b6ddd33")
    def test_initial_auto_join_on_network_suggestion(self):
        """
        Add a network suggestion with enableAutojoin bit set to false, ensure the device doesn't
        auto connect to this network

        Steps:
        1. Create a network suggestion.
        2. Set EnableAutojoin to false.
        3. Add this suggestion
        4. Ensure device doesn't connect to his network
        """
        network_suggestion = self.wpa_psk_5g
        # Set suggestion auto join initial to false.
        network_suggestion[AutoJoin] = False
        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([network_suggestion]),
            "Failed to add suggestions")
        # Enable suggestions by the app.
        self.dut.log.debug("Enabling suggestions from test")
        self.set_approved(True)
        wutils.start_wifi_connection_scan_and_return_status(self.dut)
        asserts.assert_false(
            wutils.wait_for_connect(self.dut, network_suggestion[WifiEnums.SSID_KEY],
                                    assert_on_fail=False), "Device should not connect.")

    @test_tracker_info(uuid="ff4e451f-a380-4ff5-a5c2-dd9b1633d5e5")
    def test_user_override_auto_join_on_network_suggestion(self):
        """
        Add a network suggestion, user change the auto join to false, ensure the device doesn't
        auto connect to this network

        Steps:
        1. Create a network suggestion.
        2. Add this suggestion, and ensure we connect to this network
        3. Simulate user change the auto join to false.
        4. Toggle the Wifi off and on
        4. Ensure device doesn't connect to his network
        """
        network_suggestion = self.wpa_psk_5g
        self.add_suggestions_and_ensure_connection([network_suggestion],
                                                   network_suggestion[WifiEnums.SSID_KEY], False)
        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        self.dut.log.info(wifi_info)
        network_id = wifi_info[WifiEnums.NETID_KEY]
        # Simulate user disable auto join through Settings.
        self.dut.log.info("Disable auto join on suggestion")
        self.dut.droid.wifiEnableAutojoin(network_id, False)
        wutils.wifi_toggle_state(self.dut, False)
        wutils.wifi_toggle_state(self.dut, True)
        asserts.assert_false(
            wutils.wait_for_connect(self.dut, network_suggestion[WifiEnums.SSID_KEY],
                                    assert_on_fail=False), "Device should not connect.")

    @test_tracker_info(uuid="32201b1c-76a0-46dc-9983-2cd24312a783")
    def test_untrusted_suggestion_without_untrusted_request(self):
        """
        Add an untrusted network suggestion, when no untrusted request, will not connect to it.
        Steps:
        1. Create a untrusted network suggestion.
        2. Add this suggestion, and ensure device do not connect to this network
        3. Request untrusted network and ensure device connect to this network
        """
        network_suggestion = self.open_5g
        network_suggestion[Untrusted] = True
        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions([network_suggestion]),
            "Failed to add suggestions")
        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, network_suggestion[WifiEnums.SSID_KEY])

        # Ensure we don't connect to the network.
        asserts.assert_false(
            wutils.wait_for_connect(
                self.dut, network_suggestion[WifiEnums.SSID_KEY], assert_on_fail=False),
            "Should not connect to untrusted network suggestions with no request")
        network_request = {ClearCapabilities: True, TransportType: 1}
        req_key = self.dut.droid.connectivityRequestNetwork(network_request)

        # Start a new scan to trigger auto-join.
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, network_suggestion[WifiEnums.SSID_KEY])

        wutils.wait_for_connect(
            self.dut, network_suggestion[WifiEnums.SSID_KEY], assert_on_fail=False)

        self.dut.droid.connectivityUnregisterNetworkCallback(req_key)

