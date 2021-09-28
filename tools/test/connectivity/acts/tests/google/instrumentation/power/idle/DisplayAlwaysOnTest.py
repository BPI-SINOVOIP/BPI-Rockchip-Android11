#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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

from acts.test_utils.instrumentation.power import instrumentation_power_test
from acts.test_utils.instrumentation.device.command.adb_commands import common


class DisplayAlwaysOnTest(instrumentation_power_test.InstrumentationPowerTest):
    """Test class for running instrumentation test DisplayAlwaysOn."""

    def _prepare_device(self):
        super()._prepare_device()
        self.base_device_configuration()
        self.adb_run(common.doze_mode.toggle(True))
        self.adb_run(common.doze_always_on.toggle(True))
        self.adb_run(common.disable_sensors)

    def test_display_always_on(self):
        """Measures power when the device is rock bottom state plus display
        always on (known as doze mode)."""

        self.run_and_measure(
            'com.google.android.platform.powertests.IdleTestCase',
            'testIdleScreenOff')

        self.validate_power_results()
