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

"""Unittests for atest_arg_parser."""

import unittest

import atest_arg_parser


class AtestArgParserUnittests(unittest.TestCase):
    """Unit tests for atest_arg_parser.py"""

    def test_get_args(self):
        """Test get_args(): flatten a nested list. """
        parser = atest_arg_parser.AtestArgParser()
        parser.add_argument('-t', '--test', help='Run the tests.')
        parser.add_argument('-b', '--build', help='Run a build.')
        parser.add_argument('--generate-baseline', help='Generate a baseline.')
        test_args = ['-t', '--test',
                     '-b', '--build',
                     '--generate-baseline',
                     '-h', '--help'].sort()
        self.assertEqual(test_args, parser.get_args().sort())


if __name__ == '__main__':
    unittest.main()
