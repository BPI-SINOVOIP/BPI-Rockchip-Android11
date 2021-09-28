#!/usr/bin/env python
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

"""
ATest Integration Test Class.

The purpose is to prevent potential side-effects from breaking ATest at the
early stage while landing CLs with potential side-effects.

It forks a subprocess with ATest commands to validate if it can pass all the
finding, running logic of the python code, and waiting for TF to exit properly.
    - When running with ROBOLECTRIC tests, it runs without TF, and will exit
    the subprocess with the message "All tests passed"
    - If FAIL, it means something breaks ATest unexpectedly!
"""

from __future__ import print_function

import os
import subprocess
import sys
import tempfile
import time
import unittest

_TEST_RUN_DIR_PREFIX = 'atest_integration_tests_%s_'
_LOG_FILE = 'integration_tests.log'
_FAILED_LINE_LIMIT = 50
_INTEGRATION_TESTS = 'INTEGRATION_TESTS'
_EXIT_TEST_FAILED = 1


class ATestIntegrationTest(unittest.TestCase):
    """ATest Integration Test Class."""
    NAME = 'ATestIntegrationTest'
    EXECUTABLE = 'atest'
    OPTIONS = ''
    _RUN_CMD = '{exe} {options} {test}'
    _PASSED_CRITERIA = ['will be rescheduled', 'All tests passed']

    def setUp(self):
        """Set up stuff for testing."""
        self.full_env_vars = os.environ.copy()
        self.test_passed = False
        self.log = []

    def run_test(self, testcase):
        """Create a subprocess to execute the test command.

        Strategy:
            Fork a subprocess to wait for TF exit properly, and log the error
            if the exit code isn't 0.

        Args:
            testcase: A string of testcase name.
        """
        run_cmd_dict = {'exe': self.EXECUTABLE, 'options': self.OPTIONS,
                        'test': testcase}
        run_command = self._RUN_CMD.format(**run_cmd_dict)
        try:
            subprocess.check_output(run_command,
                                    stderr=subprocess.PIPE,
                                    env=self.full_env_vars,
                                    shell=True)
        except subprocess.CalledProcessError as e:
            self.log.append(e.output)
            return False
        return True

    def get_failed_log(self):
        """Get a trimmed failed log.

        Strategy:
            In order not to show the unnecessary log such as build log,
            it's better to get a trimmed failed log that contains the
            most important information.

        Returns:
            A trimmed failed log.
        """
        failed_log = '\n'.join(filter(None, self.log[-_FAILED_LINE_LIMIT:]))
        return failed_log


def create_test_method(testcase, log_path):
    """Create a test method according to the testcase.

    Args:
        testcase: A testcase name.
        log_path: A file path for storing the test result.

    Returns:
        A created test method, and a test function name.
    """
    test_function_name = 'test_%s' % testcase.replace(' ', '_')
    # pylint: disable=missing-docstring
    def template_test_method(self):
        self.test_passed = self.run_test(testcase)
        open(log_path, 'a').write('\n'.join(self.log))
        failed_message = 'Running command: %s failed.\n' % testcase
        failed_message += '' if self.test_passed else self.get_failed_log()
        self.assertTrue(self.test_passed, failed_message)
    return test_function_name, template_test_method


def create_test_run_dir():
    """Create the test run directory in tmp.

    Returns:
        A string of the directory path.
    """
    utc_epoch_time = int(time.time())
    prefix = _TEST_RUN_DIR_PREFIX % utc_epoch_time
    return tempfile.mkdtemp(prefix=prefix)


if __name__ == '__main__':
    # TODO(b/129029189) Implement detail comparison check for dry-run mode.
    ARGS = ' '.join(sys.argv[1:])
    if ARGS:
        ATestIntegrationTest.OPTIONS = ARGS
    TEST_PLANS = os.path.join(os.path.dirname(__file__), _INTEGRATION_TESTS)
    try:
        LOG_PATH = os.path.join(create_test_run_dir(), _LOG_FILE)
        with open(TEST_PLANS) as test_plans:
            for test in test_plans:
                # Skip test when the line startswith #.
                if not test.strip() or test.strip().startswith('#'):
                    continue
                test_func_name, test_func = create_test_method(
                    test.strip(), LOG_PATH)
                setattr(ATestIntegrationTest, test_func_name, test_func)
        SUITE = unittest.TestLoader().loadTestsFromTestCase(ATestIntegrationTest)
        RESULTS = unittest.TextTestRunner(verbosity=2).run(SUITE)
    finally:
        if RESULTS.failures:
            print('Full test log is saved to %s' % LOG_PATH)
            sys.exit(_EXIT_TEST_FAILED)
        else:
            os.remove(LOG_PATH)
