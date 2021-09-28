#!/usr/bin/env python3
#
# Copyright 2018, The Android Open Source Project
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

"""Unittests for result_reporter."""

# pylint: disable=line-too-long

import sys
import unittest

from io import StringIO
from unittest import mock

import result_reporter

from test_runners import test_runner_base


RESULT_PASSED_TEST = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
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

RESULT_PASSED_TEST_MODULE_2 = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule2',
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

RESULT_PASSED_TEST_RUNNER_2_NO_MODULE = test_runner_base.TestResult(
    runner_name='someTestRunner2',
    group_name=None,
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

RESULT_FAILED_TEST = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='someClassName2#sestName2',
    status=test_runner_base.FAILED_STATUS,
    details='someTrace',
    test_count=1,
    test_time='',
    runner_total=None,
    group_total=2,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

RESULT_RUN_FAILURE = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='someClassName#sostName',
    status=test_runner_base.ERROR_STATUS,
    details='someRunFailureReason',
    test_count=1,
    test_time='',
    runner_total=None,
    group_total=2,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

RESULT_INVOCATION_FAILURE = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name=None,
    test_name=None,
    status=test_runner_base.ERROR_STATUS,
    details='someInvocationFailureReason',
    test_count=1,
    test_time='',
    runner_total=None,
    group_total=None,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

RESULT_IGNORED_TEST = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='someClassName#sostName',
    status=test_runner_base.IGNORED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

RESULT_ASSUMPTION_FAILED_TEST = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='someClassName#sostName',
    status=test_runner_base.ASSUMPTION_FAILED,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={},
    test_run_name='com.android.UnitTests'
)

ADDITIONAL_INFO_PERF01_TEST01 = {u'repetition_index': u'0',
                                 u'cpu_time': u'10001.10001',
                                 u'name': u'perfName01',
                                 u'repetitions': u'0', u'run_type': u'iteration',
                                 u'label': u'2123', u'threads': u'1',
                                 u'time_unit': u'ns', u'iterations': u'1001',
                                 u'run_name': u'perfName01',
                                 u'real_time': u'11001.11001'}

RESULT_PERF01_TEST01 = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='somePerfClass01#perfName01',
    status=test_runner_base.PASSED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info=ADDITIONAL_INFO_PERF01_TEST01,
    test_run_name='com.android.UnitTests'
)

RESULT_PERF01_TEST02 = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='somePerfClass01#perfName02',
    status=test_runner_base.PASSED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={u'repetition_index': u'0', u'cpu_time': u'10002.10002',
                     u'name': u'perfName02',
                     u'repetitions': u'0', u'run_type': u'iteration',
                     u'label': u'2123', u'threads': u'1',
                     u'time_unit': u'ns', u'iterations': u'1002',
                     u'run_name': u'perfName02',
                     u'real_time': u'11002.11002'},
    test_run_name='com.android.UnitTests'
)

RESULT_PERF01_TEST03_NO_CPU_TIME = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='somePerfClass01#perfName03',
    status=test_runner_base.PASSED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={u'repetition_index': u'0',
                     u'name': u'perfName03',
                     u'repetitions': u'0', u'run_type': u'iteration',
                     u'label': u'2123', u'threads': u'1',
                     u'time_unit': u'ns', u'iterations': u'1003',
                     u'run_name': u'perfName03',
                     u'real_time': u'11003.11003'},
    test_run_name='com.android.UnitTests'
)

RESULT_PERF02_TEST01 = test_runner_base.TestResult(
    runner_name='someTestRunner',
    group_name='someTestModule',
    test_name='somePerfClass02#perfName11',
    status=test_runner_base.PASSED_STATUS,
    details=None,
    test_count=1,
    test_time='(10ms)',
    runner_total=None,
    group_total=2,
    additional_info={u'repetition_index': u'0', u'cpu_time': u'20001.20001',
                     u'name': u'perfName11',
                     u'repetitions': u'0', u'run_type': u'iteration',
                     u'label': u'2123', u'threads': u'1',
                     u'time_unit': u'ns', u'iterations': u'2001',
                     u'run_name': u'perfName11',
                     u'real_time': u'210001.21001'},
    test_run_name='com.android.UnitTests'
)

