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
import os
import statistics
from acts import asserts
from acts import context
from acts import base_test
from acts import utils
from acts.controllers.utils_lib import ssh
from acts.metrics.loggers.blackbox import BlackboxMappedMetricLogger
from acts.test_utils.wifi import ota_chamber
from acts.test_utils.wifi import ota_sniffer
from acts.test_utils.wifi import wifi_performance_test_utils as wputils
from acts.test_utils.wifi import wifi_retail_ap as retail_ap
from acts.test_utils.wifi import wifi_test_utils as wutils
from functools import partial


class WifiPingTest(base_test.BaseTestClass):
    """Class for ping-based Wifi performance tests.

    This class implements WiFi ping performance tests such as range and RTT.
    The class setups up the AP in the desired configurations, configures
    and connects the phone to the AP, and runs  For an example config file to
    run this test class see example_connectivity_performance_ap_sta.json.
    """

    TEST_TIMEOUT = 10
    RSSI_POLL_INTERVAL = 0.2
    SHORT_SLEEP = 1
    MED_SLEEP = 5
    MAX_CONSECUTIVE_ZEROS = 5
    DISCONNECTED_PING_RESULT = {
        'connected': 0,
        'rtt': [],
        'time_stamp': [],
        'ping_interarrivals': [],
        'packet_loss_percentage': 100
    }

    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.testcase_metric_logger = (
            BlackboxMappedMetricLogger.for_test_case())
        self.testclass_metric_logger = (
            BlackboxMappedMetricLogger.for_test_class())
        self.publish_testcase_metrics = True

    def setup_class(self):
        self.dut = self.android_devices[-1]
        req_params = [
            'ping_test_params', 'testbed_params', 'main_network',
            'RetailAccessPoints', 'RemoteServer'
        ]
        opt_params = ['golden_files_list', 'OTASniffer']
        self.unpack_userparams(req_params, opt_params)
        self.testclass_params = self.ping_test_params
        self.num_atten = self.attenuators[0].instrument.num_atten
        self.ping_server = ssh.connection.SshConnection(
            ssh.settings.from_config(self.RemoteServer[0]['ssh_config']))
        self.access_point = retail_ap.create(self.RetailAccessPoints)[0]
        if hasattr(self, 'OTASniffer'):
            self.sniffer = ota_sniffer.create(self.OTASniffer)[0]
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

    def process_testclass_results(self):
        """Saves all test results to enable comparison."""
        testclass_summary = {}
        for test in self.testclass_results:
            if 'range' in test['test_name']:
                testclass_summary[test['test_name']] = test['range']
        # Save results
        results_file_path = os.path.join(self.log_path,
                                         'testclass_summary.json')
        with open(results_file_path, 'w') as results_file:
            json.dump(testclass_summary, results_file, indent=4)

    def pass_fail_check_ping_rtt(self, result):
        """Check the test result and decide if it passed or failed.

        The function computes RTT statistics and fails any tests in which the
        tail of the ping latency results exceeds the threshold defined in the
        configuration file.

        Args:
            result: dict containing ping results and other meta data
        """
        ignored_fraction = (self.testclass_params['rtt_ignored_interval'] /
                            self.testclass_params['rtt_ping_duration'])
        sorted_rtt = [
            sorted(x['rtt'][round(ignored_fraction * len(x['rtt'])):])
            for x in result['ping_results']
        ]
        disconnected = any([len(x) == 0 for x in sorted_rtt])
        if disconnected:
            asserts.fail('Test failed. DUT disconnected at least once.')

        rtt_at_test_percentile = [
            x[int((1 - self.testclass_params['rtt_test_percentile'] / 100) *
                  len(x))] for x in sorted_rtt
        ]
        # Set blackbox metric
        if self.publish_testcase_metrics:
            self.testcase_metric_logger.add_metric('ping_rtt',
                                                   max(rtt_at_test_percentile))
        # Evaluate test pass/fail
        rtt_failed = any([
            rtt > self.testclass_params['rtt_threshold'] * 1000
            for rtt in rtt_at_test_percentile
        ])
        if rtt_failed:
            asserts.fail('Test failed. RTTs at test percentile = {}'.format(
                rtt_at_test_percentile))
        else:
            asserts.explicit_pass(
                'Test Passed. RTTs at test percentile = {}'.format(
                    rtt_at_test_percentile))

    def pass_fail_check_ping_range(self, result):
        """Check the test result and decide if it passed or failed.

        Checks whether the attenuation at which ping packet losses begin to
        exceed the threshold matches the range derived from golden
        rate-vs-range result files. The test fails is ping range is
        range_gap_threshold worse than RvR range.

        Args:
            result: dict containing ping results and meta data
        """
        # Get target range
        rvr_range = self.get_range_from_rvr()
        # Set Blackbox metric
        if self.publish_testcase_metrics:
            self.testcase_metric_logger.add_metric('ping_range',
                                                   result['range'])
        # Evaluate test pass/fail
        test_message = ('Attenuation at range is {}dB. Golden range is {}dB. '
                        'LLStats at Range: {}'.format(
                            result['range'], rvr_range,
                            result['llstats_at_range']))
        if result['range'] - rvr_range < -self.testclass_params[
                'range_gap_threshold']:
            asserts.fail(test_message)
        else:
            asserts.explicit_pass(test_message)

    def pass_fail_check(self, result):
        if 'range' in result['testcase_params']['test_type']:
            self.pass_fail_check_ping_range(result)
        else:
            self.pass_fail_check_ping_rtt(result)

    def process_ping_results(self, testcase_params, ping_range_result):
        """Saves and plots ping results.

        Args:
            ping_range_result: dict containing ping results and metadata
        """
        # Compute range
        ping_loss_over_att = [
            x['packet_loss_percentage']
            for x in ping_range_result['ping_results']
        ]
        ping_loss_above_threshold = [
            x > self.testclass_params['range_ping_loss_threshold']
            for x in ping_loss_over_att
        ]
        for idx in range(len(ping_loss_above_threshold)):
            if all(ping_loss_above_threshold[idx:]):
                range_index = max(idx, 1) - 1
                break
        else:
            range_index = -1
        ping_range_result['atten_at_range'] = testcase_params['atten_range'][
            range_index]
        ping_range_result['peak_throughput_pct'] = 100 - min(
            ping_loss_over_att)
        ping_range_result['range'] = (ping_range_result['atten_at_range'] +
                                      ping_range_result['fixed_attenuation'])
        ping_range_result['llstats_at_range'] = (
            'TX MCS = {0} ({1:.1f}%). '
            'RX MCS = {2} ({3:.1f}%)'.format(
                ping_range_result['llstats'][range_index]['summary']
                ['common_tx_mcs'], ping_range_result['llstats'][range_index]
                ['summary']['common_tx_mcs_freq'] * 100,
                ping_range_result['llstats'][range_index]['summary']
                ['common_rx_mcs'], ping_range_result['llstats'][range_index]
                ['summary']['common_rx_mcs_freq'] * 100))

        # Save results
        results_file_path = os.path.join(
            self.log_path, '{}.json'.format(self.current_test_name))
        with open(results_file_path, 'w') as results_file:
            json.dump(ping_range_result, results_file, indent=4)

        # Plot results
        if 'range' not in self.current_test_name:
            figure = wputils.BokehFigure(
                self.current_test_name,
                x_label='Timestamp (s)',
                primary_y_label='Round Trip Time (ms)')
            for idx, result in enumerate(ping_range_result['ping_results']):
                if len(result['rtt']) > 1:
                    x_data = [
                        t - result['time_stamp'][0]
                        for t in result['time_stamp']
                    ]
                    figure.add_line(
                        x_data, result['rtt'], 'RTT @ {}dB'.format(
                            ping_range_result['attenuation'][idx]))

            output_file_path = os.path.join(
                self.log_path, '{}.html'.format(self.current_test_name))
            figure.generate_figure(output_file_path)

    def get_range_from_rvr(self):
        """Function gets range from RvR golden results

        The function fetches the attenuation at which the RvR throughput goes
        to zero.

        Returns:
            rvr_range: range derived from looking at rvr curves
        """
        # Fetch the golden RvR results
        test_name = self.current_test_name
        rvr_golden_file_name = 'test_rvr_TCP_DL_' + '_'.join(
            test_name.split('_')[3:])
        golden_path = [
            file_name for file_name in self.golden_files_list
            if rvr_golden_file_name in file_name
        ]
        if len(golden_path) == 0:
            rvr_range = float('nan')
            return rvr_range

        # Get 0 Mbps attenuation and backoff by low_rssi_backoff_from_range
        with open(golden_path[0], 'r') as golden_file:
            golden_results = json.load(golden_file)
        try:
            atten_idx = golden_results['throughput_receive'].index(0)
            rvr_range = (golden_results['attenuation'][atten_idx - 1] +
                         golden_results['fixed_attenuation'])
        except ValueError:
            rvr_range = float('nan')
        return rvr_range

    def run_ping_test(self, testcase_params):
        """Main function to test ping.

        The function sets up the AP in the correct channel and mode
        configuration and calls get_ping_stats while sweeping attenuation

        Args:
            testcase_params: dict containing all test parameters
        Returns:
            test_result: dict containing ping results and other meta data
        """
        # Prepare results dict
        llstats_obj = wputils.LinkLayerStats(self.dut)
        test_result = collections.OrderedDict()
        test_result['testcase_params'] = testcase_params.copy()
        test_result['test_name'] = self.current_test_name
        test_result['ap_config'] = self.access_point.ap_settings.copy()
        test_result['attenuation'] = testcase_params['atten_range']
        test_result['fixed_attenuation'] = self.testbed_params[
            'fixed_attenuation'][str(testcase_params['channel'])]
        test_result['rssi_results'] = []
        test_result['ping_results'] = []
        test_result['llstats'] = []
        # Setup sniffer
        if self.testbed_params['sniffer_enable']:
            self.sniffer.start_capture(
                testcase_params['test_network'],
                testcase_params['ping_duration'] *
                len(testcase_params['atten_range']) + self.TEST_TIMEOUT)
        # Run ping and sweep attenuation as needed
        zero_counter = 0
        for atten in testcase_params['atten_range']:
            for attenuator in self.attenuators:
                attenuator.set_atten(atten, strict=False)
            rssi_future = wputils.get_connected_rssi_nb(
                self.dut,
                int(testcase_params['ping_duration'] / 2 /
                    self.RSSI_POLL_INTERVAL), self.RSSI_POLL_INTERVAL,
                testcase_params['ping_duration'] / 2)
            # Refresh link layer stats
            llstats_obj.update_stats()
            current_ping_stats = wputils.get_ping_stats(
                self.ping_server, self.dut_ip,
                testcase_params['ping_duration'],
                testcase_params['ping_interval'], testcase_params['ping_size'])
            current_rssi = rssi_future.result()
            test_result['rssi_results'].append(current_rssi)
            llstats_obj.update_stats()
            curr_llstats = llstats_obj.llstats_incremental.copy()
            test_result['llstats'].append(curr_llstats)
            if current_ping_stats['connected']:
                self.log.info(
                    'Attenuation = {0}dB\tPacket Loss = {1}%\t'
                    'Avg RTT = {2:.2f}ms\tRSSI = {3} [{4},{5}]\t'.format(
                        atten, current_ping_stats['packet_loss_percentage'],
                        statistics.mean(current_ping_stats['rtt']),
                        current_rssi['signal_poll_rssi']['mean'],
                        current_rssi['chain_0_rssi']['mean'],
                        current_rssi['chain_1_rssi']['mean']))
                if current_ping_stats['packet_loss_percentage'] == 100:
                    zero_counter = zero_counter + 1
                else:
                    zero_counter = 0
            else:
                self.log.info(
                    'Attenuation = {}dB. Disconnected.'.format(atten))
                zero_counter = zero_counter + 1
            test_result['ping_results'].append(current_ping_stats.as_dict())
            if zero_counter == self.MAX_CONSECUTIVE_ZEROS:
                self.log.info('Ping loss stable at 100%. Stopping test now.')
                for idx in range(
                        len(testcase_params['atten_range']) -
                        len(test_result['ping_results'])):
                    test_result['ping_results'].append(
                        self.DISCONNECTED_PING_RESULT)
                break
        if self.testbed_params['sniffer_enable']:
            self.sniffer.stop_capture()
        return test_result

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
        if 'low' in testcase_params['ap_power']:
            self.log.info('Setting low AP power.')
            self.access_point.set_power(band, 0)
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
                                check_connectivity=True)
        self.dut_ip = self.dut.droid.connectivityGetIPv4Addresses('wlan0')[0]
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
            else:
                atten.offset = 0

    def setup_ping_test(self, testcase_params):
        """Function that gets devices ready for the test.

        Args:
            testcase_params: dict containing test-specific parameters
        """
        # Configure AP
        self.setup_ap(testcase_params)
        # Set attenuator to 0 dB
        for attenuator in self.attenuators:
            attenuator.set_atten(0, strict=False)
        # Reset, configure, and connect DUT
        self.setup_dut(testcase_params)

    def get_range_start_atten(self, testcase_params):
        """Gets the starting attenuation for this ping test.

        This function is used to get the starting attenuation for ping range
        tests. This implementation returns the default starting attenuation,
        however, defining this function enables a more involved configuration
        for over-the-air test classes.

        Args:
            testcase_params: dict containing all test params
        """
        return self.testclass_params['range_atten_start']

    def compile_test_params(self, testcase_params):
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
        if testcase_params['test_type'] == 'test_ping_range':
            testcase_params.update(
                ping_interval=self.testclass_params['range_ping_interval'],
                ping_duration=self.testclass_params['range_ping_duration'],
                ping_size=self.testclass_params['ping_size'],
            )
        elif testcase_params['test_type'] == 'test_fast_ping_rtt':
            testcase_params.update(
                ping_interval=self.testclass_params['rtt_ping_interval']
                ['fast'],
                ping_duration=self.testclass_params['rtt_ping_duration'],
                ping_size=self.testclass_params['ping_size'],
            )
        elif testcase_params['test_type'] == 'test_slow_ping_rtt':
            testcase_params.update(
                ping_interval=self.testclass_params['rtt_ping_interval']
                ['slow'],
                ping_duration=self.testclass_params['rtt_ping_duration'],
                ping_size=self.testclass_params['ping_size'])

        if testcase_params['test_type'] == 'test_ping_range':
            start_atten = self.get_range_start_atten(testcase_params)
            num_atten_steps = int(
                (self.testclass_params['range_atten_stop'] - start_atten) /
                self.testclass_params['range_atten_step'])
            testcase_params['atten_range'] = [
                start_atten + x * self.testclass_params['range_atten_step']
                for x in range(0, num_atten_steps)
            ]
        else:
            testcase_params['atten_range'] = self.testclass_params[
                'rtt_test_attenuation']
        return testcase_params

    def _test_ping(self, testcase_params):
        """ Function that gets called for each range test case

        The function gets called in each range test case. It customizes the
        range test based on the test name of the test that called it

        Args:
            testcase_params: dict containing preliminary set of parameters
        """
        # Compile test parameters from config and test name
        testcase_params = self.compile_test_params(testcase_params)
        # Run ping test
        self.setup_ping_test(testcase_params)
        ping_result = self.run_ping_test(testcase_params)
        # Postprocess results
        self.testclass_results.append(ping_result)
        self.process_ping_results(testcase_params, ping_result)
        self.pass_fail_check(ping_result)

    def generate_test_cases(self, ap_power, channels, modes, chain_mask,
                            test_types):
        test_cases = []
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }
        for channel, mode, chain, test_type in itertools.product(
                channels, modes, chain_mask, test_types):
            if channel not in allowed_configs[mode]:
                continue
            testcase_name = '{}_ch{}_{}_ch{}'.format(test_type, channel, mode,
                                                     chain)
            testcase_params = collections.OrderedDict(test_type=test_type,
                                                      ap_power=ap_power,
                                                      channel=channel,
                                                      mode=mode,
                                                      chain_mask=chain)
            setattr(self, testcase_name,
                    partial(self._test_ping, testcase_params))
            test_cases.append(testcase_name)
        return test_cases


