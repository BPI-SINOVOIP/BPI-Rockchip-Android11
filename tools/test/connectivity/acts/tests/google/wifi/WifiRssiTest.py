#!/usr/bin/env python3.4
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import collections
import itertools
import json
import logging
import math
import numpy
import os
import statistics
from acts import asserts
from acts import base_test
from acts import context
from acts import utils
from acts.controllers.utils_lib import ssh
from acts.controllers import iperf_server as ipf
from acts.metrics.loggers.blackbox import BlackboxMappedMetricLogger
from acts.test_utils.wifi import ota_chamber
from acts.test_utils.wifi import wifi_performance_test_utils as wputils
from acts.test_utils.wifi import wifi_retail_ap as retail_ap
from acts.test_utils.wifi import wifi_test_utils as wutils
from concurrent.futures import ThreadPoolExecutor
from functools import partial

SHORT_SLEEP = 1
MED_SLEEP = 6
CONST_3dB = 3.01029995664
RSSI_ERROR_VAL = float('nan')


class WifiRssiTest(base_test.BaseTestClass):
    """Class to test WiFi RSSI reporting.

    This class tests RSSI reporting on android devices. The class tests RSSI
    accuracy by checking RSSI over a large attenuation range, checks for RSSI
    stability over time when attenuation is fixed, and checks that RSSI quickly
    and reacts to changes attenuation by checking RSSI trajectories over
    configurable attenuation waveforms.For an example config file to run this
    test class see example_connectivity_performance_ap_sta.json.
    """
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_test_metrics = True

    def setup_class(self):
        self.dut = self.android_devices[0]
        req_params = [
            'RemoteServer', 'RetailAccessPoints', 'rssi_test_params',
            'main_network', 'testbed_params'
        ]
        self.unpack_userparams(req_params)
        self.testclass_params = self.rssi_test_params
        self.num_atten = self.attenuators[0].instrument.num_atten
        self.iperf_server = self.iperf_servers[0]
        self.iperf_client = self.iperf_clients[0]
        self.remote_server = ssh.connection.SshConnection(
            ssh.settings.from_config(self.RemoteServer[0]['ssh_config']))
        self.access_point = retail_ap.create(self.RetailAccessPoints)[0]
        self.log_path = os.path.join(logging.log_path, 'results')
        os.makedirs(self.log_path, exist_ok=True)
        self.log.info('Access Point Configuration: {}'.format(
            self.access_point.ap_settings))
        if hasattr(self, 'bdf'):
            self.log.info('Pushing WiFi BDF to DUT.')
            wputils.push_bdf(self.dut, self.bdf)
        if hasattr(self, 'firmware'):
            self.log.info('Pushing WiFi firmware to DUT.')
            wlanmdsp = [
                file for file in self.firmware if "wlanmdsp.mbn" in file
            ][0]
            data_msc = [file for file in self.firmware
                        if "Data.msc" in file][0]
            wputils.push_firmware(self.dut, wlanmdsp, data_msc)
        self.testclass_results = []

        # Turn WiFi ON
        if self.testclass_params.get('airplane_mode', 1):
            self.log.info('Turning on airplane mode.')
            asserts.assert_true(utils.force_airplane_mode(self.dut, True),
                                "Can not turn on airplane mode.")
        wutils.wifi_toggle_state(self.dut, True)

    def teardown_test(self):
        self.iperf_server.stop()

    def pass_fail_check_rssi_stability(self, testcase_params,
                                       postprocessed_results):
        """Check the test result and decide if it passed or failed.

        Checks the RSSI test result and fails the test if the standard
        deviation of signal_poll_rssi is beyond the threshold defined in the
        config file.

        Args:
            testcase_params: dict containing test-specific parameters
            postprocessed_results: compiled arrays of RSSI measurements
        """
        # Set Blackbox metric values
        if self.publish_test_metrics:
            self.testcase_metric_logger.add_metric(
                'signal_poll_rssi_stdev',
                max(postprocessed_results['signal_poll_rssi']['stdev']))
            self.testcase_metric_logger.add_metric(
                'chain_0_rssi_stdev',
                max(postprocessed_results['chain_0_rssi']['stdev']))
            self.testcase_metric_logger.add_metric(
                'chain_1_rssi_stdev',
                max(postprocessed_results['chain_1_rssi']['stdev']))

        # Evaluate test pass/fail
        test_failed = any([
            stdev > self.testclass_params['stdev_tolerance']
            for stdev in postprocessed_results['signal_poll_rssi']['stdev']
        ])
        test_message = (
            'RSSI stability {0}. Standard deviation was {1} dB '
            '(limit {2}), per chain standard deviation [{3}, {4}] dB'.format(
                'failed' * test_failed + 'passed' * (not test_failed), [
                    float('{:.2f}'.format(x))
                    for x in postprocessed_results['signal_poll_rssi']['stdev']
                ], self.testclass_params['stdev_tolerance'], [
                    float('{:.2f}'.format(x))
                    for x in postprocessed_results['chain_0_rssi']['stdev']
                ], [
                    float('{:.2f}'.format(x))
                    for x in postprocessed_results['chain_1_rssi']['stdev']
                ]))
        if test_failed:
            asserts.fail(test_message)
        asserts.explicit_pass(test_message)

    def pass_fail_check_rssi_accuracy(self, testcase_params,
                                      postprocessed_results):
        """Check the test result and decide if it passed or failed.

        Checks the RSSI test result and compares and compute its deviation from
        the predicted RSSI. This computation is done for all reported RSSI
        values. The test fails if any of the RSSI values specified in
        rssi_under_test have an average error beyond what is specified in the
        configuration file.

        Args:
            postprocessed_results: compiled arrays of RSSI measurements
            testcase_params: dict containing params such as list of RSSIs under
            test, i.e., can cause test to fail and boolean indicating whether
            to look at absolute RSSI accuracy, or centered RSSI accuracy.
            Centered accuracy is computed after systematic RSSI shifts are
            removed.
        """
        test_failed = False
        test_message = ''
        if testcase_params['absolute_accuracy']:
            error_type = 'absolute'
        else:
            error_type = 'centered'

        for key, val in postprocessed_results.items():
            # Compute the error metrics ignoring invalid RSSI readings
            # If all readings invalid, set error to RSSI_ERROR_VAL
            if 'rssi' in key and 'predicted' not in key:
                filtered_error = [x for x in val['error'] if not math.isnan(x)]
                if filtered_error:
                    avg_shift = statistics.mean(filtered_error)
                    if testcase_params['absolute_accuracy']:
                        avg_error = statistics.mean(
                            [abs(x) for x in filtered_error])
                    else:
                        avg_error = statistics.mean(
                            [abs(x - avg_shift) for x in filtered_error])
                else:
                    avg_error = RSSI_ERROR_VAL
                    avg_shift = RSSI_ERROR_VAL
                # Set Blackbox metric values
                if self.publish_test_metrics:
                    self.testcase_metric_logger.add_metric(
                        '{}_error'.format(key), avg_error)
                    self.testcase_metric_logger.add_metric(
                        '{}_shift'.format(key), avg_shift)
                # Evaluate test pass/fail
                rssi_failure = (avg_error >
                                self.testclass_params['abs_tolerance']
                                ) or math.isnan(avg_error)
                if rssi_failure and key in testcase_params['rssi_under_test']:
                    test_message = test_message + (
                        '{} failed ({} error = {:.2f} dB, '
                        'shift = {:.2f} dB)\n').format(key, error_type,
                                                       avg_error, avg_shift)
                    test_failed = True
                elif rssi_failure:
                    test_message = test_message + (
                        '{} failed (ignored) ({} error = {:.2f} dB, '
                        'shift = {:.2f} dB)\n').format(key, error_type,
                                                       avg_error, avg_shift)
                else:
                    test_message = test_message + (
                        '{} passed ({} error = {:.2f} dB, '
                        'shift = {:.2f} dB)\n').format(key, error_type,
                                                       avg_error, avg_shift)
        if test_failed:
            asserts.fail(test_message)
        asserts.explicit_pass(test_message)

    def post_process_rssi_sweep(self, rssi_result):
        """Postprocesses and saves JSON formatted results.

        Args:
            rssi_result: dict containing attenuation, rssi and other meta
            data
        Returns:
            postprocessed_results: compiled arrays of RSSI data used in
            pass/fail check
        """
        # Save output as text file
        results_file_path = os.path.join(self.log_path, self.current_test_name)
        with open(results_file_path, 'w') as results_file:
            json.dump(rssi_result, results_file, indent=4)
        # Compile results into arrays of RSSIs suitable for plotting
        # yapf: disable
        postprocessed_results = collections.OrderedDict(
            [('signal_poll_rssi', {}),
             ('signal_poll_avg_rssi', {}),
             ('scan_rssi', {}),
             ('chain_0_rssi', {}),
             ('chain_1_rssi', {}),
             ('total_attenuation', []),
             ('predicted_rssi', [])])
        # yapf: enable
        for key, val in postprocessed_results.items():
            if 'scan_rssi' in key:
                postprocessed_results[key]['data'] = [
                    x for data_point in rssi_result['rssi_result'] for x in
                    data_point[key][rssi_result['connected_bssid']]['data']
                ]
                postprocessed_results[key]['mean'] = [
                    x[key][rssi_result['connected_bssid']]['mean']
                    for x in rssi_result['rssi_result']
                ]
                postprocessed_results[key]['stdev'] = [
                    x[key][rssi_result['connected_bssid']]['stdev']
                    for x in rssi_result['rssi_result']
                ]
            elif 'predicted_rssi' in key:
                postprocessed_results['total_attenuation'] = [
                    att + rssi_result['fixed_attenuation'] +
                    rssi_result['dut_front_end_loss']
                    for att in rssi_result['attenuation']
                ]
                postprocessed_results['predicted_rssi'] = [
                    rssi_result['ap_tx_power'] - att
                    for att in postprocessed_results['total_attenuation']
                ]
            elif 'rssi' in key:
                postprocessed_results[key]['data'] = [
                    x for data_point in rssi_result['rssi_result']
                    for x in data_point[key]['data']
                ]
                postprocessed_results[key]['mean'] = [
                    x[key]['mean'] for x in rssi_result['rssi_result']
                ]
                postprocessed_results[key]['stdev'] = [
                    x[key]['stdev'] for x in rssi_result['rssi_result']
                ]
        # Compute RSSI errors
        for key, val in postprocessed_results.items():
            if 'chain' in key:
                postprocessed_results[key]['error'] = [
                    postprocessed_results[key]['mean'][idx] + CONST_3dB -
                    postprocessed_results['predicted_rssi'][idx]
                    for idx in range(
                        len(postprocessed_results['predicted_rssi']))
                ]
            elif 'rssi' in key and 'predicted' not in key:
                postprocessed_results[key]['error'] = [
                    postprocessed_results[key]['mean'][idx] -
                    postprocessed_results['predicted_rssi'][idx]
                    for idx in range(
                        len(postprocessed_results['predicted_rssi']))
                ]
        return postprocessed_results

    def plot_rssi_vs_attenuation(self, postprocessed_results):
        """Function to plot RSSI vs attenuation sweeps

        Args:
            postprocessed_results: compiled arrays of RSSI data.
        """
        figure = wputils.BokehFigure(self.current_test_name,
                                     x_label='Attenuation (dB)',
                                     primary_y_label='RSSI (dBm)')
        figure.add_line(postprocessed_results['total_attenuation'],
                        postprocessed_results['signal_poll_rssi']['mean'],
                        'Signal Poll RSSI',
                        marker='circle')
        figure.add_line(postprocessed_results['total_attenuation'],
                        postprocessed_results['scan_rssi']['mean'],
                        'Scan RSSI',
                        marker='circle')
        figure.add_line(postprocessed_results['total_attenuation'],
                        postprocessed_results['chain_0_rssi']['mean'],
                        'Chain 0 RSSI',
                        marker='circle')
        figure.add_line(postprocessed_results['total_attenuation'],
                        postprocessed_results['chain_1_rssi']['mean'],
                        'Chain 1 RSSI',
                        marker='circle')
        figure.add_line(postprocessed_results['total_attenuation'],
                        postprocessed_results['predicted_rssi'],
                        'Predicted RSSI',
                        marker='circle')

        output_file_path = os.path.join(self.log_path,
                                        self.current_test_name + '.html')
        figure.generate_figure(output_file_path)

    def plot_rssi_vs_time(self, rssi_result, postprocessed_results,
                          center_curves):
        """Function to plot RSSI vs time.

        Args:
            rssi_result: dict containing raw RSSI data
            postprocessed_results: compiled arrays of RSSI data
            center_curvers: boolean indicating whether to shift curves to align
            them with predicted RSSIs
        """
        figure = wputils.BokehFigure(
            self.current_test_name,
            x_label='Time (s)',
            primary_y_label=center_curves * 'Centered' + 'RSSI (dBm)',
        )

        # yapf: disable
        rssi_time_series = collections.OrderedDict(
            [('signal_poll_rssi', []),
             ('signal_poll_avg_rssi', []),
             ('scan_rssi', []),
             ('chain_0_rssi', []),
             ('chain_1_rssi', []),
             ('predicted_rssi', [])])
        # yapf: enable
        for key, val in rssi_time_series.items():
            if 'predicted_rssi' in key:
                rssi_time_series[key] = [
                    x for x in postprocessed_results[key] for copies in range(
                        len(rssi_result['rssi_result'][0]['signal_poll_rssi']
                            ['data']))
                ]
            elif 'rssi' in key:
                if center_curves:
                    filtered_error = [
                        x for x in postprocessed_results[key]['error']
                        if not math.isnan(x)
                    ]
                    if filtered_error:
                        avg_shift = statistics.mean(filtered_error)
                    else:
                        avg_shift = 0
                    rssi_time_series[key] = [
                        x - avg_shift
                        for x in postprocessed_results[key]['data']
                    ]
                else:
                    rssi_time_series[key] = postprocessed_results[key]['data']
            time_vec = [
                self.testclass_params['polling_frequency'] * x
                for x in range(len(rssi_time_series[key]))
            ]
            if len(rssi_time_series[key]) > 0:
                figure.add_line(time_vec, rssi_time_series[key], key)

        output_file_path = os.path.join(self.log_path,
                                        self.current_test_name + '.html')
        figure.generate_figure(output_file_path)

    def plot_rssi_distribution(self, postprocessed_results):
        """Function to plot RSSI distributions.

        Args:
            postprocessed_results: compiled arrays of RSSI data
        """
        monitored_rssis = ['signal_poll_rssi', 'chain_0_rssi', 'chain_1_rssi']

        rssi_dist = collections.OrderedDict()
        for rssi_key in monitored_rssis:
            rssi_data = postprocessed_results[rssi_key]
            rssi_dist[rssi_key] = collections.OrderedDict()
            unique_rssi = sorted(set(rssi_data['data']))
            rssi_counts = []
            for value in unique_rssi:
                rssi_counts.append(rssi_data['data'].count(value))
            total_count = sum(rssi_counts)
            rssi_dist[rssi_key]['rssi_values'] = unique_rssi
            rssi_dist[rssi_key]['rssi_pdf'] = [
                x / total_count for x in rssi_counts
            ]
            rssi_dist[rssi_key]['rssi_cdf'] = []
            cum_prob = 0
            for prob in rssi_dist[rssi_key]['rssi_pdf']:
                cum_prob += prob
                rssi_dist[rssi_key]['rssi_cdf'].append(cum_prob)

        figure = wputils.BokehFigure(self.current_test_name,
                                     x_label='RSSI (dBm)',
                                     primary_y_label='p(RSSI = x)',
                                     secondary_y_label='p(RSSI <= x)')
        for rssi_key, rssi_data in rssi_dist.items():
            figure.add_line(x_data=rssi_data['rssi_values'],
                            y_data=rssi_data['rssi_pdf'],
                            legend='{} PDF'.format(rssi_key),
                            y_axis='default')
            figure.add_line(x_data=rssi_data['rssi_values'],
                            y_data=rssi_data['rssi_cdf'],
                            legend='{} CDF'.format(rssi_key),
                            y_axis='secondary')
        output_file_path = os.path.join(self.log_path,
                                        self.current_test_name + '_dist.html')
        figure.generate_figure(output_file_path)

    def run_rssi_test(self, testcase_params):
        """Test function to run RSSI tests.

        The function runs an RSSI test in the current device/AP configuration.
        Function is called from another wrapper function that sets up the
        testbed for the RvR test

        Args:
            testcase_params: dict containing test-specific parameters
        Returns:
            rssi_result: dict containing rssi_result and meta data
        """
        # Run test and log result
        rssi_result = collections.OrderedDict()
        rssi_result['test_name'] = self.current_test_name
        rssi_result['testcase_params'] = testcase_params
        rssi_result['ap_settings'] = self.access_point.ap_settings.copy()
        rssi_result['attenuation'] = list(testcase_params['rssi_atten_range'])
        rssi_result['connected_bssid'] = self.main_network[
            testcase_params['band']].get('BSSID', '00:00:00:00')
        channel_mode_combo = '{}_{}'.format(str(testcase_params['channel']),
                                            testcase_params['mode'])
        channel_str = str(testcase_params['channel'])
        if channel_mode_combo in self.testbed_params['ap_tx_power']:
            rssi_result['ap_tx_power'] = self.testbed_params['ap_tx_power'][
                channel_mode_combo]
        else:
            rssi_result['ap_tx_power'] = self.testbed_params['ap_tx_power'][
                str(testcase_params['channel'])]
        rssi_result['fixed_attenuation'] = self.testbed_params[
            'fixed_attenuation'][channel_str]
        rssi_result['dut_front_end_loss'] = self.testbed_params[
            'dut_front_end_loss'][channel_str]

        self.log.info('Start running RSSI test.')
        rssi_result['rssi_result'] = []
        rssi_result['llstats'] = []
        llstats_obj = wputils.LinkLayerStats(self.dut)
        # Start iperf traffic if required by test
        if testcase_params['active_traffic'] and testcase_params[
                'traffic_type'] == 'iperf':
            self.iperf_server.start(tag=0)
            if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
                iperf_server_address = self.dut_ip
            else:
                iperf_server_address = wputils.get_server_address(
                    self.remote_server, self.dut_ip, '255.255.255.0')
            executor = ThreadPoolExecutor(max_workers=1)
            thread_future = executor.submit(
                self.iperf_client.start, iperf_server_address,
                testcase_params['iperf_args'], 0,
                testcase_params['traffic_timeout'] + SHORT_SLEEP)
            executor.shutdown(wait=False)
        elif testcase_params['active_traffic'] and testcase_params[
                'traffic_type'] == 'ping':
            thread_future = wputils.get_ping_stats_nb(
                self.remote_server, self.dut_ip,
                testcase_params['traffic_timeout'], 0.02, 64)
        for atten in testcase_params['rssi_atten_range']:
            # Set Attenuation
            self.log.info('Setting attenuation to {} dB'.format(atten))
            for attenuator in self.attenuators:
                attenuator.set_atten(atten)
            llstats_obj.update_stats()
            current_rssi = collections.OrderedDict()
            current_rssi = wputils.get_connected_rssi(
                self.dut, testcase_params['connected_measurements'],
                self.testclass_params['polling_frequency'],
                testcase_params['first_measurement_delay'])
            current_rssi['scan_rssi'] = wputils.get_scan_rssi(
                self.dut, testcase_params['tracked_bssid'],
                testcase_params['scan_measurements'])
            rssi_result['rssi_result'].append(current_rssi)
            llstats_obj.update_stats()
            curr_llstats = llstats_obj.llstats_incremental.copy()
            rssi_result['llstats'].append(curr_llstats)
            self.log.info(
                'Connected RSSI at {0:.2f} dB is {1:.2f} [{2:.2f}, {3:.2f}] dB'
                .format(atten, current_rssi['signal_poll_rssi']['mean'],
                        current_rssi['chain_0_rssi']['mean'],
                        current_rssi['chain_1_rssi']['mean']))
        # Stop iperf traffic if needed
        for attenuator in self.attenuators:
            attenuator.set_atten(0)
        if testcase_params['active_traffic']:
            thread_future.result()
            if testcase_params['traffic_type'] == 'iperf':
                self.iperf_server.stop()
        return rssi_result

    def setup_ap(self, testcase_params):
        """Function that gets devices ready for the test.

        Args:
            testcase_params: dict containing test-specific parameters
        """
        if '2G' in testcase_params['band']:
            frequency = wutils.WifiEnums.channel_2G_to_freq[
                testcase_params['channel']]
        else:
            frequency = wutils.WifiEnums.channel_5G_to_freq[
                testcase_params['channel']]
        if frequency in wutils.WifiEnums.DFS_5G_FREQUENCIES:
            self.access_point.set_region(self.testbed_params['DFS_region'])
        else:
            self.access_point.set_region(self.testbed_params['default_region'])
        self.access_point.set_channel(testcase_params['band'],
                                      testcase_params['channel'])
        self.access_point.set_bandwidth(testcase_params['band'],
                                        testcase_params['mode'])
        self.log.info('Access Point Configuration: {}'.format(
            self.access_point.ap_settings))

    def setup_dut(self, testcase_params):
        """Sets up the DUT in the configuration required by the test."""
        # Check battery level before test
        if not wputils.health_check(self.dut, 10):
            asserts.skip('Battery level too low. Skipping test.')
        # Turn screen off to preserve battery
        self.dut.go_to_sleep()
        if wputils.validate_network(self.dut,
                                    testcase_params['test_network']['SSID']):
            self.log.info('Already connected to desired network')
        else:
            wutils.wifi_toggle_state(self.dut, True)
            wutils.reset_wifi(self.dut)
            self.main_network[testcase_params['band']][
                'channel'] = testcase_params['channel']
            wutils.set_wifi_country_code(self.dut,
                self.testclass_params['country_code'])
            wutils.wifi_connect(self.dut,
                                self.main_network[testcase_params['band']],
                                num_of_tries=5)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses('wlan0')[0]

    def setup_rssi_test(self, testcase_params):
        """Main function to test RSSI.

        The function sets up the AP in the correct channel and mode
        configuration and called rssi_test to sweep attenuation and measure
        RSSI

        Args:
            testcase_params: dict containing test-specific parameters
        Returns:
            rssi_result: dict containing rssi_results and meta data
        """
        # Configure AP
        self.setup_ap(testcase_params)
        # Initialize attenuators
        for attenuator in self.attenuators:
            attenuator.set_atten(testcase_params['rssi_atten_range'][0])
        # Connect DUT to Network
        self.setup_dut(testcase_params)

    def get_traffic_timeout(self, testcase_params):
        """Function to comput iperf session length required in RSSI test.

        Args:
            testcase_params: dict containing test-specific parameters
        Returns:
            traffic_timeout: length of iperf session required in rssi test
        """
        atten_step_duration = testcase_params['first_measurement_delay'] + (
            testcase_params['connected_measurements'] *
            self.testclass_params['polling_frequency']
        ) + testcase_params['scan_measurements'] * MED_SLEEP
        timeout = len(testcase_params['rssi_atten_range']
                      ) * atten_step_duration + MED_SLEEP
        return timeout

    def compile_rssi_vs_atten_test_params(self, testcase_params):
        """Function to complete compiling test-specific parameters

        Args:
            testcase_params: dict containing test-specific parameters
        """
        testcase_params.update(
            connected_measurements=self.
            testclass_params['rssi_vs_atten_connected_measurements'],
            scan_measurements=self.
            testclass_params['rssi_vs_atten_scan_measurements'],
            first_measurement_delay=MED_SLEEP,
            rssi_under_test=self.testclass_params['rssi_vs_atten_metrics'],
            absolute_accuracy=1)

        testcase_params['band'] = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[
            testcase_params['band']]
        testcase_params['tracked_bssid'] = [
            self.main_network[testcase_params['band']].get(
                'BSSID', '00:00:00:00')
        ]

        num_atten_steps = int((self.testclass_params['rssi_vs_atten_stop'] -
                               self.testclass_params['rssi_vs_atten_start']) /
                              self.testclass_params['rssi_vs_atten_step'])
        testcase_params['rssi_atten_range'] = [
            self.testclass_params['rssi_vs_atten_start'] +
            x * self.testclass_params['rssi_vs_atten_step']
            for x in range(0, num_atten_steps)
        ]
        testcase_params['traffic_timeout'] = self.get_traffic_timeout(
            testcase_params)

        if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
            testcase_params['iperf_args'] = '-i 1 -t {} -J'.format(
                testcase_params['traffic_timeout'])
        else:
            testcase_params['iperf_args'] = '-i 1 -t {} -J -R'.format(
                testcase_params['traffic_timeout'])
        return testcase_params

    def compile_rssi_stability_test_params(self, testcase_params):
        """Function to complete compiling test-specific parameters

        Args:
            testcase_params: dict containing test-specific parameters
        """
        testcase_params.update(
            connected_measurements=int(
                self.testclass_params['rssi_stability_duration'] /
                self.testclass_params['polling_frequency']),
            scan_measurements=0,
            first_measurement_delay=MED_SLEEP,
            rssi_atten_range=self.testclass_params['rssi_stability_atten'])
        testcase_params['band'] = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[
            testcase_params['band']]
        testcase_params['tracked_bssid'] = [
            self.main_network[testcase_params['band']].get(
                'BSSID', '00:00:00:00')
        ]

        testcase_params['traffic_timeout'] = self.get_traffic_timeout(
            testcase_params)
        if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
            testcase_params['iperf_args'] = '-i 1 -t {} -J'.format(
                testcase_params['traffic_timeout'])
        else:
            testcase_params['iperf_args'] = '-i 1 -t {} -J -R'.format(
                testcase_params['traffic_timeout'])
        return testcase_params

    def compile_rssi_tracking_test_params(self, testcase_params):
        """Function to complete compiling test-specific parameters

        Args:
            testcase_params: dict containing test-specific parameters
        """
        testcase_params.update(connected_measurements=int(
            1 / self.testclass_params['polling_frequency']),
                               scan_measurements=0,
                               first_measurement_delay=0,
                               rssi_under_test=['signal_poll_rssi'],
                               absolute_accuracy=0)
        testcase_params['band'] = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[
            testcase_params['band']]
        testcase_params['tracked_bssid'] = [
            self.main_network[testcase_params['band']].get(
                'BSSID', '00:00:00:00')
        ]

        rssi_atten_range = []
        for waveform in self.testclass_params['rssi_tracking_waveforms']:
            waveform_vector = []
            for section in range(len(waveform['atten_levels']) - 1):
                section_limits = waveform['atten_levels'][section:section + 2]
                up_down = (1 - 2 * (section_limits[1] < section_limits[0]))
                temp_section = list(
                    range(section_limits[0], section_limits[1] + up_down,
                          up_down * waveform['step_size']))
                temp_section = [
                    temp_section[idx] for idx in range(len(temp_section))
                    for n in range(waveform['step_duration'])
                ]
                waveform_vector += temp_section
            waveform_vector = waveform_vector * waveform['repetitions']
            rssi_atten_range = rssi_atten_range + waveform_vector
        testcase_params['rssi_atten_range'] = rssi_atten_range
        testcase_params['traffic_timeout'] = self.get_traffic_timeout(
            testcase_params)

        if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
            testcase_params['iperf_args'] = '-i 1 -t {} -J'.format(
                testcase_params['traffic_timeout'])
        else:
            testcase_params['iperf_args'] = '-i 1 -t {} -J -R'.format(
                testcase_params['traffic_timeout'])
        return testcase_params

    def _test_rssi_vs_atten(self, testcase_params):
        """Function that gets called for each test case of rssi_vs_atten

        The function gets called in each rssi test case. The function
        customizes the test based on the test name of the test that called it

        Args:
            testcase_params: dict containing test-specific parameters
        """
        testcase_params = self.compile_rssi_vs_atten_test_params(
            testcase_params)

        self.setup_rssi_test(testcase_params)
        rssi_result = self.run_rssi_test(testcase_params)
        rssi_result['postprocessed_results'] = self.post_process_rssi_sweep(
            rssi_result)
        self.testclass_results.append(rssi_result)
        self.plot_rssi_vs_attenuation(rssi_result['postprocessed_results'])
        self.pass_fail_check_rssi_accuracy(
            testcase_params, rssi_result['postprocessed_results'])

    def _test_rssi_stability(self, testcase_params):
        """ Function that gets called for each test case of rssi_stability

        The function gets called in each stability test case. The function
        customizes test based on the test name of the test that called it
        """
        testcase_params = self.compile_rssi_stability_test_params(
            testcase_params)

        self.setup_rssi_test(testcase_params)
        rssi_result = self.run_rssi_test(testcase_params)
        rssi_result['postprocessed_results'] = self.post_process_rssi_sweep(
            rssi_result)
        self.testclass_results.append(rssi_result)
        self.plot_rssi_vs_time(rssi_result,
                               rssi_result['postprocessed_results'], 1)
        self.plot_rssi_distribution(rssi_result['postprocessed_results'])
        self.pass_fail_check_rssi_stability(
            testcase_params, rssi_result['postprocessed_results'])

    def _test_rssi_tracking(self, testcase_params):
        """ Function that gets called for each test case of rssi_tracking

        The function gets called in each rssi test case. The function
        customizes the test based on the test name of the test that called it
        """
        testcase_params = self.compile_rssi_tracking_test_params(
            testcase_params)

        self.setup_rssi_test(testcase_params)
        rssi_result = self.run_rssi_test(testcase_params)
        rssi_result['postprocessed_results'] = self.post_process_rssi_sweep(
            rssi_result)
        self.testclass_results.append(rssi_result)
        self.plot_rssi_vs_time(rssi_result,
                               rssi_result['postprocessed_results'], 1)
        self.pass_fail_check_rssi_accuracy(
            testcase_params, rssi_result['postprocessed_results'])

    def generate_test_cases(self, test_types, channels, modes, traffic_modes):
        """Function that auto-generates test cases for a test class."""
        test_cases = []
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }

        for channel, mode, traffic_mode, test_type in itertools.product(
                channels, modes, traffic_modes, test_types):
            if channel not in allowed_configs[mode]:
                continue
            test_name = test_type + '_ch{}_{}_{}'.format(
                channel, mode, traffic_mode)
            testcase_params = collections.OrderedDict(
                channel=channel,
                mode=mode,
                active_traffic=(traffic_mode == 'ActiveTraffic'),
                traffic_type=self.user_params['rssi_test_params']
                ['traffic_type'],
            )
            test_function = getattr(self, '_{}'.format(test_type))
            setattr(self, test_name, partial(test_function, testcase_params))
            test_cases.append(test_name)
        return test_cases


