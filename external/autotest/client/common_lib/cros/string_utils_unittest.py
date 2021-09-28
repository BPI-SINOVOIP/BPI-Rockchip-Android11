#!/usr/bin/python2
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for string_utils."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import mock
import unittest

import common
from autotest_lib.client.common_lib.cros import string_utils


class JoinLongestWithLengthLimitTest(unittest.TestCase):
    """Unit test of join_longest_with_length_limit."""
    def setUp(self):
        """Setup."""
        self.strings = ['abc', '12', 'sssss']

    def test_basic(self):
        """The basic test case."""
        result = list(string_utils.join_longest_with_length_limit(
                self.strings, 6, separator=','))
        self.assertEqual(len(result), 2)
        self.assertTrue(type(result[0]) is str)

    def test_short_strings(self):
        """Test with short strings to be joined with big limit."""
        sep = mock.MagicMock()
        result = list(string_utils.join_longest_with_length_limit(
                self.strings, 100, separator=sep))
        sep.join.assert_called_once()

    def test_string_too_long(self):
        """Test with too long string to be joined."""
        strings = ['abc', '12', 'sssss', 'a very long long long string']
        with self.assertRaises(string_utils.StringTooLongError):
            list(string_utils.join_longest_with_length_limit(strings, 6))

    def test_long_sep(self):
        """Test with long seperator."""
        result = list(string_utils.join_longest_with_length_limit(
                self.strings, 6, separator='|very long separator|'))
        # Though the string to be joined is short, we still will have 3 result
        # because each of them plus separator is longer than the limit.
        self.assertEqual(len(result), 3)

    def test_do_not_join(self):
        """Test yielding list instead of string."""
        result = list(string_utils.join_longest_with_length_limit(
                self.strings, 6, separator=',', do_join=False))
        self.assertEqual(len(result), 2)
        self.assertTrue(type(result[0]) is list)


if __name__ == '__main__':
    unittest.main()
