#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging

from vts.runners.host import const
from vts.runners.host import test_runner
from vts.testcases.template.hal_hidl_gtest import hal_hidl_gtest


class VtsHalWifiSupplicantV1_0Host(hal_hidl_gtest.HidlHalGTest):
    """Host test class to run the WiFi Supplicant V1.0 HAL's VTS tests."""

    WIFI_DIRECT_FEATURE_NAME = "android.hardware.wifi.direct"

    def setUpClass(self):
        """Disable android framework."""
        super(VtsHalWifiSupplicantV1_0Host, self).setUpClass()
        self.dut = self.android_devices[0]
        self.shell = self.dut.shell
        self.dut.stop(True)

    def tearDownClass(self):
        """Enable android framework."""
        self.dut.start()
        super(VtsHalWifiSupplicantV1_0Host, self).tearDownClass()

    def CreateTestCases(self):
        """Get all registered test components and create test case objects."""
        pm_list = self.shell.Execute("pm list features")
        self._p2p_on = self.WIFI_DIRECT_FEATURE_NAME in pm_list[const.STDOUT][0]
        logging.info("Wifi P2P Feature Supported: %s", self._p2p_on)
        super(VtsHalWifiSupplicantV1_0Host, self).CreateTestCases()

    # @Override
    def CreateTestCase(self, path, tag=''):
        """Create a list of VtsHalWifiSupplicantV1_0TestCase objects.

        Args:
            path: string, absolute path of a gtest binary on device
            tag: string, a tag that will be appended to the end of test name

        Returns:
            A list of VtsHalWifiSupplicantV1_0TestCase objects
        """
        gtest_cases = super(VtsHalWifiSupplicantV1_0Host, self).CreateTestCase(path, tag)
        for gtest_case in gtest_cases:
            if not self._p2p_on:
                gtest_case.args += " --p2p_off"
        return gtest_cases


if __name__ == "__main__":
    test_runner.main()
