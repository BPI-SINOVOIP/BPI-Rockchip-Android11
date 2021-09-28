#!/usr/bin/env python3.4
#
#   Copyright 2018 - The Android Open Source Project
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

import os
import re
import time
import urllib.request

import acts.base_test
import acts.signals as signals

from acts import asserts
from acts.base_test import BaseTestClass
from acts.libs.ota import ota_updater
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const as cconst

import acts.test_utils.net.net_test_utils as nutils
import acts.test_utils.wifi.wifi_test_utils as wutils

VPN_CONST = cconst.VpnProfile
VPN_TYPE = cconst.VpnProfileType
VPN_PARAMS = cconst.VpnReqParams


class CoreNetworkingOTATest(BaseTestClass):
    """Core networking auto update tests"""

    def setup_class(self):
        super(CoreNetworkingOTATest, self).setup_class()
        ota_updater.initialize(self.user_params, self.android_devices)

        self.dut = self.android_devices[0]
        req_params = dir(VPN_PARAMS)
        req_params = [x for x in req_params if not x.startswith('__')]
        req_params.extend(["download_file", "file_size"])
        self.unpack_userparams(req_params)

        vpn_params = {'vpn_username': self.vpn_username,
                      'vpn_password': self.vpn_password,
                      'psk_secret': self.psk_secret,
                      'client_pkcs_file_name': self.client_pkcs_file_name,
                      'cert_path_vpnserver': self.cert_path_vpnserver,
                      'cert_password': self.cert_password}

        # generate legacy vpn profiles
        wutils.wifi_connect(self.dut, self.wifi_network)
        self.xauth_rsa = nutils.generate_legacy_vpn_profile(
            self.dut, vpn_params,
            VPN_TYPE.IPSEC_XAUTH_RSA,
            self.vpn_server_addresses[VPN_TYPE.IPSEC_XAUTH_RSA.name][0],
            self.ipsec_server_type[0], self.log_path)
        self.l2tp_rsa = nutils.generate_legacy_vpn_profile(
            self.dut, vpn_params,
            VPN_TYPE.L2TP_IPSEC_RSA,
            self.vpn_server_addresses[VPN_TYPE.L2TP_IPSEC_RSA.name][0],
            self.ipsec_server_type[0], self.log_path)
        self.hybrid_rsa = nutils.generate_legacy_vpn_profile(
            self.dut, vpn_params,
            VPN_TYPE.IPSEC_HYBRID_RSA,
            self.vpn_server_addresses[VPN_TYPE.IPSEC_HYBRID_RSA.name][0],
            self.ipsec_server_type[0], self.log_path)
        self.vpn_profiles = [self.l2tp_rsa, self.hybrid_rsa, self.xauth_rsa]

        # verify legacy vpn
        for prof in self.vpn_profiles:
            nutils.legacy_vpn_connection_test_logic(self.dut, prof)

        # Run OTA below, if ota fails then abort all tests.
        try:
          for ad in self.android_devices:
              ota_updater.update(ad)
        except Exception as err:
            raise signals.TestAbortClass(
                "Failed up apply OTA update. Aborting tests")

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)

    """Tests"""

    def test_legacy_vpn_ota_xauth_rsa(self):
        nutils.legacy_vpn_connection_test_logic(self.dut, self.xauth_rsa)

    def test_legacy_vpn_ota_l2tp_rsa(self):
        nutils.legacy_vpn_connection_test_logic(self.dut, self.l2tp_rsa)

    def test_legacy_vpn_ota_hybrid_rsa(self):
        nutils.legacy_vpn_connection_test_logic(self.dut, self.hybrid_rsa)
