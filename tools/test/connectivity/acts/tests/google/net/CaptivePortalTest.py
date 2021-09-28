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

import time

from acts import asserts
from acts import base_test
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const as cconst
from acts.test_utils.net import connectivity_test_utils as cutils
from acts.test_utils.net import ui_utils as uutils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

WifiEnums = wutils.WifiEnums
IFACE = "InterfaceName"
TIME_OUT = 20
WLAN = "wlan0"
ACCEPT_CONTINUE = "Accept and Continue"


class CaptivePortalTest(base_test.BaseTestClass):
    """Tests for Captive portal."""

    def setup_class(self):
        """Setup devices for tests and unpack params.

        Required params:
          1. rk_captive_portal: SSID of ruckus captive portal network in dict
          2. gg_captive_portal: SSID of guestgate network in dict
          3. uicd_workflows: uicd workflow that specify click actions to accept
             a captive portal connection. Ex: Click on SignIn, Accept & Continue
             //wireless/android/platform/testing/wifi/configs/uicd/
          4. uic_zip: Zip file location of UICD application
        """
        self.dut = self.android_devices[0]
        req_params = ["rk_captive_portal", "gg_captive_portal"]
        self.unpack_userparams(req_param_names=req_params,)
        wutils.wifi_test_device_init(self.dut)

    def teardown_class(self):
        """Reset devices."""
        cutils.set_private_dns(self.dut, cconst.PRIVATE_DNS_MODE_OPPORTUNISTIC)
        wutils.reset_wifi(self.dut)
        self.dut.droid.telephonyToggleDataConnection(True)

    def setup_test(self):
        """Setup device."""
        wutils.reset_wifi(self.dut)
        self.dut.unlock_screen()
        self._go_to_wifi_settings()

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)

    ### Helper methods ###

    def _go_to_wifi_settings(self):
        """Go to wifi settings to perform UI actions for Captive portal."""
        self.dut.adb.shell("am start -a android.settings.SETTINGS")
        asserts.assert_true(
            uutils.has_element(self.dut, text="Network & internet"),
            "Failed to find 'Network & internet' icon")
        uutils.wait_and_click(self.dut, text="Network & internet")
        uutils.wait_and_click(self.dut, text="Not connected")

    def _verify_captive_portal(self, network, click_accept=ACCEPT_CONTINUE):
        """Connect to captive portal network using uicd workflow.

        Steps:
            1. Connect to captive portal network
            2. Run uicd workflow to accept connection
            3. Verify internet connectivity

        Args:
            network: captive portal network to connect to
            click_accept: Notification to select to accept captive portal
        """
        # connect to captive portal wifi network
        wutils.connect_to_wifi_network(
            self.dut, network, check_connectivity=False)

        # run ui automator
        uutils.wait_and_click(self.dut, text="%s" % network["SSID"])
        if uutils.has_element(self.dut, text="%s" % click_accept):
            uutils.wait_and_click(self.dut, text="%s" % click_accept)

        # wait for sometime for captive portal connection to go through
        curr_time = time.time()
        while time.time() < curr_time + TIME_OUT:
            link_prop = self.dut.droid.connectivityGetActiveLinkProperties()
            self.log.debug("Link properties %s" % link_prop)
            if link_prop and link_prop[IFACE] == WLAN:
                break
            time.sleep(2)

        # verify connectivity
        asserts.assert_true(
            wutils.validate_connection(self.dut, ping_gateway=False),
            "Failed to connect to internet. Captive portal test failed")

    ### Test Cases ###

    @test_tracker_info(uuid="b035b4f9-40f7-42f6-9941-ec27afe15040")
    @WifiBaseTest.wifi_test_wrap
    def test_ruckus_captive_portal_default(self):
        """Verify captive portal network.

        Steps:
            1. Set default private dns mode
            2. Connect to ruckus captive portal network
            3. Verify connectivity
        """
        # set private dns to opportunistic
        cutils.set_private_dns(self.dut, cconst.PRIVATE_DNS_MODE_OPPORTUNISTIC)

        # verify connection to captive portal network
        self._verify_captive_portal(self.rk_captive_portal)

    @test_tracker_info(uuid="8ea18d80-0170-41b1-8945-fe14bcd4feab")
    @WifiBaseTest.wifi_test_wrap
    def test_ruckus_captive_portal_private_dns_off(self):
        """Verify captive portal network.

        Steps:
            1. Turn off private dns mode
            2. Connect to ruckus captive portal network
            3. Verify connectivity
        """
        # turn off private dns
        cutils.set_private_dns(self.dut, cconst.PRIVATE_DNS_MODE_OFF)

        # verify connection to captive portal network
        self._verify_captive_portal(self.rk_captive_portal)

    @test_tracker_info(uuid="e8e05907-55f7-40e5-850c-b3111ceb31a4")
    @WifiBaseTest.wifi_test_wrap
    def test_ruckus_captive_portal_private_dns_strict(self):
        """Verify captive portal network.

        Steps:
            1. Set strict private dns mode
            2. Connect to ruckus captive portal network
            3. Verify connectivity
        """
        # set private dns to strict mode
        cutils.set_private_dns(self.dut,
                               cconst.PRIVATE_DNS_MODE_STRICT,
                               cconst.DNS_GOOGLE)

        # verify connection to captive portal network
        self._verify_captive_portal(self.rk_captive_portal)

    @test_tracker_info(uuid="76e49800-f141-4fd2-9969-562585eb1e7a")
    def test_guestgate_captive_portal_default(self):
        """Verify captive portal network.

        Steps:
            1. Set default private dns mode
            2. Connect to guestgate captive portal network
            3. Verify connectivity
        """
        # set private dns to opportunistic
        cutils.set_private_dns(self.dut, cconst.PRIVATE_DNS_MODE_OPPORTUNISTIC)

        # verify connection to captive portal network
        self._verify_captive_portal(self.gg_captive_portal)

    @test_tracker_info(uuid="0aea0cac-0f42-406b-84ba-62c1ef74adfc")
    def test_guestgate_captive_portal_private_dns_off(self):
        """Verify captive portal network.

        Steps:
            1. Turn off private dns mode
            2. Connect to guestgate captive portal network
            3. Verify connectivity
        """
        # turn off private dns
        cutils.set_private_dns(self.dut, cconst.PRIVATE_DNS_MODE_OFF)

        # verify connection to captive portal network
        self._verify_captive_portal(self.gg_captive_portal)

    @test_tracker_info(uuid="39124dcc-2fd3-4d33-b129-a1c8150b7f2a")
    def test_guestgate_captive_portal_private_dns_strict(self):
        """Verify captive portal network.

        Steps:
            1. Set strict private dns mode
            2. Connect to guestgate captive portal network
            3. Verify connectivity
        """
        # set private dns to strict mode
        cutils.set_private_dns(self.dut,
                               cconst.PRIVATE_DNS_MODE_STRICT,
                               cconst.DNS_GOOGLE)

        # verify connection to captive portal network
        self._verify_captive_portal(self.gg_captive_portal)
