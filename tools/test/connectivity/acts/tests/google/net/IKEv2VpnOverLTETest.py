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


from acts import base_test
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.wifi import wifi_test_utils as wutils

VPN_CONST = connectivity_const.VpnProfile
VPN_TYPE = connectivity_const.VpnProfileType
VPN_PARAMS = connectivity_const.VpnReqParams


class IKEv2VpnOverLTETest(base_test.BaseTestClass):
  """IKEv2 VPN tests."""

  def setup_class(self):

    required_params = dir(VPN_PARAMS)
    required_params = [x for x in required_params if not x.startswith("__")]
    self.unpack_userparams(req_param_names=required_params)
    self.vpn_params = {
        "vpn_username": self.vpn_username,
        "vpn_password": self.vpn_password,
        "psk_secret": self.psk_secret,
        "client_pkcs_file_name": self.client_pkcs_file_name,
        "cert_path_vpnserver": self.cert_path_vpnserver,
        "cert_password": self.cert_password,
        "vpn_identity": self.vpn_identity,
    }

    for ad in self.android_devices:
      wutils.wifi_test_device_init(ad)
      nutils.verify_lte_data_and_tethering_supported(ad)
    self.tmo_dut = self.android_devices[0]
    self.vzw_dut = self.android_devices[1]

  def on_fail(self, test_name, begin_time):
    for ad in self.android_devices:
      ad.take_bug_report(test_name, begin_time)

  ### Test cases ###

  @test_tracker_info(uuid="31fac6c5-f76c-403c-8b76-29c01557a48a")
  def test_ikev2_psk_vpn_tmo(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_PSK
    server_addr = self.vpn_server_addresses[vpn.name][0]
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="c28adef0-6578-4841-a833-e52a5b16a390")
  def test_ikev2_mschapv2_vpn_tmo(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_USER_PASS
    server_addr = self.vpn_server_addresses[vpn.name][0]
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="6c7daad9-ae7a-493d-bbab-9001068f22c5")
  def test_ikev2_rsa_vpn_tmo(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_RSA
    server_addr = self.vpn_server_addresses[vpn.name][0]
    self.vpn_params["server_addr"] = server_addr
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="1275a2f-e939-4557-879d-fbbd9c5dbd93")
  def test_ikev2_psk_vpn_vzw(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_PSK
    server_addr = self.vpn_server_addresses[vpn.name][0]
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="fd146163-f28d-4514-96a0-82f51b70e218")
  def test_ikev2_mschapv2_vpn_vzw(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_USER_PASS
    server_addr = self.vpn_server_addresses[vpn.name][0]
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="722de9b5-834f-4854-b4a6-e31860628fe9")
  def test_ikev2_rsa_vpn_vzw(self):
    vpn = VPN_TYPE.IKEV2_IPSEC_RSA
    server_addr = self.vpn_server_addresses[vpn.name][0]
    self.vpn_params["server_addr"] = server_addr
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    vpn_profile = nutils.generate_ikev2_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn, server_addr, self.log_path)
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)
