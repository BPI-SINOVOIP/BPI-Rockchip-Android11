#!/usr/bin/python3
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

#import acts.test_utils.wifi.wifi_test_utils as wutils

import time
import random
import re
import logging
import acts.controllers.packet_capture as packet_capture

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import nsd_const as nconsts
from acts.test_utils.wifi.aware import aware_const as aconsts
from acts.test_utils.wifi.aware import aware_test_utils as autils
from acts.test_utils.wifi.aware.AwareBaseTest import AwareBaseTest
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.controllers.ap_lib.hostapd_constants import CHANNEL_MAP

WifiEnums = wutils.WifiEnums

class ProtocolsMultiCountryTest(AwareBaseTest):
    def __init__(self, controllers):
        AwareBaseTest.__init__(self, controllers)
        self.basetest_name = (
            "ping6_ib_unsolicited_passive_multicountry",
            "ping6_ib_solicited_active_multicountry",
            )

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
        super().setup_class()
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
            wutils.wifi_test_device_init(ad)

        if hasattr(self, 'packet_capture'):
            self.packet_capture = self.packet_capture[0]
        self.channel_list_2g = WifiEnums.ALL_2G_FREQUENCIES
        self.channel_list_5g = WifiEnums.ALL_5G_FREQUENCIES

    def setup_test(self):
        super(ProtocolsMultiCountryTest, self).setup_test()
        for ad in self.android_devices:
            ad.ed.clear_all_events()

    def test_time(self,begin_time):
        super(ProtocolsMultiCountryTest, self).setup_test()
        for ad in self.android_devices:
            ad.cat_adb_log(begin_time)

    def teardown_test(self):
        super(ProtocolsMultiCountryTest, self).teardown_test()
        for ad in self.android_devices:
            ad.adb.shell("cmd wifiaware reset")

 
    """Set of tests for Wi-Fi Aware data-paths: validating protocols running on
    top of a data-path"""

    SERVICE_NAME = "GoogleTestServiceXY"

    def run_ping6(self, dut, peer_ipv6):
        """Run a ping6 over the specified device/link
    Args:
      dut: Device on which to execute ping6
      peer_ipv6: Scoped IPv6 address of the peer to ping
    """
        cmd = "ping6 -c 3 -W 5 %s" % peer_ipv6
        results = dut.adb.shell(cmd)
        self.log.info("cmd='%s' -> '%s'", cmd, results)
        if results == "":
            asserts.fail("ping6 empty results - seems like a failure")

    ########################################################################

    def get_ndp_freq(self, dut):
        """ get aware interface status"""
        get_nda0 = "timeout 3 logcat | grep getNdpConfirm | grep Channel"
        out_nda01 = dut.adb.shell(get_nda0)
        out_nda0 = re.findall("Channel = (\d+)", out_nda01)
        #out_nda0 = dict(out_nda02[-1:])
        return out_nda0


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
    ########################################################################

    @test_tracker_info(uuid="3b09e666-c526-4879-8180-77d9a55a2833")
    def ping6_ib_unsolicited_passive_multicountry(self, country):
        """Validate that ping6 works correctly on an NDP created using Aware
        discovery with UNSOLICITED/PASSIVE sessions."""
        p_dut = self.android_devices[0]
        s_dut = self.android_devices[1]
        wutils.set_wifi_country_code(p_dut, country)
        wutils.set_wifi_country_code(s_dut, country)
        #p_dut.adb.shell("timeout 12 logcat -c")
        # create NDP
        (p_req_key, s_req_key, p_aware_if, s_aware_if, p_ipv6,
         s_ipv6) = autils.create_ib_ndp(
             p_dut,
             s_dut,
             p_config=autils.create_discovery_config(
                 self.SERVICE_NAME, aconsts.PUBLISH_TYPE_UNSOLICITED),
             s_config=autils.create_discovery_config(
                 self.SERVICE_NAME, aconsts.SUBSCRIBE_TYPE_PASSIVE),
             device_startup_offset=self.device_startup_offset)
        self.log.info("Interface names: P=%s, S=%s", p_aware_if, s_aware_if)
        self.log.info("Interface addresses (IPv6): P=%s, S=%s", p_ipv6, s_ipv6)
        ndpfreg =int(self.get_ndp_freq(p_dut)[-1])
        ndp_channel = str(CHANNEL_MAP[ndpfreg])
        n = int(ndp_channel)
        if n in range(len(self.channel_list_2g)):
            ndp_band = '2g'
        else:
            ndp_band = '5g'
        p_dut.log.info('ndp frequency : {}'.format(ndpfreg))
        p_dut.log.info('ndp channel : {}'.format(ndp_channel))
        p_dut.log.info('ndp band : {}'.format(ndp_band))
        if hasattr(self, 'packet_capture'):
            self.conf_packet_capture(ndp_band, ndpfreg)
       # run ping6
        self.run_ping6(p_dut, s_ipv6)
        self.run_ping6(s_dut, p_ipv6)

        # clean-up
        p_dut.droid.connectivityUnregisterNetworkCallback(p_req_key)
        s_dut.droid.connectivityUnregisterNetworkCallback(s_req_key)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        time.sleep(10)

    @test_tracker_info(uuid="6b54951f-bf0b-4d26-91d6-c9b3b8452873")
    def ping6_ib_solicited_active_multicountry(self, country):
        """Validate that ping6 works correctly on an NDP created using Aware
        discovery with SOLICITED/ACTIVE sessions."""
        p_dut = self.android_devices[0]
        s_dut = self.android_devices[1]
        wutils.set_wifi_country_code(p_dut, country)
        wutils.set_wifi_country_code(s_dut, country)

        # create NDP
        (p_req_key, s_req_key, p_aware_if, s_aware_if, p_ipv6,
         s_ipv6) = autils.create_ib_ndp(
             p_dut,
             s_dut,
             p_config=autils.create_discovery_config(
                 self.SERVICE_NAME, aconsts.PUBLISH_TYPE_SOLICITED),
             s_config=autils.create_discovery_config(
                 self.SERVICE_NAME, aconsts.SUBSCRIBE_TYPE_ACTIVE),
             device_startup_offset=self.device_startup_offset)
        self.log.info("Interface names: P=%s, S=%s", p_aware_if, s_aware_if)
        self.log.info("Interface addresses (IPv6): P=%s, S=%s", p_ipv6, s_ipv6)
        ndpfreg =int(self.get_ndp_freq(p_dut)[-1])
        ndp_channel = str(CHANNEL_MAP[ndpfreg])
        n = int(ndp_channel)
        if n in range(len(self.channel_list_2g)):
            ndp_band = '2g'
        else:
            ndp_band = '5g'
        p_dut.log.info('ndp frequency : {}'.format(ndpfreg))
        p_dut.log.info('ndp channel : {}'.format(ndp_channel))
        p_dut.log.info('ndp band : {}'.format(ndp_band))
        if hasattr(self, 'packet_capture'):
            self.conf_packet_capture(ndp_band, ndpfreg)
        # run ping6
        self.run_ping6(p_dut, s_ipv6)
        self.run_ping6(s_dut, p_ipv6)

        # clean-up
        p_dut.droid.connectivityUnregisterNetworkCallback(p_req_key)
        s_dut.droid.connectivityUnregisterNetworkCallback(s_req_key)
        if hasattr(self, 'packet_capture'):
            wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        time.sleep(10)
