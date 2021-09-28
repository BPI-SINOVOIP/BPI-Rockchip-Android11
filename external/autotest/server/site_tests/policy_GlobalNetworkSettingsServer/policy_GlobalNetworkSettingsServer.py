# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pickle
import socket

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.client.cros.enterprise.network_config import NetworkConfig
from autotest_lib.server import autotest
from autotest_lib.server import site_linux_system
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base


class policy_GlobalNetworkSettingsServer(wifi_cell_test_base.WiFiCellTestBase):
    version = 1


    def cleanup(self):
        """Clear TPM and catch errant socket exceptions."""
        try:
            super(policy_GlobalNetworkSettingsServer, self).cleanup()
        except socket.error as e:
            # Some of the WiFi components are closed when the DUT reboots,
            # and a socket error is raised when cleanup tries to close them
            # again.
            logging.info(e)

        tpm_utils.ClearTPMIfOwned(self.host)
        self.host.reboot()


    def run_client_test(self, gnc_settings, policy_network_config,
                        user_network_config):
        """
        Run the client side test.

        Pickle the NetworkConfig objects so they can be passed to the client
        test.

        @param gnc_settings: GlobalNetworkConfiguration policies to enable.
        @param policy_network_config: NetworkConfig of the policy-defined
            network.
        @param user_network_config: NetworkConfig of the user-defined
            network.

        """
        client_at = autotest.Autotest(self.host)
        client_at.run_test(
            'policy_GlobalNetworkSettings',
            gnc_settings=gnc_settings,
            policy_network_pickle=pickle.dumps(policy_network_config),
            user_network_pickle=pickle.dumps(user_network_config),
            check_client_result=True)


    def run_once(self, host=None, gnc_settings=None):
        """
        Set up an AP for a WiFi authentication type then run the client test.

        @param host: A host object representing the DUT.
        @param gnc_settings: GlobalNetworkConfiguration policies to enable.

        """
        self.host = host

        # Clear TPM to ensure that client test can enroll device.
        tpm_utils.ClearTPMIfOwned(self.host)

        self.context.router.require_capabilities(
                [site_linux_system.LinuxSystem.CAPABILITY_MULTI_AP])
        self.context.router.deconfig()

        # Configure 2 open APs.
        for ssid, channel in [('Policy_Network', 5), ('User_Network', 149)]:
            n_mode = hostap_config.HostapConfig.MODE_11N_MIXED
            ap_config = hostap_config.HostapConfig(channel=channel,
                                                   mode=n_mode,
                                                   ssid=ssid)
            self.context.configure(ap_config, multi_interface=True)

        policy_network_config = NetworkConfig(
                                    self.context.router.get_ssid(instance=0))
        user_network_config = NetworkConfig(
                                  self.context.router.get_ssid(instance=1))

        if gnc_settings.get('AllowOnlyPolicyNetworksToAutoconnect'):
            policy_network_config.autoconnect = True

        # Run the client test with both the policy and user networks available.
        self.run_client_test(gnc_settings, policy_network_config,
                             user_network_config)

        # The AllowOnlyPolicyNetworksToConnectIfAvailable policy behaves
        # differently depending on what networks are available. For this test,
        # take down the policy network and run the client test again.
        if gnc_settings.get('AllowOnlyPolicyNetworksToConnectIfAvailable'):
            self.context.router.deconfig_aps(instance=0)

            self.run_client_test(gnc_settings, policy_network_config,
                                 user_network_config)

        self.context.router.deconfig()
