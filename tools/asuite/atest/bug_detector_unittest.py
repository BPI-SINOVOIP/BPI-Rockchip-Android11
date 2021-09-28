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

"""Unittests for bug_detector."""

# pylint: disable=line-too-long

import datetime
import json
import os
import unittest

from unittest import mock

import bug_detector
import constants
import unittest_constants as uc

TEST_DICT = {
    'test1': {
        'latest_exit_code': 5,
        'updated_at': ''
    },
    'test2': {
        'latest_exit_code': 0,
        'updated_at': ''
    }
}

class BugDetectorUnittest(unittest.TestCase):
    """Unit test for bug_detector.py"""

    def setUp(self):
        """Set up stuff for testing."""
        self.history_file = os.path.join(uc.TEST_DATA_DIR, 'bug_detector.json')
        self.detector = bug_detector.BugDetector(['test1'], 5, self.history_file)
        self._reset_history_file()
        self.history_file2 = os.path.join(uc.TEST_DATA_DIR, 'bug_detector2.json')

    def tearDown(self):
        """Run after execution of every test"""
        if os.path.isfile(self.history_file):
            os.remove(self.history_file)
        if os.path.isfile(self.history_file2):
            os.remove(self.history_file2)

    def _reset_history_file(self):
        """Reset test history file."""
        with open(self.history_file, 'w') as outfile:
            json.dump(TEST_DICT, outfile)

    def _make_test_file(self, file_size):
        temp_history = {}
        for i in range(file_size):
            latest_bug = {
                i: {
                    'latest_exit_code': i,
                    'updated_at': datetime.datetime.now().isoformat()
                }
            }
            temp_history.update(latest_bug)
        with open(self.history_file2, 'w') as outfile:
            json.dump(temp_history, outfile, indent=0)

    @mock.patch.object(bug_detector.BugDetector, 'update_history')
    def test_get_detect_key(self, _):
        """Test get_detect_key."""
        # argv without -v
        argv = ['test2', 'test1']
        want_key = 'test1 test2'
        dtr = bug_detector.BugDetector(argv, 0)
        self.assertEqual(dtr.get_detect_key(argv), want_key)

        # argv with -v
        argv = ['-v', 'test2', 'test1']
        want_key = 'test1 test2'
        dtr = bug_detector.BugDetector(argv, 0)
        self.assertEqual(dtr.get_detect_key(argv), want_key)

        # argv with --verbose
        argv = ['--verbose', 'test2', 'test3', 'test1']
        want_key = 'test1 test2 test3'
        dtr = bug_detector.BugDetector(argv, 0)
        self.assertEqual(dtr.get_detect_key(argv), want_key)

    def test_get_history(self):
        """Test get_history."""
        self.assertEqual(self.detector.get_history(), TEST_DICT)

    @mock.patch.object(bug_detector.BugDetector, 'update_history')
    def test_detect_bug_caught(self, _):
        """Test detect_bug_caught."""
        self._reset_history_file()
        dtr = bug_detector.BugDetector(['test1'], 0, self.history_file)
        success = 1
        self.assertEqual(dtr.detect_bug_caught(), success)

    def test_update_history(self):
        """Test update_history."""
        constants.UPPER_LIMIT = 10
        constants.TRIM_TO_SIZE = 3

        mock_file_size = 0
        self._make_test_file(mock_file_size)
        dtr = bug_detector.BugDetector(['test1'], 0, self.history_file2)
        self.assertTrue('test1' in dtr.history)

        # History is larger than constants.UPPER_LIMIT. Trim to size.
        mock_file_size = 10
        self._make_test_file(mock_file_size)
        dtr = bug_detector.BugDetector(['test1'], 0, self.history_file2)
        self.assertEqual(len(dtr.history), constants.TRIM_TO_SIZE)
        keys = ['test1', '9', '8']
        for key in keys:
            self.assertTrue(key in dtr.history)

        # History is not larger than constants.UPPER_LIMIT.
        mock_file_size = 5
        self._make_test_file(mock_file_size)
        dtr = bug_detector.BugDetector(['test1'], 0, self.history_file2)
        self.assertEqual(len(dtr.history), mock_file_size+1)
        keys = ['test1', '4', '3', '2', '1', '0']
        for key in keys:
            self.assertTrue(key in dtr.history)

if __name__ == '__main__':
    unittest.main()
