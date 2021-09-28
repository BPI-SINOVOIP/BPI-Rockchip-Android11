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

"""Unittests for cache_finder."""

# pylint: disable=line-too-long

import unittest
import os

from unittest import mock

import atest_utils
import unittest_constants as uc

from test_finders import cache_finder


#pylint: disable=protected-access
class CacheFinderUnittests(unittest.TestCase):
    """Unit tests for cache_finder.py"""
    def setUp(self):
        """Set up stuff for testing."""
        self.cache_finder = cache_finder.CacheFinder()

    @mock.patch.object(atest_utils, 'get_test_info_cache_path')
    def test_find_test_by_cache(self, mock_get_cache_path):
        """Test find_test_by_cache method."""
        uncached_test = 'mytest1'
        cached_test = 'hello_world_test'
        uncached_test2 = 'mytest2'
        test_cache_root = os.path.join(uc.TEST_DATA_DIR, 'cache_root')
        # Hit matched cache file but no original_finder in it,
        # should return None.
        mock_get_cache_path.return_value = os.path.join(
            test_cache_root,
            'cd66f9f5ad63b42d0d77a9334de6bb73.cache')
        self.assertIsNone(self.cache_finder.find_test_by_cache(uncached_test))
        # Hit matched cache file and original_finder is in it,
        # should return cached test infos.
        mock_get_cache_path.return_value = os.path.join(
            test_cache_root,
            '78ea54ef315f5613f7c11dd1a87f10c7.cache')
        self.assertIsNotNone(self.cache_finder.find_test_by_cache(cached_test))
        # Does not hit matched cache file, should return cached test infos.
        mock_get_cache_path.return_value = os.path.join(
            test_cache_root,
            '39488b7ac83c56d5a7d285519fe3e3fd.cache')
        self.assertIsNone(self.cache_finder.find_test_by_cache(uncached_test2))

if __name__ == '__main__':
    unittest.main()
