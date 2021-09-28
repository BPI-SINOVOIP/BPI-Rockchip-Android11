# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pickle
import socket

from autotest_lib.server import autotest
from autotest_lib.server import site_linux_system
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server.cros.network import wifi_cell_test_base


class policy_WiFiPrecedenceServer(wifi_cell_test_base.WiFiCellTestBase):
    version = 1


    def cleanup(self):
        """Cleanup for this test."""
        try:
            super(policy_WiFiPrecedenceServer, self).cleanup()
        except socket.error as e:
            # Some of the WiFi components are closed when the DUT reboots,
            # and a socket error is raised when cleanup tries to close them
            # again.
            logging.info(e)

        if self.test == 'device_vs_user':
            tpm_utils.ClearTPMIfOwned(self.host)
            self.host.reboot()


    def run_once(self, host=None, ap_configs=None, network1_config=None,
                 network2_config=None, precedence=None, test=None):
        """
        Set up the APs then run the client side tests.

        Clears the TPM because because the client test needs to enroll.

        @param host: A host object representing the DUT.
        @param ap_configs: List containing HostapConfig objects to setup APs.
        @param network1_config: NetworkConfig object for the client-side
            configuration of network1.
        @param network1_config: NetworkConfig object for the client-side
            configuration of network2.
        @param precedence: One of 1 or 2: which of the APs the
            DUT should connect to.

        """
        self.context.router.require_capabilities(
                [site_linux_system.LinuxSystem.CAPABILITY_MULTI_AP])
        self.context.router.deconfig()
        for ap_config in ap_configs:
            self.context.configure(ap_config, multi_interface=True)

        self.host = host
        self.test = test

        # Clear TPM to ensure that client test can enroll device.
        if self.test == 'device_vs_user':
            tpm_utils.ClearTPMIfOwned(self.host)

        client_at = autotest.Autotest(self.host)

        client_at.run_test(
                'policy_WiFiPrecedence',
                # The config objects must be pickled before they can be
                # passed to the client test.
                network1_pickle=pickle.dumps(network1_config),
                network2_pickle=pickle.dumps(network2_config),
                precedence=precedence,
                test=self.test,
                check_client_result=True)

        self.context.router.deconfig()
