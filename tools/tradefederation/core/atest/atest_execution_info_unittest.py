#!/usr/bin/env python
#
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unittest for atest_execution_info."""

import time
import unittest

from test_runners import test_runner_base
import atest_execution_info as aei
import result_reporter

RESULT_TEST_TEMPLATE = test_runner_base.TestResult(
    runner_name='someRunner',
    group_name='someModule',
    test_name='someClassName#sostName',
    status=test_runner_base.PASSED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

# pylint: disable=protected-access
class AtestRunInfoUnittests(unittest.TestCase):
    """Unit tests for atest_execution_info.py"""

    def test_arrange_test_result_one_module(self):
        """Test _arrange_test_result method with only one module."""
        pass_1 = self._create_test_result(status=test_runner_base.PASSED_STATUS)
        pass_2 = self._create_test_result(status=test_runner_base.PASSED_STATUS)
        pass_3 = self._create_test_result(status=test_runner_base.PASSED_STATUS)
        fail_1 = self._create_test_result(status=test_runner_base.FAILED_STATUS)
        fail_2 = self._create_test_result(status=test_runner_base.FAILED_STATUS)
        ignore_1 = self._create_test_result(status=test_runner_base.IGNORED_STATUS)
        reporter_1 = result_reporter.ResultReporter()
        reporter_1.all_test_results.extend([pass_1, pass_2, pass_3])
        reporter_2 = result_reporter.ResultReporter()
        reporter_2.all_test_results.extend([fail_1, fail_2, ignore_1])
        info_dict = {}
        aei.AtestExecutionInfo._arrange_test_result(info_dict, [reporter_1, reporter_2])
        expect_summary = {aei._STATUS_IGNORED_KEY : 1,
                          aei._STATUS_FAILED_KEY : 2,
                          aei._STATUS_PASSED_KEY : 3}
        self.assertEqual(expect_summary, info_dict[aei._TOTAL_SUMMARY_KEY])

    def test_arrange_test_result_multi_module(self):
        """Test _arrange_test_result method with multi module."""
        group_a_pass_1 = self._create_test_result(group_name='grpup_a',
                                                  status=test_runner_base.PASSED_STATUS)
        group_b_pass_1 = self._create_test_result(group_name='grpup_b',
                                                  status=test_runner_base.PASSED_STATUS)
        group_c_pass_1 = self._create_test_result(group_name='grpup_c',
                                                  status=test_runner_base.PASSED_STATUS)
        group_b_fail_1 = self._create_test_result(group_name='grpup_b',
                                                  status=test_runner_base.FAILED_STATUS)
        group_c_fail_1 = self._create_test_result(group_name='grpup_c',
                                                  status=test_runner_base.FAILED_STATUS)
        group_c_ignore_1 = self._create_test_result(group_name='grpup_c',
                                                    status=test_runner_base.IGNORED_STATUS)
        reporter_1 = result_reporter.ResultReporter()
        reporter_1.all_test_results.extend([group_a_pass_1, group_b_pass_1, group_c_pass_1])
        reporter_2 = result_reporter.ResultReporter()
        reporter_2.all_test_results.extend([group_b_fail_1, group_c_fail_1, group_c_ignore_1])

        info_dict = {}
        aei.AtestExecutionInfo._arrange_test_result(info_dict, [reporter_1, reporter_2])
        expect_group_a_summary = {aei._STATUS_IGNORED_KEY : 0,
                                  aei._STATUS_FAILED_KEY : 0,
                                  aei._STATUS_PASSED_KEY : 1}
        self.assertEqual(
            expect_group_a_summary,
            info_dict[aei._TEST_RUNNER_KEY]['someRunner']['grpup_a'][aei._SUMMARY_KEY])

        expect_group_b_summary = {aei._STATUS_IGNORED_KEY : 0,
                                  aei._STATUS_FAILED_KEY : 1,
                                  aei._STATUS_PASSED_KEY : 1}
        self.assertEqual(
            expect_group_b_summary,
            info_dict[aei._TEST_RUNNER_KEY]['someRunner']['grpup_b'][aei._SUMMARY_KEY])

        expect_group_c_summary = {aei._STATUS_IGNORED_KEY : 1,
                                  aei._STATUS_FAILED_KEY : 1,
                                  aei._STATUS_PASSED_KEY : 1}
        self.assertEqual(
            expect_group_c_summary,
            info_dict[aei._TEST_RUNNER_KEY]['someRunner']['grpup_c'][aei._SUMMARY_KEY])

        expect_total_summary = {aei._STATUS_IGNORED_KEY : 1,
                                aei._STATUS_FAILED_KEY : 2,
                                aei._STATUS_PASSED_KEY : 3}
        self.assertEqual(expect_total_summary, info_dict[aei._TOTAL_SUMMARY_KEY])

    def test_preparation_time(self):
        """Test preparation_time method."""
        start_time = time.time()
        aei.PREPARE_END_TIME = None
        self.assertTrue(aei.preparation_time(start_time) is None)
        aei.PREPARE_END_TIME = time.time()
        self.assertFalse(aei.preparation_time(start_time) is None)

    def test_arrange_test_result_multi_runner(self):
        """Test _arrange_test_result method with multi runner."""
        runner_a_pass_1 = self._create_test_result(runner_name='runner_a',
                                                   status=test_runner_base.PASSED_STATUS)
        runner_a_pass_2 = self._create_test_result(runner_name='runner_a',
                                                   status=test_runner_base.PASSED_STATUS)
        runner_a_pass_3 = self._create_test_result(runner_name='runner_a',
                                                   status=test_runner_base.PASSED_STATUS)
        runner_b_fail_1 = self._create_test_result(runner_name='runner_b',
                                                   status=test_runner_base.FAILED_STATUS)
        runner_b_fail_2 = self._create_test_result(runner_name='runner_b',
                                                   status=test_runner_base.FAILED_STATUS)
        runner_b_ignore_1 = self._create_test_result(runner_name='runner_b',
                                                     status=test_runner_base.IGNORED_STATUS)

        reporter_1 = result_reporter.ResultReporter()
        reporter_1.all_test_results.extend([runner_a_pass_1, runner_a_pass_2, runner_a_pass_3])
        reporter_2 = result_reporter.ResultReporter()
        reporter_2.all_test_results.extend([runner_b_fail_1, runner_b_fail_2, runner_b_ignore_1])
        info_dict = {}
        aei.AtestExecutionInfo._arrange_test_result(info_dict, [reporter_1, reporter_2])
        expect_group_a_summary = {aei._STATUS_IGNORED_KEY : 0,
                                  aei._STATUS_FAILED_KEY : 0,
                                  aei._STATUS_PASSED_KEY : 3}
        self.assertEqual(
            expect_group_a_summary,
            info_dict[aei._TEST_RUNNER_KEY]['runner_a']['someModule'][aei._SUMMARY_KEY])

        expect_group_b_summary = {aei._STATUS_IGNORED_KEY : 1,
                                  aei._STATUS_FAILED_KEY : 2,
                                  aei._STATUS_PASSED_KEY : 0}
        self.assertEqual(
            expect_group_b_summary,
            info_dict[aei._TEST_RUNNER_KEY]['runner_b']['someModule'][aei._SUMMARY_KEY])

        expect_total_summary = {aei._STATUS_IGNORED_KEY : 1,
                                aei._STATUS_FAILED_KEY : 2,
                                aei._STATUS_PASSED_KEY : 3}
        self.assertEqual(expect_total_summary, info_dict[aei._TOTAL_SUMMARY_KEY])

    def _create_test_result(self, **kwargs):
        """A Helper to create TestResult"""
        test_info = test_runner_base.TestResult(**RESULT_TEST_TEMPLATE._asdict())
        return test_info._replace(**kwargs)

if __name__ == "__main__":
    unittest.main()
