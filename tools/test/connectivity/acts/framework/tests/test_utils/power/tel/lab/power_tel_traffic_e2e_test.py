#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import unittest
import tests.test_utils.power.tel.lab.mock_bokeh
import acts.test_utils.power.cellular.cellular_traffic_power_test as ctpt
import mobly.config_parser as mobly_config_parser
from acts.test_utils.power.tel_simulations.LteSimulation import LteSimulation
from acts.controllers.rohdeschwarz_lib import cmw500_cellular_simulator as cmw
from unittest import mock


class PowerTelTrafficE2eTest(unittest.TestCase):
    """ E2E sanity test for the power cellular traffic tests """
    @classmethod
    def setUpClass(cls):
        cls.PTTT = ctpt.PowerTelTrafficTest
        cls.PTTT.log = mock.Mock()
        cls.PTTT.log_path = ''

    @mock.patch('json.load')
    @mock.patch('builtins.open')
    @mock.patch('os.chmod')
    @mock.patch('os.system')
    @mock.patch('time.sleep')
    @mock.patch(
        'acts.test_utils.tel.tel_test_utils.toggle_airplane_mode_by_adb')
    @mock.patch('acts.test_utils.wifi.wifi_test_utils.reset_wifi')
    @mock.patch('acts.test_utils.wifi.wifi_test_utils.wifi_toggle_state')
    @mock.patch(
        'acts.metrics.loggers.blackbox.BlackboxMetricLogger.for_test_case')
    @mock.patch(
        'acts.test_utils.power.loggers.power_metric_logger.PowerMetricLogger.for_test_case'
    )
    def test_e2e(self, *args):

        # Configure the test
        test_to_mock = 'test_lte_traffic_direction_dlul_blimit_0_0'
        self.tb_key = 'testbed_configs'
        test_run_config = mobly_config_parser.TestRunConfig()
        test_run_config.testbed_name = 'MockTestBed'
        test_run_config.log_path = '/tmp'
        test_run_config.summary_writer = mock.MagicMock()
        test = self.PTTT(test_run_config)
        mock_android = mock.Mock()
        mock_android.model = 'coral'
        test.unpack_userparams(
            android_devices=[mock_android],
            monsoons=[mock.Mock()],
            iperf_servers=[mock.Mock(), mock.Mock()],
            packet_senders=[mock.Mock(), mock.Mock()],
            custom_files=['pass_fail_threshold_coral.json', 'rockbottom_coral.sh'],
            simulation=mock.Mock(spec=LteSimulation),
            mon_freq=5000,
            mon_duration=0,
            mon_offset=0,
            current_test_name=test_to_mock,
            test_name=test_to_mock,
            test_result=mock.Mock(),
            bug_report={},
            dut_rockbottom=mock.Mock(),
            start_tel_traffic=mock.Mock(),
            init_simulation=mock.Mock(),
            initialize_simulator=mock.Mock(return_value=mock.Mock(
                spec=cmw.CMW500CellularSimulator)),
            collect_power_data=mock.Mock(),
            get_iperf_results=mock.Mock(return_value={
                'ul': 0,
                'dl': 0
            }),
            pass_fail_check=mock.Mock())

        # Emulate lifecycle
        test.setup_class()
        test.setup_test()
        test.power_tel_traffic_test()
        test.teardown_test()
        test.teardown_class()

        self.assertTrue(test.start_tel_traffic.called,
                        'Start traffic was not called')
        self.assertTrue(test.init_simulation.called,
                        'Simulation was not initialized')
        self.assertTrue(test.initialize_simulator.called,
                        'Simulator was not initialized')
        self.assertTrue(test.collect_power_data.called,
                        'Power data was not collected')
        self.assertTrue(test.get_iperf_results.called,
                        'Did not get iperf results')
        self.assertTrue(test.pass_fail_check.called,
                        'Pass/Fail check was not performed')


if __name__ == '__main__':
    unittest.main()
