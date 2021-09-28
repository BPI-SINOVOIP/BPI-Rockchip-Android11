# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import netperf_runner
from autotest_lib.server.cros.network import netperf_session
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_Perf(wifi_cell_test_base.WiFiCellTestBase):
    """Test maximal achievable bandwidth on several channels per band.

    Conducts a performance test for a set of specified router configurations
    and reports results as keyval pairs.

    """

    version = 1

    NETPERF_CONFIGS = [
            netperf_runner.NetperfConfig(
                       netperf_runner.NetperfConfig.TEST_TYPE_TCP_STREAM),
            netperf_runner.NetperfConfig(
                       netperf_runner.NetperfConfig.TEST_TYPE_TCP_MAERTS),
            netperf_runner.NetperfConfig(
                       netperf_runner.NetperfConfig.TEST_TYPE_UDP_STREAM),
            netperf_runner.NetperfConfig(
                       netperf_runner.NetperfConfig.TEST_TYPE_UDP_MAERTS),
    ]


    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params list of HostapConfig objects.

        """
        if 'governor' in commandline_args:
            self._governor = commandline_args['governor']
            # validate governor string. Not all machines will support all of
            # these governors, but this at least ensures that a potentially
            # valid governor was passed in
            if self._governor not in ('performance', 'powersave', 'userspace',
                                      'ondemand', 'conservative', 'schedutil'):
                logging.warning('Unrecognized CPU governor "%s". Running test '
                        'without setting CPU governor...' % self._governor)
                self._governor = None
        else:
            self._governor = None
        self._ap_configs = additional_params


    def do_run(self, ap_config, session, power_save, governor):
        """Run a single set of perf tests, for a given AP and DUT config.

        @param ap_config: the AP configuration that is being used
        @param session: a netperf session instance
        @param power_save: whether or not to use power-save mode on the DUT
                           (boolean)

        """
        def get_current_governor(host):
            """
            @ return the CPU governor name used on a machine. If cannot find
                     the governor info of the host, or if there are multiple
                     different governors being used on different cores, return
                     'default'.
            """
            try:
                governors = set(utils.get_scaling_governor_states(host))
                if len(governors) != 1:
                    return 'default'
                return next(iter(governors))
            except:
                return 'default'
        if governor:
            client_governor = utils.get_scaling_governor_states(
                    self.context.client.host)
            router_governor = utils.get_scaling_governor_states(
                    self.context.router.host)
            utils.set_scaling_governors(governor, self.context.client.host)
            utils.set_scaling_governors(governor, self.context.router.host)
            governor_name = governor
        else:
            # try to get machine's current governor
            governor_name = get_current_governor(self.context.client.host)
            if governor_name != get_current_governor(self.context.router.host):
                governor_name = 'default'
            if governor_name == self._governor:
                # If CPU governor is already set to self._governor, don't
                # perform the run twice
                return

        self.context.client.powersave_switch(power_save)
        session.warmup_stations()
        ps_tag = 'PS%s' % ('on' if power_save else 'off')
        governor_tag = 'governor-%s' % governor_name
        ap_config_tag = '_'.join([ap_config.perf_loggable_description,
                                  ps_tag, governor_tag])
        signal_level = self.context.client.wifi_signal_level
        signal_description = '_'.join([ap_config_tag, 'signal'])
        self.write_perf_keyval({signal_description: signal_level})
        for config in self.NETPERF_CONFIGS:
            results = session.run(config)
            if not results:
                logging.error('Failed to take measurement for %s',
                              config.tag)
                continue
            values = [result.throughput for result in results]
            self.output_perf_value(config.tag, values, units='Mbps',
                                   higher_is_better=True,
                                   graph=ap_config_tag)
            result = netperf_runner.NetperfResult.from_samples(results)
            self.write_perf_keyval(result.get_keyval(
                prefix='_'.join([ap_config_tag, config.tag])))
        if governor:
            utils.restore_scaling_governor_states(client_governor,
                    self.context.client.host)
            utils.restore_scaling_governor_states(router_governor,
                    self.context.router.host)


    def run_once(self):
        """Test body."""
        start_time = time.time()
        for ap_config in self._ap_configs:
            # Set up the router and associate the client with it.
            self.context.configure(ap_config)
            # self.context.configure has a similar check - but that one only
            # errors out if the AP *requires* VHT i.e. AP is requesting
            # MODE_11AC_PURE and the client does not support it.
            # For wifi_perf, we don't want to run MODE_11AC_MIXED on the AP if
            # the client does not support VHT, as we are guaranteed to get the
            # same results at 802.11n/HT40 in that case.
            if ap_config.is_11ac and not self.context.client.is_vht_supported():
                raise error.TestNAError('Client does not have AC support')
            assoc_params = xmlrpc_datatypes.AssociationParameters(
                    ssid=self.context.router.get_ssid(),
                    security_config=ap_config.security_config)
            self.context.assert_connect_wifi(assoc_params)
            session = netperf_session.NetperfSession(self.context.client,
                                                     self.context.router)

            # Flag a test error if we disconnect for any reason.
            with self.context.client.assert_no_disconnects():
                # Conduct the performance tests while toggling powersave mode.
                for power_save in (True, False):
                    for governor in sorted(set([None, self._governor])):
                        self.do_run(ap_config, session, power_save, governor)

            # Clean up router and client state for the next run.
            self.context.client.shill.disconnect(self.context.router.get_ssid())
            self.context.router.deconfig()
        end_time = time.time()
        logging.info('Running time %0.1f seconds.', end_time - start_time)
