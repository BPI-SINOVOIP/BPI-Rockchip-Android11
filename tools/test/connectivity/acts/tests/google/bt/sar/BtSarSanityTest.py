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
import re
import time
from acts import asserts
from acts.test_utils.bt.BtSarBaseTest import BtSarBaseTest


class BtSarSanityTest(BtSarBaseTest):
    """Class to run sanity checks on BT SAR mechanisms.

    This class defines sanity test cases on BT SAR. The tests include
    a software state sanity check and a software power sanity check.
    """
    def __init__(self, controllers):
        super().__init__(controllers)

    def test_bt_sar_sanity_check_state(self):
        """ Test for BT SAR State Sanity

        BT SAR Software Sanity Test to ensure that the correct signal state
        gets propagated to the firmware. This is done by comparing expected
        device state with that read from device's logcat
        """
        #Iterating through the BT SAR scenarios
        for scenario in range(0, self.bt_sar_df.shape[0]):
            # Reading BT SAR table row into dict
            read_scenario = self.bt_sar_df.loc[scenario].to_dict()

            start_time = self.dut.adb.shell('date +%s.%m')
            time.sleep(1)

            #Setting SAR state to the read BT SAR row
            enforced_state = self.set_sar_state(self.dut, read_scenario)

            #Reading device state from logcat after forcing SAR State
            device_state = self.get_current_device_state(self.dut, start_time)

            #Comparing read device state to expected device state
            for key in enforced_state.keys():
                key_regex = r'{}:\s*(\d)'.format(key)
                try:
                    propagated_value = int(
                        re.findall(key_regex, device_state)[0])
                except TypeError:
                    propagated_value = 'NA'

                if enforced_state[key] == propagated_value:
                    self.log.info(
                        'scenario: {}, state : {}, forced_value: {}, value:{}'.
                        format(scenario, key, enforced_state[key],
                               propagated_value))
                else:
                    self.log.error(
                        'scenario:{}, state : {}, forced_value: {}, value:{}'.
                        format(scenario, key, enforced_state[key],
                               propagated_value))

    def test_bt_sar_sanity_check_power(self):
        """ Test for BT SAR Power Cap Sanity

        BT SAR Power Cap Sanity Test to ensure that the correct SAR power
        cap corresponding to the forced SAR state gets propagated to the
        firmware. This is done by comparing expected power cap read from
        the BT SAR file to the power cap read from logcat
        """

        sar_df = self.bt_sar_df
        sar_df['power_cap'] = -128

        #Iterating through the BT SAR scenarios
        for scenario in range(0, self.bt_sar_df.shape[0]):

            # Reading BT SAR table row into dict
            read_scenario = sar_df.loc[scenario].to_dict()

            start_time = self.dut.adb.shell('date +%s.%m')
            time.sleep(1)

            #Setting SAR state to the read BT SAR row
            self.set_sar_state(self.dut, read_scenario)

            #Reading device power cap from logcat after forcing SAR State
            scenario_power_cap = self.get_current_power_cap(
                self.dut, start_time)
            sar_df.loc[scenario, 'power_cap'] = scenario_power_cap
            self.log.info('scenario: {}, '
                          'sar_power: {}, power_cap:{}'.format(
                              scenario, sar_df.loc[scenario, 'BluetoothPower'],
                              sar_df.loc[scenario, 'power_cap']))

        results_file_path = os.path.join(
            self.log_path, '{}.csv'.format(self.current_test_name))
        sar_df.to_csv(results_file_path)

        # Comparing read device power cap to expected device power cap
        if sar_df['power_cap'].equals(sar_df['BluetoothPower']):
            asserts.explicit_pass('Power Caps were set according to the table')
        else:
            asserts.fail("Power Caps didn't match powers in the table")
