#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import os
import time
import numpy as np
import acts.test_utils.bt.bt_test_utils as bt_utils
import acts.test_utils.wifi.wifi_performance_test_utils as wifi_utils

from acts import asserts
from functools import partial
from acts.test_utils.bt.BtSarBaseTest import BtSarBaseTest


class BtSarTpcTest(BtSarBaseTest):
    """ Class to define BT SAR Transmit power control tests in

    This class defines tests to detect any anomalies in the
    Transmit power control mechanisms
    """
    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = ['scenario_count']
        self.unpack_userparams(req_params)
        self.tests = self.generate_test_cases()

    def setup_class(self):
        super().setup_class()

        # Pushing custom table
        self.backup_sar_path = os.path.join(self.dut.device_log_path,
                                            self.BACKUP_BT_SAR_TABLE_NAME)
        self.dut.adb.pull(self.sar_file_path, self.backup_sar_path)
        self.push_table(self.dut, self.custom_sar_path)

        self.attenuator.set_atten(self.atten_min)
        self.pathloss = int(self.calibration_params['pathloss'])

        self.sar_df = self.bt_sar_df.copy()
        self.sar_df['power_cap'] = -128
        self.sar_df['TPC_result'] = -1

    def setup_test(self):
        super().setup_test()

        self.tpc_sweep_range = range(self.atten_min, self.pl10_atten)

        self.tpc_plots_figure = wifi_utils.BokehFigure(
            title='{}_{}'.format(self.current_test_name, 'curve'),
            x_label='Pathloss(dBm)',
            primary_y_label='Tx Power(dBm)')

        self.tpc_plots_derivative_figure = wifi_utils.BokehFigure(
            title='{}_{}'.format(self.current_test_name, 'curve_derivative'),
            x_label='Pathloss(dBm)',
            primary_y_label='Tx Power(dB)')

    def teardown_class(self):
        super().teardown_class()
        result_file_name = '{}.csv'.format(self.__class__.__name__)
        result_file_path = os.path.join(self.log_path, result_file_name)
        self.sar_df.to_csv(result_file_path)

        # Pushing default table back
        self.push_table(self.dut, self.backup_sar_path)

    def generate_test_cases(self):
        """Function to generate test cases.
        Function to generate a test case per BT SAR table row.

        Returns: list of generated test cases.
        """
        test_cases = []
        for scenario in range(int(self.scenario_count)):
            test_name = 'test_bt_sar_tpc_{}'.format(scenario)
            setattr(self, test_name, partial(self._test_bt_sar_tpc, scenario))
            test_cases.append(test_name)
        return test_cases

    def process_tpc_results(self, tx_power_list, pwlv_list):
        """Processes the results of tpc sweep.

        Processes tpc sweep to ensure that tx power changes
        as expected while sweeping.

        Args:
            tx_power_list: list of tx power measured during the TPC sweep.
            pwlv_list: list of power levels observed during the TPC sweep.

        Returns:
            tpc_verdict : result of the tpc sweep; PASS/FAIL.
            tx_power_derivative : peaks observed during TPC sweep.
        """

        tpc_verdict = 'FAIL'

        # Locating power level changes in the sweep
        pwlv_derivative_bool = list(np.diff([pwlv_list[0]] + pwlv_list) == 1)

        # Diff-ing the list to get derivative of the list
        tx_power_derivative = np.diff([tx_power_list[0]] + tx_power_list)

        # Checking for negative peaks
        if tx_power_derivative.min() < self.tpc_threshold['negative']:
            return [tpc_verdict, tx_power_derivative]

        # Locating legitimate tx power changes
        tx_power_derivative_bool = [
            self.tpc_threshold['positive'][0] < x <
            self.tpc_threshold['positive'][1] for x in tx_power_derivative
        ]

        # Ensuring that changes in power level correspond to tx power changes
        if pwlv_derivative_bool == tx_power_derivative_bool:
            tpc_verdict = 'PASS'
            return [tpc_verdict, tx_power_derivative]

        return [tpc_verdict, tx_power_derivative]

    def _test_bt_sar_tpc(self, scenario):
        """Performs TCP sweep for the given scenario.

        Function performs and evaluates TPC sweep for a given scenario.

        Args:
            scenario: row of the BT SAR table.
        """

        master_tx_power_list = []
        pwlv_list = []

        # Reading BT SAR Scenario from the table
        start_time = self.dut.adb.shell('date +%s.%m')
        time.sleep(1)

        # Forcing the SAR state
        read_scenario = self.sar_df.loc[scenario].to_dict()
        self.set_sar_state(self.dut, read_scenario)

        # Reading power cap
        self.sar_df.loc[scenario, 'power_cap'] = self.get_current_power_cap(
            self.dut, start_time)

        # TPC sweep
        for atten in self.tpc_sweep_range:

            self.attenuator.set_atten(atten)
            self.log.info('Current TPC attenuation {} dB; scenario {}'.format(
                atten, scenario))

            processed_bqr = bt_utils.get_bt_metric(self.android_devices,
                                                   self.duration)
            # Recording master rssi and pwlv
            master_tx_power_list.append(
                processed_bqr['rssi'][self.bt_device_controller.serial] +
                self.pathloss + atten)
            pwlv_list.append(processed_bqr['pwlv'][self.dut.serial])

        # Processing tpc sweep results
        [self.sar_df.loc[scenario, 'TPC_result'], tx_power_derivative
         ] = self.process_tpc_results(master_tx_power_list, pwlv_list)

        # Plot TPC curves
        self.tpc_plots_figure.add_line(self.tpc_sweep_range,
                                       master_tx_power_list,
                                       str(scenario),
                                       marker='circle')

        self.tpc_plots_derivative_figure.add_line(self.tpc_sweep_range,
                                                  tx_power_derivative,
                                                  str(scenario),
                                                  marker='circle')

        self.tpc_plots_figure.generate_figure()
        self.tpc_plots_derivative_figure.generate_figure()

        # Saving the TPC curves
        plot_file_path = os.path.join(self.log_path,
                                      '{}.html'.format(self.current_test_name))
        wifi_utils.BokehFigure.save_figures(
            [self.tpc_plots_figure, self.tpc_plots_derivative_figure],
            plot_file_path)

        # Asserting pass and fail
        if self.sar_df.loc[scenario, 'TPC_result'] == 'FAIL':
            asserts.fail('TPC sweep failed for scenario: {}'.format(scenario))

        else:
            asserts.explicit_pass(
                'TPC sweep passed for scenario: {}'.format(scenario))
