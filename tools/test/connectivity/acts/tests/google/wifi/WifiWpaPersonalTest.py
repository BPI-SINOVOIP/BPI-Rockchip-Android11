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


import acts.test_utils.wifi.wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.controllers.openwrt_lib.openwrt_constants import OpenWrtWifiSecurity
from acts.test_decorators import test_tracker_info
from acts import asserts


WifiEnums = wutils.WifiEnums


class WifiWpaPersonalTest(WifiBaseTest):
  """ Wi-Fi WPA test

      Test Bed Requirement:
        * One Android device
        * One OpenWrt Wi-Fi AP.
  """
  def setup_class(self):
    super().setup_class()
    self.dut = self.android_devices[0]

    if 'OpenWrtAP' in self.user_params:
      self.openwrt = self.access_points[0]
      self.configure_openwrt_ap_and_start(wpa1_network=True)

    req_params = ["OpenWrtAP"]
    opt_params = ["pixel_models", "cnss_diag_file"]
    self.unpack_userparams(req_params, opt_params)
    self.wpa_psk_2g = self.wpa1_networks[0]["2g"]
    self.wpa_psk_5g = self.wpa1_networks[0]["5g"]

  def setup_test(self):
    super().setup_test()
    for ad in self.android_devices:
      ad.droid.wakeLockAcquireBright()
      ad.droid.wakeUpNow()
      wutils.wifi_toggle_state(ad, True)

  def teardown_test(self):
    super().teardown_test()
    for ad in self.android_devices:
      ad.droid.wakeLockRelease()
      ad.droid.goToSleepNow()
    wutils.reset_wifi(self.dut)

  def teardown_class(self):
    super().teardown_class()

  def on_fail(self, test_name, begin_time):
    super().on_fail(test_name, begin_time)
    self.dut.cat_adb_log(test_name, begin_time)
    self.dut.take_bug_report(test_name, begin_time)

  def verify_wpa_network_encryption(self, encryption):
    result = wutils.get_wlan0_link(self.dut)
    if encryption == 'psk+ccmp':
      asserts.assert_true(
          result['pairwise_cipher'] == 'CCMP' and
          result['group_cipher'] == 'CCMP',
          'DUT does not connect to {} encryption network'.format(encryption))
    elif encryption == 'psk+tkip':
      asserts.assert_true(
          result['pairwise_cipher'] == 'TKIP' and
          result['group_cipher'] == 'TKIP',
          'DUT does not connect to {} encryption network'.format(encryption))
    elif encryption == 'psk+tkip+ccmp':
      asserts.assert_true(
          result['pairwise_cipher'] == 'CCMP' and
          result['group_cipher'] == 'TKIP',
          'DUT does not connect to {} encryption network'.format(encryption))

  """ Tests"""

  @test_tracker_info(uuid="0c68a772-b70c-47d6-88ab-1b069c1d8005")
  def test_connect_to_wpa_psk_ccmp_2g(self):
    """
      Change AP's security type to "WPA" and cipher to "CCMP".
      Connect to 2g network.
    """
    self.openwrt.log.info("Enable WPA-PSK CCMP on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_CCMP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_2g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_2g)
    self.verify_wpa_network_encryption(OpenWrtWifiSecurity.WPA_PSK_CCMP)

  @test_tracker_info(uuid="4722dffc-2960-4459-9729-0f8114af2321")
  def test_connect_to_wpa_psk_ccmp_5g(self):
    """
      Change AP's security type to "WPA" and cipher to "CCMp".
      Connect to 5g network.
    """
    self.openwrt.log.info("Enable WPA-PSK CCMp on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_CCMP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_5g)
    self.verify_wpa_network_encryption(OpenWrtWifiSecurity.WPA_PSK_CCMP)

  @test_tracker_info(uuid="4759503e-ef9c-430b-9306-b96a347ca3de")
  def test_connect_to_wpa_psk_tkip_2g(self):
    """
    Steps:
        Change AP's security type to "WPA" and cipher to "TKIP".
        Connect to 2g network.
    """
    self.openwrt.log.info("Enable WPA-TKIP on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_TKIP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_2g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_2g)
    self.verify_wpa_network_encryption(OpenWrtWifiSecurity.WPA_PSK_TKIP)

  @test_tracker_info(uuid="9c836ca6-af14-4d6b-a98e-227fb29e84ee")
  def test_connect_to_wpa_psk_tkip_5g(self):
    """
    Steps:
        Change AP's security type to "WPA" and cipher to "TKIP".
        Connect to 5g network.
    """
    self.openwrt.log.info("Enable WPA-PSK TKIP on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_TKIP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_5g)

  @test_tracker_info(uuid="c03b362b-cd03-4e34-a99a-ef80a9db6db9")
  def test_connect_to_wpa_psk_tkip_and_ccmp_2g(self):
    """
    Steps:
        Change AP's security type to "WPA" and cipher to "CCMP and TKIP".
        Connect to 2g network.
    """
    self.openwrt.log.info("Enable WPA-PSK CCMP and TKIP on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_TKIP_AND_CCMP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_2g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_2g)
    self.verify_wpa_network_encryption(
        OpenWrtWifiSecurity.WPA_PSK_TKIP_AND_CCMP)

  @test_tracker_info(uuid="203d7e7f-536d-4feb-9aa2-648f1f9a685d")
  def test_connect_to_wpa_psk_tkip_and_ccmp_5g(self):
    """
    Steps:
        Change AP's security type to "WPA" and cipher to "CCMP and TKIP".
        Connect to 5g network.
    """
    self.openwrt.log.info("Enable WPA-PSK CCMP and TKIP on OpenWrt AP")
    self.openwrt.set_wpa_encryption(OpenWrtWifiSecurity.WPA_PSK_TKIP_AND_CCMP)
    wutils.start_wifi_connection_scan_and_ensure_network_found(
        self.dut, self.wpa_psk_5g[WifiEnums.SSID_KEY])
    wutils.connect_to_wifi_network(self.dut, self.wpa_psk_5g)
    self.verify_wpa_network_encryption(
        OpenWrtWifiSecurity.WPA_PSK_TKIP_AND_CCMP)
