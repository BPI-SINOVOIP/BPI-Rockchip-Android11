#!/usr/bin/env python3
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

"""Unittests for event_handler."""

# pylint: disable=line-too-long

import unittest

from importlib import reload
from unittest import mock

from test_runners import atest_tf_test_runner as atf_tr
from test_runners import event_handler as e_h
from test_runners import test_runner_base


EVENTS_NORMAL = [
    ('TEST_MODULE_STARTED', {
        'moduleContextFileName':'serial-util1146216{974}2772610436.ser',
        'moduleName':'someTestModule'}),
    ('TEST_RUN_STARTED', {'testCount': 2, 'runName': 'com.android.UnitTests'}),
    ('TEST_STARTED', {'start_time':52, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_ENDED', {'end_time':1048, 'className':'someClassName',
                    'testName':'someTestName'}),
    ('TEST_STARTED', {'start_time':48, 'className':'someClassName2',
                      'testName':'someTestName2'}),
    ('TEST_FAILED', {'className':'someClassName2', 'testName':'someTestName2',
                     'trace': 'someTrace'}),
    ('TEST_ENDED', {'end_time':9876450, 'className':'someClassName2',
                    'testName':'someTestName2'}),
    ('TEST_RUN_ENDED', {}),
    ('TEST_MODULE_ENDED', {'foo': 'bar'}),
]

EVENTS_RUN_FAILURE = [
    ('TEST_MODULE_STARTED', {
        'moduleContextFileName': 'serial-util11462169742772610436.ser',
        'moduleName': 'someTestModule'}),
    ('TEST_RUN_STARTED', {'testCount': 2, 'runName': 'com.android.UnitTests'}),
    ('TEST_STARTED', {'start_time':10, 'className': 'someClassName',
                      'testName':'someTestName'}),
    ('TEST_RUN_FAILED', {'reason': 'someRunFailureReason'})
]


EVENTS_INVOCATION_FAILURE = [
    ('TEST_RUN_STARTED', {'testCount': None, 'runName': 'com.android.UnitTests'}),
    ('INVOCATION_FAILED', {'cause': 'someInvocationFailureReason'})
]

EVENTS_MISSING_TEST_RUN_STARTED_EVENT = [
    ('TEST_STARTED', {'start_time':52, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_ENDED', {'end_time':1048, 'className':'someClassName',
                    'testName':'someTestName'}),
]

EVENTS_NOT_BALANCED_BEFORE_RAISE = [
    ('TEST_MODULE_STARTED', {
        'moduleContextFileName':'serial-util1146216{974}2772610436.ser',
        'moduleName':'someTestModule'}),
    ('TEST_RUN_STARTED', {'testCount': 2, 'runName': 'com.android.UnitTests'}),
    ('TEST_STARTED', {'start_time':10, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_ENDED', {'end_time':18, 'className':'someClassName',
                    'testName':'someTestName'}),
    ('TEST_STARTED', {'start_time':19, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_FAILED', {'className':'someClassName2', 'testName':'someTestName2',
                     'trace': 'someTrace'}),
]

EVENTS_IGNORE = [
    ('TEST_MODULE_STARTED', {
        'moduleContextFileName':'serial-util1146216{974}2772610436.ser',
        'moduleName':'someTestModule'}),
    ('TEST_RUN_STARTED', {'testCount': 2, 'runName': 'com.android.UnitTests'}),
    ('TEST_STARTED', {'start_time':8, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_ENDED', {'end_time':18, 'className':'someClassName',
                    'testName':'someTestName'}),
    ('TEST_STARTED', {'start_time':28, 'className':'someClassName2',
                      'testName':'someTestName2'}),
    ('TEST_IGNORED', {'className':'someClassName2', 'testName':'someTestName2',
                      'trace': 'someTrace'}),
    ('TEST_ENDED', {'end_time':90, 'className':'someClassName2',
                    'testName':'someTestName2'}),
    ('TEST_RUN_ENDED', {}),
    ('TEST_MODULE_ENDED', {'foo': 'bar'}),
]

EVENTS_WITH_PERF_INFO = [
    ('TEST_MODULE_STARTED', {
        'moduleContextFileName':'serial-util1146216{974}2772610436.ser',
        'moduleName':'someTestModule'}),
    ('TEST_RUN_STARTED', {'testCount': 2, 'runName': 'com.android.UnitTests'}),
    ('TEST_STARTED', {'start_time':52, 'className':'someClassName',
                      'testName':'someTestName'}),
    ('TEST_ENDED', {'end_time':1048, 'className':'someClassName',
                    'testName':'someTestName'}),
    ('TEST_STARTED', {'start_time':48, 'className':'someClassName2',
                      'testName':'someTestName2'}),
    ('TEST_FAILED', {'className':'someClassName2', 'testName':'someTestName2',
                     'trace': 'someTrace'}),
    ('TEST_ENDED', {'end_time':9876450, 'className':'someClassName2',
                    'testName':'someTestName2', 'cpu_time':'1234.1234(ns)',
                    'real_time':'5678.5678(ns)', 'iterations':'6666'}),
    ('TEST_STARTED', {'start_time':10, 'className':'someClassName3',
                      'testName':'someTestName3'}),
    ('TEST_ENDED', {'end_time':70, 'className':'someClassName3',
                    'testName':'someTestName3', 'additional_info_min':'102773',
                    'additional_info_mean':'105973', 'additional_info_median':'103778'}),
    ('TEST_RUN_ENDED', {}),
    ('TEST_MODULE_ENDED', {'foo': 'bar'}),
]

