# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros import stress
from autotest_lib.server.cros.network import wifi_cell_test_base

_DELAY = 10
_START_TIMEOUT_SECONDS = 20


class network_WiFi_SuspendStress(wifi_cell_test_base.WiFiCellTestBase):
    """Test suspend and resume with powerd_dbus_suspend command and assert wifi
    connectivity to router.
    """
    version = 1


    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params list of tuple(HostapConfig,
                                               AssociationParameters).
        """
        self._configurations = additional_params


    def stress_wifi_suspend(self):
        """ Perform the suspend stress."""

        boot_id = self._host.get_boot_id()
        self.context.client.do_suspend(_DELAY)
        self._host.test_wait_for_resume(boot_id)
        state_info = self.context.wait_for_connection(
                self.context.router.get_ssid())
        self._timings.append(state_info.time)


    def run_once(self, suspends=5):
        """Run suspend stress test.

        @param suspends: Number of suspend iterations

        """
        self._host = self.context.client.host

        for router_conf, client_conf in self._configurations:
            self.context.configure(ap_config=router_conf)
            assoc_params = xmlrpc_datatypes.AssociationParameters(
                is_hidden=client_conf.is_hidden,
                security_config=client_conf.security_config,
                ssid=self.context.router.get_ssid())
            self.context.assert_connect_wifi(assoc_params)
            self._timings = list()

            stressor = stress.CountedStressor(self.stress_wifi_suspend)

            stressor.start(suspends)
            stressor.wait()

            perf_dict = {'fastest': max(self._timings),
                         'slowest': min(self._timings),
                         'average': (float(sum(self._timings)) /
                                     len(self._timings))}
            for key in perf_dict:
                self.output_perf_value(description=key,
                    value=perf_dict[key],
                    units='seconds',
                    higher_is_better=False,
                    graph=router_conf.perf_loggable_description)

            # Explicitly disconnect and clear the shill profile, in case
            # we're running another configuration after this. Especially on
            # Hidden tests, the DUT may still think it can connect to
            # previously-discovered networks, causing extra connection failures
            # and delays along the way.
            self.context.client.shill.disconnect(client_conf.ssid)
            if not self.context.client.shill.init_test_network_state():
                raise error.TestError('Failed to set up shill profile.')
