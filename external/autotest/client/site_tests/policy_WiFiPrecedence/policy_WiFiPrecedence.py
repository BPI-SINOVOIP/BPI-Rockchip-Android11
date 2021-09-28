# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pickle

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.enterprise import enterprise_network_api


class policy_WiFiPrecedence(enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def cleanup(self):
        """Re-enable ethernet after the test is completed."""
        if hasattr(self, 'net_api'):
            self.net_api.chrome_net_context.enable_network_device('Ethernet')
        super(policy_WiFiPrecedence, self).cleanup()


    def test_precedence(self, network1, network2, precedence, test):
        """
        Ensure DUT connects to network with higher precedence.

        DUT is given 2 network configs and must connect to the one specified
        by |precedence|.

        @param network1: A NetworkConfig object representing a network.
        @param network2: A NetworkConfig object representing a network.
        @param precedence: The int 1 or 2 that indicates
            which network should autoconnect.
        @param test: Name of the test being run.

        @raises error.TestFail: If DUT does not connect to the |precedence|
            network.

        """
        if test == 'managed_vs_unmanaged':
            # Connect and disconnect from the unmanaged network so the network
            # is a "remembered" network on the DUT.
            self.net_api.connect_to_network(network2.ssid)
            self.net_api.disconnect_from_network(network2.ssid)

        # If the networks are the same, ignore the precedence checks.
        if network1.ssid != network2.ssid:
            if (self.net_api.is_network_connected(network1.ssid) and
                    precedence == 2):
                raise error.TestFail(
                        'DUT autoconnected to network1, but '
                        'should have preferred network2.')
            elif (self.net_api.is_network_connected(network2.ssid) and
                  precedence == 1):
                raise error.TestFail(
                        'DUT autoconnected to network2, but '
                        'should have preferred network1.')

        if (not self.net_api.is_network_connected(network1.ssid) and
              not self.net_api.is_network_connected(network2.ssid)):
            raise error.TestFail('DUT did not connect to a network.')


    def run_once(self, network1_pickle=None, network2_pickle=None,
                 precedence=None, test=None):
        """
        Setup and run the test configured for the specified test case.

        @param network_pickle1: A pickled version of a NetworkConfig
            object representing network1.
        @param network_pickle2: A pickled version of a NetworkConfig
            object representing network2.
        @param precedence: The int 1 or 2 that indicates which network
            should autoconnect.
        @param test: Name of the test being run.

        @raises error.TestFail: If DUT does not connect to the |precedence|
            network.

        """
        if network1_pickle is None or network2_pickle is None:
            raise error.TestError('network1 and network2 cannot be None.')

        network1 = pickle.loads(network1_pickle)
        network2 = pickle.loads(network2_pickle)

        network_policy = network1.policy()

        device_policy = {}
        if test == 'device_vs_user':
            device_policy['device_policies'] = {
                'DeviceOpenNetworkConfiguration': network2.policy()}
        elif test != 'managed_vs_unmanaged':
            # Concatenate the network policies.
            network_policy['NetworkConfigurations'].append(
                    network2.policy()['NetworkConfigurations'][0])

        self.setup_case(
            user_policies={
                'OpenNetworkConfiguration': network_policy
            },
            extension_paths=[
                enterprise_network_api.NETWORK_TEST_EXTENSION_PATH
            ],
            enroll=bool(device_policy),
            **device_policy
        )

        self.net_api = enterprise_network_api.\
            ChromeEnterpriseNetworkContext(self.cr)
        # Disable ethernet so device will default to WiFi.
        self.net_api.disable_network_device('Ethernet')

        self.test_precedence(network1, network2, precedence, test)
