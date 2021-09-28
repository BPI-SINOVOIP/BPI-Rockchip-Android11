# Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from autotest_lib.server import site_linux_system
from autotest_lib.client.common_lib import error
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros.network import iw_runner
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.client.common_lib.cros.network import xmlrpc_security_types
from autotest_lib.server.cros.network import wifi_cell_test_base
from autotest_lib.server.cros.network import hostap_config

class network_WiFi_RoamFT(wifi_cell_test_base.WiFiCellTestBase):
    """Tests roam on low signal using FT-PSK between APs

    This test seeks to associate the DUT with an AP with a set of
    association parameters, create a second AP with a second set of
    parameters but the same SSID, and lower the transmission power of
    the first AP. We seek to observe that the DUT successfully
    connects to the second AP in a reasonable amount of time.

    Roaming using FT-PSK is different from standard roaming in that
    there is a special key exchange protocol that needs to occur
    between the APs prior to a successful roam. In order for this
    communication to work, we need to construct a specific interface
    architecture as shown below:
                 _________                       _________
                |         |                     |         |
                |   br0   |                     |   br1   |
                |_________|                     |_________|
                   |   |                           |   |
               ____|   |____                   ____|   |____
         _____|____     ____|____         ____|____     ____|_____
        |          |   |         |       |         |   |          |
        | managed0 |   |  veth0  | <---> |  veth1  |   | managed1 |
        |__________|   |_________|       |_________|   |__________|

    The managed0 and managed1 interfaces cannot communicate with each
    other without a bridge. However, the same bridge cannot be used
    to bridge the two interfaces either (you can't read from a bridge
    that you write to as well without putting the bridge in
    promiscuous mode). Thus, we create a virtual ethernet interface
    with one peer on either bridge to allow the bridges to forward
    traffic between managed0 and managed1.
    """

    version = 1
    TIMEOUT_SECONDS = 15
    GLOBAL_FT_PROPERTY = "WiFi.GlobalFTEnabled"

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


    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params list of xmlrpc_security_types security config.

        """
        self._security_configs = additional_params

    def test_body(self, config):
        """Test body.

        @param config xmlrpc_security_types security config to use in the APs
                      and DUT.
        """

        if config.ft_mode == xmlrpc_security_types.WPAConfig.FT_MODE_PURE:
            self.context.client.require_capabilities(
                [site_linux_system.LinuxSystem.CAPABILITY_SME])

        mac0 = '02:00:00:00:03:00'
        mac1 = '02:00:00:00:04:00'
        id0 = '020000000300'
        id1 = '020000000400'
        key0 = '0f0e0d0c0b0a09080706050403020100'
        key1 = '000102030405060708090a0b0c0d0e0f'
        mdid = 'a1b2'
        router0_conf = hostap_config.HostapConfig(channel=1,
                       mode=hostap_config.HostapConfig.MODE_11G,
                       security_config=config,
                       bssid=mac0,
                       mdid=mdid,
                       nas_id=id0,
                       r1kh_id=id0,
                       r0kh='%s %s %s' % (mac1, id1, key0),
                       r1kh='%s %s %s' % (mac1, mac1, key1),
                       use_bridge=True)
        n_caps = [hostap_config.HostapConfig.N_CAPABILITY_HT40_PLUS]
        ac_caps = [hostap_config.HostapConfig.AC_CAPABILITY_SHORT_GI_80]
        channel_width_80_mhz = hostap_config.HostapConfig.VHT_CHANNEL_WIDTH_80
        router1_conf = hostap_config.HostapConfig(channel=157,
                       mode=hostap_config.HostapConfig.MODE_11AC_PURE,
                       n_capabilities=n_caps,
                       ac_capabilities=ac_caps,
                       vht_channel_width=channel_width_80_mhz,
                       vht_center_channel=155,
                       security_config=config,
                       bssid=mac1,
                       mdid=mdid,
                       nas_id=id1,
                       r1kh_id=id1,
                       r0kh='%s %s %s' % (mac0, id0, key1),
                       r1kh='%s %s %s' % (mac0, mac0, key0),
                       use_bridge=True)
        client_conf = xmlrpc_datatypes.AssociationParameters(
                      security_config=config)

        # Configure the inital AP.
        self.context.configure(router0_conf)
        router_ssid = self.context.router.get_ssid()

        # Connect to the inital AP.
        client_conf.ssid = router_ssid
        self.context.assert_connect_wifi(client_conf)

        # Setup a second AP with the same SSID.
        router1_conf.ssid = router_ssid
        self.context.configure(router1_conf, multi_interface=True)

        # Get BSSIDs of the two APs
        bssid0 = self.context.router.get_hostapd_mac(0)
        bssid1 = self.context.router.get_hostapd_mac(1)

        # Wait for DUT to see the second AP
        utils.poll_for_condition(
            condition=lambda: self.dut_sees_bss(bssid1),
            exception=error.TestFail('Timed out waiting for DUT'
                                     'to see second AP'),
            timeout=self.TIMEOUT_SECONDS,
            sleep_interval=1)

        # Check which AP we are currently connected.
        # This is to include the case that wpa_supplicant
        # automatically roam to AP2 during the scan.
        interface = self.context.client.wifi_if
        curr_bssid = self.context.client.iw_runner.get_current_bssid(interface)
        if curr_bssid == bssid0:
            current_if = self.context.router.get_hostapd_interface(0)
            roam_to_bssid = bssid1
        else:
            current_if = self.context.router.get_hostapd_interface(1)
            roam_to_bssid = bssid0

        br0 = router0_conf.bridge
        br1 = router1_conf.bridge
        self.veth0 = 'veth0'
        self.veth1 = 'veth1'

        # Cleanup veth interfaces from previous runs
        self.context.router.delete_link(self.veth0)
        self.context.router.delete_link(self.veth1)

        # Set up virtual ethernet interface so APs can talk to each other
        try:
            self.context.router.router.run('ip link add %s type veth peer name '
                                           '%s' % (self.veth0, self.veth1))
            self.context.router.router.run('ifconfig %s up' % self.veth0)
            self.context.router.router.run('ifconfig %s up' % self.veth1)
            self.context.router.router.run('ip link set %s master %s' %
                                           (self.veth0, br0))
            self.context.router.router.run('ip link set %s master %s' %
                                           (self.veth1, br1))
        except Exception as e:
            raise error.TestFail('veth configuration failed: %s' % e)


        # Set the tx power of the current interface
        # This should fix the tx power at 100mBm == 1dBm. It turns out that
        # set_tx_power does not actually change the signal level seen from the
        # DUT sufficiently to force a roam (It might vary from -45 to -30), so
        # this autotest takes advantage of wpa_supplicant's preference for
        # 5GHz channels.
        self.context.router.iw_runner.set_tx_power(current_if, 'fixed 100')

        # Expect that the DUT will re-connect to the new AP.
        self.context.client._wpa_cli_proxy.run_wpa_cli_cmd('scan')
        logging.info("Attempting to roam.")
        if not self.context.client.wait_for_roam(
               roam_to_bssid, timeout_seconds=self.TIMEOUT_SECONDS):
            self.context.client._wpa_cli_proxy.run_wpa_cli_cmd('scan')
            logging.info("Attempting to roam again.")
            if not self.context.client.wait_for_roam(
                   roam_to_bssid, timeout_seconds=self.TIMEOUT_SECONDS):
                raise error.TestFail('Failed to roam.')

        self.context.client.shill.disconnect(router_ssid)
        self.context.router.deconfig()

    def run_once(self,host):
        """
        Set global FT switch and call test_body.

        TODO(matthewmwang): rewrite test so that it is more reliable.
        """
        self.context.client.require_capabilities(
            [site_linux_system.LinuxSystem.CAPABILITY_SUPPLICANT_ROAMING])

        assert len(self._security_configs) == 1 or \
                len(self._security_configs) == 2

        with self.context.client.set_manager_property(
                self.GLOBAL_FT_PROPERTY, True):
            self.test_body(self._security_configs[0])
        if len(self._security_configs) > 1:
            logging.info("Disabling FT and trying again")
            with self.context.client.set_manager_property(
                    self.GLOBAL_FT_PROPERTY, False):
                self.test_body(self._security_configs[1])

    def cleanup(self):
        """Cleanup function."""

        if hasattr(self, "veth0"):
            self.context.router.delete_link(self.veth0)
        super(network_WiFi_RoamFT, self).cleanup()
