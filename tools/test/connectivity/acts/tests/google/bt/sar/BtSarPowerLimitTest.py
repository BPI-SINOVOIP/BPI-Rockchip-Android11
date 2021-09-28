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
import acts.test_utils.bt.bt_test_utils as bt_utils
import acts.test_utils.wifi.wifi_performance_test_utils as wifi_utils

from acts import asserts
from acts.test_utils.bt.BtSarBaseTest import BtSarBaseTest


class BtSarPowerLimitTest(BtSarBaseTest):
    """Class to define BT SAR power cap tests.

    This class defines tests that iterate over and force different
    states in the BT SAR table and calculates the TX power at the
    antenna port.
    """
    def setup_test(self):
        super().setup_test()

        # BokehFigure object
        self.plot = wifi_utils.BokehFigure(title='{}'.format(
            self.current_test_name),
                                           x_label='Scenarios',
                                           primary_y_label='TX power(dBm)')

    def save_sar_plot(self, df, results_file_path):
        """ Saves SAR plot to the path given.

        Args:
            df: Processed SAR table sweep results
            results_file_path: path where the bokeh figure is to saved
        """
        self.plot.add_line(df.index,
                           df['expected_tx_power'],
                           'expected',
                           marker='circle')
        self.plot.add_line(df.index,
                           df['measured_tx_power'],
                           'measured',
                           marker='circle')

        results_file_path = os.path.join(
            self.log_path, '{}.html'.format(self.current_test_name))
        self.plot.generate_figure()
        wifi_utils.BokehFigure.save_figures([self.plot], results_file_path)

    def sweep_table(self, sort_order=''):
        """Iterates over the BT SAR table and forces signal states.

        Iterates over BT SAR table and forces signal states,
        measuring RSSI and power level for each state.

        Args:
            sort_order: decides the sort order of the BT SAR Table.
        Returns:
            sar_df : SAR table sweep results in pandas dataframe
        """

        sar_df = self.bt_sar_df.copy()
        sar_df['power_cap'] = -128
        sar_df['slave_rssi'] = -128
        sar_df['master_rssi'] = -128
        sar_df['pwlv'] = -1

        # Sorts the table
        if sort_order:
            if sort_order.lower() == 'ascending':
                sar_df = sar_df.sort_values(by=['BluetoothPower'],
                                            ascending=True)
            else:
                sar_df = sar_df.sort_values(by=['BluetoothPower'],
                                            ascending=False)
        # Sweeping BT SAR table
        for scenario in range(sar_df.shape[0]):
            # Reading BT SAR Scenario from the table
            read_scenario = sar_df.loc[scenario].to_dict()

            start_time = self.dut.adb.shell('date +%s.%m')
            time.sleep(1)
            self.set_sar_state(self.dut, read_scenario)

            # Appending BT metrics to the table
            sar_df.loc[scenario, 'power_cap'] = self.get_current_power_cap(
                self.dut, start_time)
            processed_bqr_results = bt_utils.get_bt_metric(
                self.android_devices, self.duration)
            sar_df.loc[scenario, 'slave_rssi'] = processed_bqr_results['rssi'][
                self.bt_device_controller.serial]
            sar_df.loc[scenario, 'master_rssi'] = processed_bqr_results[
                'rssi'][self.dut.serial]
            sar_df.loc[scenario, 'pwlv'] = processed_bqr_results['pwlv'][
                self.dut.serial]
            self.log.info(
                'scenario:{}, power_cap:{},  s_rssi:{}, m_rssi:{}, m_pwlv:{}'.
                format(scenario, sar_df.loc[scenario, 'power_cap'],
                       sar_df.loc[scenario, 'slave_rssi'],
                       sar_df.loc[scenario, 'master_rssi'],
                       sar_df.loc[scenario, 'pwlv']))
        self.log.info('BT SAR Table swept')

        return sar_df

    def process_table(self, sar_df):
        """Processes the results of sweep_table and computes BT TX power.

        Processes the results of sweep_table and computes BT TX power
        after factoring in the path loss and FTM offsets.

        Args:
             sar_df: BT SAR table after the sweep

        Returns:
            sar_df: processed BT SAR table
        """

        pathloss = self.calibration_params['pathloss']

        # Adding a target power column
        sar_df['target_power'] = sar_df['pwlv'].astype(str).map(
            self.calibration_params['target_power'])

        # Adding a ftm  power column
        sar_df['ftm_power'] = sar_df['pwlv'].astype(str).map(
            self.calibration_params['ftm_power'])

        # BT SAR Backoff for each scenario
        sar_df['backoff'] = sar_df['target_power'] - sar_df['power_cap'] / 4

        sar_df['measured_tx_power'] = sar_df['slave_rssi'] + pathloss
        sar_df['expected_tx_power'] = sar_df['ftm_power'] - sar_df['backoff']

        sar_df['delta'] = sar_df['expected_tx_power'] - sar_df[
            'measured_tx_power']
        self.log.info('Sweep results processed')

        results_file_path = os.path.join(self.log_path, self.current_test_name)
        sar_df.to_csv('{}.csv'.format(results_file_path))
        self.save_sar_plot(sar_df, results_file_path)

        return sar_df

    def process_results(self, sar_df):
        """Determines the test results of the sweep.

         Parses the processed table with computed BT TX power values
         to return pass or fail.

        Args:
             sar_df: processed BT SAR table
        """

        # checks for errors at particular points in the sweep
        max_error_result = abs(sar_df['delta']) > self.max_error_threshold
        if False in max_error_result:
            asserts.fail('Maximum Error Threshold Exceeded')

        # checks for error accumulation across the sweep
        if sar_df['delta'].sum() > self.agg_error_threshold:
            asserts.fail(
                'Aggregate Error Threshold Exceeded. Error: {} Threshold: {}'.
                format(sar_df['delta'].sum(), self.agg_error_threshold))

        else:
            asserts.explicit_pass('Measured and Expected Power Values in line')

    def test_bt_sar_table(self):
        sar_df = self.sweep_table()
        sar_df = self.process_table(sar_df)
        self.process_results(sar_df)

    def test_bt_sar_custom_table(self):
        # Pushes the custom file
        backup_sar_path = os.path.join(self.dut.device_log_path,
                                       self.BACKUP_BT_SAR_TABLE_NAME)
        self.push_table(self.dut, self.custom_sar_path, backup_sar_path)

        # Connect master and slave
        bt_utils.connect_phone_to_headset(self.dut, self.bt_device, 60)

        sar_df = self.sweep_table()
        sar_df = self.process_table(sar_df)

        # Pushing the backup table back
        self.push_table(self.dut, backup_sar_path)

        self.process_results(sar_df)
