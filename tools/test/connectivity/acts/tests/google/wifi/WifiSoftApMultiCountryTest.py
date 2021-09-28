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

import queue
import logging
import random
import time
import re
import acts.controllers.packet_capture as packet_capture
import acts.signals as signals

from acts import asserts
from acts import utils
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import socket_test_utils as sutils
from acts.test_utils.tel import tel_defines
from acts.test_utils.tel import tel_test_utils as tel_utils
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_2G
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_5G
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_AUTO
from acts.test_utils.wifi import wifi_constants
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.controllers.ap_lib.hostapd_constants import CHANNEL_MAP


WifiEnums = wutils.WifiEnums

class WifiSoftApMultiCountryTest(WifiBaseTest):

    def __init__(self, configs):
        super().__init__(configs)
        self.basetest_name = (
                "test_full_tether_startup_auto_one_client_ping_softap_multicountry",
                "test_full_tether_startup_2G_one_client_ping_softap_multicountry",
                "test_full_tether_startup_5G_one_client_ping_softap_multicountry")
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
        """It will setup the required dependencies from config file and configure
           the devices for softap mode testing.

        Returns:
            True if successfully configured the requirements for testing.
        """
        super().setup_class()
        self.dut = self.android_devices[0]
        self.dut_client = self.android_devices[1]
        if hasattr(self, 'packet_capture'):
            self.packet_capture = self.packet_capture[0]

        self.channel_list_2g = WifiEnums.ALL_2G_FREQUENCIES
        self.channel_list_5g = WifiEnums.ALL_5G_FREQUENCIES
        req_params = ["dbs_supported_models"]
        opt_param = ["open_network"]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)
        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start()
        elif "OpenWrtAP" in self.user_params:
            self.configure_openwrt_ap_and_start(open_network=True)
        self.open_network = self.open_network[0]["2g"]
        # Do a simple version of init - mainly just sync the time and enable
        # verbose logging.  This test will fail if the DUT has a sim and cell
        # data is disabled.  We would also like to test with phones in less
        # constrained states (or add variations where we specifically
        # constrain).
        utils.require_sl4a((self.dut, self.dut_client))
        utils.sync_device_time(self.dut)
        utils.sync_device_time(self.dut_client)
        # Enable verbose logging on the duts
        self.dut.droid.wifiEnableVerboseLogging(1)
        asserts.assert_equal(self.dut.droid.wifiGetVerboseLoggingLevel(), 1,
            "Failed to enable WiFi verbose logging on the softap dut.")
        self.dut_client.droid.wifiEnableVerboseLogging(1)
        asserts.assert_equal(self.dut_client.droid.wifiGetVerboseLoggingLevel(), 1,
            "Failed to enable WiFi verbose logging on the client dut.")
        wutils.wifi_toggle_state(self.dut, True)
        wutils.wifi_toggle_state(self.dut_client, True)
        self.AP_IFACE = 'wlan0'
        if self.dut.model in self.dbs_supported_models:
            self.AP_IFACE = 'wlan1'

    def teardown_class(self):
        if self.dut.droid.wifiIsApEnabled():
            wutils.stop_wifi_tethering(self.dut)
        wutils.reset_wifi(self.dut)
        wutils.reset_wifi(self.dut_client)
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    def teardown_test(self):
        super().teardown_test()
        if self.dut.droid.wifiIsApEnabled():
            wutils.stop_wifi_tethering(self.dut)

    """ Snifferconfig Functions """
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

    """ Helper Functions """
    def create_softap_config(self):
        """Create a softap config with ssid and password."""
        ap_ssid = "softap_" + utils.rand_ascii_str(8)
        ap_password = utils.rand_ascii_str(8)
        self.dut.log.info("softap setup: %s %s", ap_ssid, ap_password)
        config = {wutils.WifiEnums.SSID_KEY: ap_ssid}
        config[wutils.WifiEnums.PWD_KEY] = ap_password
        return config

    def validate_full_tether_startup(self, band=None, test_ping=False):
        """Test full startup of wifi tethering

        1. Report current state.
        2. Switch to AP mode.
        3. verify SoftAP active.
        4. Shutdown wifi tethering.
        5. verify back to previous mode.
        """
        initial_wifi_state = self.dut.droid.wifiCheckState()
        self.dut.log.info("current state: %s", initial_wifi_state)
        config = self.create_softap_config()
        wutils.start_wifi_tethering(
                self.dut,
                config[wutils.WifiEnums.SSID_KEY],
                config[wutils.WifiEnums.PWD_KEY],
                band=band)

        if test_ping:
            self.validate_ping_between_softap_and_client(config)

        wutils.stop_wifi_tethering(self.dut)
        asserts.assert_false(self.dut.droid.wifiIsApEnabled(),
                             "SoftAp is still reported as running")
        if initial_wifi_state:
            wutils.wait_for_wifi_state(self.dut, True)
        elif self.dut.droid.wifiCheckState():
            asserts.fail(
                    "Wifi was disabled before softap and now it is enabled")

    def validate_ping_between_softap_and_client(self, config):
        """Test ping between softap and its client.

        Connect one android device to the wifi hotspot.
        Verify they can ping each other.

        Args:
            config: wifi network config with SSID, password
        """
        wutils.wifi_connect(self.dut_client, config, check_connectivity=False)
        softap_frequency = int(self.get_wlan1_status(self.dut)['freq'])
        softap_channel = str(CHANNEL_MAP[softap_frequency])
        n = int(softap_channel)
        if n in range(len(self.channel_list_2g)):
            softap_band = '2g'
        else:
            softap_band = '5g'
        self.dut.log.info('softap frequency : {}'.format(softap_frequency))
        self.dut.log.info('softap channel : {}'.format(softap_channel))
        self.dut.log.info('softap band : {}'.format(softap_band))
        if hasattr(self, 'packet_capture'):
            self.conf_packet_capture(softap_band, softap_frequency)
        dut_ip = self.dut.droid.connectivityGetIPv4Addresses(self.AP_IFACE)[0]
        dut_client_ip = self.dut_client.droid.connectivityGetIPv4Addresses('wlan0')[0]

        self.dut.log.info("Try to ping %s" % dut_client_ip)
        asserts.assert_true(
            utils.adb_shell_ping(self.dut, count=10, dest_ip=dut_client_ip, timeout=20),
            "%s ping %s failed" % (self.dut.serial, dut_client_ip))

        self.dut_client.log.info("Try to ping %s" % dut_ip)
        asserts.assert_true(
            utils.adb_shell_ping(self.dut_client, count=10, dest_ip=dut_ip, timeout=20),
            "%s ping %s failed" % (self.dut_client.serial, dut_ip))

        wutils.stop_wifi_tethering(self.dut)

    def get_wlan1_status(self, dut):
        """ get wlan1 interface status"""
        get_wlan1 = 'hostapd_cli status'
        out_wlan1 = dut.adb.shell(get_wlan1)
        out_wlan1 = dict(re.findall(r'(\S+)=(".*?"|\S+)', out_wlan1))
        return out_wlan1

    """ Tests Begin """

    @test_tracker_info(uuid="6ce4fb40-6fa7-452f-ba17-ea3fe47d325d")
    def test_full_tether_startup_2G_one_client_ping_softap_multicountry(self, country):
        """(AP) 1 Device can connect to 2G hotspot

        Steps:
        1. Setting country code for each device
        2. Turn on DUT's 2G softap
        3. Client connects to the softap
        4. Client and DUT ping each other
        """
        wutils.set_wifi_country_code(self.dut, country)
        wutils.set_wifi_country_code(self.dut_client, country)
        self.validate_full_tether_startup(WIFI_CONFIG_APBAND_2G, test_ping=True)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)

    @test_tracker_info(uuid="ae4629e6-08d5-4b51-ac34-6c2485f54df5")
    def test_full_tether_startup_5G_one_client_ping_softap_multicountry(self, country):
        """(AP) 1 Device can connect to 2G hotspot

        Steps:
        1. Setting country code for each device
        2. Turn on DUT's 5G softap
        3. Client connects to the softap
        4. Client and DUT ping each other
        """
        wutils.set_wifi_country_code(self.dut, country)
        wutils.set_wifi_country_code(self.dut_client, country)
        self.validate_full_tether_startup(WIFI_CONFIG_APBAND_5G, test_ping=True)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)

    @test_tracker_info(uuid="84a10203-cb02-433c-92a7-e8aa2348cc02")
    def test_full_tether_startup_auto_one_client_ping_softap_multicountry(self, country):
        """(AP) 1 Device can connect to hotspot

        Steps:
        1.Setting country code for each device
        2. Turn on DUT's softap
        3. Client connects to the softap
        4. Client and DUT ping each other
        """
        wutils.set_wifi_country_code(self.dut, country)
        wutils.set_wifi_country_code(self.dut_client, country)
        self.validate_full_tether_startup(
            WIFI_CONFIG_APBAND_AUTO, test_ping=True)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)

    """ Tests End """


if __name__ == "__main__":
    pass
