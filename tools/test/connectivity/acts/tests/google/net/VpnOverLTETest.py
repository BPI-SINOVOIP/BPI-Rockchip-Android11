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


from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

VPN_CONST = connectivity_const.VpnProfile
VPN_TYPE = connectivity_const.VpnProfileType
VPN_PARAMS = connectivity_const.VpnReqParams


class VpnOverLTETest(WifiBaseTest):
  """VPN tests over LTE."""

  def setup_class(self):
    required_params = dir(VPN_PARAMS)
    required_params = [x for x in required_params if not x.startswith("__")]
    self.unpack_userparams(req_param_names=required_params)

    for ad in self.android_devices:
      wutils.wifi_test_device_init(ad)
      nutils.verify_lte_data_and_tethering_supported(ad)
    self.tmo_dut = self.android_devices[0]
    self.vzw_dut = self.android_devices[1]

    self.vpn_params = {
        "vpn_username": self.vpn_username,
        "vpn_password": self.vpn_password,
        "psk_secret": self.psk_secret,
        "client_pkcs_file_name": self.client_pkcs_file_name,
        "cert_path_vpnserver": self.cert_path_vpnserver,
        "cert_password": self.cert_password
    }

  def on_fail(self, test_name, begin_time):
    for ad in self.android_devices:
      ad.take_bug_report(test_name, begin_time)

  ### Test cases ###

  @test_tracker_info(uuid="ea6b1f3e-2791-499d-b696-cacc22ef0476")
  def test_vpn_l2tp_ipsec_psk_strongswan_tmo(self):
    """Verify L2TP IPSec PSk VPN over TMO."""
    vpn = VPN_TYPE.L2TP_IPSEC_PSK
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="5b7e2135-12d8-4775-bcca-5ed6ca96c589")
  def test_vpn_l2tp_ipsec_rsa_strongswan_tmo(self):
    """Verify L2TP IPSec RSA VPN over TMO."""
    vpn = VPN_TYPE.L2TP_IPSEC_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="dec913c9-e9a0-4744-80c7-9bdacd2f55f0")
  def test_vpn_ipsec_xauth_psk_strongswan_tmo(self):
    """Verify IPSec XAUTH PSK over TMO."""
    vpn = VPN_TYPE.IPSEC_XAUTH_PSK
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="259709fe-158e-4a1b-aece-df584a3836d3")
  def test_vpn_ipsec_xauth_rsa_strongswan_tmo(self):
    """Verify IPSec XAUTH RSA over TMO."""
    vpn = VPN_TYPE.IPSEC_XAUTH_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="8106eedb-31a9-4855-9afd-5f24ddbb46b0")
  def test_vpn_ipsec_hybrid_rsa_strongswan_tmo(self):
    """Verify IPSec Hybrid RSA over TMO."""
    vpn = VPN_TYPE.IPSEC_HYBRID_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.tmo_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.tmo_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="0b8d78a9-a7dd-4e8d-ae85-018f5172fa08")
  def test_vpn_l2tp_ipsec_psk_strongswan_vzw(self):
    """Verify L2TP IPSec PSk VPN over VZW."""
    vpn = VPN_TYPE.L2TP_IPSEC_PSK
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="ed0ba740-4fa5-4a32-9013-2aab3563d058")
  def test_vpn_l2tp_ipsec_rsa_strongswan_vzw(self):
    """Verify L2TP IPSec RSA VPN over VZW."""
    vpn = VPN_TYPE.L2TP_IPSEC_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="c555d6b7-fb42-4b31-8527-d2a065bdcaec")
  def test_vpn_ipsec_xauth_psk_strongswan_vzw(self):
    """Verify IPSec XAUTH PSK over VZW."""
    vpn = VPN_TYPE.IPSEC_XAUTH_PSK
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="cdc65925-8c75-4bf9-9377-3da5a3f0a7d4")
  def test_vpn_ipsec_xauth_rsa_strongswan_vzw(self):
    """Verify IPSec XAUTH RSA over VZW."""
    vpn = VPN_TYPE.IPSEC_XAUTH_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)

  @test_tracker_info(uuid="e75d6fac-616c-49fb-a2d4-621e1e8c1108")
  def test_vpn_ipsec_hybrid_rsa_strongswan_vzw(self):
    """Verify IPSec Hybrid RSA over VZW."""
    vpn = VPN_TYPE.IPSEC_HYBRID_RSA
    vpn_profile = nutils.generate_legacy_vpn_profile(
        self.vzw_dut, self.vpn_params, vpn,
        self.vpn_server_addresses[vpn.name][0], self.ipsec_server_type[0],
        self.log_path)
    vpn_addr = self.vpn_verify_addresses[vpn.name][0]
    nutils.legacy_vpn_connection_test_logic(self.vzw_dut, vpn_profile, vpn_addr)
