# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import xmlrpc_datatypes
from autotest_lib.server.cros.network import wifi_cell_test_base


class network_WiFi_AssocConfigPerformance(wifi_cell_test_base.WiFiCellTestBase):
    """Test for performance of discovery, association, and configuration, and
    that WiFi comes back up properly on every attempt.
    Measure and report the performance of the suspend/resume cycle.
    """
    version = 1


    def parse_additional_arguments(self, commandline_args, additional_params):
        """Hook into super class to take control files parameters.

        @param commandline_args dict of parsed parameters from the autotest.
        @param additional_params list of HostapConfig objects.
        """
        self._ap_configs = additional_params


    def _report_perf(self, descript, perf_times, graph_description):
        """Output a summary of performance to logs and to perf value json

        @param descript: string description of the performance numbers,
                         labelling which operation they describe
        @param perf_times: list of float values representing how long the
                           specified operation took, for each iteration
        """
        perf_dict = {descript + '_fastest': min(perf_times),
                     descript + '_slowest': max(perf_times),
                     descript + '_average': sum(perf_times) / len(perf_times)}

        logging.info('Operation: %s', descript)

        for key in perf_dict:
            self.output_perf_value(
                    description=key,
                    value=perf_dict[key],
                    units='seconds',
                    higher_is_better=False,
                    graph=graph_description)
            logging.info('%s: %s', key, perf_dict[key])


    def _connect_disconnect_performance(self, ap_config):
        """Connect to the router, and disconnect, fully removing entries
        for for the router's ssid.

        @param ap_config: the AP configuration that is being used.

        @returns: serialized AssociationResult from xmlrpc_datatypes
        """
        # connect to router
        self.context.configure(ap_config)
        client_conf = xmlrpc_datatypes.AssociationParameters()
        client_conf.ssid = self.context.router.get_ssid()
        assoc_result = self.context.client.shill.connect_wifi(client_conf)
        # ensure successful connection
        if assoc_result['success']:
            logging.info('WiFi connected')
        else:
            raise error.TestFail(assoc_result['failure_reason'])
        # Clean up router and client state for the next run.
        self.context.client.shill.disconnect(client_conf.ssid)
        self.context.router.deconfig()
        return assoc_result


    def _suspend_resume_performance(self, ap_config, suspend_duration):
        """Test that the DUT can be placed in suspend and then resume that
        WiFi connection, and report the performance of that resume.

        @param ap_config: the AP configuration that is being used.
        @param suspend_duration: integer value for how long to suspend.

        @returns dictionary with two keys:
            connect_time: resume time reported, how long it took for the
                          service state to reach connected again
            total_time: total time reported, how long to connect on resume
                        as well as perform other checks on the
                        frequency and subnet, and ping the router
        """
        self.context.configure(ap_config)
        client_conf = xmlrpc_datatypes.AssociationParameters()
        client_conf.ssid = self.context.router.get_ssid()
        # need to connect before suspending
        assoc_result = self.context.client.shill.connect_wifi(client_conf)
        if assoc_result['success']:
            logging.info('Wifi connected')
        else:
            raise error.TestFail(assoc_result['failure_reason'])

        # this suspend is blocking
        self.context.client.do_suspend(suspend_duration)
        start_time = time.time()
        # now wait for the connection to resume after the suspend
        assoc_result = self.context.wait_for_connection(client_conf.ssid)
        # check for success of the connection
        if assoc_result.state not in self.context.client.CONNECTED_STATES:
            raise error.TestFail('WiFi failed to resume connection')

        total_time = time.time() - start_time
        timings = {'connect_time': assoc_result.time, 'total_time': total_time}

        # Clean up router and client state for the next run.
        self.context.client.shill.disconnect(client_conf.ssid)
        self.context.router.deconfig()
        return timings


    def run_once(self, num_iterations=1, suspend_duration=10):
        """Body of the test."""

        # Measure performance for each HostapConfig
        for ap_config in self._ap_configs:
            # need to track the performance numbers for each operation
            connect_disconnect = []
            suspend_resume = []
            for _ in range(0, num_iterations):
                connect_disconnect.append(
                        self._connect_disconnect_performance(ap_config))
            for _ in range(0, num_iterations):
                suspend_resume.append(self._suspend_resume_performance(
                        ap_config, suspend_duration))

            graph_descript = ap_config.perf_loggable_description
            logging.info('Configuration: %s', graph_descript)

            # process reported performance to group the numbers by step
            discovery = [perf['discovery_time'] for perf in connect_disconnect]
            assoc = [perf['association_time'] for perf in connect_disconnect]
            config = [perf['configuration_time'] for perf in connect_disconnect]

            # report the performance from the connect_disconnect
            prefix = 'connect_disconnect'
            self._report_perf(
                    descript=prefix + '_discovery',
                    perf_times=discovery,
                    graph_description=graph_descript)
            self._report_perf(
                    descript=prefix + '_association',
                    perf_times=assoc,
                    graph_description=graph_descript)
            self._report_perf(
                    descript=prefix + '_configuration',
                    perf_times=config,
                    graph_description=graph_descript)

            # process reported performance to group the numbers by step
            # suspend_resume has different keys than connect_disconnect
            connect_times = [perf['connect_time'] for perf in suspend_resume]
            total_times = [perf['total_time'] for perf in suspend_resume]

            # report the performance from suspend_resume
            prefix = 'suspend_resume'
            self._report_perf(
                    descript=prefix + '_connection',
                    perf_times=connect_times,
                    graph_description=graph_descript)
            self._report_perf(
                    descript=prefix + '_total',
                    perf_times=total_times,
                    graph_description=graph_descript)


