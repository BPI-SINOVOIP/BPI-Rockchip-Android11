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

import time
import random
import re
import logging
import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.test_utils.tel.tel_test_utils as tel_utils
import acts.utils as utils
from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts import signals
from acts.controllers import packet_capture
from acts.controllers.ap_lib.hostapd_constants import BAND_2G
from acts.controllers.ap_lib.hostapd_constants import BAND_5G


WifiEnums = wutils.WifiEnums


class WifiChannelSwitchStressTest(WifiBaseTest):

    def setup_class(self):
        super().setup_class()
        self.dut = self.android_devices[0]
        self.dut_client = self.android_devices[1]
        utils.require_sl4a((self.dut, self.dut_client))

        if hasattr(self, 'packet_capture'):
            self.packet_capture = self.packet_capture[0]

        req_params = ["dbs_supported_models"]
        opt_param = ["stress_count", "cs_count"]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        self.AP_IFACE = 'wlan0'
        if self.dut.model in self.dbs_supported_models:
            self.AP_IFACE = 'wlan1'

        for ad in self.android_devices:
            wutils.wifi_test_device_init(ad)
            utils.sync_device_time(ad)
            wutils.set_wifi_country_code(ad, WifiEnums.CountryCode.US)

    def setup_test(self):
        super().setup_test()
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
            try:
                if self.dut.droid.wifiIsApEnabled():
                    wutils.stop_wifi_tethering(self.dut)
            except signals.TestFailure:
                pass
        wutils.wifi_toggle_state(self.dut_client, True)
        init_sim_state = tel_utils.is_sim_ready(self.log, self.dut)
        if init_sim_state:
            self.check_cell_data_and_enable()
        self.config = wutils.create_softap_config()
        self.channel_list_2g = self.generate_random_list(
            WifiEnums.ALL_2G_FREQUENCIES)
        self.channel_list_5g = self.generate_random_list(
            WifiEnums.NONE_DFS_5G_FREQUENCIES)

    def teardown_test(self):
        super().teardown_test()
        for ad in self.android_devices:
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()
            wutils.reset_wifi(ad)
        try:
            wutils.stop_wifi_tethering(self.dut)
        except signals.TestFailure:
            pass

    def on_fail(self, test_name, begin_time):
        try:
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        except signals.TestFailure:
            pass
        super().on_fail(test_name, begin_time)

    def check_cell_data_and_enable(self):
        """Make sure that cell data is enabled if there is a sim present.

        If a sim is active, cell data needs to be enabled to allow provisioning
        checks through (when applicable).  This is done to relax hardware
        requirements on DUTs - without this check, running this set of tests
        after other wifi tests may cause failures.
        """
        if not self.dut.droid.telephonyIsDataEnabled():
            self.dut.log.info("need to enable data")
            self.dut.droid.telephonyToggleDataConnection(True)
            asserts.assert_true(self.dut.droid.telephonyIsDataEnabled(),
                                "Failed to enable cell data for dut.")

    def get_wlan0_link(self, dut):
        """ get wlan0 interface status"""
        get_wlan0 = 'wpa_cli -iwlan0 -g@android:wpa_wlan0 IFNAME=wlan0 status'
        out = dut.adb.shell(get_wlan0)
        out = dict(re.findall(r'(\S+)=(".*?"|\S+)', out))
        asserts.assert_true("ssid" in out,
                            "Client doesn't connect to any network")
        return out

    def get_wlan1_status(self, dut):
        """ get wlan1 interface status"""
        get_wlan1 = 'hostapd_cli status'
        out_wlan1 = dut.adb.shell(get_wlan1)
        out_wlan1 = dict(re.findall(r'(\S+)=(".*?"|\S+)', out_wlan1))
        return out_wlan1

    def generate_random_list(self, lst):
        """Generate a list where
        the previous and subsequent items
        do not repeat"""
        channel_list = []
        num = random.choice(lst)
        channel_list.append(num)
        for i in range(1, self.stress_count):
            num = random.choice(lst)
            while num == channel_list[-1]:
                num = random.choice(lst)
            channel_list.append(num)
        return channel_list

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

    @test_tracker_info(uuid="b1a8b00e-eca8-4eba-99e9-c7a3beb2a009")
    def test_softap_channel_switch_stress_2g(self):
        """
        1. Disable DUT's Wi-Fi
        2. Enable CLIENT's Wi-Fi
        3. Check DUT's sim is ready or not
        4. Enable DUT's mobile data
        5. Bring up DUT's softap in 2g
        6. CLIENT connect to DUT
        7. DUT switch to different 2g channel
        8. Verify CLIENT follow the change
        9, Repeat step 7 and 8
        """
        wutils.start_wifi_tethering(self.dut,
                                    self.config[wutils.WifiEnums.SSID_KEY],
                                    self.config[wutils.WifiEnums.PWD_KEY],
                                    WifiEnums.WIFI_CONFIG_APBAND_2G)
        wutils.connect_to_wifi_network(self.dut_client, self.config)
        time.sleep(10)
        for count in range(len(self.channel_list_2g)):
            self.dut.log.info("2g channel switch iteration : {}".format(count+1))
            channel_2g = str(self.channel_list_2g[count])
            # Configure sniffer before set SoftAP channel
            if hasattr(self, 'packet_capture'):
                self.conf_packet_capture(BAND_2G, channel_2g)
            # Set SoftAP channel
            wutils.set_softap_channel(self.dut,
                                      self.AP_IFACE,
                                      self.cs_count, channel_2g)
            time.sleep(10)
            softap_frequency = int(self.get_wlan1_status(self.dut)['freq'])
            self.dut.log.info('softap frequency : {}'.format(softap_frequency))
            client_frequency = int(self.get_wlan0_link(self.dut_client)["freq"])
            self.dut_client.log.info(
                "client frequency : {}".format(client_frequency))
            asserts.assert_true(
                softap_frequency == client_frequency,
                "hotspot frequency != client frequency")
            if hasattr(self, 'packet_capture'):
                wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)

    @test_tracker_info(uuid="3411cb7c-2609-433a-97b6-202a096dc71b")
    def test_softap_channel_switch_stress_5g(self):
        """
        1. Disable DUT's Wi-Fi
        2. Enable CLIENT's Wi-Fi
        3. Check DUT's sim is ready or not
        4. Enable DUT's mobile data
        5. Bring up DUT's softap in 5g
        6. CLIENT connect to DUT
        7. DUT switch to different 5g channel
        8. Verify CLIENT follow the change
        9, Repeat step 7 and 8
        """
        wutils.start_wifi_tethering(self.dut,
                                    self.config[wutils.WifiEnums.SSID_KEY],
                                    self.config[wutils.WifiEnums.PWD_KEY],
                                    WifiEnums.WIFI_CONFIG_APBAND_5G)
        wutils.connect_to_wifi_network(self.dut_client, self.config)
        time.sleep(10)
        for count in range(len(self.channel_list_5g)):
            self.dut.log.info("5g channel switch iteration : {}".format(count+1))
            channel_5g = str(self.channel_list_5g[count])
            # Configure sniffer before set SoftAP channel
            if hasattr(self, 'packet_capture'):
                self.conf_packet_capture(BAND_5G, channel_5g)
            # Set SoftAP channel
            wutils.set_softap_channel(self.dut,
                                      self.AP_IFACE,
                                      self.cs_count, channel_5g)
            time.sleep(10)
            softap_frequency = int(self.get_wlan1_status(self.dut)['freq'])
            self.dut.log.info('softap frequency : {}'.format(softap_frequency))
            client_frequency = int(self.get_wlan0_link(self.dut_client)["freq"])
            self.dut_client.log.info(
                "client frequency : {}".format(client_frequency))
            asserts.assert_true(
                softap_frequency == client_frequency,
                "hotspot frequency != client frequency")
            if hasattr(self, 'packet_capture'):
                wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)

    @test_tracker_info(uuid="0f279058-119f-49fc-b8d6-fb2991cc35aa")
    def test_softap_channel_switch_stress_2g_5g(self):
        """
        1. Disable DUT's Wi-Fi
        2. Enable CLIENT's Wi-Fi
        3. Check DUT's sim is ready or not
        4. Enable DUT's mobile data
        5. Bring up DUT's softap in 2g
        6. CLIENT connect to DUT
        7. DUT switch to different 2g channel
        8. Verify CLIENT follow the change
        9. DUT switch to 5g channel
        10. Verify CLIENT follow the change
        11. Repeat step 7 to 10
        """
        for count in range(self.stress_count):
            self.log.info("2g 5g channel switch iteration : {}".format(count+1))
            wutils.start_wifi_tethering(self.dut,
                                        self.config[wutils.WifiEnums.SSID_KEY],
                                        self.config[wutils.WifiEnums.PWD_KEY],
                                        WifiEnums.WIFI_CONFIG_APBAND_2G)
            wutils.connect_to_wifi_network(self.dut_client, self.config)
            self.log.info("wait 10 secs for client reconnect to dut")
            time.sleep(10)
            channel_2g = self.channel_list_2g[count]
            if hasattr(self, 'packet_capture'):
                self.conf_packet_capture(BAND_2G, channel_2g)
            wutils.set_softap_channel(self.dut,
                                      self.AP_IFACE,
                                      self.cs_count, channel_2g)
            time.sleep(10)
            softap_frequency = int(self.get_wlan1_status(self.dut)['freq'])
            self.dut.log.info('softap frequency : {}'.format(softap_frequency))
            client_frequency = int(self.get_wlan0_link(self.dut_client)["freq"])
            self.dut_client.log.info(
                "client frequency : {}".format(client_frequency))
            asserts.assert_true(
                softap_frequency == client_frequency,
                "hotspot frequency != client frequency")
            wutils.stop_wifi_tethering(self.dut)
            if hasattr(self, 'packet_capture'):
                wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
            self.dut.log.info('switch to SoftAP 5g')

            # switch to SoftAp 5g
            wutils.start_wifi_tethering(self.dut,
                                        self.config[wutils.WifiEnums.SSID_KEY],
                                        self.config[wutils.WifiEnums.PWD_KEY],
                                        WifiEnums.WIFI_CONFIG_APBAND_5G)
            wutils.connect_to_wifi_network(self.dut_client, self.config)
            self.log.info("wait 10 secs for client reconnect to dut")
            time.sleep(10)
            channel_5g = self.channel_list_5g[count]
            if hasattr(self, 'packet_capture'):
                self.conf_packet_capture(BAND_5G, channel_5g)
            wutils.set_softap_channel(self.dut,
                                      self.AP_IFACE,
                                      self.cs_count, channel_5g)
            time.sleep(10)
            softap_frequency = int(self.get_wlan1_status(self.dut)['freq'])
            self.dut.log.info('softap frequency : {}'.format(softap_frequency))
            client_frequency = int(self.get_wlan0_link(self.dut_client)["freq"])
            self.dut_client.log.info(
                "client frequency : {}".format(client_frequency))
            asserts.assert_true(
                softap_frequency == client_frequency,
                "hotspot frequency != client frequency")
            wutils.stop_wifi_tethering(self.dut)
            if hasattr(self, 'packet_capture'):
                wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
