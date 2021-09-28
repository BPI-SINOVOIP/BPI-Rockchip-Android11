#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

from acts import signals
from acts.base_test import BaseTestClass
from acts import asserts


class FuchsiaLoggingTest(BaseTestClass):
    def setup_class(self):
        super().setup_class()
        self.dut = self.fuchsia_devices[0]
        self.message = "Logging Test"

    def test_log_err(self):
        result = self.dut.logging_lib.logE(self.message)
        if result.get("error") is None:
            signals.TestPass(result.get("result"))
        else:
            signals.TestFailure(result.get("error"))

    def test_log_info(self):
        result = self.dut.logging_lib.logI(self.message)
        if result.get("error") is None:
            signals.TestPass(result.get("result"))
        else:
            signals.TestFailure(result.get("error"))

    def test_log_warn(self):
        result = self.dut.logging_lib.logW(self.message)
        if result.get("error") is None:
            signals.TestPass(result.get("result"))
        else:
            signals.TestFailure(result.get("error"))
