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

from acts.test_utils.power.PowerBaseTest import PowerBaseTest


class PowerBaselineTest(PowerBaseTest):
    """Power baseline test.

    Tests power consumption on rockbottom to verify the ability to set power
    consumption to a minimum during connectivity power tests.
    """

    def test_power_baseline(self):
        """Measures power when the device is on rockbottom. """

        # Make the device go to sleep
        self.dut.droid.goToSleepNow()

        # Measure power
        result = self.collect_power_data()

        # Check if power measurement is below the required value
        self.pass_fail_check(result.average_current)
