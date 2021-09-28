# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pickle
import re

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.enterprise import enterprise_network_api


class policy_GlobalNetworkSettings(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def cleanup(self):
        """Re-enable ethernet after the test is completed."""
        if hasattr(self, 'net_api'):
            self.net_api.chrome_net_context.enable_network_device('Ethernet')
        super(policy_GlobalNetworkSettings, self).cleanup()


    def _test_only_connect_if_available(self, policy_error, user_error):
        """
        Verify the AllowOnlyPolicyNetworksToConnectIfAvailable policy.

        If both networks are available, only the policy network should
        connect. If the policy network is unavailable, then the user network
        may connect. Ensure the errors are caused by the policy blocking or
        allowing the correct connections.

        @param policy_error: Error (if any) raised from connecting to policy
                network.
        @param user_error: Error (if any) raised from connecting to user
                network.

        @raises error.TestFail: If errors do not match policy behavior.

        """
        OUT_OF_RANGE_ERROR = 'The SSID: .* is not in WiFi range of the DUT'
        BLOCKED_ERROR = ('Could not connect to .* network. '
                         'Error returned by chrome.networkingPrivate.'
                         'startConnect API: blocked-by-policy')

        if policy_error and user_error:
            raise error.TestFail(
                    'Unable to connect to user or policy network: %s'
                    % policy_error)
        elif not policy_error and user_error:
            if not re.match(BLOCKED_ERROR, str(user_error)):
                raise error.TestFail('Network API received unrecognized '
                                     'error connecting to the user '
                                     'network: %s' % user_error)
        elif policy_error and not user_error:
            if not re.match(OUT_OF_RANGE_ERROR, str(policy_error)):
                raise error.TestFail('Network API received unrecognized '
                                     'error connecting to the policy '
                                     'network: %s' % policy_error)
        elif not (policy_error or user_error):
            raise error.TestFail('User network connected despite policy '
                                 'network being available')


    def _test_wifi_disabled_policy(self, policy_error, user_error):
        """
        Verify the DisableNetworkTypes policy on WiFi.

        Both networks should show the 'No wifi networks found' error.

        @param policy_error: Error (if any) raised from connecting to policy
                network.
        @param user_error: Error (if any) raised from connecting to user
                network.

        @raises error.TestFail: If errors do not match policy behavior.

        """
        for err in [policy_error, user_error]:
            if err is None:
                raise error.TestFail('DUT was able to connect to WiFi, but '
                                     'WiFi should be blocked.')
            elif str(err) != 'No wifi networks found.':
                raise error.TestFail('Network API received unrecognized '
                                     'error connecting to network: %s'
                                     % err)


    def _test_autoconnect_policy(self, ssid, policy_network, user_network):
        """
        Verify the AllowOnlyPolicyNetworksToAutoconnect policy.

        Disconnect from the network. The policy network should reconnect,
        and the user network should not.

        @param ssid: SSID of connected network.
        @param policy_network: Network policy defined network.
        @param user_network: User defined network.

        @raises error.TestFail: If errors do not match policy behavior.

        """
        self.net_api.disconnect_from_network(ssid)

        if self.net_api.is_network_connected(ssid):
            if ssid == user_network.ssid:
                raise error.TestFail(
                        'User network autoconnected despite '
                        'AllowOnlyPolicyNetworksToAutoconnect=True')
        elif ssid == policy_network.ssid:
            raise error.TestFail(
                    'Policy network did not autoconnect, despite '
                    'AllowOnlyPolicyNetworksToAutoconnect=True')


    def _test_allow_only_policy(self, policy_error, user_error):
        """
        Verify the AllowOnlyPolicyNetworksToConnect policy.

        policy_error should be None, user_error should not.

        @param policy_error: Error (if any) raised from connecting to policy
                network.
        @param user_error: Error (if any) raised from connecting to user
                network.

        @raises error.TestFail: If errors do not match policy behavior.

        """
        if policy_error:
            raise error.TestFail('DUT should have connected to policy '
                                 'network, but did not: %s' % policy_error)
        if not user_error:
            raise error.TestFail('DUT was able to connect to user '
                                 'network, but should have been blocked.')


    def test_global_settings(self, gnc_settings, policy_network, user_network):
        """
        Attempt to connect to the policy network, then the user_network.

        Ensure connection behavior matches GlobalNetworkConfiguration.

        @param gnc_settings: GlobalNetworkConfiguration dictionary value.
        @param policy_network_pickle: NetworkConfig object representing
                the policy network configuration.
        @param user_network_pickle: NetworkConfig object representing
                user network configuration.

        @raise error.TestFail: DUT behavior does not match policy settings.

        """
        # Store connection errors to check later.
        network_errors = {}
        for ssid in [policy_network.ssid, user_network.ssid]:
            network_errors[ssid] = None
            try:
                self.net_api.connect_to_network(ssid)
            except error.TestFail as e:
                network_errors[ssid] = e
                continue

            if gnc_settings.get('AllowOnlyPolicyNetworksToAutoconnect'):
                self._test_autoconnect_policy(ssid,
                                              policy_network,
                                              user_network)
                continue

            if not self.net_api.is_network_connected(ssid):
                raise error.TestFail(
                        'Did not connect to network (%s)' % ssid)

        policy_error = network_errors[policy_network.ssid]
        user_error = network_errors[user_network.ssid]

        if gnc_settings.get('AllowOnlyPolicyNetworksToConnectIfAvailable'):
            self._test_only_connect_if_available(policy_error, user_error)

        elif 'WiFi' in gnc_settings.get('DisableNetworkTypes', {}):
            self._test_wifi_disabled_policy(policy_error, user_error)

        elif gnc_settings.get('AllowOnlyPolicyNetworksToConnect'):
            self._test_allow_only_policy(policy_error, user_error)


    def run_once(self, gnc_settings=None, policy_network_pickle=None,
                 user_network_pickle=None):
        """
        Setup and run the test configured for the specified test case.

        policy_network is in the network policy, and user_network is not.
        The GlobalNetworkConfiguration settings modify how the DUT is able to
        connect to both of these networks.

        @param gnc_settings: GlobalNetworkConfiguration dictionary value.
        @param policy_network_pickle: Pickled NetworkConfig object to set as
                the policy network configuration.
        @param user_network_pickle: Pickled NetworkConfig object to set as the
                user network configuration.

        @raises error.TestFail: If DUT's actions do not match policy settings.

        """
        if policy_network_pickle is None or user_network_pickle is None:
            raise error.TestError('Networks cannot be None')

        policy_network = pickle.loads(policy_network_pickle)
        user_network = pickle.loads(user_network_pickle)

        self.setup_case(
            user_policies={'OpenNetworkConfiguration': policy_network.policy()},
            device_policies={'DeviceOpenNetworkConfiguration': {
                             'GlobalNetworkConfiguration': gnc_settings}},
            extension_paths=[
                enterprise_network_api.NETWORK_TEST_EXTENSION_PATH
            ],
            enroll=True
        )

        self.net_api = enterprise_network_api.\
                ChromeEnterpriseNetworkContext(self.cr)
        # Disable ethernet so device will default to WiFi
        self.net_api.disable_network_device('Ethernet')

        self.test_global_settings(gnc_settings, policy_network, user_network)