#pylint: disable=protected-access
#pylint: disable=invalid-name
class ResultReporterUnittests(unittest.TestCase):
    """Unit tests for result_reporter.py"""

    def setUp(self):
        self.rr = result_reporter.ResultReporter()

    def tearDown(self):
        mock.patch.stopall()

    @mock.patch.object(result_reporter.ResultReporter, '_print_group_title')
    @mock.patch.object(result_reporter.ResultReporter, '_update_stats')
    @mock.patch.object(result_reporter.ResultReporter, '_print_result')
    def test_process_test_result(self, mock_print, mock_update, mock_title):
        """Test process_test_result method."""
        # Passed Test
        self.assertTrue('someTestRunner' not in self.rr.runners)
        self.rr.process_test_result(RESULT_PASSED_TEST)
        self.assertTrue('someTestRunner' in self.rr.runners)
        group = self.rr.runners['someTestRunner'].get('someTestModule')
        self.assertIsNotNone(group)
        mock_title.assert_called_with(RESULT_PASSED_TEST)
        mock_update.assert_called_with(RESULT_PASSED_TEST, group)
        mock_print.assert_called_with(RESULT_PASSED_TEST)
        # Failed Test
        mock_title.reset_mock()
        self.rr.process_test_result(RESULT_FAILED_TEST)
        mock_title.assert_not_called()
        mock_update.assert_called_with(RESULT_FAILED_TEST, group)
        mock_print.assert_called_with(RESULT_FAILED_TEST)
        # Test with new Group
        mock_title.reset_mock()
        self.rr.process_test_result(RESULT_PASSED_TEST_MODULE_2)
        self.assertTrue('someTestModule2' in self.rr.runners['someTestRunner'])
        mock_title.assert_called_with(RESULT_PASSED_TEST_MODULE_2)
        # Test with new Runner
        mock_title.reset_mock()
        self.rr.process_test_result(RESULT_PASSED_TEST_RUNNER_2_NO_MODULE)
        self.assertTrue('someTestRunner2' in self.rr.runners)
        mock_title.assert_called_with(RESULT_PASSED_TEST_RUNNER_2_NO_MODULE)

    def test_print_result_run_name(self):
        """Test print run name function in print_result method."""
        try:
            rr = result_reporter.ResultReporter()
            capture_output = StringIO()
            sys.stdout = capture_output
            run_name = 'com.android.UnitTests'
            rr._print_result(test_runner_base.TestResult(
                runner_name='runner_name',
                group_name='someTestModule',
                test_name='someClassName#someTestName',
                status=test_runner_base.FAILED_STATUS,
                details='someTrace',
                test_count=2,
                test_time='(2h44m36.402s)',
                runner_total=None,
                group_total=2,
                additional_info={},
                test_run_name=run_name
            ))
            # Make sure run name in the first line.
            capture_output_str = capture_output.getvalue().strip()
            self.assertTrue(run_name in capture_output_str.split('\n')[0])
            run_name2 = 'com.android.UnitTests2'
            capture_output = StringIO()
            sys.stdout = capture_output
            rr._print_result(test_runner_base.TestResult(
                runner_name='runner_name',
                group_name='someTestModule',
                test_name='someClassName#someTestName',
                status=test_runner_base.FAILED_STATUS,
                details='someTrace',
                test_count=2,
                test_time='(2h43m36.402s)',
                runner_total=None,
                group_total=2,
                additional_info={},
                test_run_name=run_name2
            ))
            # Make sure run name in the first line.
            capture_output_str = capture_output.getvalue().strip()
            self.assertTrue(run_name2 in capture_output_str.split('\n')[0])
        finally:
            sys.stdout = sys.__stdout__

    def test_register_unsupported_runner(self):
        """Test register_unsupported_runner method."""
        self.rr.register_unsupported_runner('NotSupported')
        runner = self.rr.runners['NotSupported']
        self.assertIsNotNone(runner)
        self.assertEqual(runner, result_reporter.UNSUPPORTED_FLAG)

    def test_update_stats_passed(self):
        """Test _update_stats method."""
        # Passed Test
        group = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST, group)
        self.assertEqual(self.rr.run_stats.passed, 1)
        self.assertEqual(self.rr.run_stats.failed, 0)
        self.assertEqual(self.rr.run_stats.run_errors, False)
        self.assertEqual(self.rr.failed_tests, [])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 0)
        self.assertEqual(group.ignored, 0)
        self.assertEqual(group.run_errors, False)
        # Passed Test New Group
        group2 = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST_MODULE_2, group2)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 0)
        self.assertEqual(self.rr.run_stats.run_errors, False)
        self.assertEqual(self.rr.failed_tests, [])
        self.assertEqual(group2.passed, 1)
        self.assertEqual(group2.failed, 0)
        self.assertEqual(group.ignored, 0)
        self.assertEqual(group2.run_errors, False)

    def test_update_stats_failed(self):
        """Test _update_stats method."""
        # Passed Test
        group = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST, group)
        # Passed Test New Group
        group2 = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST_MODULE_2, group2)
        # Failed Test Old Group
        self.rr._update_stats(RESULT_FAILED_TEST, group)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 1)
        self.assertEqual(self.rr.run_stats.run_errors, False)
        self.assertEqual(self.rr.failed_tests, [RESULT_FAILED_TEST.test_name])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 1)
        self.assertEqual(group.ignored, 0)
        self.assertEqual(group.total, 2)
        self.assertEqual(group2.total, 1)
        self.assertEqual(group.run_errors, False)
        # Test Run Failure
        self.rr._update_stats(RESULT_RUN_FAILURE, group)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 1)
        self.assertEqual(self.rr.run_stats.run_errors, True)
        self.assertEqual(self.rr.failed_tests, [RESULT_FAILED_TEST.test_name])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 1)
        self.assertEqual(group.ignored, 0)
        self.assertEqual(group.run_errors, True)
        self.assertEqual(group2.run_errors, False)
        # Invocation Failure
        self.rr._update_stats(RESULT_INVOCATION_FAILURE, group)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 1)
        self.assertEqual(self.rr.run_stats.run_errors, True)
        self.assertEqual(self.rr.failed_tests, [RESULT_FAILED_TEST.test_name])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 1)
        self.assertEqual(group.ignored, 0)
        self.assertEqual(group.run_errors, True)

    def test_update_stats_ignored_and_assumption_failure(self):
        """Test _update_stats method."""
        # Passed Test
        group = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST, group)
        # Passed Test New Group
        group2 = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PASSED_TEST_MODULE_2, group2)
        # Failed Test Old Group
        self.rr._update_stats(RESULT_FAILED_TEST, group)
        # Test Run Failure
        self.rr._update_stats(RESULT_RUN_FAILURE, group)
        # Invocation Failure
        self.rr._update_stats(RESULT_INVOCATION_FAILURE, group)
        # Ignored Test
        self.rr._update_stats(RESULT_IGNORED_TEST, group)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 1)
        self.assertEqual(self.rr.run_stats.run_errors, True)
        self.assertEqual(self.rr.failed_tests, [RESULT_FAILED_TEST.test_name])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 1)
        self.assertEqual(group.ignored, 1)
        self.assertEqual(group.run_errors, True)
        # 2nd Ignored Test
        self.rr._update_stats(RESULT_IGNORED_TEST, group)
        self.assertEqual(self.rr.run_stats.passed, 2)
        self.assertEqual(self.rr.run_stats.failed, 1)
        self.assertEqual(self.rr.run_stats.run_errors, True)
        self.assertEqual(self.rr.failed_tests, [RESULT_FAILED_TEST.test_name])
        self.assertEqual(group.passed, 1)
        self.assertEqual(group.failed, 1)
        self.assertEqual(group.ignored, 2)
        self.assertEqual(group.run_errors, True)

        # Assumption_Failure test
        self.rr._update_stats(RESULT_ASSUMPTION_FAILED_TEST, group)
        self.assertEqual(group.assumption_failed, 1)
        # 2nd Assumption_Failure test
        self.rr._update_stats(RESULT_ASSUMPTION_FAILED_TEST, group)
        self.assertEqual(group.assumption_failed, 2)

    def test_print_summary_ret_val(self):
        """Test print_summary method's return value."""
        # PASS Case
        self.rr.process_test_result(RESULT_PASSED_TEST)
        self.assertEqual(0, self.rr.print_summary())
        # PASS Case + Fail Case
        self.rr.process_test_result(RESULT_FAILED_TEST)
        self.assertNotEqual(0, self.rr.print_summary())
        # PASS Case + Fail Case + PASS Case
        self.rr.process_test_result(RESULT_PASSED_TEST_MODULE_2)
        self.assertNotEqual(0, self.rr.print_summary())

    def test_print_summary_ret_val_err_stat(self):
        """Test print_summary method's return value."""
        # PASS Case
        self.rr.process_test_result(RESULT_PASSED_TEST)
        self.assertEqual(0, self.rr.print_summary())
        # PASS Case + Fail Case
        self.rr.process_test_result(RESULT_RUN_FAILURE)
        self.assertNotEqual(0, self.rr.print_summary())
        # PASS Case + Fail Case + PASS Case
        self.rr.process_test_result(RESULT_PASSED_TEST_MODULE_2)
        self.assertNotEqual(0, self.rr.print_summary())

    def test_update_perf_info(self):
        """Test update_perf_info method."""
        group = result_reporter.RunStat()
        # 1. Test PerfInfo after RESULT_PERF01_TEST01
        # _update_stats() will call _update_perf_info()
        self.rr._update_stats(RESULT_PERF01_TEST01, group)
        correct_perf_info = []
        # trim the time form 10001.10001 to 10001
        trim_perf01_test01 = {u'repetition_index': u'0', u'cpu_time': u'10001',
                              u'name': u'perfName01',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'1001',
                              u'run_name': u'perfName01',
                              u'real_time': u'11001',
                              'test_name': 'somePerfClass01#perfName01'}
        correct_perf_info.append(trim_perf01_test01)
        self.assertEqual(self.rr.run_stats.perf_info.perf_info,
                         correct_perf_info)
        # 2. Test PerfInfo after RESULT_PERF01_TEST01
        self.rr._update_stats(RESULT_PERF01_TEST02, group)
        trim_perf01_test02 = {u'repetition_index': u'0', u'cpu_time': u'10002',
                              u'name': u'perfName02',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'1002',
                              u'run_name': u'perfName02',
                              u'real_time': u'11002',
                              'test_name': 'somePerfClass01#perfName02'}
        correct_perf_info.append(trim_perf01_test02)
        self.assertEqual(self.rr.run_stats.perf_info.perf_info,
                         correct_perf_info)
        # 3. Test PerfInfo after RESULT_PERF02_TEST01
        self.rr._update_stats(RESULT_PERF02_TEST01, group)
        trim_perf02_test01 = {u'repetition_index': u'0', u'cpu_time': u'20001',
                              u'name': u'perfName11',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'2001',
                              u'run_name': u'perfName11',
                              u'real_time': u'210001',
                              'test_name': 'somePerfClass02#perfName11'}
        correct_perf_info.append(trim_perf02_test01)
        self.assertEqual(self.rr.run_stats.perf_info.perf_info,
                         correct_perf_info)
        # 4. Test PerfInfo after RESULT_PERF01_TEST03_NO_CPU_TIME
        self.rr._update_stats(RESULT_PERF01_TEST03_NO_CPU_TIME, group)
        # Nothing added since RESULT_PERF01_TEST03_NO_CPU_TIME lack of cpu_time
        self.assertEqual(self.rr.run_stats.perf_info.perf_info,
                         correct_perf_info)

    def test_classify_perf_info(self):
        """Test _classify_perf_info method."""
        group = result_reporter.RunStat()
        self.rr._update_stats(RESULT_PERF01_TEST01, group)
        self.rr._update_stats(RESULT_PERF01_TEST02, group)
        self.rr._update_stats(RESULT_PERF02_TEST01, group)
        # trim the time form 10001.10001 to 10001
        trim_perf01_test01 = {u'repetition_index': u'0', u'cpu_time': u'10001',
                              u'name': u'perfName01',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'1001',
                              u'run_name': u'perfName01',
                              u'real_time': u'11001',
                              'test_name': 'somePerfClass01#perfName01'}
        trim_perf01_test02 = {u'repetition_index': u'0', u'cpu_time': u'10002',
                              u'name': u'perfName02',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'1002',
                              u'run_name': u'perfName02',
                              u'real_time': u'11002',
                              'test_name': 'somePerfClass01#perfName02'}
        trim_perf02_test01 = {u'repetition_index': u'0', u'cpu_time': u'20001',
                              u'name': u'perfName11',
                              u'repetitions': u'0', u'run_type': u'iteration',
                              u'label': u'2123', u'threads': u'1',
                              u'time_unit': u'ns', u'iterations': u'2001',
                              u'run_name': u'perfName11',
                              u'real_time': u'210001',
                              'test_name': 'somePerfClass02#perfName11'}
        correct_classify_perf_info = {"somePerfClass01":[trim_perf01_test01,
                                                         trim_perf01_test02],
                                      "somePerfClass02":[trim_perf02_test01]}
        classify_perf_info, max_len = self.rr.run_stats.perf_info._classify_perf_info()
        correct_max_len = {'real_time': 6, 'cpu_time': 5, 'name': 10,
                           'iterations': 9, 'time_unit': 2}
        self.assertEqual(max_len, correct_max_len)
        self.assertEqual(classify_perf_info, correct_classify_perf_info)


if __name__ == '__main__':
    unittest.main()