class EventHandlerUnittests(unittest.TestCase):
    """Unit tests for event_handler.py"""

    def setUp(self):
        reload(e_h)
        self.mock_reporter = mock.Mock()
        self.fake_eh = e_h.EventHandler(self.mock_reporter,
                                        atf_tr.AtestTradefedTestRunner.NAME)

    def tearDown(self):
        mock.patch.stopall()

    def test_process_event_normal_results(self):
        """Test process_event method for normal test results."""
        for name, data in EVENTS_NORMAL:
            self.fake_eh.process_event(name, data)
        call1 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName#someTestName',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=1,
            test_time='(996ms)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        call2 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName2#someTestName2',
            status=test_runner_base.FAILED_STATUS,
            details='someTrace',
            test_count=2,
            test_time='(2h44m36.402s)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call1, call2])

    def test_process_event_run_failure(self):
        """Test process_event method run failure."""
        for name, data in EVENTS_RUN_FAILURE:
            self.fake_eh.process_event(name, data)
        call = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName#someTestName',
            status=test_runner_base.ERROR_STATUS,
            details='someRunFailureReason',
            test_count=1,
            test_time='',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call])

    def test_process_event_invocation_failure(self):
        """Test process_event method with invocation failure."""
        for name, data in EVENTS_INVOCATION_FAILURE:
            self.fake_eh.process_event(name, data)
        call = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name=None,
            test_name=None,
            status=test_runner_base.ERROR_STATUS,
            details='someInvocationFailureReason',
            test_count=0,
            test_time='',
            runner_total=None,
            group_total=None,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call])

    def test_process_event_missing_test_run_started_event(self):
        """Test process_event method for normal test results."""
        for name, data in EVENTS_MISSING_TEST_RUN_STARTED_EVENT:
            self.fake_eh.process_event(name, data)
        call = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name=None,
            test_name='someClassName#someTestName',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=1,
            test_time='(996ms)',
            runner_total=None,
            group_total=None,
            additional_info={},
            test_run_name=None
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call])

    # pylint: disable=protected-access
    def test_process_event_not_balanced(self):
        """Test process_event method with start/end event name not balanced."""
        for name, data in EVENTS_NOT_BALANCED_BEFORE_RAISE:
            self.fake_eh.process_event(name, data)
        call = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName#someTestName',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=1,
            test_time='(8ms)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call])
        # Event pair: TEST_STARTED -> TEST_RUN_ENDED
        # It should raise TradeFedExitError in _check_events_are_balanced()
        name = 'TEST_RUN_ENDED'
        data = {}
        self.assertRaises(e_h.EventHandleError,
                          self.fake_eh._check_events_are_balanced,
                          name, self.mock_reporter)
        # Event pair: TEST_RUN_STARTED -> TEST_MODULE_ENDED
        # It should raise TradeFedExitError in _check_events_are_balanced()
        name = 'TEST_MODULE_ENDED'
        data = {'foo': 'bar'}
        self.assertRaises(e_h.EventHandleError,
                          self.fake_eh._check_events_are_balanced,
                          name, self.mock_reporter)

    def test_process_event_ignore(self):
        """Test _process_event method for normal test results."""
        for name, data in EVENTS_IGNORE:
            self.fake_eh.process_event(name, data)
        call1 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName#someTestName',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=1,
            test_time='(10ms)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        call2 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName2#someTestName2',
            status=test_runner_base.IGNORED_STATUS,
            details=None,
            test_count=2,
            test_time='(62ms)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call1, call2])

    def test_process_event_with_additional_info(self):
        """Test process_event method with perf information."""
        for name, data in EVENTS_WITH_PERF_INFO:
            self.fake_eh.process_event(name, data)
        call1 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName#someTestName',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=1,
            test_time='(996ms)',
            runner_total=None,
            group_total=2,
            additional_info={},
            test_run_name='com.android.UnitTests'
        ))

        test_additional_info = {'cpu_time':'1234.1234(ns)', 'real_time':'5678.5678(ns)',
                                'iterations':'6666'}
        call2 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName2#someTestName2',
            status=test_runner_base.FAILED_STATUS,
            details='someTrace',
            test_count=2,
            test_time='(2h44m36.402s)',
            runner_total=None,
            group_total=2,
            additional_info=test_additional_info,
            test_run_name='com.android.UnitTests'
        ))

        test_additional_info2 = {'additional_info_min':'102773',
                                 'additional_info_mean':'105973',
                                 'additional_info_median':'103778'}
        call3 = mock.call(test_runner_base.TestResult(
            runner_name=atf_tr.AtestTradefedTestRunner.NAME,
            group_name='someTestModule',
            test_name='someClassName3#someTestName3',
            status=test_runner_base.PASSED_STATUS,
            details=None,
            test_count=3,
            test_time='(60ms)',
            runner_total=None,
            group_total=2,
            additional_info=test_additional_info2,
            test_run_name='com.android.UnitTests'
        ))
        self.mock_reporter.process_test_result.assert_has_calls([call1, call2, call3])

if __name__ == '__main__':
    unittest.main()
