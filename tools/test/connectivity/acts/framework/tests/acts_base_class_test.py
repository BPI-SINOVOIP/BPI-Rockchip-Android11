#!/usr/bin/env python3
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

import shutil
import tempfile
import unittest

import mock
import mock_controller

from acts import asserts
from acts import base_test
from acts import error
from acts import signals

from mobly import base_test as mobly_base_test
import mobly.config_parser as mobly_config_parser

MSG_EXPECTED_EXCEPTION = 'This is an expected exception.'
MSG_EXPECTED_TEST_FAILURE = 'This is an expected test failure.'
MSG_UNEXPECTED_EXCEPTION = 'Unexpected exception!'

MOCK_EXTRA = {'key': 'value', 'answer_to_everything': 42}


def never_call():
    raise Exception(MSG_UNEXPECTED_EXCEPTION)


class SomeError(Exception):
    """A custom exception class used for tests in this module."""


class ActsBaseClassTest(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()
        self.tb_key = 'testbed_configs'
        self.test_run_config = mobly_config_parser.TestRunConfig()
        self.test_run_config.testbed_name = 'SampleTestBed'
        self.test_run_config.controller_configs = {
            self.tb_key: {
                'name': self.test_run_config.testbed_name,
            },
        }
        self.test_run_config.log_path = self.tmp_dir
        self.test_run_config.user_params = {'some_param': 'hahaha'}
        self.test_run_config.summary_writer = mock.MagicMock()
        self.mock_test_name = 'test_something'

    def tearDown(self):
        shutil.rmtree(self.tmp_dir)

    def test_current_test_case_name(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.assert_true(
                    self.current_test_name == 'test_func',
                    'Got unexpected test name %s.' % self.current_test_name)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)

    def test_self_tests_list(self):
        class MockBaseTest(base_test.BaseTestClass):
            def __init__(self, controllers):
                super(MockBaseTest, self).__init__(controllers)
                self.tests = ('test_something', )

            def test_something(self):
                pass

            def test_never(self):
                # This should not execute it's not on default test list.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_something')

    def test_self_tests_list_fail_by_convention(self):
        class MockBaseTest(base_test.BaseTestClass):
            def __init__(self, controllers):
                super(MockBaseTest, self).__init__(controllers)
                self.tests = ('not_a_test_something', )

            def not_a_test_something(self):
                pass

            def test_never(self):
                # This should not execute it's not on default test list.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        expected_msg = ('Test case name not_a_test_something does not follow '
                        'naming convention test_\*, abort.')
        with self.assertRaisesRegex(base_test.Error, expected_msg):
            bt_cls.run()

    def test_cli_test_selection_match_self_tests_list(self):
        class MockBaseTest(base_test.BaseTestClass):
            def __init__(self, controllers):
                super(MockBaseTest, self).__init__(controllers)
                self.tests = ('test_star1', 'test_star2', 'test_question_mark',
                              'test_char_seq', 'test_no_match')

            def test_star1(self):
                pass

            def test_star2(self):
                pass

            def test_question_mark(self):
                pass

            def test_char_seq(self):
                pass

            def test_no_match(self):
                # This should not execute because it does not match any regex
                # in the cmd line input.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        test_names = [
            'test_st*r1', 'test_*2', 'test_?uestion_mark', 'test_c[fghi]ar_seq'
        ]
        bt_cls.run(test_names=test_names)
        passed_names = [p.test_name for p in bt_cls.results.passed]
        self.assertEqual(len(passed_names), len(test_names))
        for test in [
                'test_star1', 'test_star2', 'test_question_mark',
                'test_char_seq'
        ]:
            self.assertIn(test, passed_names)

    def test_default_execution_of_all_tests(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_something(self):
                pass

            def not_a_test(self):
                # This should not execute its name doesn't follow test case
                # naming convention.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_something')

    def test_missing_requested_test_func(self):
        class MockBaseTest(base_test.BaseTestClass):
            def __init__(self, controllers):
                super(MockBaseTest, self).__init__(controllers)
                self.tests = ('test_something', )

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        self.assertFalse(bt_cls.results.executed)
        self.assertTrue(bt_cls.results.skipped)

    def test_setup_class_fail_by_exception(self):
        call_check = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def setup_class(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                # This should not execute because setup_class failed.
                never_call()

            def on_skip(self, test_name, begin_time):
                call_check('haha')

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, 'test_something')
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)
        call_check.assert_called_once_with('haha')

    def test_setup_test_fail_by_exception(self):
        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                # This should not execute because setup_test failed.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_something'])
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_setup_test_fail_by_test_signal(self):
        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                raise signals.TestFailure(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                # This should not execute because setup_test failed.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_something'])
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 1,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_setup_test_fail_by_return_False(self):
        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                return False

            def test_something(self):
                # This should not execute because setup_test failed.
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_something'])
        actual_record = bt_cls.results.failed[0]
        expected_msg = 'Setup for %s failed.' % self.mock_test_name
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, expected_msg)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 1,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_run_fail_by_ActsError_(self):
        class MockBaseTest(base_test.BaseTestClass):
            def __init__(self, controllers):
                super(MockBaseTest, self).__init__(controllers)

            def test_something(self):
                raise error.ActsError()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_something'])
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_teardown_test_assert_fail(self):
        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                asserts.assert_true(False, MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_teardown_test_raise_exception(self):
        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_teardown_test_executed_if_test_pass(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                my_mock('teardown_test')

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        my_mock.assert_called_once_with('teardown_test')
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 0,
            'Passed': 1,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_teardown_test_executed_if_setup_test_fails(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def teardown_test(self):
                my_mock('teardown_test')

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        my_mock.assert_called_once_with('teardown_test')
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_teardown_test_executed_if_test_fails(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                my_mock('teardown_test')

            def test_something(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        my_mock.assert_called_once_with('teardown_test')
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_exception_executed_if_teardown_test_fails(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def on_exception(self, test_name, begin_time):
                my_mock('on_exception')

            def teardown_test(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        my_mock.assert_called_once_with('on_exception')
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_fail_executed_if_test_fails(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def on_fail(self, test_name, begin_time):
                my_mock('on_fail')

            def test_something(self):
                asserts.assert_true(False, MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        my_mock.assert_called_once_with('on_fail')
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 1,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_fail_executed_if_test_setup_fails_by_exception(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def on_fail(self, test_name, begin_time):
                my_mock('on_fail')

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        my_mock.assert_called_once_with('on_fail')
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_fail_executed_if_test_setup_fails_by_return_False(self):
        my_mock = mock.MagicMock()

        class MockBaseTest(base_test.BaseTestClass):
            def setup_test(self):
                return False

            def on_fail(self, test_name, begin_time):
                my_mock('on_fail')

            def test_something(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        my_mock.assert_called_once_with('on_fail')
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details,
                         'Setup for test_something failed.')
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 1,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_failure_to_call_procedure_function_is_recorded(self):
        class MockBaseTest(base_test.BaseTestClass):
            # Wrong method signature; will raise exception
            def on_pass(self):
                pass

            def test_something(self):
                asserts.explicit_pass(MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertIn('_on_pass', actual_record.extra_errors)
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_failure_in_procedure_functions_is_recorded(self):
        expected_msg = 'Something failed in on_pass.'

        class MockBaseTest(base_test.BaseTestClass):
            def on_pass(self, test_name, begin_time):
                raise Exception(expected_msg)

            def test_something(self):
                asserts.explicit_pass(MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_both_teardown_and_test_body_raise_exceptions(self):
        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                asserts.assert_true(False, MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                raise Exception('Test Body Exception.')

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, 'Test Body Exception.')
        self.assertIsNone(actual_record.extras)
        self.assertEqual(actual_record.extra_errors['teardown_test'].details,
                         'This is an expected exception.')
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_explicit_pass_but_teardown_test_raises_an_exception(self):
        """Test record result should be marked as UNKNOWN as opposed to PASS.
        """
        class MockBaseTest(base_test.BaseTestClass):
            def teardown_test(self):
                asserts.assert_true(False, MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                asserts.explicit_pass('Test Passed!')

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, 'Test Passed!')
        self.assertIsNone(actual_record.extras)
        self.assertEqual(actual_record.extra_errors['teardown_test'].details,
                         'This is an expected exception.')
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_pass_raise_exception(self):
        class MockBaseTest(base_test.BaseTestClass):
            def on_pass(self, test_name, begin_time):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                asserts.explicit_pass(MSG_EXPECTED_EXCEPTION,
                                      extras=MOCK_EXTRA)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)
        expected_summary = {
            'Error': 1,
            'Executed': 1,
            'Failed': 0,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_on_fail_raise_exception(self):
        class MockBaseTest(base_test.BaseTestClass):
            def on_fail(self, test_name, begin_time):
                raise Exception(MSG_EXPECTED_EXCEPTION)

            def test_something(self):
                asserts.fail(MSG_EXPECTED_EXCEPTION, extras=MOCK_EXTRA)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(bt_cls.results.error, [])
        self.assertEqual(actual_record.test_name, self.mock_test_name)
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)
        expected_summary = {
            'Error': 0,
            'Executed': 1,
            'Failed': 1,
            'Passed': 0,
            'Requested': 1,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_abort_class(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_1(self):
                pass

            def test_2(self):
                asserts.abort_class(MSG_EXPECTED_EXCEPTION)
                never_call()

            def test_3(self):
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_1', 'test_2', 'test_3'])
        self.assertEqual(bt_cls.results.passed[0].test_name, 'test_1')
        self.assertEqual(bt_cls.results.failed[0].details,
                         MSG_EXPECTED_EXCEPTION)
        expected_summary = {
            'Error': 0,
            'Executed': 2,
            'Failed': 1,
            'Passed': 1,
            'Requested': 3,
            'Skipped': 0
        }
        self.assertEqual(bt_cls.results.summary_dict(), expected_summary)

    def test_uncaught_exception(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                raise Exception(MSG_EXPECTED_EXCEPTION)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)

    def test_fail(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.fail(MSG_EXPECTED_EXCEPTION, extras=MOCK_EXTRA)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_true(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.assert_true(False,
                                    MSG_EXPECTED_EXCEPTION,
                                    extras=MOCK_EXTRA)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_equal_pass(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.assert_equal(1, 1, extras=MOCK_EXTRA)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)

    def test_assert_equal_fail(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.assert_equal(1, 2, extras=MOCK_EXTRA)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIn('1 != 2', actual_record.details)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_equal_fail_with_msg(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.assert_equal(1,
                                     2,
                                     msg=MSG_EXPECTED_EXCEPTION,
                                     extras=MOCK_EXTRA)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        expected_msg = '1 != 2 ' + MSG_EXPECTED_EXCEPTION
        self.assertIn(expected_msg, actual_record.details)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_raises_pass(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                with asserts.assert_raises(SomeError, extras=MOCK_EXTRA):
                    raise SomeError(MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)

    def test_assert_raises_regex_pass(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                with asserts.assert_raises_regex(
                        SomeError,
                        expected_regex=MSG_EXPECTED_EXCEPTION,
                        extras=MOCK_EXTRA):
                    raise SomeError(MSG_EXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)

    def test_assert_raises_fail_with_noop(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                with asserts.assert_raises_regex(
                        SomeError,
                        expected_regex=MSG_EXPECTED_EXCEPTION,
                        extras=MOCK_EXTRA):
                    pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, 'SomeError not raised')
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_raises_fail_with_wrong_regex(self):
        wrong_msg = 'ha'

        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                with asserts.assert_raises_regex(
                        SomeError,
                        expected_regex=MSG_EXPECTED_EXCEPTION,
                        extras=MOCK_EXTRA):
                    raise SomeError(wrong_msg)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.failed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        expected_details = ('"This is an expected exception." does not match '
                            '"%s"') % wrong_msg
        self.assertEqual(actual_record.details, expected_details)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_assert_raises_fail_with_wrong_error(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                with asserts.assert_raises_regex(
                        SomeError,
                        expected_regex=MSG_EXPECTED_EXCEPTION,
                        extras=MOCK_EXTRA):
                    raise AttributeError(MSG_UNEXPECTED_EXCEPTION)

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run()
        actual_record = bt_cls.results.error[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_UNEXPECTED_EXCEPTION)
        self.assertIsNone(actual_record.extras)

    def test_explicit_pass(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.explicit_pass(MSG_EXPECTED_EXCEPTION,
                                      extras=MOCK_EXTRA)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_implicit_pass(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                pass

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.passed[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertIsNone(actual_record.details)
        self.assertIsNone(actual_record.extras)

    def test_skip(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.skip(MSG_EXPECTED_EXCEPTION, extras=MOCK_EXTRA)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.skipped[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_skip_if(self):
        class MockBaseTest(base_test.BaseTestClass):
            def test_func(self):
                asserts.skip_if(False, MSG_UNEXPECTED_EXCEPTION)
                asserts.skip_if(True,
                                MSG_EXPECTED_EXCEPTION,
                                extras=MOCK_EXTRA)
                never_call()

        bt_cls = MockBaseTest(self.test_run_config)
        bt_cls.run(test_names=['test_func'])
        actual_record = bt_cls.results.skipped[0]
        self.assertEqual(actual_record.test_name, 'test_func')
        self.assertEqual(actual_record.details, MSG_EXPECTED_EXCEPTION)
        self.assertEqual(actual_record.extras, MOCK_EXTRA)

    def test_unpack_userparams_required(self):
        """Missing a required param should raise an error."""
        required = ['some_param']
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(required)
        expected_value = self.test_run_config.user_params['some_param']
        self.assertEqual(bc.some_param, expected_value)

    def test_unpack_userparams_required_missing(self):
        """Missing a required param should raise an error."""
        required = ['something']
        bc = base_test.BaseTestClass(self.test_run_config)
        expected_msg = ('Missing required user param "%s" in test '
                        'configuration.') % required[0]
        with self.assertRaises(mobly_base_test.Error, msg=expected_msg):
            bc.unpack_userparams(required)

    def test_unpack_userparams_optional(self):
        """If an optional param is specified, the value should be what's in the
        config.
        """
        opt = ['some_param']
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(opt_param_names=opt)
        expected_value = self.test_run_config.user_params['some_param']
        self.assertEqual(bc.some_param, expected_value)

    def test_unpack_userparams_optional_with_default(self):
        """If an optional param is specified with a default value, and the
        param is not in the config, the value should be the default value.
        """
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(optional_thing='whatever')
        self.assertEqual(bc.optional_thing, 'whatever')

    def test_unpack_userparams_default_overwrite_by_optional_param_list(self):
        """If an optional param is specified in kwargs, and the param is in the
        config, the value should be the one in the config.
        """
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(some_param='whatever')
        expected_value = self.test_run_config.user_params['some_param']
        self.assertEqual(bc.some_param, expected_value)

    def test_unpack_userparams_default_overwrite_by_required_param_list(self):
        """If an optional param is specified in kwargs, the param is in the
        required param list, and the param is not specified in the config, the
        param's alue should be the default value and there should be no error
        thrown.
        """
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(req_param_names=['a_kwarg_param'],
                             a_kwarg_param='whatever')
        self.assertEqual(bc.a_kwarg_param, 'whatever')

    def test_unpack_userparams_optional_missing(self):
        """Missing an optional param should not raise an error."""
        opt = ['something']
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(opt_param_names=opt)

    def test_unpack_userparams_basic(self):
        """Required and optional params are unpacked properly."""
        required = ['something']
        optional = ['something_else']
        configs = self.test_run_config.copy()
        configs.user_params['something'] = 42
        configs.user_params['something_else'] = 53
        bc = base_test.BaseTestClass(configs)
        bc.unpack_userparams(req_param_names=required,
                             opt_param_names=optional)
        self.assertEqual(bc.something, 42)
        self.assertEqual(bc.something_else, 53)

    def test_unpack_userparams_default_overwrite(self):
        default_arg_val = 'haha'
        actual_arg_val = 'wawa'
        arg_name = 'arg1'
        configs = self.test_run_config.copy()
        configs.user_params[arg_name] = actual_arg_val
        bc = base_test.BaseTestClass(configs)
        bc.unpack_userparams(opt_param_names=[arg_name], arg1=default_arg_val)
        self.assertEqual(bc.arg1, actual_arg_val)

    def test_unpack_userparams_default_None(self):
        bc = base_test.BaseTestClass(self.test_run_config)
        bc.unpack_userparams(arg1='haha')
        self.assertEqual(bc.arg1, 'haha')

    def test_register_controller_no_config(self):
        base_cls = base_test.BaseTestClass(self.test_run_config)
        with self.assertRaisesRegexp(signals.ControllerError,
                                     'No corresponding config found for'):
            base_cls.register_controller(mock_controller)

    def test_register_optional_controller_no_config(self):
        base_cls = base_test.BaseTestClass(self.test_run_config)
        self.assertIsNone(
            base_cls.register_controller(mock_controller, required=False))

    def test_register_controller_third_party_dup_register(self):
        """Verifies correctness of registration, internal tally of controllers
        objects, and the right error happen when a controller module is
        registered twice.
        """
        mock_test_config = self.test_run_config.copy()
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        mock_test_config.controller_configs[mock_ctrlr_config_name] = [
            'magic1', 'magic2'
        ]
        base_cls = base_test.BaseTestClass(mock_test_config)
        base_cls.register_controller(mock_controller)
        registered_name = 'mock_controller'
        controller_objects = base_cls._controller_manager._controller_objects
        self.assertTrue(registered_name in controller_objects)
        mock_ctrlrs = controller_objects[registered_name]
        self.assertEqual(mock_ctrlrs[0].magic, 'magic1')
        self.assertEqual(mock_ctrlrs[1].magic, 'magic2')
        expected_msg = 'Controller module .* has already been registered.'
        with self.assertRaisesRegexp(signals.ControllerError, expected_msg):
            base_cls.register_controller(mock_controller)

    def test_register_optional_controller_third_party_dup_register(self):
        """Verifies correctness of registration, internal tally of controllers
        objects, and the right error happen when an optional controller module
        is registered twice.
        """
        mock_test_config = self.test_run_config.copy()
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        mock_test_config.controller_configs[mock_ctrlr_config_name] = [
            'magic1', 'magic2'
        ]
        base_cls = base_test.BaseTestClass(mock_test_config)
        base_cls.register_controller(mock_controller, required=False)
        expected_msg = 'Controller module .* has already been registered.'
        with self.assertRaisesRegexp(signals.ControllerError, expected_msg):
            base_cls.register_controller(mock_controller, required=False)

    def test_register_controller_builtin_dup_register(self):
        """Same as test_register_controller_third_party_dup_register, except
        this is for a builtin controller module.
        """
        mock_test_config = self.test_run_config.copy()
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        mock_ref_name = 'haha'
        setattr(mock_controller, 'ACTS_CONTROLLER_REFERENCE_NAME',
                mock_ref_name)
        try:
            mock_ctrlr_ref_name = mock_controller.ACTS_CONTROLLER_REFERENCE_NAME
            mock_test_config.controller_configs[mock_ctrlr_config_name] = [
                'magic1', 'magic2'
            ]
            base_cls = base_test.BaseTestClass(mock_test_config)
            base_cls.register_controller(mock_controller, builtin=True)
            self.assertTrue(hasattr(base_cls, mock_ref_name))
            self.assertTrue(mock_controller.__name__ in
                            base_cls._controller_manager._controller_objects)
            mock_ctrlrs = getattr(base_cls, mock_ctrlr_ref_name)
            self.assertEqual(mock_ctrlrs[0].magic, 'magic1')
            self.assertEqual(mock_ctrlrs[1].magic, 'magic2')
            expected_msg = 'Controller module .* has already been registered.'
            with self.assertRaisesRegexp(signals.ControllerError,
                                         expected_msg):
                base_cls.register_controller(mock_controller, builtin=True)
        finally:
            delattr(mock_controller, 'ACTS_CONTROLLER_REFERENCE_NAME')

    def test_register_controller_no_get_info(self):
        mock_test_config = self.test_run_config.copy()
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        mock_ref_name = 'haha'
        get_info = getattr(mock_controller, 'get_info')
        delattr(mock_controller, 'get_info')
        try:
            mock_test_config.controller_configs[mock_ctrlr_config_name] = [
                'magic1', 'magic2'
            ]
            base_cls = base_test.BaseTestClass(mock_test_config)
            base_cls.register_controller(mock_controller)
            self.assertEqual(base_cls.results.controller_info, [])
        finally:
            setattr(mock_controller, 'get_info', get_info)

    def test_register_controller_return_value(self):
        mock_test_config = self.test_run_config.copy()
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        mock_test_config.controller_configs[mock_ctrlr_config_name] = [
            'magic1', 'magic2'
        ]
        base_cls = base_test.BaseTestClass(mock_test_config)
        magic_devices = base_cls.register_controller(mock_controller)
        self.assertEqual(magic_devices[0].magic, 'magic1')
        self.assertEqual(magic_devices[1].magic, 'magic2')


if __name__ == '__main__':
    unittest.main()
