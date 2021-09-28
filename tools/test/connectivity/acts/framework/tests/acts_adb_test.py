#!/usr/bin/env python3
#
#   Copyright 2017 - The Android Open Source Project
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

import unittest
import mock
from acts.controllers import adb
from acts.controllers.adb import AdbError


class MockJob(object):
    def __init__(self, exit_status=0, stderr='', stdout=''):
        self.exit_status = exit_status
        self.stderr = stderr
        self.stdout = stdout


class MockAdbProxy(adb.AdbProxy):
    def __init__(self):
        pass


class ADBTest(unittest.TestCase):
    """A class for testing acts/controllers/adb.py"""

    def test__exec_cmd_failure_old_adb(self):
        mock_job = MockJob(exit_status=1, stderr='error: device not found')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            with self.assertRaises(adb.AdbError):
                MockAdbProxy()._exec_cmd(cmd)

    def test__exec_cmd_failure_new_adb(self):
        mock_job = MockJob(
            exit_status=1, stderr='error: device \'DEADBEEF\' not found')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            with self.assertRaises(adb.AdbError):
                MockAdbProxy()._exec_cmd(cmd)

    def test__exec_cmd_pass_ret_1(self):
        mock_job = MockJob(exit_status=1, stderr='error not related to adb')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            MockAdbProxy()._exec_cmd(cmd)

    def test__exec_cmd_pass_basic(self):
        mock_job = MockJob(exit_status=0, stderr='', stdout='FEEDACAB')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            MockAdbProxy()._exec_cmd(cmd)

    def test__exec_cmd_pass_no_stdout(self):
        mock_job = MockJob(exit_status=0, stderr='', stdout='')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            MockAdbProxy()._exec_cmd(cmd)

    def test__exec_cmd_raises_on_bind_error(self):
        """Tests _exec_cmd raises an AdbError on port forwarding failure."""
        mock_job = MockJob(exit_status=1,
                           stderr='error: cannot bind listener: '
                                  'Address already in use',
                           stdout='')
        cmd = ['adb', '-s', '"SOME_SERIAL"', 'shell', '"SOME_SHELL_CMD"']
        with mock.patch('acts.libs.proc.job.run', return_value=mock_job):
            with self.assertRaises(AdbError):
                MockAdbProxy()._exec_cmd(cmd)

    def test__get_version_number_gets_version_number(self):
        """Tests the positive case for AdbProxy.get_version_number()."""
        proxy = MockAdbProxy()
        expected_version_number = 39
        proxy.version = lambda: ('Android Debug Bridge version 1.0.%s\nblah' %
                                 expected_version_number)
        self.assertEqual(expected_version_number, proxy.get_version_number())

    def test__get_version_number_raises_upon_parse_failure(self):
        """Tests the failure case for AdbProxy.get_version_number()."""
        proxy = MockAdbProxy()
        proxy.version = lambda: 'Bad format'
        with self.assertRaises(AdbError):
            proxy.get_version_number()


if __name__ == "__main__":
    unittest.main()