class WifiRssi_2GHz_ActiveTraffic_Test(WifiRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ['test_rssi_stability', 'test_rssi_vs_atten'], [1, 2, 6, 10, 11],
            ['VHT20'], ['ActiveTraffic'])


class WifiRssi_5GHz_ActiveTraffic_Test(WifiRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ['test_rssi_stability', 'test_rssi_vs_atten'],
            [36, 40, 44, 48, 149, 153, 157, 161], ['VHT20', 'VHT40', 'VHT80'],
            ['ActiveTraffic'])


class WifiRssi_AllChannels_ActiveTraffic_Test(WifiRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ['test_rssi_stability', 'test_rssi_vs_atten'],
            [1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            ['VHT20', 'VHT40', 'VHT80'], ['ActiveTraffic'])


class WifiRssi_SampleChannels_NoTraffic_Test(WifiRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ['test_rssi_stability', 'test_rssi_vs_atten'], [6, 36, 149],
            ['VHT20', 'VHT40', 'VHT80'], ['NoTraffic'])


class WifiRssiTrackingTest(WifiRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(['test_rssi_tracking'],
                                              [6, 36, 149],
                                              ['VHT20', 'VHT40', 'VHT80'],
                                              ['ActiveTraffic', 'NoTraffic'])


# Over-the air version of RSSI tests
class WifiOtaRssiTest(WifiRssiTest):
    """Class to test over-the-air rssi tests.

    This class implements measures WiFi RSSI tests in an OTA chamber.
    It allows setting orientation and other chamber parameters to study
    performance in varying channel conditions
    """
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_test_metrics = False

    def setup_class(self):
        WifiRssiTest.setup_class(self)
        self.ota_chamber = ota_chamber.create(
            self.user_params['OTAChamber'])[0]

    def teardown_class(self):
        self.ota_chamber.reset_chamber()
        self.process_testclass_results()

    def teardown_test(self):
        if self.ota_chamber.current_mode == 'continuous':
            self.ota_chamber.reset_chamber()

    def extract_test_id(self, testcase_params, id_fields):
        test_id = collections.OrderedDict(
            (param, testcase_params[param]) for param in id_fields)
        return test_id

    def process_testclass_results(self):
        """Saves all test results to enable comparison."""
        testclass_data = collections.OrderedDict()
        for test_result in self.testclass_results:
            current_params = test_result['testcase_params']

            channel = current_params['channel']
            channel_data = testclass_data.setdefault(
                channel,
                collections.OrderedDict(orientation=[],
                                        rssi=collections.OrderedDict(
                                            signal_poll_rssi=[],
                                            chain_0_rssi=[],
                                            chain_1_rssi=[])))

            channel_data['orientation'].append(current_params['orientation'])
            channel_data['rssi']['signal_poll_rssi'].append(
                test_result['postprocessed_results']['signal_poll_rssi']
                ['mean'][0])
            channel_data['rssi']['chain_0_rssi'].append(
                test_result['postprocessed_results']['chain_0_rssi']['mean']
                [0])
            channel_data['rssi']['chain_1_rssi'].append(
                test_result['postprocessed_results']['chain_1_rssi']['mean']
                [0])

        chamber_mode = self.testclass_results[0]['testcase_params'][
            'chamber_mode']
        if chamber_mode == 'orientation':
            x_label = 'Angle (deg)'
        elif chamber_mode == 'stepped stirrers':
            x_label = 'Position Index'

        # Publish test class metrics
        for channel, channel_data in testclass_data.items():
            for rssi_metric, rssi_metric_value in channel_data['rssi'].items():
                metric_name = 'ota_summary_ch{}.avg_{}'.format(
                    channel, rssi_metric)
                metric_value = numpy.mean(rssi_metric_value)
                self.testclass_metric_logger.add_metric(
                    metric_name, metric_value)

        # Plot test class results
        plots = []
        for channel, channel_data in testclass_data.items():
            current_plot = wputils.BokehFigure(
                title='Channel {} - Rssi vs. Position'.format(channel),
                x_label=x_label,
                primary_y_label='RSSI (dBm)',
            )
            for rssi_metric, rssi_metric_value in channel_data['rssi'].items():
                legend = rssi_metric
                current_plot.add_line(channel_data['orientation'],
                                      rssi_metric_value, legend)
            current_plot.generate_figure()
            plots.append(current_plot)
        current_context = context.get_current_context().get_full_output_path()
        plot_file_path = os.path.join(current_context, 'results.html')
        wputils.BokehFigure.save_figures(plots, plot_file_path)

    def setup_rssi_test(self, testcase_params):
        # Test setup
        WifiRssiTest.setup_rssi_test(self, testcase_params)
        if testcase_params['chamber_mode'] == 'StirrersOn':
            self.ota_chamber.start_continuous_stirrers()
        else:
            self.ota_chamber.set_orientation(testcase_params['orientation'])

    def compile_ota_rssi_test_params(self, testcase_params):
        """Function to complete compiling test-specific parameters

        Args:
            testcase_params: dict containing test-specific parameters
        """
        if "rssi_over_orientation" in self.test_name:
            rssi_test_duration = self.testclass_params[
                'rssi_over_orientation_duration']
        elif "rssi_variation" in self.test_name:
            rssi_test_duration = self.testclass_params[
                'rssi_variation_duration']

        testcase_params.update(
            connected_measurements=int(
                rssi_test_duration /
                self.testclass_params['polling_frequency']),
            scan_measurements=0,
            first_measurement_delay=MED_SLEEP,
            rssi_atten_range=[
                self.testclass_params['rssi_ota_test_attenuation']
            ])
        testcase_params['band'] = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[
            testcase_params['band']]
        testcase_params['tracked_bssid'] = [
            self.main_network[testcase_params['band']].get(
                'BSSID', '00:00:00:00')
        ]

        testcase_params['traffic_timeout'] = self.get_traffic_timeout(
            testcase_params)
        if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
            testcase_params['iperf_args'] = '-i 1 -t {} -J'.format(
                testcase_params['traffic_timeout'])
        else:
            testcase_params['iperf_args'] = '-i 1 -t {} -J -R'.format(
                testcase_params['traffic_timeout'])
        return testcase_params

    def _test_ota_rssi(self, testcase_params):
        testcase_params = self.compile_ota_rssi_test_params(testcase_params)

        self.setup_rssi_test(testcase_params)
        rssi_result = self.run_rssi_test(testcase_params)
        rssi_result['postprocessed_results'] = self.post_process_rssi_sweep(
            rssi_result)
        self.testclass_results.append(rssi_result)
        self.plot_rssi_vs_time(rssi_result,
                               rssi_result['postprocessed_results'], 1)
        self.plot_rssi_distribution(rssi_result['postprocessed_results'])

    def generate_test_cases(self, test_types, channels, modes, traffic_modes,
                            chamber_modes, orientations):
        test_cases = []
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }

        for (channel, mode, traffic, chamber_mode, orientation,
             test_type) in itertools.product(channels, modes, traffic_modes,
                                             chamber_modes, orientations,
                                             test_types):
            if channel not in allowed_configs[mode]:
                continue
            test_name = test_type + '_ch{}_{}_{}_{}deg'.format(
                channel, mode, traffic, orientation)
            testcase_params = collections.OrderedDict(
                channel=channel,
                mode=mode,
                active_traffic=(traffic == 'ActiveTraffic'),
                traffic_type=self.user_params['rssi_test_params']
                ['traffic_type'],
                chamber_mode=chamber_mode,
                orientation=orientation)
            test_function = self._test_ota_rssi
            setattr(self, test_name, partial(test_function, testcase_params))
            test_cases.append(test_name)
        return test_cases


class WifiOtaRssi_Accuracy_Test(WifiOtaRssiTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(['test_rssi_vs_atten'],
                                              [6, 36, 149], ['VHT20'],
                                              ['ActiveTraffic'],
                                              ['orientation'],
                                              list(range(0, 360, 45)))


class WifiOtaRssi_StirrerVariation_Test(WifiOtaRssiTest):
    def __init__(self, controllers):
        WifiRssiTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(['test_rssi_variation'],
                                              [6, 36, 149], ['VHT20'],
                                              ['ActiveTraffic'],
                                              ['StirrersOn'], [0])


class WifiOtaRssi_TenDegree_Test(WifiOtaRssiTest):
    def __init__(self, controllers):
        WifiRssiTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(['test_rssi_over_orientation'],
                                              [6, 36, 149], ['VHT20'],
                                              ['ActiveTraffic'],
                                              ['orientation'],
                                              list(range(0, 360, 10)))
