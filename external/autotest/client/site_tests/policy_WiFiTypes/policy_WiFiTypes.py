# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pickle

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.enterprise import enterprise_network_api


class policy_WiFiTypes(enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def cleanup(self):
        """Re-enable ethernet after the test is completed."""
        if hasattr(self, 'net_api'):
            self.net_api.chrome_net_context.enable_network_device('Ethernet')
        super(policy_WiFiTypes, self).cleanup()


    def run_once(self, network_pickle):
        """
        Setup and run the test configured for the specified test case.

        @param network_pickle: A pickled NetworkConfig object of the network
            to connect to.

        """
        network = pickle.loads(network_pickle)

        # Test with both autoconnect on and off.
        for autoconnect in [False, True]:
            network.autoconnect = autoconnect

            self.setup_case(
                user_policies={'OpenNetworkConfiguration': network.policy()},
                extension_paths=[
                    enterprise_network_api.NETWORK_TEST_EXTENSION_PATH
                ],
            )

            self.net_api = enterprise_network_api.\
                ChromeEnterpriseNetworkContext(self.cr)
            # Disable ethernet so device will default to WiFi
            self.net_api.disable_network_device('Ethernet')

            if not autoconnect:
                self.net_api.connect_to_network(network.ssid)

            if not self.net_api.is_network_connected(network.ssid):
                raise error.TestFail(
                        'No connection to network (%s) when autoconnect is %s.'
                        % (network.ssid, autoconnect))
