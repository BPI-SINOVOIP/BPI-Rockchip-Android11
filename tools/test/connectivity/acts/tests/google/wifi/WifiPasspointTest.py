#!/usr/bin/env python3.4
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

import itertools
import pprint
import queue
import time

from acts.test_utils.net import ui_utils as uutils
import acts.test_utils.wifi.wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest


from acts import asserts
from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.utils import force_airplane_mode

WifiEnums = wutils.WifiEnums

DEFAULT_TIMEOUT = 10
OSU_TEST_TIMEOUT = 300

# Constants for providers.
GLOBAL_RE = 0
OSU_BOINGO = 0
BOINGO = 1
ATT = 2

# Constants used for various device operations.
RESET = 1
TOGGLE = 2

UNKNOWN_FQDN = "@#@@!00fffffx"

# Constants for Boingo UI automator
EDIT_TEXT_CLASS_NAME = "android.widget.EditText"
PASSWORD_TEXT = "Password"
PASSPOINT_BUTTON = "Get Passpoint"
BOINGO_UI_TEXT = "Online Sign Up"

class WifiPasspointTest(WifiBaseTest):
    """Tests for APIs in Android's WifiManager class.

    Test Bed Requirement:
    * One Android device
    * Several Wi-Fi networks visible to the device, including an open Wi-Fi
      network.
    """

    def setup_class(self):
        super().setup_class()
        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        req_params = ["passpoint_networks",
                      "boingo_username",
                      "boingo_password",]
        self.unpack_userparams(req_param_names=req_params,)
        asserts.assert_true(
            len(self.passpoint_networks) > 0,
            "Need at least one Passpoint network.")
        wutils.wifi_toggle_state(self.dut, True)
        self.unknown_fqdn = UNKNOWN_FQDN


    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        self.dut.unlock_screen()


    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        passpoint_configs = self.dut.droid.getPasspointConfigs()
        for config in passpoint_configs:
            wutils.delete_passpoint(self.dut, config)
        wutils.reset_wifi(self.dut)

    """Helper Functions"""


    def install_passpoint_profile(self, passpoint_config):
        """Install the Passpoint network Profile.

        Args:
            passpoint_config: A JSON dict of the Passpoint configuration.

        """
        asserts.assert_true(WifiEnums.SSID_KEY in passpoint_config,
                "Key '%s' must be present in network definition." %
                WifiEnums.SSID_KEY)
        # Install the Passpoint profile.
        self.dut.droid.addUpdatePasspointConfig(passpoint_config)


    def check_passpoint_connection(self, passpoint_network):
        """Verify the device is automatically able to connect to the Passpoint
           network.

           Args:
               passpoint_network: SSID of the Passpoint network.

        """
        ad = self.dut
        ad.ed.clear_all_events()
        try:
            wutils.start_wifi_connection_scan_and_return_status(ad)
            wutils.wait_for_connect(ad)
        except:
            pass
        # Re-verify we are connected to the correct network.
        network_info = self.dut.droid.wifiGetConnectionInfo()
        self.log.info("Network Info: %s" % network_info)
        if not network_info or not network_info[WifiEnums.SSID_KEY] or \
            network_info[WifiEnums.SSID_KEY] not in passpoint_network:
              raise signals.TestFailure(
                  "Device did not connect to passpoint network.")


    def get_configured_passpoint_and_delete(self):
        """Get configured Passpoint network and delete using its FQDN."""
        passpoint_config = self.dut.droid.getPasspointConfigs()
        if not len(passpoint_config):
            raise signals.TestFailure("Failed to fetch the list of configured"
                                      "passpoint networks.")
        if not wutils.delete_passpoint(self.dut, passpoint_config[0]):
            raise signals.TestFailure("Failed to delete Passpoint configuration"
                                      " with FQDN = %s" % passpoint_config[0])

    def ui_automator_boingo(self):
        """Run UI automator for boingo passpoint."""
        # Verify the boingo login page shows
        asserts.assert_true(
            uutils.has_element(self.dut, text=BOINGO_UI_TEXT),
            "Failed to launch boingohotspot login page")

        # Go to the bottom of the page
        for _ in range(3):
            self.dut.adb.shell("input swipe 300 900 300 300")

        # Enter username
        uutils.wait_and_input_text(self.dut,
                                   input_text=self.boingo_username,
                                   text="",
                                   class_name=EDIT_TEXT_CLASS_NAME)
        self.dut.adb.shell("input keyevent 111")  # collapse keyboard
        self.dut.adb.shell("input swipe 300 900 300 750")  # swipe up to show text

        # Enter password
        uutils.wait_and_input_text(self.dut,
                                   input_text=self.boingo_password,
                                   text=PASSWORD_TEXT)
        self.dut.adb.shell("input keyevent 111")  # collapse keyboard
        self.dut.adb.shell("input swipe 300 900 300 750")  # swipe up to show text

        # Login
        uutils.wait_and_click(self.dut, text=PASSPOINT_BUTTON)
        time.sleep(DEFAULT_TIMEOUT)

    def start_subscription_provisioning(self, state):
        """Start subscription provisioning with a default provider."""

        self.unpack_userparams(('osu_configs',))
        asserts.assert_true(
            len(self.osu_configs) > 0,
            "Need at least one osu config.")
        osu_config = self.osu_configs[OSU_BOINGO]
        # Clear all previous events.
        self.dut.ed.clear_all_events()
        self.dut.droid.startSubscriptionProvisioning(osu_config)
        start_time = time.time()
        while time.time() < start_time + OSU_TEST_TIMEOUT:
            dut_event = self.dut.ed.pop_event("onProvisioningCallback",
                                              DEFAULT_TIMEOUT * 18)
            if dut_event['data']['tag'] == 'success':
                self.log.info("Passpoint Provisioning Success")
                # Reset WiFi after provisioning success.
                if state == RESET:
                    wutils.reset_wifi(self.dut)
                    time.sleep(DEFAULT_TIMEOUT)
                # Toggle WiFi after provisioning success.
                elif state == TOGGLE:
                    wutils.toggle_wifi_off_and_on(self.dut)
                    time.sleep(DEFAULT_TIMEOUT)
                break
            if dut_event['data']['tag'] == 'failure':
                raise signals.TestFailure(
                    "Passpoint Provisioning is failed with %s" %
                    dut_event['data'][
                        'reason'])
                break
            if dut_event['data']['tag'] == 'status':
                self.log.info(
                    "Passpoint Provisioning status %s" % dut_event['data'][
                        'status'])
                if int(dut_event['data']['status']) == 7:
                    self.ui_automator_boingo()
        # Clear all previous events.
        self.dut.ed.clear_all_events()

        # Verify device connects to the Passpoint network.
        time.sleep(DEFAULT_TIMEOUT)
        current_passpoint = self.dut.droid.wifiGetConnectionInfo()
        if current_passpoint[WifiEnums.SSID_KEY] not in osu_config[
            "expected_ssids"]:
            raise signals.TestFailure("Device did not connect to the %s"
                                      " passpoint network" % osu_config[
                                          "expected_ssids"])
        # Delete the Passpoint profile.
        self.get_configured_passpoint_and_delete()
        wutils.wait_for_disconnect(self.dut)


    """Tests"""

    @test_tracker_info(uuid="b0bc0153-77bb-4594-8f19-cea2c6bd2f43")
    def test_add_passpoint_network(self):
        """Add a Passpoint network and verify device connects to it.

        Steps:
            1. Install a Passpoint Profile.
            2. Verify the device connects to the required Passpoint SSID.
            3. Get the Passpoint configuration added above.
            4. Delete Passpoint configuration using its FQDN.
            5. Verify that we are disconnected from the Passpoint network.

        """
        passpoint_config = self.passpoint_networks[BOINGO]
        self.install_passpoint_profile(passpoint_config)
        ssid = passpoint_config[WifiEnums.SSID_KEY]
        self.check_passpoint_connection(ssid)
        self.get_configured_passpoint_and_delete()
        wutils.wait_for_disconnect(self.dut)


    @test_tracker_info(uuid="eb29d6e2-a755-4c9c-9e4e-63ea2277a64a")
    def test_update_passpoint_network(self):
        """Update a previous Passpoint network and verify device still connects
           to it.

        1. Install a Passpoint Profile.
        2. Verify the device connects to the required Passpoint SSID.
        3. Update the Passpoint Profile.
        4. Verify device is still connected to the Passpoint SSID.
        5. Get the Passpoint configuration added above.
        6. Delete Passpoint configuration using its FQDN.

        """
        passpoint_config = self.passpoint_networks[BOINGO]
        self.install_passpoint_profile(passpoint_config)
        ssid = passpoint_config[WifiEnums.SSID_KEY]
        self.check_passpoint_connection(ssid)

        # Update passpoint configuration using the original profile because we
        # do not have real profile with updated credentials to use.
        self.install_passpoint_profile(passpoint_config)

        # Wait for a Disconnect event from the supplicant.
        wutils.wait_for_disconnect(self.dut)

        # Now check if we are again connected with the updated profile.
        self.check_passpoint_connection(ssid)

        self.get_configured_passpoint_and_delete()
        wutils.wait_for_disconnect(self.dut)


    @test_tracker_info(uuid="b6e8068d-faa1-49f2-b421-c60defaed5f0")
    def test_add_delete_list_of_passpoint_network(self):
        """Add multiple passpoint networks, list them and delete one by one.

        1. Install Passpoint Profile A.
        2. Install Passpoint Profile B.
        3. Get all the Passpoint configurations added above and verify.
        6. Ensure all Passpoint configurations can be deleted.

        """
        for passpoint_config in self.passpoint_networks[:2]:
            self.install_passpoint_profile(passpoint_config)
            time.sleep(DEFAULT_TIMEOUT)
        configs = self.dut.droid.getPasspointConfigs()
        #  It is length -1 because ATT profile will be handled separately
        if not len(configs) or len(configs) != len(self.passpoint_networks[:2]):
            raise signals.TestFailure("Failed to fetch some or all of the"
                                      " configured passpoint networks.")
        for config in configs:
            if not wutils.delete_passpoint(self.dut, config):
                raise signals.TestFailure("Failed to delete Passpoint"
                                          " configuration with FQDN = %s" %
                                          config)


    @test_tracker_info(uuid="a53251be-7aaf-41fc-a5f3-63984269d224")
    def test_delete_unknown_fqdn(self):
        """Negative test to delete Passpoint profile using an unknown FQDN.

        1. Pass an unknown FQDN for removal.
        2. Verify that it was not successful.

        """
        if wutils.delete_passpoint(self.dut, self.unknown_fqdn):
            raise signals.TestFailure("Failed because an unknown FQDN"
                                      " was successfully deleted.")


    @test_tracker_info(uuid="bf03c03a-e649-4e2b-a557-1f791bd98951")
    def test_passpoint_failover(self):
        """Add a pair of passpoint networks and test failover when one of the"
           profiles is removed.

        1. Install a Passpoint Profile A and B.
        2. Verify device connects to a Passpoint network and get SSID.
        3. Delete the current Passpoint profile using its FQDN.
        4. Verify device fails over and connects to the other Passpoint SSID.
        5. Delete Passpoint configuration using its FQDN.

        """
        # Install both Passpoint profiles on the device.
        passpoint_ssid = list()
        for passpoint_config in self.passpoint_networks[:2]:
            passpoint_ssid.extend(passpoint_config[WifiEnums.SSID_KEY])
            self.install_passpoint_profile(passpoint_config)
            time.sleep(DEFAULT_TIMEOUT)

        # Get the current network and the failover network.
        wutils.wait_for_connect(self.dut)
        current_passpoint = self.dut.droid.wifiGetConnectionInfo()
        current_ssid = current_passpoint[WifiEnums.SSID_KEY]
        if current_ssid not in passpoint_ssid:
           raise signals.TestFailure("Device did not connect to any of the "
                                     "configured Passpoint networks.")

        expected_ssid =  self.passpoint_networks[0][WifiEnums.SSID_KEY]
        if current_ssid in expected_ssid:
            expected_ssid = self.passpoint_networks[1][WifiEnums.SSID_KEY]

        # Remove the current Passpoint profile.
        for network in self.passpoint_networks[:2]:
            if current_ssid in network[WifiEnums.SSID_KEY]:
                if not wutils.delete_passpoint(self.dut, network["fqdn"]):
                    raise signals.TestFailure("Failed to delete Passpoint"
                                              " configuration with FQDN = %s" %
                                              network["fqdn"])
        # Verify device fails over and connects to the other passpoint network.
        time.sleep(DEFAULT_TIMEOUT)

        current_passpoint = self.dut.droid.wifiGetConnectionInfo()
        if current_passpoint[WifiEnums.SSID_KEY] not in expected_ssid:
            raise signals.TestFailure("Device did not failover to the %s"
                                      " passpoint network" % expected_ssid)

        # Delete the remaining Passpoint profile.
        self.get_configured_passpoint_and_delete()
        wutils.wait_for_disconnect(self.dut)


    @test_tracker_info(uuid="37ae0223-0cb7-43f3-8ba8-474fad6e4b71")
    def test_install_att_passpoint_profile(self):
        """Add an AT&T Passpoint profile.

        It is used for only installing the profile for other tests.
        """
        isFound = False
        for passpoint_config in self.passpoint_networks:
            if 'att' in passpoint_config['fqdn']:
                isFound = True
                self.install_passpoint_profile(passpoint_config)
                break
        if not isFound:
            raise signals.TestFailure("cannot find ATT profile.")


    @test_tracker_info(uuid="e3e826d2-7c39-4c37-ab3f-81992d5aa0e8")
    @WifiBaseTest.wifi_test_wrap
    def test_att_passpoint_network(self):
        """Add a AT&T Passpoint network and verify device connects to it.

        Steps:
            1. Install a AT&T Passpoint Profile.
            2. Verify the device connects to the required Passpoint SSID.
            3. Get the Passpoint configuration added above.
            4. Delete Passpoint configuration using its FQDN.
            5. Verify that we are disconnected from the Passpoint network.

        """
        carriers = ["att"]
        operator = get_operator_name(self.log, self.dut)
        asserts.skip_if(operator not in carriers,
                        "Device %s does not have a ATT sim" % self.dut.model)

        passpoint_config = self.passpoint_networks[ATT]
        self.install_passpoint_profile(passpoint_config)
        ssid = passpoint_config[WifiEnums.SSID_KEY]
        self.check_passpoint_connection(ssid)
        self.get_configured_passpoint_and_delete()
        wutils.wait_for_disconnect(self.dut)


    @test_tracker_info(uuid="c85c81b2-7133-4635-8328-9498169ae802")
    def test_start_subscription_provisioning(self):
        self.start_subscription_provisioning(0)


    @test_tracker_info(uuid="fd09a643-0d4b-45a9-881a-a771f9707ab1")
    def test_start_subscription_provisioning_and_reset_wifi(self):
        self.start_subscription_provisioning(RESET)


    @test_tracker_info(uuid="f43ea759-673f-4567-aa11-da3bc2cabf08")
    def test_start_subscription_provisioning_and_toggle_wifi(self):
        self.start_subscription_provisioning(TOGGLE)

    @test_tracker_info(uuid="ad6d5eb8-a3c5-4ce0-9e10-d0f201cd0f40")
    def test_user_override_auto_join_on_passpoint_network(self):
        """Add a Passpoint network, simulate user change the auto join to false, ensure the device
        doesn't auto connect to this passponit network

        Steps:
            1. Install a Passpoint Profile.
            2. Verify the device connects to the required Passpoint SSID.
            3. Disable auto join Passpoint configuration using its FQDN.
            4. disable and enable Wifi toggle, ensure we don't connect back
        """
        passpoint_config = self.passpoint_networks[BOINGO]
        self.install_passpoint_profile(passpoint_config)
        ssid = passpoint_config[WifiEnums.SSID_KEY]
        self.check_passpoint_connection(ssid)
        self.dut.log.info("Disable auto join on passpoint")
        self.dut.droid.wifiEnableAutojoinPasspoint(passpoint_config['fqdn'], False)
        wutils.wifi_toggle_state(self.dut, False)
        wutils.wifi_toggle_state(self.dut, True)
        asserts.assert_false(
            wutils.wait_for_connect(self.dut, ssid, assert_on_fail=False),
            "Device should not connect.")
