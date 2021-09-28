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

"""Unittests for test_suite_test_runner."""

# pylint: disable=line-too-long

import unittest

from unittest import mock

import unittest_utils

from test_finders import test_info
from test_runners import suite_plan_test_runner


# pylint: disable=protected-access
class SuitePlanTestRunnerUnittests(unittest.TestCase):
    """Unit tests for test_suite_test_runner.py"""

    def setUp(self):
        self.suite_tr = suite_plan_test_runner.SuitePlanTestRunner(results_dir='')

    def tearDown(self):
        mock.patch.stopall()

    @mock.patch('atest_utils.get_result_server_args')
    def test_generate_run_commands(self, mock_resultargs):
        """Test _generate_run_command method.
        Strategy:
            suite_name: cts --> run_cmd: cts-tradefed run commandAndExit cts
            suite_name: cts-common --> run_cmd:
                                cts-tradefed run commandAndExit cts-common
        """
        test_infos = set()
        suite_name = 'cts'
        t_info = test_info.TestInfo(suite_name,
                                    suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                    {suite_name},
                                    suite=suite_name)
        test_infos.add(t_info)

        # Basic Run Cmd
        run_cmd = []
        exe_cmd = suite_plan_test_runner.SuitePlanTestRunner.EXECUTABLE % suite_name
        run_cmd.append(suite_plan_test_runner.SuitePlanTestRunner._RUN_CMD.format(
            exe=exe_cmd,
            test=suite_name,
            args=''))
        mock_resultargs.return_value = []
        unittest_utils.assert_strict_equal(
            self,
            self.suite_tr.generate_run_commands(test_infos, ''),
            run_cmd)

        # Run cmd with --serial LG123456789.
        run_cmd = []
        run_cmd.append(suite_plan_test_runner.SuitePlanTestRunner._RUN_CMD.format(
            exe=exe_cmd,
            test=suite_name,
            args='--serial LG123456789'))
        unittest_utils.assert_strict_equal(
            self,
            self.suite_tr.generate_run_commands(test_infos, {'SERIAL':'LG123456789'}),
            run_cmd)

        test_infos = set()
        suite_name = 'cts-common'
        suite = 'cts'
        t_info = test_info.TestInfo(suite_name,
                                    suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                    {suite_name},
                                    suite=suite)
        test_infos.add(t_info)

        # Basic Run Cmd
        run_cmd = []
        exe_cmd = suite_plan_test_runner.SuitePlanTestRunner.EXECUTABLE % suite
        run_cmd.append(suite_plan_test_runner.SuitePlanTestRunner._RUN_CMD.format(
            exe=exe_cmd,
            test=suite_name,
            args=''))
        mock_resultargs.return_value = []
        unittest_utils.assert_strict_equal(
            self,
            self.suite_tr.generate_run_commands(test_infos, ''),
            run_cmd)

        # Run cmd with --serial LG123456789.
        run_cmd = []
        run_cmd.append(suite_plan_test_runner.SuitePlanTestRunner._RUN_CMD.format(
            exe=exe_cmd,
            test=suite_name,
            args='--serial LG123456789'))
        unittest_utils.assert_strict_equal(
            self,
            self.suite_tr.generate_run_commands(test_infos, {'SERIAL':'LG123456789'}),
            run_cmd)

    @mock.patch('subprocess.Popen')
    @mock.patch.object(suite_plan_test_runner.SuitePlanTestRunner, 'run')
    @mock.patch.object(suite_plan_test_runner.SuitePlanTestRunner,
                       'generate_run_commands')
    def test_run_tests(self, _mock_gen_cmd, _mock_run, _mock_popen):
        """Test run_tests method."""
        test_infos = []
        extra_args = []
        mock_reporter = mock.Mock()
        _mock_gen_cmd.return_value = ["cmd1", "cmd2"]
        # Test Build Pass
        _mock_popen.return_value.returncode = 0
        self.assertEqual(
            0,
            self.suite_tr.run_tests(test_infos, extra_args, mock_reporter))

        # Test Build Pass
        _mock_popen.return_value.returncode = 1
        self.assertNotEqual(
            0,
            self.suite_tr.run_tests(test_infos, extra_args, mock_reporter))

if __name__ == '__main__':
    unittest.main()
