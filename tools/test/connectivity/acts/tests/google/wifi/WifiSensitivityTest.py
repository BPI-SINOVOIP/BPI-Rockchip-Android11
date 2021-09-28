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
import csv
import itertools
import json
import logging
import numpy
import os
from acts import asserts
from acts import context
from acts import base_test
from acts import utils
from acts.controllers import iperf_client
from acts.controllers.utils_lib import ssh
from acts.metrics.loggers.blackbox import BlackboxMappedMetricLogger
from acts.test_utils.wifi import ota_chamber
from acts.test_utils.wifi import wifi_performance_test_utils as wputils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi import wifi_retail_ap as retail_ap
from functools import partial
from WifiRvrTest import WifiRvrTest
from WifiPingTest import WifiPingTest


class WifiSensitivityTest(WifiRvrTest, WifiPingTest):
    """Class to test WiFi sensitivity tests.

    This class implements measures WiFi sensitivity per rate. It heavily
    leverages the WifiRvrTest class and introduced minor differences to set
    specific rates and the access point, and implements a different pass/fail
    check. For an example config file to run this test class see
    example_connectivity_performance_ap_sta.json.
    """

    RSSI_POLL_INTERVAL = 0.2
    VALID_TEST_CONFIGS = {
        1: ['legacy', 'VHT20'],
        2: ['legacy', 'VHT20'],
        6: ['legacy', 'VHT20'],
        10: ['legacy', 'VHT20'],
        11: ['legacy', 'VHT20'],
        36: ['legacy', 'VHT20', 'VHT40', 'VHT80'],
        40: ['legacy', 'VHT20'],
        44: ['legacy', 'VHT20'],
        48: ['legacy', 'VHT20'],
        149: ['legacy', 'VHT20', 'VHT40', 'VHT80'],
        153: ['legacy', 'VHT20'],
        157: ['legacy', 'VHT20'],
        161: ['legacy', 'VHT20']
    }
    RateTuple = collections.namedtuple(('RateTuple'),
                                       ['mcs', 'streams', 'data_rate'])
    #yapf:disable
    VALID_RATES = {
        'legacy_2GHz': [
            RateTuple(54, 1, 54), RateTuple(48, 1, 48),
            RateTuple(36, 1, 36), RateTuple(24, 1, 24),
            RateTuple(18, 1, 18), RateTuple(12, 1, 12),
            RateTuple(11, 1, 11), RateTuple(9, 1, 9),
            RateTuple(6, 1, 6), RateTuple(5.5, 1, 5.5),
            RateTuple(2, 1, 2), RateTuple(1, 1, 1)],
        'legacy_5GHz': [
            RateTuple(54, 1, 54), RateTuple(48, 1, 48),
            RateTuple(36, 1, 36), RateTuple(24, 1, 24),
            RateTuple(18, 1, 18), RateTuple(12, 1, 12),
            RateTuple(9, 1, 9), RateTuple(6, 1, 6)],
        'HT20': [
            RateTuple(7, 1, 72.2), RateTuple(6, 1, 65),
            RateTuple(5, 1, 57.8), RateTuple(4, 1, 43.3),
            RateTuple(3, 1, 26), RateTuple(2, 1, 21.7),
            RateTuple(1, 1, 14.4), RateTuple(0, 1, 7.2),
            RateTuple(15, 2, 144.4), RateTuple(14, 2, 130),
            RateTuple(13, 2, 115.6), RateTuple(12, 2, 86.7),
            RateTuple(11, 2, 57.8), RateTuple(10, 2, 43.4),
            RateTuple(9, 2, 28.9), RateTuple(8, 2, 14.4)],
        'VHT20': [
            RateTuple(9, 1, 96), RateTuple(8, 1, 86.7),
            RateTuple(7, 1, 72.2), RateTuple(6, 1, 65),
            RateTuple(5, 1, 57.8), RateTuple(4, 1, 43.3),
            RateTuple(3, 1, 28.9), RateTuple(2, 1, 21.7),
            RateTuple(1, 1, 14.4), RateTuple(0, 1, 7.2),
            RateTuple(9, 2, 192), RateTuple(8, 2, 173.3),
            RateTuple(7, 2, 144.4), RateTuple(6, 2, 130.3),
            RateTuple(5, 2, 115.6), RateTuple(4, 2, 86.7),
            RateTuple(3, 2, 57.8), RateTuple(2, 2, 43.3),
            RateTuple(1, 2, 28.9), RateTuple(0, 2, 14.4)],
        'VHT40': [
            RateTuple(9, 1, 96), RateTuple(8, 1, 86.7),
            RateTuple(7, 1, 72.2), RateTuple(6, 1, 65),
            RateTuple(5, 1, 57.8), RateTuple(4, 1, 43.3),
            RateTuple(3, 1, 28.9), RateTuple(2, 1, 21.7),
            RateTuple(1, 1, 14.4), RateTuple(0, 1, 7.2),
            RateTuple(9, 2, 192), RateTuple(8, 2, 173.3),
            RateTuple(7, 2, 144.4), RateTuple(6, 2, 130.3),
            RateTuple(5, 2, 115.6), RateTuple(4, 2, 86.7),
            RateTuple(3, 2, 57.8), RateTuple(2, 2, 43.3),
            RateTuple(1, 2, 28.9), RateTuple(0, 2, 14.4)],
        'VHT80': [
            RateTuple(9, 1, 96), RateTuple(8, 1, 86.7),
            RateTuple(7, 1, 72.2), RateTuple(6, 1, 65),
            RateTuple(5, 1, 57.8), RateTuple(4, 1, 43.3),
            RateTuple(3, 1, 28.9), RateTuple(2, 1, 21.7),
            RateTuple(1, 1, 14.4), RateTuple(0, 1, 7.2),
            RateTuple(9, 2, 192), RateTuple(8, 2, 173.3),
            RateTuple(7, 2, 144.4), RateTuple(6, 2, 130.3),
            RateTuple(5, 2, 115.6), RateTuple(4, 2, 86.7),
            RateTuple(3, 2, 57.8), RateTuple(2, 2, 43.3),
            RateTuple(1, 2, 28.9), RateTuple(0, 2, 14.4)],
    }
    #yapf:enable

    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_testcase_metrics = True

    def setup_class(self):
        """Initializes common test hardware and parameters.

        This function initializes hardwares and compiles parameters that are
        common to all tests in this class.
        """
        self.dut = self.android_devices[-1]
        req_params = [
            'RetailAccessPoints', 'sensitivity_test_params', 'testbed_params',
            'RemoteServer'
        ]
        opt_params = ['main_network', 'golden_files_list']
        self.unpack_userparams(req_params, opt_params)
        self.testclass_params = self.sensitivity_test_params
        self.num_atten = self.attenuators[0].instrument.num_atten
        self.ping_server = ssh.connection.SshConnection(
            ssh.settings.from_config(self.RemoteServer[0]['ssh_config']))
        self.iperf_server = self.iperf_servers[0]
        self.iperf_client = self.iperf_clients[0]
        self.access_point = retail_ap.create(self.RetailAccessPoints)[0]
        self.log.info('Access Point Configuration: {}'.format(
            self.access_point.ap_settings))
        self.log_path = os.path.join(logging.log_path, 'results')
        os.makedirs(self.log_path, exist_ok=True)
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
        self.atten_dut_chain_map = {}
        self.testclass_results = []

        # Turn WiFi ON
        if self.testclass_params.get('airplane_mode', 1):
            self.log.info('Turning on airplane mode.')
            asserts.assert_true(utils.force_airplane_mode(self.dut, True),
                                "Can not turn on airplane mode.")
        wutils.wifi_toggle_state(self.dut, True)

    def teardown_class(self):
        # Turn WiFi OFF
        for dev in self.android_devices:
            wutils.wifi_toggle_state(dev, False)
        self.process_testclass_results()

    def pass_fail_check(self, result):
        """Checks sensitivity against golden results and decides on pass/fail.

        Args:
            result: dict containing attenuation, throughput and other meta
                data
        """
        try:
            golden_path = next(file_name
                               for file_name in self.golden_files_list
                               if 'sensitivity_targets' in file_name)
            with open(golden_path, 'r') as golden_file:
                golden_results = json.load(golden_file)
            golden_sensitivity = golden_results[
                self.current_test_name]['sensitivity']
        except:
            golden_sensitivity = float('nan')

        result_string = ('Throughput = {}%, Sensitivity = {}.'
                         'Target Sensitivity = {}'.format(
                             result['peak_throughput_pct'],
                             result['sensitivity'], golden_sensitivity))
        if result['peak_throughput_pct'] < 95:
            self.log.warning('Result unreliable. Peak rate unstable')
        if result['sensitivity'] - golden_sensitivity < self.testclass_params[
                'sensitivity_tolerance']:
            asserts.explicit_pass('Test Passed. {}'.format(result_string))
        else:
            asserts.fail('Test Failed. {}'.format(result_string))

    def process_testclass_results(self):
        """Saves and plots test results from all executed test cases."""
        # write json output
        testclass_results_dict = collections.OrderedDict()
        id_fields = ['mode', 'rate', 'num_streams', 'chain_mask']
        channels_tested = []
        for result in self.testclass_results:
            testcase_params = result['testcase_params']
            test_id = self.extract_test_id(testcase_params, id_fields)
            test_id = tuple(test_id.items())
            if test_id not in testclass_results_dict:
                testclass_results_dict[test_id] = collections.OrderedDict()
            channel = testcase_params['channel']
            if channel not in channels_tested:
                channels_tested.append(channel)
            if result['peak_throughput_pct'] >= 95:
                testclass_results_dict[test_id][channel] = result[
                    'sensitivity']
            else:
                testclass_results_dict[test_id][channel] = ''

        # calculate average metrics
        metrics_dict = collections.OrderedDict()
        id_fields = ['channel', 'mode', 'num_streams', 'chain_mask']
        for test_id in testclass_results_dict.keys():
            for channel in testclass_results_dict[test_id].keys():
                metric_tag = collections.OrderedDict(test_id, channel=channel)
                metric_tag = self.extract_test_id(metric_tag, id_fields)
                metric_tag = tuple(metric_tag.items())
                metrics_dict.setdefault(metric_tag, [])
                sensitivity_result = testclass_results_dict[test_id][channel]
                if sensitivity_result != '':
                    metrics_dict[metric_tag].append(sensitivity_result)
        for metric_tag_tuple, metric_data in metrics_dict.items():
            metric_tag_dict = collections.OrderedDict(metric_tag_tuple)
            metric_tag = 'ch{}_{}_nss{}_chain{}'.format(
                metric_tag_dict['channel'], metric_tag_dict['mode'],
                metric_tag_dict['num_streams'], metric_tag_dict['chain_mask'])
            metric_key = "{}.avg_sensitivity".format(metric_tag)
            metric_value = numpy.nanmean(metric_data)
            self.testclass_metric_logger.add_metric(metric_key, metric_value)

        # write csv
        csv_header = ['Mode', 'MCS', 'Streams', 'Chain', 'Rate (Mbps)']
        for channel in channels_tested:
            csv_header.append('Ch. ' + str(channel))
        results_file_path = os.path.join(self.log_path, 'results.csv')
        with open(results_file_path, mode='w') as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=csv_header)
            writer.writeheader()
            for test_id, test_results in testclass_results_dict.items():
                test_id_dict = dict(test_id)
                if 'legacy' in test_id_dict['mode']:
                    rate_list = self.VALID_RATES['legacy_2GHz']
                else:
                    rate_list = self.VALID_RATES[test_id_dict['mode']]
                data_rate = next(rate.data_rate for rate in rate_list
                                 if rate[:-1] == (test_id_dict['rate'],
                                                  test_id_dict['num_streams']))
                row_value = {
                    'Mode': test_id_dict['mode'],
                    'MCS': test_id_dict['rate'],
                    'Streams': test_id_dict['num_streams'],
                    'Chain': test_id_dict['chain_mask'],
                    'Rate (Mbps)': data_rate,
                }
                for channel in channels_tested:
                    row_value['Ch. ' + str(channel)] = test_results.pop(
                        channel, ' ')
                writer.writerow(row_value)

        if not self.testclass_params['traffic_type'].lower() == 'ping':
            WifiRvrTest.process_testclass_results(self)

    def process_rvr_test_results(self, testcase_params, rvr_result):
        """Post processes RvR results to compute sensitivity.

        Takes in the results of the RvR tests and computes the sensitivity of
        the current rate by looking at the point at which throughput drops
        below the percentage specified in the config file. The function then
        calls on its parent class process_test_results to plot the result.

        Args:
            rvr_result: dict containing attenuation, throughput and other meta
            data
        """
        rvr_result['peak_throughput'] = max(rvr_result['throughput_receive'])
        rvr_result['peak_throughput_pct'] = 100
        throughput_check = [
            throughput < rvr_result['peak_throughput'] *
            (self.testclass_params['throughput_pct_at_sensitivity'] / 100)
            for throughput in rvr_result['throughput_receive']
        ]
        consistency_check = [
            idx for idx in range(len(throughput_check))
            if all(throughput_check[idx:])
        ]
        rvr_result['atten_at_range'] = rvr_result['attenuation'][
            consistency_check[0] - 1]
        rvr_result['range'] = rvr_result['fixed_attenuation'] + (
            rvr_result['atten_at_range'])
        rvr_result['sensitivity'] = self.testclass_params['ap_tx_power'] + (
            self.testbed_params['ap_tx_power_offset'][str(
                testcase_params['channel'])] - rvr_result['range'])
        WifiRvrTest.process_test_results(self, rvr_result)

    def process_ping_test_results(self, testcase_params, ping_result):
        """Post processes RvR results to compute sensitivity.

        Takes in the results of the RvR tests and computes the sensitivity of
        the current rate by looking at the point at which throughput drops
        below the percentage specified in the config file. The function then
        calls on its parent class process_test_results to plot the result.

        Args:
            rvr_result: dict containing attenuation, throughput and other meta
            data
        """
        WifiPingTest.process_ping_results(self, testcase_params, ping_result)
        ping_result['sensitivity'] = self.testclass_params['ap_tx_power'] + (
            self.testbed_params['ap_tx_power_offset'][str(
                testcase_params['channel'])] - ping_result['range'])

    def setup_sensitivity_test(self, testcase_params):
        if testcase_params['traffic_type'].lower() == 'ping':
            self.setup_ping_test(testcase_params)
            self.run_sensitivity_test = self.run_ping_test
            self.process_sensitivity_test_results = (
                self.process_ping_test_results)
        else:
            self.setup_rvr_test(testcase_params)
            self.run_sensitivity_test = self.run_rvr_test
            self.process_sensitivity_test_results = (
                self.process_rvr_test_results)

    def setup_ap(self, testcase_params):
        """Sets up the AP and attenuator to compensate for AP chain imbalance.

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
        self.access_point.set_power(band, testcase_params['ap_tx_power'])
        self.access_point.set_rate(band, testcase_params['mode'],
                                   testcase_params['num_streams'],
                                   testcase_params['rate'],
                                   testcase_params['short_gi'])
        # Set attenuator offsets and set attenuators to initial condition
        atten_offsets = self.testbed_params['chain_offset'][str(
            testcase_params['channel'])]
        for atten in self.attenuators:
            if 'AP-Chain-0' in atten.path:
                atten.offset = atten_offsets[0]
            elif 'AP-Chain-1' in atten.path:
                atten.offset = atten_offsets[1]
            else:
                atten.offset = 0
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
        if wputils.validate_network(self.dut,
                                    testcase_params['test_network']['SSID']):
            self.log.info('Already connected to desired network')
        else:
            wutils.reset_wifi(self.dut)
            wutils.set_wifi_country_code(self.dut,
                                         self.testclass_params['country_code'])
            testcase_params['test_network']['channel'] = testcase_params[
                'channel']
            wutils.wifi_connect(self.dut,
                                testcase_params['test_network'],
                                num_of_tries=5,
                                check_connectivity=False)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses('wlan0')[0]
        # Activate/attenuate the correct chains
        if testcase_params['channel'] not in self.atten_dut_chain_map.keys():
            self.atten_dut_chain_map[testcase_params[
                'channel']] = wputils.get_current_atten_dut_chain_map(
                    self.attenuators, self.dut, self.ping_server)
        self.log.info("Current Attenuator-DUT Chain Map: {}".format(
            self.atten_dut_chain_map[testcase_params['channel']]))
        for idx, atten in enumerate(self.attenuators):
            if self.atten_dut_chain_map[testcase_params['channel']][
                    idx] == testcase_params['attenuated_chain']:
                atten.offset = atten.instrument.max_atten

    def extract_test_id(self, testcase_params, id_fields):
        test_id = collections.OrderedDict(
            (param, testcase_params[param]) for param in id_fields)
        return test_id

    def get_start_atten(self, testcase_params):
        """Gets the starting attenuation for this sensitivity test.

        The function gets the starting attenuation by checking whether a test
        as the next higher MCS has been executed. If so it sets the starting
        point a configurable number of dBs below the next MCS's sensitivity.

        Returns:
            start_atten: starting attenuation for current test
        """
        # Get the current and reference test config. The reference test is the
        # one performed at the current MCS+1
        current_rate = testcase_params['rate']
        ref_test_params = self.extract_test_id(
            testcase_params,
            ['channel', 'mode', 'rate', 'num_streams', 'chain_mask'])
        if 'legacy' in testcase_params['mode']:
            if testcase_params['channel'] <= 13:
                rate_list = self.VALID_RATES['legacy_2GHz']
            else:
                rate_list = self.VALID_RATES['legacy_5GHz']
            ref_index = max(
                0,
                rate_list.index(self.RateTuple(current_rate, 1, current_rate))
                - 1)
            ref_test_params['rate'] = rate_list[ref_index].mcs
        else:
            ref_test_params['rate'] = current_rate + 1

        # Check if reference test has been run and set attenuation accordingly
        previous_params = [
            self.extract_test_id(
                result['testcase_params'],
                ['channel', 'mode', 'rate', 'num_streams', 'chain_mask'])
            for result in self.testclass_results
        ]

        try:
            ref_index = previous_params.index(ref_test_params)
            start_atten = self.testclass_results[ref_index][
                'atten_at_range'] - (
                    self.testclass_params['adjacent_mcs_range_gap'])
        except ValueError:
            self.log.warning(
                'Reference test not found. Starting from {} dB'.format(
                    self.testclass_params['atten_start']))
            start_atten = self.testclass_params['atten_start']
            start_atten = max(start_atten, 0)
        return start_atten

    def compile_test_params(self, testcase_params):
        """Function that generates test params based on the test name."""
        band = self.access_point.band_lookup_by_channel(
            testcase_params['channel'])
        testcase_params['test_network'] = self.main_network[band]
        if testcase_params['chain_mask'] in ['0', '1']:
            testcase_params['attenuated_chain'] = 'DUT-Chain-{}'.format(
                1 if testcase_params['chain_mask'] == '0' else 0)
        else:
            # Set attenuated chain to -1. Do not set to None as this will be
            # compared to RF chain map which may include None
            testcase_params['attenuated_chain'] = -1

        self.testclass_params[
            'range_ping_loss_threshold'] = 100 - self.testclass_params[
                'throughput_pct_at_sensitivity']
        if self.testclass_params['traffic_type'] == 'UDP':
            testcase_params['iperf_args'] = '-i 1 -t {} -J -u -b {}'.format(
                self.testclass_params['iperf_duration'],
                self.testclass_params['UDP_rates'][testcase_params['mode']])
        elif self.testclass_params['traffic_type'] == 'TCP':
            testcase_params['iperf_args'] = '-i 1 -t {} -J'.format(
                self.testclass_params['iperf_duration'])

        if self.testclass_params['traffic_type'] != 'ping' and isinstance(
                self.iperf_client, iperf_client.IPerfClientOverAdb):
            testcase_params['iperf_args'] += ' -R'
            testcase_params['use_client_output'] = True
        else:
            testcase_params['use_client_output'] = False

        return testcase_params

    def _test_sensitivity(self, testcase_params):
        """ Function that gets called for each test case

        The function gets called in each rvr test case. The function customizes
        the rvr test based on the test name of the test that called it
        """
        # Compile test parameters from config and test name
        testcase_params = self.compile_test_params(testcase_params)
        testcase_params.update(self.testclass_params)
        testcase_params['atten_start'] = self.get_start_atten(testcase_params)
        num_atten_steps = int(
            (testcase_params['atten_stop'] - testcase_params['atten_start']) /
            testcase_params['atten_step'])
        testcase_params['atten_range'] = [
            testcase_params['atten_start'] + x * testcase_params['atten_step']
            for x in range(0, num_atten_steps)
        ]

        # Prepare devices and run test
        self.setup_sensitivity_test(testcase_params)
        result = self.run_sensitivity_test(testcase_params)
        self.process_sensitivity_test_results(testcase_params, result)

        # Post-process results
        self.testclass_results.append(result)
        self.pass_fail_check(result)

    def generate_test_cases(self, channels, modes, chain_mask):
        """Function that auto-generates test cases for a test class."""
        test_cases = []
        for channel in channels:
            requested_modes = [
                mode for mode in modes
                if mode in self.VALID_TEST_CONFIGS[channel]
            ]
            for mode in requested_modes:
                if 'VHT' in mode:
                    rates = self.VALID_RATES[mode]
                elif 'HT' in mode:
                    rates = self.VALID_RATES[mode]
                elif 'legacy' in mode and channel < 14:
                    rates = self.VALID_RATES['legacy_2GHz']
                elif 'legacy' in mode and channel > 14:
                    rates = self.VALID_RATES['legacy_5GHz']
                else:
                    raise ValueError('Invalid test mode.')
                for chain, rate in itertools.product(chain_mask, rates):
                    testcase_params = collections.OrderedDict(
                        channel=channel,
                        mode=mode,
                        rate=rate.mcs,
                        num_streams=rate.streams,
                        short_gi=1,
                        chain_mask=chain)
                    if chain in ['0', '1'] and rate[1] == 2:
                        # Do not test 2-stream rates in single chain mode
                        continue
                    if 'legacy' in mode:
                        testcase_name = ('test_sensitivity_ch{}_{}_{}_nss{}'
                                         '_ch{}'.format(
                                             channel, mode,
                                             str(rate.mcs).replace('.', 'p'),
                                             rate.streams, chain))
                    else:
                        testcase_name = ('test_sensitivity_ch{}_{}_mcs{}_nss{}'
                                         '_ch{}'.format(
                                             channel, mode, rate.mcs,
                                             rate.streams, chain))
                    setattr(self, testcase_name,
                            partial(self._test_sensitivity, testcase_params))
                    test_cases.append(testcase_name)
        return test_cases


class WifiSensitivity_AllChannels_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            [6, 36, 40, 44, 48, 149, 153, 157, 161],
            ['VHT20', 'VHT40', 'VHT80'], ['0', '1', '2x2'])


class WifiSensitivity_SampleChannels_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases([6, 36, 149],
                                              ['VHT20', 'VHT40', 'VHT80'],
                                              ['0', '1', '2x2'])


class WifiSensitivity_2GHz_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases([1, 2, 6, 10, 11], ['VHT20'],
                                              ['0', '1', '2x2'])


class WifiSensitivity_5GHz_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            [36, 40, 44, 48, 149, 153, 157, 161], ['VHT20', 'VHT40', 'VHT80'],
            ['0', '1', '2x2'])


class WifiSensitivity_UNII1_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases([36, 40, 44, 48],
                                              ['VHT20', 'VHT40', 'VHT80'],
                                              ['0', '1', '2x2'])


class WifiSensitivity_UNII3_Test(WifiSensitivityTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases([149, 153, 157, 161],
                                              ['VHT20', 'VHT40', 'VHT80'],
                                              ['0', '1', '2x2'])


# Over-the air version of senstivity tests
class WifiOtaSensitivityTest(WifiSensitivityTest):
    """Class to test over-the-air senstivity.

    This class implements measures WiFi sensitivity tests in an OTA chamber.
    It allows setting orientation and other chamber parameters to study
    performance in varying channel conditions
    """
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_testcase_metrics = False

    def setup_class(self):
        WifiSensitivityTest.setup_class(self)
        self.current_chain_mask = '2x2'
        self.ota_chamber = ota_chamber.create(
            self.user_params['OTAChamber'])[0]

    def teardown_class(self):
        WifiSensitivityTest.teardown_class(self)
        self.ota_chamber.reset_chamber()

    def setup_sensitivity_test(self, testcase_params):
        # Setup turntable
        self.ota_chamber.set_orientation(testcase_params['orientation'])
        # Continue test setup
        WifiSensitivityTest.setup_sensitivity_test(self, testcase_params)

    def setup_dut(self, testcase_params):
        """Sets up the DUT in the configuration required by the test.

        Args:
            testcase_params: dict containing AP and other test params
        """
        # Configure the right INI settings
        if testcase_params['chain_mask'] != self.current_chain_mask:
            self.log.info('Updating WiFi chain mask to: {}'.format(
                testcase_params['chain_mask']))
            self.current_chain_mask = testcase_params['chain_mask']
            if testcase_params['chain_mask'] in ['0', '1']:
                wputils.set_ini_single_chain_mode(
                    self.dut, int(testcase_params['chain_mask']))
            else:
                wputils.set_ini_two_chain_mode(self.dut)
        # Check battery level before test
        if not wputils.health_check(self.dut, 10):
            asserts.skip('Battery level too low. Skipping test.')
        # Turn screen off to preserve battery
        self.dut.go_to_sleep()
        if wputils.validate_network(self.dut,
                                    testcase_params['test_network']['SSID']):
            self.log.info('Already connected to desired network')
        else:
            wutils.reset_wifi(self.dut)
            wutils.set_wifi_country_code(self.dut,
                                         self.testclass_params['country_code'])
            testcase_params['test_network']['channel'] = testcase_params[
                'channel']
            wutils.wifi_connect(self.dut,
                                testcase_params['test_network'],
                                num_of_tries=5,
                                check_connectivity=False)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses('wlan0')[0]

    def process_testclass_results(self):
        """Saves and plots test results from all executed test cases."""
        testclass_results_dict = collections.OrderedDict()
        id_fields = ['channel', 'mode', 'rate']
        plots = []
        for result in self.testclass_results:
            test_id = self.extract_test_id(result['testcase_params'],
                                           id_fields)
            test_id = tuple(test_id.items())
            chain_mask = result['testcase_params']['chain_mask']
            num_streams = result['testcase_params']['num_streams']
            line_id = (chain_mask, num_streams)
            if test_id not in testclass_results_dict:
                testclass_results_dict[test_id] = collections.OrderedDict()
            if line_id not in testclass_results_dict[test_id]:
                testclass_results_dict[test_id][line_id] = {
                    'orientation': [],
                    'sensitivity': []
                }
            testclass_results_dict[test_id][line_id]['orientation'].append(
                result['testcase_params']['orientation'])
            if result['peak_throughput_pct'] >= 95:
                testclass_results_dict[test_id][line_id]['sensitivity'].append(
                    result['sensitivity'])
            else:
                testclass_results_dict[test_id][line_id]['sensitivity'].append(
                    float('nan'))

        for test_id, test_data in testclass_results_dict.items():
            test_id_dict = dict(test_id)
            if 'legacy' in test_id_dict['mode']:
                test_id_str = 'Channel {} - {} {}Mbps'.format(
                    test_id_dict['channel'], test_id_dict['mode'],
                    test_id_dict['rate'])
            else:
                test_id_str = 'Channel {} - {} MCS{}'.format(
                    test_id_dict['channel'], test_id_dict['mode'],
                    test_id_dict['rate'])
            curr_plot = wputils.BokehFigure(
                title=str(test_id_str),
                x_label='Orientation (deg)',
                primary_y_label='Sensitivity (dBm)')
            for line_id, line_results in test_data.items():
                curr_plot.add_line(line_results['orientation'],
                                   line_results['sensitivity'],
                                   legend='Nss{} - Chain Mask {}'.format(
                                       line_id[1], line_id[0]),
                                   marker='circle')
                if 'legacy' in test_id_dict['mode']:
                    metric_tag = 'ota_summary_ch{}_{}_{}_ch{}'.format(
                        test_id_dict['channel'], test_id_dict['mode'],
                        test_id_dict['rate'], line_id[0])
                else:
                    metric_tag = 'ota_summary_ch{}_{}_mcs{}_nss{}_ch{}'.format(
                        test_id_dict['channel'], test_id_dict['mode'],
                        test_id_dict['rate'], line_id[1], line_id[0])

                metric_name = metric_tag + '.avg_sensitivity'
                metric_value = numpy.nanmean(line_results['sensitivity'])
                self.testclass_metric_logger.add_metric(
                    metric_name, metric_value)
                self.log.info(("Average Sensitivity for {}: {:.1f}").format(
                    metric_tag, metric_value))
            current_context = (
                context.get_current_context().get_full_output_path())
            output_file_path = os.path.join(current_context,
                                            str(test_id_str) + '.html')
            curr_plot.generate_figure(output_file_path)
            plots.append(curr_plot)
        output_file_path = os.path.join(current_context, 'results.html')
        wputils.BokehFigure.save_figures(plots, output_file_path)

    def get_start_atten(self, testcase_params):
        """Gets the starting attenuation for this sensitivity test.

        The function gets the starting attenuation by checking whether a test
        at the same rate configuration has executed. If so it sets the starting
        point a configurable number of dBs below the reference test.

        Returns:
            start_atten: starting attenuation for current test
        """
        # Get the current and reference test config. The reference test is the
        # one performed at the current MCS+1
        ref_test_params = self.extract_test_id(
            testcase_params,
            ['channel', 'mode', 'rate', 'num_streams', 'chain_mask'])
        # Check if reference test has been run and set attenuation accordingly
        previous_params = [
            self.extract_test_id(
                result['testcase_params'],
                ['channel', 'mode', 'rate', 'num_streams', 'chain_mask'])
            for result in self.testclass_results
        ]
        try:
            ref_index = previous_params[::-1].index(ref_test_params)
            ref_index = len(previous_params) - 1 - ref_index
            start_atten = self.testclass_results[ref_index][
                'atten_at_range'] - (
                    self.testclass_params['adjacent_mcs_range_gap'])
        except ValueError:
            print('Reference test not found. Starting from {} dB'.format(
                self.testclass_params['atten_start']))
            start_atten = self.testclass_params['atten_start']
        start_atten = max(start_atten, 0)
        return start_atten

    def generate_test_cases(self, channels, modes, requested_rates, chain_mask,
                            angles):
        """Function that auto-generates test cases for a test class."""
        test_cases = []
        for channel in channels:
            requested_modes = [
                mode for mode in modes
                if mode in self.VALID_TEST_CONFIGS[channel]
            ]
            for chain, mode in itertools.product(chain_mask, requested_modes):
                if 'VHT' in mode:
                    valid_rates = self.VALID_RATES[mode]
                elif 'HT' in mode:
                    valid_rates = self.VALID_RATES[mode]
                elif 'legacy' in mode and channel < 14:
                    valid_rates = self.VALID_RATES['legacy_2GHz']
                elif 'legacy' in mode and channel > 14:
                    valid_rates = self.VALID_RATES['legacy_5GHz']
                else:
                    raise ValueError('Invalid test mode.')
                for rate, angle in itertools.product(valid_rates, angles):
                    testcase_params = collections.OrderedDict(
                        channel=channel,
                        mode=mode,
                        rate=rate.mcs,
                        num_streams=rate.streams,
                        short_gi=1,
                        chain_mask=chain,
                        orientation=angle)
                    if rate not in requested_rates:
                        continue
                    if str(chain) in ['0', '1'] and rate[1] == 2:
                        # Do not test 2-stream rates in single chain mode
                        continue
                    if 'legacy' in mode:
                        testcase_name = ('test_sensitivity_ch{}_{}_{}_nss{}'
                                         '_ch{}_{}deg'.format(
                                             channel, mode,
                                             str(rate.mcs).replace('.', 'p'),
                                             rate.streams, chain, angle))
                    else:
                        testcase_name = ('test_sensitivity_ch{}_{}_mcs{}_nss{}'
                                         '_ch{}_{}deg'.format(
                                             channel, mode, rate.mcs,
                                             rate.streams, chain, angle))
                    setattr(self, testcase_name,
                            partial(self._test_sensitivity, testcase_params))
                    test_cases.append(testcase_name)
        return test_cases


class WifiOtaSensitivity_TenDegree_Test(WifiOtaSensitivityTest):
    def __init__(self, controllers):
        WifiOtaSensitivityTest.__init__(self, controllers)
        requested_channels = [6, 36, 149]
        requested_rates = [
            self.RateTuple(8, 1, 86.7),
            self.RateTuple(2, 1, 21.7),
            self.RateTuple(8, 2, 173.3),
            self.RateTuple(2, 2, 43.3)
        ]
        self.tests = self.generate_test_cases(requested_channels,
                                              ['VHT20', 'VHT80'],
                                              requested_rates, ['2x2'],
                                              list(range(0, 360, 10)))


class WifiOtaSensitivity_PerChain_TenDegree_Test(WifiOtaSensitivityTest):
    def __init__(self, controllers):
        WifiOtaSensitivityTest.__init__(self, controllers)
        requested_channels = [6, 36, 149]
        requested_rates = [
            self.RateTuple(2, 1, 21.7),
            self.RateTuple(2, 2, 43.3)
        ]
        self.tests = self.generate_test_cases(requested_channels, ['VHT20'],
                                              requested_rates,
                                              ['0', '1', '2x2'],
                                              list(range(0, 360, 10)))


class WifiOtaSensitivity_ThirtyDegree_Test(WifiOtaSensitivityTest):
    def __init__(self, controllers):
        WifiOtaSensitivityTest.__init__(self, controllers)
        requested_channels = [6, 36, 149]
        requested_rates = [
            self.RateTuple(9, 1, 96),
            self.RateTuple(8, 1, 86.7),
            self.RateTuple(7, 1, 72.2),
            self.RateTuple(4, 1, 43.3),
            self.RateTuple(2, 1, 21.7),
            self.RateTuple(0, 1, 7.2),
            self.RateTuple(9, 2, 192),
            self.RateTuple(8, 2, 173.3),
            self.RateTuple(7, 2, 144.4),
            self.RateTuple(4, 2, 86.7),
            self.RateTuple(2, 2, 43.3),
            self.RateTuple(0, 2, 14.4)
        ]
        self.tests = self.generate_test_cases(requested_channels,
                                              ['VHT20', 'VHT80'],
                                              requested_rates, ['2x2'],
                                              list(range(0, 360, 30)))


class WifiOtaSensitivity_45Degree_Test(WifiOtaSensitivityTest):
    def __init__(self, controllers):
        WifiOtaSensitivityTest.__init__(self, controllers)
        requested_rates = [
            self.RateTuple(8, 1, 86.7),
            self.RateTuple(2, 1, 21.7),
            self.RateTuple(8, 2, 173.3),
            self.RateTuple(2, 2, 43.3)
        ]
        self.tests = self.generate_test_cases(
            [1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161], ['VHT20', 'VHT80'],
            requested_rates, ['2x2'], list(range(0, 360, 45)))
