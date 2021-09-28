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
import logging
import os
from acts import asserts
from acts import base_test
from acts.controllers import iperf_server as ipf
from acts.controllers import iperf_client as ipc
from acts.metrics.loggers.blackbox import BlackboxMappedMetricLogger
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi import wifi_performance_test_utils as wputils
from WifiRvrTest import WifiRvrTest

AccessPointTuple = collections.namedtuple(('AccessPointTuple'),
                                          ['ap_settings'])


class WifiSoftApRvrTest(WifiRvrTest):
    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)
        self.tests = ('test_rvr_TCP_DL_2GHz', 'test_rvr_TCP_UL_2GHz',
                      'test_rvr_TCP_DL_5GHz', 'test_rvr_TCP_UL_5GHz')
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
        req_params = ['sap_test_params', 'testbed_params', 'AndroidDevice']
        opt_params = ['main_network', 'golden_files_list']
        self.unpack_userparams(req_params, opt_params)
        self.testclass_params = self.sap_test_params
        self.num_atten = self.attenuators[0].instrument.num_atten
        self.iperf_server = ipf.create([{
            'AndroidDevice':
            self.android_devices[0].serial,
            'port':
            '5201'
        }])[0]
        self.iperf_client = ipc.create([{
            'AndroidDevice':
            self.android_devices[1].serial,
            'port':
            '5201'
        }])[0]

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
                file for file in self.firmware if 'wlanmdsp.mbn' in file
            ][0]
            data_msc = [file for file in self.firmware
                        if 'Data.msc' in file][0]
            wputils.push_firmware(self.dut, wlanmdsp, data_msc)
        self.testclass_results = []

        # Turn WiFi ON
        for dev in self.android_devices:
            wutils.wifi_toggle_state(dev, True)

    def teardown_class(self):
        # Turn WiFi OFF
        wutils.stop_wifi_tethering(self.android_devices[0])
        for dev in self.android_devices:
            wutils.wifi_toggle_state(dev, False)
        self.process_testclass_results()

    def teardown_test(self):
        wutils.stop_wifi_tethering(self.android_devices[0])

    def get_sap_connection_info(self):
        info = {}
        info['client_ip_address'] = self.android_devices[
            1].droid.connectivityGetIPv4Addresses('wlan0')[0]
        info['ap_ip_address'] = self.android_devices[
            0].droid.connectivityGetIPv4Addresses('wlan1')[0]
        info['frequency'] = self.android_devices[1].adb.shell(
            'wpa_cli status | grep freq').split('=')[1]
        info['channel'] = wutils.WifiEnums.freq_to_channel[int(
            info['frequency'])]
        return info

    def setup_sap_rvr_test(self, testcase_params):
        """Function that gets devices ready for the test.

        Args:
            testcase_params: dict containing test-specific parameters
        """
        for dev in self.android_devices:
            if not wputils.health_check(dev, 20):
                asserts.skip('DUT health check failed. Skipping test.')
        # Reset WiFi on all devices
        for dev in self.android_devices:
            wutils.reset_wifi(dev)
            wutils.set_wifi_country_code(dev, wutils.WifiEnums.CountryCode.US)

        # Setup Soft AP
        sap_config = wutils.create_softap_config()
        self.log.info('SoftAP Config: {}'.format(sap_config))
        wutils.start_wifi_tethering(self.android_devices[0],
                                    sap_config[wutils.WifiEnums.SSID_KEY],
                                    sap_config[wutils.WifiEnums.PWD_KEY],
                                    testcase_params['sap_band_enum'])
        # Set attenuator to 0 dB
        [self.attenuators[i].set_atten(0) for i in range(self.num_atten)]
        # Connect DUT to Network
        network = {
            'SSID': sap_config[wutils.WifiEnums.SSID_KEY],
            'password': sap_config[wutils.WifiEnums.PWD_KEY]
        }
        wutils.wifi_connect(self.android_devices[1],
                            network,
                            num_of_tries=5,
                            check_connectivity=False)
        # Compile meta data
        self.access_point = AccessPointTuple(sap_config)
        testcase_params['connection_info'] = self.get_sap_connection_info()
        testcase_params['channel'] = testcase_params['connection_info'][
            'channel']
        if testcase_params['channel'] < 13:
            testcase_params['mode'] = 'VHT20'
        else:
            testcase_params['mode'] = 'VHT80'
        testcase_params['iperf_server_address'] = testcase_params[
            'connection_info']['ap_ip_address']

    def compile_test_params(self, testcase_params):
        """Function that completes all test params based on the test name.

        Args:
            testcase_params: dict containing test-specific parameters
        """
        num_atten_steps = int((self.testclass_params['atten_stop'] -
                               self.testclass_params['atten_start']) /
                              self.testclass_params['atten_step'])
        testcase_params['atten_range'] = [
            self.testclass_params['atten_start'] +
            x * self.testclass_params['atten_step']
            for x in range(0, num_atten_steps)
        ]

        if testcase_params['traffic_direction'] == 'DL':
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

    def _test_sap_rvr(self, testcase_params):
        """ Function that gets called for each test case

        Args:
            testcase_params: dict containing test-specific parameters
        """
        # Compile test parameters from config and test name
        testcase_params = self.compile_test_params(testcase_params)

        self.setup_sap_rvr_test(testcase_params)
        result = self.run_rvr_test(testcase_params)
        self.testclass_results.append(result)
        self.process_test_results(result)
        self.pass_fail_check(result)

    #Test cases
    def test_rvr_TCP_DL_2GHz(self):
        testcase_params = collections.OrderedDict(
            sap_band='2GHz',
            sap_band_enum=wutils.WifiEnums.WIFI_CONFIG_APBAND_2G,
            traffic_type='TCP',
            traffic_direction='DL')
        self._test_sap_rvr(testcase_params)

    def test_rvr_TCP_UL_2GHz(self):
        testcase_params = collections.OrderedDict(
            sap_band='2GHz',
            sap_band_enum=wutils.WifiEnums.WIFI_CONFIG_APBAND_2G,
            traffic_type='TCP',
            traffic_direction='UL')
        self._test_sap_rvr(testcase_params)

    def test_rvr_TCP_DL_5GHz(self):
        testcase_params = collections.OrderedDict(
            sap_band='5GHz',
            sap_band_enum=wutils.WifiEnums.WIFI_CONFIG_APBAND_5G,
            traffic_type='TCP',
            traffic_direction='DL')
        self._test_sap_rvr(testcase_params)

    def test_rvr_TCP_UL_5GHz(self):
        testcase_params = collections.OrderedDict(
            sap_band='5GHz',
            sap_band_enum=wutils.WifiEnums.WIFI_CONFIG_APBAND_5G,
            traffic_type='TCP',
            traffic_direction='UL')
        self._test_sap_rvr(testcase_params)
