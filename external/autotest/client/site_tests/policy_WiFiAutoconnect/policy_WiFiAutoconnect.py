# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.enterprise import enterprise_network_api
from autotest_lib.client.cros.enterprise import network_config


class policy_WiFiAutoconnect(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def cleanup(self):
        """Re-enable ethernet after the test is completed."""
        if hasattr(self, 'net_api'):
            self.net_api.chrome_net_context.enable_network_device('Ethernet')
        super(policy_WiFiAutoconnect, self).cleanup()


    def test_wifi_autoconnect(self, ssid, autoconnect):
        """
        Verifies the behavior of the autoconnect portion of network policy.

        @param ssid: Service set identifier for wireless local area network.
        @param autoconnect: Whether policy autoconnects to network.

        @raises error.TestFail: When device's behavior does not match policy.

        """
        if not autoconnect:
            if self.net_api.is_network_connected(ssid):
                raise error.TestFail('Device autoconnected to %s, but '
                                     'autoconnect = False.'
                                     % ssid)
            self.net_api.connect_to_network(ssid)

        if not self.net_api.is_network_connected(ssid):
            raise error.TestFail('Did not connect to network (%s)' % ssid)


    def run_once(self, autoconnect=False, ssid=''):
        """
        Setup and run the test configured for the specified test case.

        @param ssid: Service set identifier for wireless local area network.
        @param autoconnect: Value of "AutoConnect" setting. Options are True,
                            False, or None

        """
        network = network_config.NetworkConfig(ssid,
                                               autoconnect=autoconnect)

        self.setup_case(
            user_policies={
                'OpenNetworkConfiguration': network.policy()
            },
            extension_paths=[
                enterprise_network_api.NETWORK_TEST_EXTENSION_PATH
            ]
        )

        self.net_api = enterprise_network_api.\
                ChromeEnterpriseNetworkContext(self.cr)

        network_available = self.net_api.is_network_in_range(
                network.ssid,
                wait_time=self.net_api.SHORT_TIMEOUT)
        if not network_available:
            raise error.TestError('SSID %s not available within %s seconds'
                                  % (network.ssid, self.net_api.SHORT_TIMEOUT))

        # Disable ethernet so device will default to WiFi
        self.net_api.disable_network_device('Ethernet')

        self.test_wifi_autoconnect(ssid, autoconnect)