class WifiPing_TwoChain_Test(WifiPingTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ap_power='standard',
            channels=[1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            modes=['VHT20', 'VHT40', 'VHT80'],
            test_types=[
                'test_ping_range', 'test_fast_ping_rtt', 'test_slow_ping_rtt'
            ],
            chain_mask=['2x2'])


class WifiPing_PerChainRange_Test(WifiPingTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ap_power='standard',
            chain_mask=['0', '1', '2x2'],
            channels=[1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            modes=['VHT20', 'VHT40', 'VHT80'],
            test_types=['test_ping_range'])


class WifiPing_LowPowerAP_Test(WifiPingTest):
    def __init__(self, controllers):
        super().__init__(controllers)
        self.tests = self.generate_test_cases(
            ap_power='low_power',
            chain_mask=['0', '1', '2x2'],
            channels=[1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            modes=['VHT20', 'VHT40', 'VHT80'],
            test_types=['test_ping_range'])


# Over-the air version of ping tests
class WifiOtaPingTest(WifiPingTest):
    """Class to test over-the-air ping

    This class tests WiFi ping performance in an OTA chamber. It enables
    setting turntable orientation and other chamber parameters to study
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
        WifiPingTest.setup_class(self)
        self.ota_chamber = ota_chamber.create(
            self.user_params['OTAChamber'])[0]

    def teardown_class(self):
        self.process_testclass_results()
        self.ota_chamber.reset_chamber()

    def process_testclass_results(self):
        """Saves all test results to enable comparison."""
        WifiPingTest.process_testclass_results(self)

        range_vs_angle = collections.OrderedDict()
        for test in self.testclass_results:
            curr_params = test['testcase_params']
            curr_config = curr_params['channel']
            if curr_config in range_vs_angle:
                range_vs_angle[curr_config]['position'].append(
                    curr_params['position'])
                range_vs_angle[curr_config]['range'].append(test['range'])
                range_vs_angle[curr_config]['llstats_at_range'].append(
                    test['llstats_at_range'])
            else:
                range_vs_angle[curr_config] = {
                    'position': [curr_params['position']],
                    'range': [test['range']],
                    'llstats_at_range': [test['llstats_at_range']]
                }
        chamber_mode = self.testclass_results[0]['testcase_params'][
            'chamber_mode']
        if chamber_mode == 'orientation':
            x_label = 'Angle (deg)'
        elif chamber_mode == 'stepped stirrers':
            x_label = 'Position Index'
        figure = wputils.BokehFigure(
            title='Range vs. Position',
            x_label=x_label,
            primary_y_label='Range (dB)',
        )
        for channel, channel_data in range_vs_angle.items():
            figure.add_line(x_data=channel_data['position'],
                            y_data=channel_data['range'],
                            hover_text=channel_data['llstats_at_range'],
                            legend='Channel {}'.format(channel))
            average_range = sum(channel_data['range']) / len(
                channel_data['range'])
            self.log.info('Average range for Channel {} is: {}dB'.format(
                channel, average_range))
            metric_name = 'ota_summary_ch{}.avg_range'.format(channel)
            self.testclass_metric_logger.add_metric(metric_name, average_range)
        current_context = context.get_current_context().get_full_output_path()
        plot_file_path = os.path.join(current_context, 'results.html')
        figure.generate_figure(plot_file_path)

        # Save results
        results_file_path = os.path.join(current_context,
                                         'testclass_summary.json')
        with open(results_file_path, 'w') as results_file:
            json.dump(range_vs_angle, results_file, indent=4)

    def setup_ping_test(self, testcase_params):
        WifiPingTest.setup_ping_test(self, testcase_params)
        # Setup turntable
        if testcase_params['chamber_mode'] == 'orientation':
            self.ota_chamber.set_orientation(testcase_params['position'])
        elif testcase_params['chamber_mode'] == 'stepped stirrers':
            self.ota_chamber.step_stirrers(testcase_params['total_positions'])

    def extract_test_id(self, testcase_params, id_fields):
        test_id = collections.OrderedDict(
            (param, testcase_params[param]) for param in id_fields)
        return test_id

    def get_range_start_atten(self, testcase_params):
        """Gets the starting attenuation for this ping test.

        The function gets the starting attenuation by checking whether a test
        at the same configuration has executed. If so it sets the starting
        point a configurable number of dBs below the reference test.

        Returns:
            start_atten: starting attenuation for current test
        """
        # Get the current and reference test config. The reference test is the
        # one performed at the current MCS+1
        ref_test_params = self.extract_test_id(testcase_params,
                                               ['channel', 'mode'])
        # Check if reference test has been run and set attenuation accordingly
        previous_params = [
            self.extract_test_id(result['testcase_params'],
                                 ['channel', 'mode'])
            for result in self.testclass_results
        ]
        try:
            ref_index = previous_params[::-1].index(ref_test_params)
            ref_index = len(previous_params) - 1 - ref_index
            start_atten = self.testclass_results[ref_index][
                'atten_at_range'] - (
                    self.testclass_params['adjacent_range_test_gap'])
        except ValueError:
            print('Reference test not found. Starting from {} dB'.format(
                self.testclass_params['range_atten_start']))
            start_atten = self.testclass_params['range_atten_start']
        return start_atten

    def generate_test_cases(self, ap_power, channels, modes, chamber_mode,
                            positions):
        test_cases = []
        allowed_configs = {
            'VHT20': [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 36, 40, 44, 48, 149, 153,
                157, 161
            ],
            'VHT40': [36, 44, 149, 157],
            'VHT80': [36, 149]
        }
        for channel, mode, position in itertools.product(
                channels, modes, positions):
            if channel not in allowed_configs[mode]:
                continue
            testcase_name = 'test_ping_range_ch{}_{}_pos{}'.format(
                channel, mode, position)
            testcase_params = collections.OrderedDict(
                test_type='test_ping_range',
                ap_power=ap_power,
                channel=channel,
                mode=mode,
                chain_mask='2x2',
                chamber_mode=chamber_mode,
                total_positions=len(positions),
                position=position)
            setattr(self, testcase_name,
                    partial(self._test_ping, testcase_params))
            test_cases.append(testcase_name)
        return test_cases


class WifiOtaPing_TenDegree_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(ap_power='standard',
                                              channels=[6, 36, 149],
                                              modes=['VHT20'],
                                              chamber_mode='orientation',
                                              positions=list(range(0, 360,
                                                                   10)))


class WifiOtaPing_45Degree_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(
            ap_power='standard',
            channels=[1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            modes=['VHT20'],
            chamber_mode='orientation',
            positions=list(range(0, 360, 45)))


class WifiOtaPing_SteppedStirrers_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(ap_power='standard',
                                              channels=[6, 36, 149],
                                              modes=['VHT20'],
                                              chamber_mode='stepped stirrers',
                                              positions=list(range(100)))


class WifiOtaPing_LowPowerAP_TenDegree_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(ap_power='low_power',
                                              channels=[6, 36, 149],
                                              modes=['VHT20'],
                                              chamber_mode='orientation',
                                              positions=list(range(0, 360,
                                                                   10)))


class WifiOtaPing_LowPowerAP_45Degree_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(
            ap_power='low_power',
            channels=[1, 6, 11, 36, 40, 44, 48, 149, 153, 157, 161],
            modes=['VHT20'],
            chamber_mode='orientation',
            positions=list(range(0, 360, 45)))


class WifiOtaPing_LowPowerAP_SteppedStirrers_Test(WifiOtaPingTest):
    def __init__(self, controllers):
        WifiOtaPingTest.__init__(self, controllers)
        self.tests = self.generate_test_cases(ap_power='low_power',
                                              channels=[6, 36, 149],
                                              modes=['VHT20'],
                                              chamber_mode='stepped stirrers',
                                              positions=list(range(100)))
