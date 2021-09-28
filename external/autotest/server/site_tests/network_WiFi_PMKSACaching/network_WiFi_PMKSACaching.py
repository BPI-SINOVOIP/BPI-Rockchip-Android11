# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros import site_eap_certs
from autotest_lib.client.common_lib.cros.network import iw_runner
from autotest_lib.client.common_lib.cros.network import ping_runner
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.client.common_lib.cros.network import xmlrpc_security_types
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_PMKSACaching(wifi_cell_test_base.WiFiCellTestBase):
    """Test that we use PMKSA caching where appropriate."""
    version = 1
    AP0_FREQUENCY = 2412
    AP1_FREQUENCY = 5220
    TIMEOUT_SECONDS = 15


    def dut_sees_bss(self, bssid):
        """
        Check if a DUT can see a BSS in scan results.

        @param bssid: string bssid of AP we expect to see in scan results.
        @return True iff scan results from DUT include the specified BSS.

        """
        runner = iw_runner.IwRunner(remote_host=self.context.client.host)
        is_requested_bss = lambda iw_bss: iw_bss.bss == bssid
        scan_results = runner.scan(self.context.client.wifi_if)
        return scan_results and filter(is_requested_bss, scan_results)


    def run_once(self):
        """Body of the test."""
        mode_n = hostap_config.HostapConfig.MODE_11N_PURE
        eap_config = xmlrpc_security_types.WPAEAPConfig(
                server_ca_cert=site_eap_certs.ca_cert_1,
                server_cert=site_eap_certs.server_cert_1,
                server_key=site_eap_certs.server_private_key_1,
                client_ca_cert=site_eap_certs.ca_cert_1,
                client_cert=site_eap_certs.client_cert_1,
                client_key=site_eap_certs.client_private_key_1,
                # PMKSA caching is only defined for WPA2.
                wpa_mode=xmlrpc_security_types.WPAConfig.MODE_PURE_WPA2)
        ap_config0 = hostap_config.HostapConfig(
                mode=mode_n, frequency=self.AP0_FREQUENCY,
                security_config=eap_config)
        self.context.configure(ap_config0)
        assoc_params = xmlrpc_datatypes.AssociationParameters(
                ssid=self.context.router.get_ssid(),
                security_config=eap_config)
        self.context.assert_connect_wifi(assoc_params)
        # Add another AP with identical configuration except in 5 Ghz.
        ap_config1 = hostap_config.HostapConfig(
                mode=mode_n, ssid=self.context.router.get_ssid(),
                frequency=self.AP1_FREQUENCY, security_config=eap_config)
        self.context.configure(ap_config1, multi_interface=True)
        bssid0 = self.context.router.get_hostapd_mac(0)
        bssid1 = self.context.router.get_hostapd_mac(1)
        utils.poll_for_condition(
                condition=lambda: self.dut_sees_bss(bssid1),
                exception=error.TestFail(
                        'Timed out waiting for DUT to see second AP'),
                timeout=self.TIMEOUT_SECONDS,
                sleep_interval=1)
        self.context.client.request_roam(bssid1)
        if not self.context.client.wait_for_roam(
                bssid1, timeout_seconds=self.TIMEOUT_SECONDS):
            raise error.TestFail('Failed to roam to second BSS.')

        self.context.router.deconfig_aps(instance=1, silent=True)
        if not self.context.client.wait_for_roam(
                bssid0, timeout_seconds=self.TIMEOUT_SECONDS):
            raise error.TestFail('Failed to fall back to first BSS.')

        pinger = ping_runner.PingRunner(host=self.context.client.host)
        utils.poll_for_condition(
                condition=lambda: pinger.simple_ping(
                        self.context.router.get_wifi_ip(0)),
                exception=error.TestFail(
                        'Timed out waiting for DUT to be able to'
                        'ping first BSS after fallback'),
                timeout=self.TIMEOUT_SECONDS,
                sleep_interval=1)
        self.context.router.confirm_pmksa_cache_use(instance=0)
