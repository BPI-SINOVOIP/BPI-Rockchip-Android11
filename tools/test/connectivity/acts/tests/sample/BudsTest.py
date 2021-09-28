#!/usr/bin/env python3.4
#
#   Copyright 2016 - The Android Open Source Project
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

from acts.base_test import BaseTestClass


class BudsTest(BaseTestClass):
    def setup_class(self):
        super().setup_class()
        self.dut = self.buds_devices[0]

    def test_make_toast(self):
        self.log.info('Battery Level: %s', self.dut.get_battery_level())
        self.log.info('Gas Gauge Current: %s', self.dut.get_gas_gauge_current())
        self.log.info('Gas Gauge Voltage: %s', self.dut.get_gas_gauge_voltage())
        self.log.info('Serial Log Dump: %s', self.dut.get_serial_log())
