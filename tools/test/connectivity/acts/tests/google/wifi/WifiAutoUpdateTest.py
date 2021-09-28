#   !/usr/bin/env python3.4
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

import re
from acts import asserts
from acts.controllers.android_device import SL4A_APK_NAME
from acts.libs.ota import ota_updater
import acts.signals as signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_5G
import acts.test_utils.wifi.wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
import acts.utils as utils

WifiEnums = wutils.WifiEnums
SSID = WifiEnums.SSID_KEY
PWD = WifiEnums.PWD_KEY
NETID = WifiEnums.NETID_KEY
# Default timeout used for reboot, toggle WiFi and Airplane mode,
# for the system to settle down after the operation.
DEFAULT_TIMEOUT = 10
BAND_2GHZ = 0
BAND_5GHZ = 1


class WifiAutoUpdateTest(WifiBaseTest):
    """Tests for APIs in Android's WifiManager class.

    Test Bed Requirement:
    * One Android device
    * Several Wi-Fi networks visible to the device, including an open Wi-Fi
      network.
    """

    def __init__(self, controllers):
        WifiBaseTest.__init__(self, controllers)
        self.tests = (
            "test_check_wifi_state_after_au",
            "test_verify_networks_after_au",
            "test_configstore_after_au",
            "test_mac_randomization_after_au",
            "test_wifi_hotspot_5g_psk_after_au",
            "test_all_networks_connectable_after_au",
            "test_connect_to_network_suggestion_after_au",
            "test_check_wifi_toggling_after_au",
            "test_connection_to_new_networks",
            "test_reset_wifi_after_au")

    def setup_class(self):
        super(WifiAutoUpdateTest, self).setup_class()
        ota_updater.initialize(self.user_params, self.android_devices)
        self.dut = self.android_devices[0]
        self.dut_client = self.android_devices[1]
        wutils.wifi_test_device_init(self.dut)
        wutils.wifi_toggle_state(self.dut, True)

        # configure APs
        self.legacy_configure_ap_and_start(wpa_network=True)
        self.wpapsk_2g = self.reference_networks[0]["2g"]
        self.wpapsk_5g = self.reference_networks[0]["5g"]
        self.open_2g = self.open_network[0]["2g"]
        self.open_5g = self.open_network[0]["5g"]

        # saved & connected networks, network suggestions
        # and new networks
        self.saved_networks = [self.open_2g]
        self.network_suggestions = [self.wpapsk_5g]
        self.connected_networks = [self.wpapsk_2g, self.open_5g]
        self.new_networks = [self.reference_networks[1]["2g"],
                             self.open_network[1]["5g"]]

        # add pre ota upgrade configuration
        self.wifi_config_list = []
        self.pre_default_mac = {}
        self.pre_random_mac = {}
        self.pst_default_mac = {}
        self.pst_random_mac = {}
        self.add_pre_update_configuration()

        # Run OTA below, if ota fails then abort all tests.
        try:
            ota_updater.update(self.dut)
        except Exception as e:
            raise signals.TestAbortClass(
                "Failed up apply OTA update. Aborting tests: %s" % e)

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()

    def teardown_class(self):
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    ### Helper Methods

    def add_pre_update_configuration(self):
        self.add_network_suggestions(self.network_suggestions)
        self.add_network_and_enable(self.saved_networks[0])
        self.add_wifi_hotspot()
        self.connect_to_multiple_networks(self.connected_networks)

    def add_wifi_hotspot(self):
        self.wifi_hotspot = {"SSID": "hotspot_%s" % utils.rand_ascii_str(6),
                             "password": "pass_%s" % utils.rand_ascii_str(6)}
        band = WIFI_CONFIG_APBAND_5G
        if self.dut.build_info["build_id"].startswith("R"):
            band = WifiEnums.WIFI_CONFIG_SOFTAP_BAND_5G
            self.wifi_hotspot[WifiEnums.AP_BAND_KEY] = band
            asserts.assert_true(
                self.dut.droid.wifiSetWifiApConfiguration(self.wifi_hotspot),
                "Failed to set WifiAp Configuration")
            wifi_ap = self.dut.droid.wifiGetApConfiguration()
            asserts.assert_true(
                wifi_ap[WifiEnums.SSID_KEY] == self.wifi_hotspot[WifiEnums.SSID_KEY],
                "Hotspot SSID doesn't match with expected SSID")
            return
        elif self.dut.build_info["build_id"].startswith("Q"):
            band = WifiEnums.WIFI_CONFIG_APBAND_5G_OLD
            self.wifi_hotspot[WifiEnums.AP_BAND_KEY] = band
            asserts.assert_true(
                self.dut.droid.wifiSetWifiApConfiguration(self.wifi_hotspot),
                "Failed to set WifiAp Configuration")
            wifi_ap = self.dut.droid.wifiGetApConfiguration()
            asserts.assert_true(
                wifi_ap[WifiEnums.SSID_KEY] == self.wifi_hotspot[WifiEnums.SSID_KEY],
                "Hotspot SSID doesn't match with expected SSID")
            return
        wutils.save_wifi_soft_ap_config(self.dut, self.wifi_hotspot, band)

    def verify_wifi_hotspot(self):
        """Verify wifi tethering."""
        wutils.start_wifi_tethering_saved_config(self.dut)
        wutils.connect_to_wifi_network(self.dut_client,
                                       self.wifi_hotspot,
                                       check_connectivity=False)
        wutils.stop_wifi_tethering(self.dut)

    def connect_to_multiple_networks(self, networks):
        """Connect to a list of wifi networks.

        Args:
            networks : list of wifi networks.
        """
        self.log.info("Connect to multiple wifi networks")
        for network in networks:
            ssid = network[SSID]
            wutils.start_wifi_connection_scan_and_ensure_network_found(
                self.dut, ssid)
            wutils.wifi_connect(self.dut, network, num_of_tries=6)
            self.wifi_config_list.append(network)
            self.pre_default_mac[network[SSID]] = self.get_sta_mac_address()
            self.pre_random_mac[network[SSID]] = \
                self.dut.droid.wifigetRandomizedMacAddress(network)

    def get_sta_mac_address(self):
        """Gets the current MAC address being used for client mode."""
        out = self.dut.adb.shell("ifconfig wlan0")
        res = re.match(".* HWaddr (\S+).*", out, re.S)
        return res.group(1)

    def add_network_suggestions(self, network_suggestions):
        """Add wifi network suggestions to DUT.

        Args:
            network_suggestions : suggestions to add.
        """
        self.dut.log.info("Adding network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiAddNetworkSuggestions(network_suggestions),
            "Failed to add suggestions")

        # Enable suggestions by the app.
        self.dut.log.debug("Enabling suggestions from test")
        self.dut.adb.shell(
            "cmd wifi network-suggestions-set-user-approved %s yes" % \
                SL4A_APK_NAME)

    def remove_suggestions_and_ensure_no_connection(self,
                                                    network_suggestions,
                                                    expected_ssid):
        """Remove network suggestions.

        Args:
            network_suggestions : suggestions to remove.
            expected_ssid : SSID to verify that DUT is not connected.
        """
        # remove network suggestion and verify disconnect
        self.dut.log.info("Removing network suggestions")
        asserts.assert_true(
            self.dut.droid.wifiRemoveNetworkSuggestions(network_suggestions),
            "Failed to remove suggestions")

        wutils.wait_for_disconnect(self.dut)
        self.dut.ed.clear_all_events()

        # Now ensure that we didn't connect back.
        asserts.assert_false(
            wutils.wait_for_connect(self.dut,
                                    expected_ssid,
                                    assert_on_fail=False),
            "Device should not connect back")

    def add_network_and_enable(self, network):
        """Add a network and enable it.

        Args:
            network : Network details for the network to be added.
        """
        self.log.info("Add a wifi network and enable it")
        ret = self.dut.droid.wifiAddNetwork(network)
        asserts.assert_true(ret != -1, "Add network %r failed" % network)
        self.wifi_config_list.append({SSID: network[SSID], NETID: ret})
        self.dut.droid.wifiEnableNetwork(ret, 0)

    def check_networks_after_autoupdate(self, networks):
        """Verify that all previously configured networks are persistent.

        Args:
            networks: List of network dicts.
        """
        network_info = self.dut.droid.wifiGetConfiguredNetworks()
        if len(network_info) != len(networks):
            msg = (
                "Number of configured networks before and after Auto-update "
                "don't match. \nBefore reboot = %s \n After reboot = %s" %
                (networks, network_info))
            raise signals.TestFailure(msg)

        # For each network, check if it exists in configured list after Auto-
        # update.
        for network in networks:
            exists = wutils.match_networks({SSID: network[SSID]}, network_info)
            if not exists:
                raise signals.TestFailure("%s network is not present in the"
                                          " configured list after Auto-update" %
                                          network[SSID])
            # Get the new network id for each network after reboot.
            network[NETID] = exists[0]["networkId"]

    def get_enabled_network(self, network1, network2):
        """Check network status and return currently unconnected network.

        Args:
            network1: dict representing a network.
            network2: dict representing a network.

        Returns:
            Network dict of the unconnected network.
        """
        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        enabled = network1
        if wifi_info[SSID] == network1[SSID]:
            enabled = network2
        return enabled

    ### Tests

    @test_tracker_info(uuid="9ff1f01e-e5ff-408b-9a95-29e87a2df2d8")
    @WifiBaseTest.wifi_test_wrap
    def test_check_wifi_state_after_au(self):
        """Check if the state of WiFi is enabled after Auto-update."""
        if not self.dut.droid.wifiCheckState():
            raise signals.TestFailure("WiFi is disabled after Auto-update!!!")

    @test_tracker_info(uuid="e3ebdbba-71dd-4281-aef8-5b3d42b88770")
    @WifiBaseTest.wifi_test_wrap
    def test_verify_networks_after_au(self):
        """Check if the previously added networks are intact.

           Steps:
               Number of networs should be the same and match each network.

        """
        self.check_networks_after_autoupdate(self.wifi_config_list)

    @test_tracker_info(uuid="799e83c2-305d-4510-921e-dac3c0dbb6c5")
    @WifiBaseTest.wifi_test_wrap
    def test_configstore_after_au(self):
        """Verify DUT automatically connects to wifi networks after ota.

           Steps:
               1. Connect to two wifi networks pre ota.
               2. Verify DUT automatically connects to 1 after ota.
               3. Re-connect to the other wifi network.
        """
        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        self.pst_default_mac[wifi_info[SSID]] = self.get_sta_mac_address()
        self.pst_random_mac[wifi_info[SSID]] = \
            self.dut.droid.wifigetRandomizedMacAddress(wifi_info)
        reconnect_to = self.get_enabled_network(self.wifi_config_list[1],
                                                self.wifi_config_list[2])
        wutils.start_wifi_connection_scan_and_ensure_network_found(
            self.dut, reconnect_to[SSID])
        wutils.wifi_connect_by_id(self.dut, reconnect_to[NETID])
        connect_data = self.dut.droid.wifiGetConnectionInfo()
        connect_ssid = connect_data[SSID]
        self.log.info("Expected SSID = %s" % reconnect_to[SSID])
        self.log.info("Connected SSID = %s" % connect_ssid)
        if connect_ssid != reconnect_to[SSID]:
            raise signals.TestFailure(
                "Device failed to reconnect to the correct"
                " network after reboot.")
        self.pst_default_mac[wifi_info[SSID]] = self.get_sta_mac_address()
        self.pst_random_mac[wifi_info[SSID]] = \
            self.dut.droid.wifigetRandomizedMacAddress(wifi_info)

        for network in self.connected_networks:
            wutils.wifi_forget_network(self.dut, network[SSID])

    @test_tracker_info(uuid="e26d0ed9-9457-4a95-a962-4d43b0032bac")
    @WifiBaseTest.wifi_test_wrap
    def test_mac_randomization_after_au(self):
        """Verify randomized MAC addrs are persistent after ota.

           Steps:
               1. Reconnect to the wifi networks configured pre ota.
               2. Get the randomized MAC addrs.
        """
        for ssid, mac in self.pst_random_mac.items():
            asserts.assert_true(
                self.pre_random_mac[ssid] == mac,
                "MAC addr of %s is %s after ota. Expected %s" %
                (ssid, mac, self.pre_random_mac[ssid]))

    @test_tracker_info(uuid="f68a65e6-97b7-4746-bad8-4c206551d87e")
    @WifiBaseTest.wifi_test_wrap
    def test_wifi_hotspot_5g_psk_after_au(self):
        """Verify hotspot after ota upgrade.

           Steps:
               1. Start wifi hotspot on the saved config.
               2. Verify DUT client connects to it.
        """
        self.verify_wifi_hotspot()

    @test_tracker_info(uuid="21f91372-88a6-44b9-a4e8-d4664823dffb")
    @WifiBaseTest.wifi_test_wrap
    def test_connect_to_network_suggestion_after_au(self):
        """Verify connection to network suggestion after ota.

           Steps:
               1. DUT has network suggestion added before OTA.
               2. Wait for the device to connect to it.
               3. Remove the suggestions and ensure the device does not
                  connect back.
        """
        wutils.reset_wifi(self.dut)
        wutils.start_wifi_connection_scan_and_return_status(self.dut)
        wutils.wait_for_connect(self.dut, self.network_suggestions[0][SSID])
        self.remove_suggestions_and_ensure_no_connection(
            self.network_suggestions, self.network_suggestions[0][SSID])

    @test_tracker_info(uuid="b8e47a4f-62fe-4a0e-b999-27ae1ebf4d19")
    @WifiBaseTest.wifi_test_wrap
    def test_connection_to_new_networks(self):
        """Check if we can connect to new networks after Auto-update.

           Steps:
               1. Connect to a PSK network.
               2. Connect to an open network.
               3. Forget ntworks added in 1 & 2.
               TODO: (@bmahadev) Add WEP network once it's ready.
        """
        for network in self.new_networks:
            wutils.connect_to_wifi_network(self.dut, network)
        for network in self.new_networks:
            wutils.wifi_forget_network(self.dut, network[SSID])

    @test_tracker_info(uuid="1d8309e4-d5a2-4f48-ba3b-895a58c9bf3a")
    @WifiBaseTest.wifi_test_wrap
    def test_all_networks_connectable_after_au(self):
        """Check if previously added networks are connectable.

           Steps:
               1. Connect to previously added PSK network using network id.
               2. Connect to previously added open network using network id.
               TODO: (@bmahadev) Add WEP network once it's ready.
        """
        network = self.wifi_config_list[0]
        if not wutils.connect_to_wifi_network_with_id(self.dut,
                                                      network[NETID],
                                                      network[SSID]):
            raise signals.TestFailure("Failed to connect to %s after OTA" %
                                      network[SSID])
        wutils.wifi_forget_network(self.dut, network[SSID])

    @test_tracker_info(uuid="05671859-38b1-4dbf-930c-18048971d075")
    @WifiBaseTest.wifi_test_wrap
    def test_check_wifi_toggling_after_au(self):
        """Check if WiFi can be toggled ON/OFF after auto-update."""
        self.log.debug("Going from on to off.")
        wutils.wifi_toggle_state(self.dut, False)
        self.log.debug("Going from off to on.")
        wutils.wifi_toggle_state(self.dut, True)

    @test_tracker_info(uuid="440edf32-4b00-42b0-9811-9f2bc4a83efb")
    @WifiBaseTest.wifi_test_wrap
    def test_reset_wifi_after_au(self):
        """"Check if WiFi can be reset after auto-update."""
        wutils.reset_wifi(self.dut)
