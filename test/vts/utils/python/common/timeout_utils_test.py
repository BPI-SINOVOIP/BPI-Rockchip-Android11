#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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

import unittest
import time

import vts.utils.python.common.timeout_utils as timeout_utils

#TODO use milliseconds after float number timeout is implemented in timeout_utils


class TimeoutUtilsTest(unittest.TestCase):
    '''Test methods for timeout_util module.'''

    def setUp(self):
        """SetUp tasks"""
        self.count = 0

    def test_timeout_no_except_no_timeout(self):
        """Tests the functionality of timeout decorator function."""
        @timeout_utils.timeout(1, no_exception=True)
        def increase(self):
            self.count += 1;

        increase(self)
        self.assertEqual(self.count, 1)

    def test_timeout_no_except_with_timeout(self):
        """Tests the functionality of timeout decorator function."""
        @timeout_utils.timeout(1, no_exception=True)
        def increase(self):
            time.sleep(2)
            self.count += 1;

        increase(self)
        self.assertEqual(self.count, 0)

    def test_timeout_with_except_with_timeout(self):
        """Tests the functionality of timeout decorator function."""
        @timeout_utils.timeout(1)
        def increase(self):
            time.sleep(2)
            self.count += 1;

        try:
            increase(self)
        except timeout_utils.TimeoutError:
            pass

        self.assertEqual(self.count, 0)

    def test_timeout_with_except_no_timeout(self):
        """Tests the functionality of timeout decorator function."""
        @timeout_utils.timeout(1)
        def increase(self):
            self.count += 1;

        increase(self)

        self.assertEqual(self.count, 1)