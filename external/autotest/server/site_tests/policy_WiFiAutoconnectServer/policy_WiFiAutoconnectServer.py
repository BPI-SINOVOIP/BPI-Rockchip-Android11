# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.server import autotest
from autotest_lib.server import site_linux_system
from autotest_lib.server.cros.network import hostap_config
from autotest_lib.server.cros.network import wifi_cell_test_base


class policy_WiFiAutoconnectServer(wifi_cell_test_base.WiFiCellTestBase):
    version = 1


    def parse_additional_arguments(self, commandline_args, additional_params):
        """
        Hook into super class to take control files parameters.

        @param commandline_args: dict of parsed parameters from the autotest.
        @param additional_params: HostapConfig object.

        """
        self._ap_config = additional_params


    def run_once(self, host, autoconnect=None):
        """
        Set up AP then run the client side autoconnect tests.

        @param host: A host object representing the DUT.
        @param autoconnect: Autoconnect setting for network policy.

        """
        self.context.router.require_capabilities(
                [site_linux_system.LinuxSystem.CAPABILITY_MULTI_AP])
        self.context.router.deconfig()
        self.context.configure(self._ap_config, multi_interface=True)

        client_at = autotest.Autotest(host)

        client_at.run_test('policy_WiFiAutoconnect',
                           ssid=self.context.router.get_ssid(instance=0),
                           autoconnect=autoconnect,
                           check_client_result=True)

        self.context.router.deconfig()
