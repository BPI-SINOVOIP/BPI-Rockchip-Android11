#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
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

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner


def AssertShellCommandSuccess(command_results, num_of_commands):
    '''Check shell command result with assertions.

    Given a shell command output, this command checks several things:
    1. result is not None
    2. result is not empty
    3. number of results is consistant with number of commands
    4. there is no error message on STDERR
    5. return code of commands are all 0

    Args:
        command_results: dict, shell command results
        num_of_commands: int, number of commands
    '''
    asserts.assertTrue(command_results is not None,
                       'command result cannot be None')
    asserts.assertEqual(len(command_results), 3, 'command result is empty')
    for item in command_results:
        asserts.assertEqual(
            len(command_results[item]), num_of_commands,
            'number of command result is not %s: %s' % (num_of_commands,
                                                        command_results))
    asserts.assertFalse(
        any(command_results[const.STDERR]),
        'received error message from stderr: %s' % command_results)
    asserts.assertFalse(
        any(command_results[const.EXIT_CODE]),
        'received non zero return code: %s' % command_results)


class VtsSelfTestBaseTest(base_test.BaseTestClass):
    '''Two hello world test cases which use the shell driver.

    Attributes:
        is_first_run: bool, whether this test run is the first run with retry attempts.
    '''
    run_count = 0

    def setUpClass(self):
        # Since we are running the actual test cases, run_as_vts_self_test
        # must be set to False.
        self.run_as_vts_self_test = False

        self.dut = self.android_devices[0]
        self.shell = self.dut.shell

    def tearDownClass(self):
        self.run_count += 1

    def testShellEcho1(self):
        '''A simple testcase which sends a command.'''
        results = self.shell.Execute(
            "echo hello_world")  # runs a shell command.
        AssertShellCommandSuccess(results, 1)
        logging.info(str(results[const.STDOUT]))  # prints the stdout
        asserts.assertEqual(results[const.STDOUT][0].strip(),
                            "hello_world")  # checks the stdout

    def testShellEcho2(self):
        '''A simple testcase which sends two commands.'''
        results = self.shell.Execute(['echo hello', 'echo world'])
        AssertShellCommandSuccess(results, 2)
        logging.info(str(results[const.STDOUT]))
        asserts.assertEqual(results[const.STDOUT][0].strip(), 'hello')
        asserts.assertEqual(results[const.STDOUT][1].strip(), 'world')

    def testDeviceTotalMem(self):
        '''Test AndroidDevice class total_memory getter function'''
        asserts.assertTrue(self.dut.total_memory > 0,
                           'Failed to get device memory info.')

    def test_getUserConfigStr1(self):
        '''Test getUserConfigStr.'''
        asserts.assertEqual(self.getUserConfigStr('a'), 'a')

    def test_getUserConfigStr2(self):
        '''Test getUserConfigStr.'''
        asserts.assertEqual(self.getUserConfigStr('b'), 'b')

    def test_getUserConfigStr3(self):
        '''Test getUserConfigStr.'''
        asserts.assertEqual(self.getUserConfigStr('c'), None)

    def test_getUserConfigStr4(self):
        '''Test getUserConfigStr.'''
        asserts.assertEqual(self.getUserConfigStr('c', to_str=True), None)

    def test_getUserConfigInt1(self):
        '''Test getUserConfigInt.'''
        asserts.assertEqual(self.getUserConfigInt('a'), 1)

    def test_getUserConfigInt2(self):
        '''Test getUserConfigInt.'''
        asserts.assertEqual(self.getUserConfigInt('b'), 2)

    def test_getUserConfigInt3(self):
        '''Test getUserConfigInt.'''
        asserts.assertEqual(self.getUserConfigInt('b', to_str=True), '2')

    def test_getUserConfigInt4(self):
        '''Test getUserConfigInt.'''
        asserts.assertEqual(self.getUserConfigInt('c'), None)

    def test_getUserConfigInt5(self):
        '''Test getUserConfigInt.'''
        asserts.assertEqual(self.getUserConfigInt('c', to_str=True), None)

    def test_getUserConfigBool1(self):
        '''Test getUserConfigBool.'''
        asserts.assertEqual(self.getUserConfigBool('a'), True)

    def test_getUserConfigBool2(self):
        '''Test getUserConfigBool.'''
        asserts.assertEqual(self.getUserConfigBool('b'), False)

    def test_getUserConfigBool3(self):
        '''Test getUserConfigBool.'''
        asserts.assertEqual(self.getUserConfigBool('b', to_str=True), 'False')

    def test_getUserConfigBool4(self):
        '''Test getUserConfigBool.'''
        asserts.assertEqual(self.getUserConfigBool('c'), None)

    def test_getUserConfigBool5(self):
        '''Test getUserConfigBool.'''
        asserts.assertEqual(self.getUserConfigBool('c', to_str=True), None)

    def test_retry_run_pass_on_2nd(self):
        """Tests retry feature."""
        asserts.assertEqual(self.run_count, 1)

    def test_retry_run_pass_on_3rd(self):
        """Tests retry feature."""
        asserts.assertEqual(self.run_count, 2)

if __name__ == "__main__":
    test_runner.main()
