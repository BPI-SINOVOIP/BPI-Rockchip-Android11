#!/usr/bin/env python3.4
#
#   Copyright 2017 - The Android Open Source Project
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
from acts import asserts
from acts import base_test
from acts import context
from acts import utils
from acts.controllers import iperf_server as ipf
from acts.controllers.utils_lib import ssh
from acts.metrics.loggers.blackbox import BlackboxMappedMetricLogger
from acts.test_utils.wifi import ota_chamber
from acts.test_utils.wifi import wifi_performance_test_utils as wputils
from acts.test_utils.wifi import wifi_retail_ap as retail_ap
from acts.test_utils.wifi import wifi_test_utils as wutils
from functools import partial

TEST_TIMEOUT = 10
SHORT_SLEEP = 1
MED_SLEEP = 6


class WifiThroughputStabilityTest(base_test.BaseTestClass):
    """Class to test WiFi throughput stability.

    This class tests throughput stability and identifies cases where throughput
    fluctuates over time. The class setups up the AP, configures and connects
    the phone, and runs iperf throughput test at several attenuations For an
    example config file to run this test class see
    example_connectivity_performance_ap_sta.json.
    """
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        # Define metrics to be uploaded to BlackBox
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_testcase_metrics = True
        # Generate test cases
        self.tests = self.generate_test_cases([6, 36, 149],
                                              ['VHT20', 'VHT40', 'VHT80'],
                                              ['TCP', 'UDP'], ['DL', 'UL'],
                                              ['high', 'low'])

    def generate_test_cases(self, channels, modes, traffic_types,
                            traffic_directions, signal_levels):
        """Function that auto-generates test cases for a test class."""
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }
        test_cases = []
        for channel, mode, traffic_type, traffic_direction, signal_level in itertools.product(
                channels, modes, traffic_types, traffic_directions,
                signal_levels):
            if channel not in allowed_configs[mode]:
                continue
            testcase_params = collections.OrderedDict(
                channel=channel,
                mode=mode,
                traffic_type=traffic_type,
                traffic_direction=traffic_direction,
                signal_level=signal_level)
            testcase_name = ('test_tput_stability'
                             '_{}_{}_{}_ch{}_{}'.format(
                                 signal_level, traffic_type, traffic_direction,
                                 channel, mode))
            setattr(self, testcase_name,
                    partial(self._test_throughput_stability, testcase_params))
            test_cases.append(testcase_name)
        return test_cases

    def setup_class(self):
        self.dut = self.android_devices[0]
        req_params = [
            'throughput_stability_test_params', 'testbed_params',
            'main_network', 'RetailAccessPoints', 'RemoteServer'
        ]
        opt_params = ['golden_files_list']
        self.unpack_userparams(req_params, opt_params)
        self.testclass_params = self.throughput_stability_test_params
        self.num_atten = self.attenuators[0].instrument.num_atten
        self.remote_server = ssh.connection.SshConnection(
            ssh.settings.from_config(self.RemoteServer[0]['ssh_config']))
        self.iperf_server = self.iperf_servers[0]
        self.iperf_client = self.iperf_clients[0]
        self.access_point = retail_ap.create(self.RetailAccessPoints)[0]
        self.log_path = os.path.join(logging.log_path, 'test_results')
        os.makedirs(self.log_path, exist_ok=True)
        self.log.info('Access Point Configuration: {}'.format(
            self.access_point.ap_settings))
        if not hasattr(self, 'golden_files_list'):
            self.golden_files_list = [
                os.path.join(self.testbed_params['golden_results_path'], file)
                for file in os.listdir(
                    self.testbed_params['golden_results_path'])
            ]
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

    def pass_fail_check(self, test_result_dict):
        """Check the test result and decide if it passed or failed.

        Checks the throughput stability test's PASS/FAIL criteria based on
        minimum instantaneous throughput, and standard deviation.

        Args:
            test_result_dict: dict containing attenuation, throughput and other
            meta data
        """
        avg_throughput = test_result_dict['iperf_results']['avg_throughput']
        min_throughput = test_result_dict['iperf_results']['min_throughput']
        std_dev_percent = (
            test_result_dict['iperf_results']['std_deviation'] /
            test_result_dict['iperf_results']['avg_throughput']) * 100
        # Set blackbox metrics
        if self.publish_testcase_metrics:
            self.testcase_metric_logger.add_metric('avg_throughput',
                                                   avg_throughput)
            self.testcase_metric_logger.add_metric('min_throughput',
                                                   min_throughput)
            self.testcase_metric_logger.add_metric('std_dev_percent',
                                                   std_dev_percent)
        # Evaluate pass/fail
        min_throughput_check = (
            (min_throughput / avg_throughput) *
            100) > self.testclass_params['min_throughput_threshold']
        std_deviation_check = std_dev_percent < self.testclass_params[
            'std_deviation_threshold']

        if min_throughput_check and std_deviation_check:
            asserts.explicit_pass(
                'Test Passed. Throughput at {0:.2f}dB attenuation is stable. '
                'Mean throughput is {1:.2f} Mbps with a standard deviation of '
                '{2:.2f}% and dips down to {3:.2f} Mbps.'.format(
                    test_result_dict['attenuation'], avg_throughput,
                    std_dev_percent, min_throughput))
        asserts.fail(
            'Test Failed. Throughput at {0:.2f}dB attenuation is unstable. '
            'Mean throughput is {1:.2f} Mbps with a standard deviation of '
            '{2:.2f}% and dips down to {3:.2f} Mbps.'.format(
                test_result_dict['attenuation'], avg_throughput,
                std_dev_percent, min_throughput))

    def post_process_results(self, test_result):
        """Extracts results and saves plots and JSON formatted results.

        Args:
            test_result: dict containing attenuation, iPerfResult object and
            other meta data
        Returns:
            test_result_dict: dict containing post-processed results including
            avg throughput, other metrics, and other meta data
        """
        # Save output as text file
        test_name = self.current_test_name
        results_file_path = os.path.join(self.log_path,
                                         '{}.txt'.format(test_name))
        test_result_dict = {}
        test_result_dict['ap_settings'] = test_result['ap_settings'].copy()
        test_result_dict['attenuation'] = test_result['attenuation']
        if test_result['iperf_result'].instantaneous_rates:
            instantaneous_rates_Mbps = [
                rate * 8 * (1.024**2)
                for rate in test_result['iperf_result'].instantaneous_rates[
                    self.testclass_params['iperf_ignored_interval']:-1]
            ]
        else:
            instantaneous_rates_Mbps = float('nan')
        test_result_dict['iperf_results'] = {
            'instantaneous_rates':
            instantaneous_rates_Mbps,
            'avg_throughput':
            numpy.mean(instantaneous_rates_Mbps),
            'std_deviation':
            test_result['iperf_result'].get_std_deviation(
                self.testclass_params['iperf_ignored_interval']) * 8,
            'min_throughput':
            min(instantaneous_rates_Mbps)
        }
        with open(results_file_path, 'w') as results_file:
            json.dump(test_result_dict, results_file)
        # Plot and save
        figure = wputils.BokehFigure(test_name,
                                     x_label='Time (s)',
                                     primary_y_label='Throughput (Mbps)')
        time_data = list(range(0, len(instantaneous_rates_Mbps)))
        figure.add_line(time_data,
                        instantaneous_rates_Mbps,
                        legend=self.current_test_name,
                        marker='circle')
        output_file_path = os.path.join(self.log_path,
                                        '{}.html'.format(test_name))
        figure.generate_figure(output_file_path)
        return test_result_dict

    def setup_ap(self, testcase_params):
        """Sets up the access point in the configuration required by the test.

        Args:
            testcase_params: dict containing AP and other test params
        """
        band = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        if '2G' in band:
            frequency = wutils.WifiEnums.channel_2G_to_freq[
                testcase_params['channel']]
        else:
            frequency = wutils.WifiEnums.channel_5G_to_freq[
                testcase_params['channel']]
        if frequency in wutils.WifiEnums.DFS_5G_FREQUENCIES:
            self.access_point.set_region(self.testbed_params['DFS_region'])
        else:
            self.access_point.set_region(self.testbed_params['default_region'])
        self.access_point.set_channel(band, testcase_params['channel'])
        self.access_point.set_bandwidth(band, testcase_params['mode'])
        self.log.info('Access Point Configuration: {}'.format(
            self.access_point.ap_settings))

    def setup_dut(self, testcase_params):
        """Sets up the DUT in the configuration required by the test.

        Args:
            testcase_params: dict containing AP and other test params
        """
        # Check battery level before test
        if not wputils.health_check(self.dut, 10):
            asserts.skip('Battery level too low. Skipping test.')
        # Turn screen off to preserve battery
        self.dut.go_to_sleep()
        band = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        if wputils.validate_network(self.dut,
                                    testcase_params['test_network']['SSID']):
            self.log.info('Already connected to desired network')
        else:
            wutils.wifi_toggle_state(self.dut, True)
            wutils.reset_wifi(self.dut)
            wutils.set_wifi_country_code(self.dut,
                self.testclass_params['country_code'])
            self.main_network[band]['channel'] = testcase_params['channel']
            wutils.wifi_connect(self.dut,
                                testcase_params['test_network'],
                                num_of_tries=5,
                                check_connectivity=False)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses('wlan0')[0]

    def setup_throughput_stability_test(self, testcase_params):
        """Function that gets devices ready for the test.

        Args:
            testcase_params: dict containing test-specific parameters
        """
        # Configure AP
        self.setup_ap(testcase_params)
        # Set attenuator to 0 dB
        self.log.info('Setting attenuation to {} dB'.format(
            testcase_params['atten_level']))
        for attenuator in self.attenuators:
            attenuator.set_atten(testcase_params['atten_level'])
        # Reset, configure, and connect DUT
        self.setup_dut(testcase_params)
        if isinstance(self.iperf_server, ipf.IPerfServerOverAdb):
            testcase_params['iperf_server_address'] = self.dut_ip
        else:
            testcase_params[
                'iperf_server_address'] = wputils.get_server_address(
                    self.remote_server, self.dut_ip, '255.255.255.0')

    def run_throughput_stability_test(self, testcase_params):
        """Main function to test throughput stability.

        The function sets up the AP in the correct channel and mode
        configuration and runs an iperf test to measure throughput.

        Args:
            testcase_params: dict containing test specific parameters
        Returns:
            test_result: dict containing test result and meta data
        """
        # Run test and log result
        # Start iperf session
        self.log.info('Starting iperf test.')
        self.iperf_server.start(tag=str(testcase_params['atten_level']))
        client_output_path = self.iperf_client.start(
            testcase_params['iperf_server_address'],
            testcase_params['iperf_args'], str(testcase_params['atten_level']),
            self.testclass_params['iperf_duration'] + TEST_TIMEOUT)
        server_output_path = self.iperf_server.stop()
        # Set attenuator to 0 dB
        for attenuator in self.attenuators:
            attenuator.set_atten(0)
        # Parse and log result
        if testcase_params['use_client_output']:
            iperf_file = client_output_path
        else:
            iperf_file = server_output_path
        try:
            iperf_result = ipf.IPerfResult(iperf_file)
        except:
            asserts.fail('Cannot get iperf result.')
        test_result = collections.OrderedDict()
        test_result['testcase_params'] = testcase_params.copy()
        test_result['ap_settings'] = self.access_point.ap_settings.copy()
        test_result['attenuation'] = testcase_params['atten_level']
        test_result['iperf_result'] = iperf_result
        self.testclass_results.append(test_result)
        return test_result

    def get_target_atten_tput(self, testcase_params):
        """Function gets attenuation used for test

        The function fetches the attenuation at which the test should be
        performed, and the expected target average throughput.

        Args:
            testcase_params: dict containing test specific parameters
        Returns:
            test_target: dict containing target test attenuation and expected
            throughput
        """
        # Fetch the golden RvR results
        rvr_golden_file_name = 'test_rvr_' + '_'.join(
            self.current_test_name.split('_')[4:])
        try:
            golden_path = next(file_name
                               for file_name in self.golden_files_list
                               if rvr_golden_file_name in file_name)
        except:
            asserts.fail('Test failed. Golden data not found.')

        with open(golden_path, 'r') as golden_file:
            golden_results = json.load(golden_file)
        test_target = {}
        if testcase_params['signal_level'] == 'low':
            # Get last test point where throughput is above
            throughput_below_target = [
                x < self.testclass_params['low_throughput_target']
                for x in golden_results['throughput_receive']
            ]
            atten_idx = throughput_below_target.index(1) - 1
            test_target['target_attenuation'] = golden_results['attenuation'][
                atten_idx]
            test_target['target_throughput'] = golden_results[
                'throughput_receive'][atten_idx]
        if testcase_params['signal_level'] == 'high':
            # Test at lowest attenuation point
            test_target['target_attenuation'] = golden_results['attenuation'][
                0]
            test_target['target_throughput'] = golden_results[
                'throughput_receive'][0]
        return test_target

    def compile_test_params(self, testcase_params):
        """Function that completes setting the test case parameters."""
        band = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[band]
        testcase_params['test_target'] = self.get_target_atten_tput(
            testcase_params)
        testcase_params['atten_level'] = testcase_params['test_target'][
            'target_attenuation']
        self.atten_level = testcase_params['atten_level']

        if (testcase_params['traffic_direction'] == 'DL'
                and not isinstance(self.iperf_server, ipf.IPerfServerOverAdb)
            ) or (testcase_params['traffic_direction'] == 'UL'
                  and isinstance(self.iperf_server, ipf.IPerfServerOverAdb)):
            testcase_params['iperf_args'] = wputils.get_iperf_arg_string(
                duration=self.testclass_params['iperf_duration'],
                reverse_direction=1,
                traffic_type=testcase_params['traffic_type'])
            testcase_params['use_client_output'] = True
        else:
            testcase_params['iperf_args'] = wputils.get_iperf_arg_string(
                duration=self.testclass_params['iperf_duration'],
                reverse_direction=0,
                traffic_type=testcase_params['traffic_type'])
            testcase_params['use_client_output'] = False

        return testcase_params

    def _test_throughput_stability(self, testcase_params):
        """ Function that gets called for each test case

        The function gets called in each test case. The function customizes
        the test based on the test name of the test that called it

        Args:
            testcase_params: dict containing test specific parameters
        """
        testcase_params = self.compile_test_params(testcase_params)
        self.setup_throughput_stability_test(testcase_params)
        test_result = self.run_throughput_stability_test(testcase_params)
        test_result_postprocessed = self.post_process_results(test_result)
        self.pass_fail_check(test_result_postprocessed)


