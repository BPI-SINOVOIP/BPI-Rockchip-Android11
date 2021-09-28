#!/usr/bin/env python3.4
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

from acts import asserts
from acts.test_decorators import test_tracker_info
import acts.test_utils.wifi.wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

WifiEnums = wutils.WifiEnums

EAP = WifiEnums.Eap
Ent = WifiEnums.Enterprise
WPA3_SECURITY = "SUITE_B_192"


class WifiWpa3EnterpriseTest(WifiBaseTest):
  """Tests for WPA3 Enterprise."""

  def setup_class(self):
    super().setup_class()

    self.dut = self.android_devices[0]
    wutils.wifi_test_device_init(self.dut)
    req_params = [
        "ec2_ca_cert", "ec2_client_cert", "ec2_client_key", "rsa3072_ca_cert",
        "rsa3072_client_cert", "rsa3072_client_key", "wpa3_ec2_network",
        "wpa3_rsa3072_network"
    ]
    self.unpack_userparams(req_param_names=req_params,)

  def setup_test(self):
    super().setup_test()
    for ad in self.android_devices:
      ad.droid.wakeLockAcquireBright()
      ad.droid.wakeUpNow()
    wutils.wifi_toggle_state(self.dut, True)

  def teardown_test(self):
    super().teardown_test()
    for ad in self.android_devices:
      ad.droid.wakeLockRelease()
      ad.droid.goToSleepNow()
    wutils.reset_wifi(self.dut)

  ### Tests ###

  @test_tracker_info(uuid="404c6165-6e23-4ec1-bc2c-9dfdd5c7dc87")
  def test_connect_to_wpa3_enterprise_ec2(self):
    asserts.skip_if(
        self.dut.build_info["build_id"].startswith("R"),
        "No SL4A support for EC certs in R builds. Skipping this testcase")
    config = {
        Ent.EAP: int(EAP.TLS),
        Ent.CA_CERT: self.ec2_ca_cert,
        WifiEnums.SSID_KEY: self.wpa3_ec2_network[WifiEnums.SSID_KEY],
        Ent.CLIENT_CERT: self.ec2_client_cert,
        Ent.PRIVATE_KEY_ID: self.ec2_client_key,
        WifiEnums.SECURITY: WPA3_SECURITY,
        "identity": self.wpa3_ec2_network["identity"],
        "domain_suffix_match": self.wpa3_ec2_network["domain"],
        "cert_algo": self.wpa3_ec2_network["cert_algo"]
    }
    wutils.connect_to_wifi_network(self.dut, config)

  @test_tracker_info(uuid="b6d22585-f7c1-418d-bd4b-b627af8c228c")
  def test_connect_to_wpa3_enterprise_rsa3072(self):
    config = {
        Ent.EAP: int(EAP.TLS),
        Ent.CA_CERT: self.rsa3072_ca_cert,
        WifiEnums.SSID_KEY: self.wpa3_rsa3072_network[WifiEnums.SSID_KEY],
        Ent.CLIENT_CERT: self.rsa3072_client_cert,
        Ent.PRIVATE_KEY_ID: self.rsa3072_client_key,
        WifiEnums.SECURITY: WPA3_SECURITY,
        "identity": self.wpa3_rsa3072_network["identity"],
        "domain_suffix_match": self.wpa3_rsa3072_network["domain"]
    }
    wutils.connect_to_wifi_network(self.dut, config)
