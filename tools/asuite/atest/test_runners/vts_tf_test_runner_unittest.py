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

"""Unittests for vts_tf_test_runner."""

# pylint: disable=line-too-long

import unittest

from unittest import mock

from test_runners import vts_tf_test_runner

# pylint: disable=protected-access
class VtsTradefedTestRunnerUnittests(unittest.TestCase):
    """Unit tests for vts_tf_test_runner.py"""

    def setUp(self):
        self.vts_tr = vts_tf_test_runner.VtsTradefedTestRunner(results_dir='')

    def tearDown(self):
        mock.patch.stopall()

    @mock.patch('subprocess.Popen')
    @mock.patch.object(vts_tf_test_runner.VtsTradefedTestRunner, 'run')
    @mock.patch.object(vts_tf_test_runner.VtsTradefedTestRunner,
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
            self.vts_tr.run_tests(test_infos, extra_args, mock_reporter))

        # Test Build Pass
        _mock_popen.return_value.returncode = 1
        self.assertNotEqual(
            0,
            self.vts_tr.run_tests(test_infos, extra_args, mock_reporter))


if __name__ == '__main__':
    unittest.main()