# Over-the air version of ping tests
class WifiOtaThroughputStabilityTest(WifiThroughputStabilityTest):
    """Class to test over-the-air ping

    This class tests WiFi ping performance in an OTA chamber. It enables
    setting turntable orientation and other chamber parameters to study
    performance in varying channel conditions
    """
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        # Define metrics to be uploaded to BlackBox
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_testcase_metrics = False

    def setup_class(self):
        WifiThroughputStabilityTest.setup_class(self)
        self.ota_chamber = ota_chamber.create(
            self.user_params['OTAChamber'])[0]

    def teardown_class(self):
        self.ota_chamber.reset_chamber()
        self.process_testclass_results()

    def extract_test_id(self, testcase_params, id_fields):
        test_id = collections.OrderedDict(
            (param, testcase_params[param]) for param in id_fields)
        return test_id

    def process_testclass_results(self):
        """Saves all test results to enable comparison."""
        testclass_data = collections.OrderedDict()
        for test in self.testclass_results:
            current_params = test['testcase_params']
            channel_data = testclass_data.setdefault(current_params['channel'],
                                                     collections.OrderedDict())
            test_id = tuple(
                self.extract_test_id(current_params, [
                    'mode', 'traffic_type', 'traffic_direction', 'signal_level'
                ]).items())
            test_data = channel_data.setdefault(
                test_id, collections.OrderedDict(position=[], throughput=[]))
            current_throughput = (numpy.mean(
                test['iperf_result'].instantaneous_rates[
                    self.testclass_params['iperf_ignored_interval']:-1])
                                  ) * 8 * (1.024**2)
            test_data['position'].append(current_params['position'])
            test_data['throughput'].append(current_throughput)

        chamber_mode = self.testclass_results[0]['testcase_params'][
            'chamber_mode']
        if chamber_mode == 'orientation':
            x_label = 'Angle (deg)'
        elif chamber_mode == 'stepped stirrers':
            x_label = 'Position Index'

        # Publish test class metrics
        for channel, channel_data in testclass_data.items():
            for test_id, test_data in channel_data.items():
                test_id_dict = dict(test_id)
                metric_tag = 'ota_summary_{}_{}_{}_ch{}_{}'.format(
                    test_id_dict['signal_level'], test_id_dict['traffic_type'],
                    test_id_dict['traffic_direction'], channel,
                    test_id_dict['mode'])
                metric_name = metric_tag + '.avg_throughput'
                metric_value = numpy.mean(test_data['throughput'])
                self.testclass_metric_logger.add_metric(
                    metric_name, metric_value)
                metric_name = metric_tag + '.min_throughput'
                metric_value = min(test_data['throughput'])
                self.testclass_metric_logger.add_metric(
                    metric_name, metric_value)

        # Plot test class results
        plots = []
        for channel, channel_data in testclass_data.items():
            current_plot = wputils.BokehFigure(
                title='Channel {} - Rate vs. Position'.format(channel),
                x_label=x_label,
                primary_y_label='Rate (Mbps)',
            )
            for test_id, test_data in channel_data.items():
                test_id_dict = dict(test_id)
                legend = '{}, {} {}, {} RSSI'.format(
                    test_id_dict['mode'], test_id_dict['traffic_type'],
                    test_id_dict['traffic_direction'],
                    test_id_dict['signal_level'])
                current_plot.add_line(test_data['position'],
                                      test_data['throughput'], legend)
            current_plot.generate_figure()
            plots.append(current_plot)
        current_context = context.get_current_context().get_full_output_path()
        plot_file_path = os.path.join(current_context, 'results.html')
        wputils.BokehFigure.save_figures(plots, plot_file_path)

    def setup_throughput_stability_test(self, testcase_params):
        WifiThroughputStabilityTest.setup_throughput_stability_test(
            self, testcase_params)
        # Setup turntable
        if testcase_params['chamber_mode'] == 'orientation':
            self.ota_chamber.set_orientation(testcase_params['position'])
        elif testcase_params['chamber_mode'] == 'stepped stirrers':
            self.ota_chamber.step_stirrers(testcase_params['total_positions'])

    def get_target_atten_tput(self, testcase_params):
        test_target = {}
        if testcase_params['signal_level'] == 'high':
            test_target['target_attenuation'] = self.testclass_params[
                'default_atten_levels'][0]
        elif testcase_params['signal_level'] == 'low':
            test_target['target_attenuation'] = self.testclass_params[
                'default_atten_levels'][1]
        test_target['target_throughput'] = 0
        return test_target

    def generate_test_cases(self, channels, modes, traffic_types,
                            traffic_directions, signal_levels, chamber_mode,
                            positions):
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }
        test_cases = []
        for channel, mode, position, traffic_type, signal_level, traffic_direction in itertools.product(
                channels, modes, positions, traffic_types, signal_levels,
                traffic_directions):
            if channel not in allowed_configs[mode]:
                continue
            testcase_params = collections.OrderedDict(
                channel=channel,
                mode=mode,
                traffic_type=traffic_type,
                traffic_direction=traffic_direction,
                signal_level=signal_level,
                chamber_mode=chamber_mode,
                total_positions=len(positions),
                position=position)
            testcase_name = ('test_tput_stability'
                             '_{}_{}_{}_ch{}_{}_pos{}'.format(
                                 signal_level, traffic_type, traffic_direction,
                                 channel, mode, position))
            setattr(self, testcase_name,
                    partial(self._test_throughput_stability, testcase_params))
            test_cases.append(testcase_name)
        return test_cases


