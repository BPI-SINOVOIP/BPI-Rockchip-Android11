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

import unittest

from host_controller.campaigns import cts
from host_controller.campaigns import gts
from host_controller.campaigns import sts
from host_controller.campaigns import vts

from host_controller.campaigns.testdata import default_testcase


class CampaignTest(unittest.TestCase):
    """Unit tests for the campaign generators."""

    def setUp(self):
        self.maxDiff = None

    def testVtsBaseline(self):
        """Tests the default device's vts scenario."""
        test_name = "vts/vts"
        results = vts.EmitConsoleCommands(
            **default_testcase.GenerateInputData(test_name))
        self.assertEqual(
            default_testcase.GenerateOutputData(test_name), results)

    def testCtsOnGsiBaseline(self):
        """Tests the default device's vts scenario."""
        test_name = "vts/cts-on-gsi"
        results = vts.EmitConsoleCommands(
            **default_testcase.GenerateInputData(test_name))
        self.assertEqual(
            default_testcase.GenerateOutputData(test_name), results)

    def testCtsBaseline(self):
        """Tests the default device's vts scenario."""
        test_name = "cts/cts"
        results = cts.EmitConsoleCommands(
            **default_testcase.GenerateInputData(test_name))
        self.assertEqual(
            default_testcase.GenerateOutputData(test_name), results)

    def testGtsBaseline(self):
        """Tests the default device's vts scenario."""
        test_name = "gts/gts"
        results = gts.EmitConsoleCommands(
            **default_testcase.GenerateInputData(test_name))
        self.assertEqual(
            default_testcase.GenerateOutputData(test_name), results)

    def testStsBaseline(self):
        """Tests the default device's vts scenario."""
        test_name = "sts/sts"
        results = sts.EmitConsoleCommands(
            **default_testcase.GenerateInputData(test_name))
        self.assertEqual(
            default_testcase.GenerateOutputData(test_name), results)


if __name__ == '__main__':
    unittest.main()
