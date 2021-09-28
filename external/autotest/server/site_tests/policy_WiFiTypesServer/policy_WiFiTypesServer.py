# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pickle

from autotest_lib.server import autotest
from autotest_lib.server import site_linux_system
from autotest_lib.server.cros.network import wifi_cell_test_base


class policy_WiFiTypesServer(wifi_cell_test_base.WiFiCellTestBase):
    version = 1


    def run_once(self, host, ap_config, network):
        """
        Set up an AP for a WiFi authentication type then run the client test.

        @param host: A host object representing the DUT.
        @param ap_config: HostapConfig object representing how to configure
            the router.
        @param network: NetworkConfig object of how to configure the client.

        """
        self.context.router.require_capabilities(
                [site_linux_system.LinuxSystem.CAPABILITY_MULTI_AP])
        self.context.router.deconfig()

        # Configure the AP
        self.context.configure(ap_config)
        network.ssid = self.context.router.get_ssid()

        client_at = autotest.Autotest(host)
        client_at.run_test('policy_WiFiTypes',
                           network_pickle=pickle.dumps(network),
                           check_client_result=True)

        self.context.router.deconfig()