class WifiOtaThroughputStability_TenDegree_Test(WifiOtaThroughputStabilityTest
                                                ):
    def __init__(self, controllers):
        WifiOtaThroughputStabilityTest.__init__(self, controllers)
        self.tests = self.generate_test_cases([6, 36, 149], ['VHT20', 'VHT80'],
                                              ['TCP'], ['DL', 'UL'],
                                              ['high', 'low'], 'orientation',
                                              list(range(0, 360, 10)))


class WifiOtaThroughputStability_45Degree_Test(WifiOtaThroughputStabilityTest):
    def __init__(self, controllers):
        WifiOtaThroughputStabilityTest.__init__(self, controllers)
        self.tests = self.generate_test_cases([6, 36, 149], ['VHT20', 'VHT80'],
                                              ['TCP'], ['DL', 'UL'],
                                              ['high', 'low'], 'orientation',
                                              list(range(0, 360, 45)))


class WifiOtaThroughputStability_SteppedStirrers_Test(
        WifiOtaThroughputStabilityTest):
    def __init__(self, controllers):
        WifiOtaThroughputStabilityTest.__init__(self, controllers)
        self.tests = self.generate_test_cases([6, 36, 149], ['VHT20', 'VHT80'],
                                              ['TCP'], ['DL', 'UL'],
                                              ['high', 'low'],
                                              'stepped stirrers',
                                              list(range(100)))
