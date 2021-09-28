#!/usr/bin/env python
#
# Copyright 2018, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unittests for test_mapping"""

import unittest
import mock

import test_mapping
import unittest_constants as uc


class TestMappingUnittests(unittest.TestCase):
    """Unit tests for test_mapping.py"""

    def test_parsing(self):
        """Test creating TestDetail object"""
        detail = test_mapping.TestDetail(uc.TEST_MAPPING_TEST)
        self.assertEqual(uc.TEST_MAPPING_TEST['name'], detail.name)
        self.assertTrue(detail.host)
        self.assertEqual([], detail.options)

    def test_parsing_with_option(self):
        """Test creating TestDetail object with option configured"""
        detail = test_mapping.TestDetail(uc.TEST_MAPPING_TEST_WITH_OPTION)
        self.assertEqual(uc.TEST_MAPPING_TEST_WITH_OPTION['name'], detail.name)
        self.assertEqual(uc.TEST_MAPPING_TEST_WITH_OPTION_STR, str(detail))

    def test_parsing_with_bad_option(self):
        """Test creating TestDetail object with bad option configured"""
        with self.assertRaises(Exception) as context:
            test_mapping.TestDetail(uc.TEST_MAPPING_TEST_WITH_BAD_OPTION)
        self.assertEqual(
            'Each option can only have one key.', str(context.exception))

    def test_parsing_with_bad_host_value(self):
        """Test creating TestDetail object with bad host value configured"""
        with self.assertRaises(Exception) as context:
            test_mapping.TestDetail(uc.TEST_MAPPING_TEST_WITH_BAD_HOST_VALUE)
        self.assertEqual(
            'host can only have boolean value.', str(context.exception))

    @mock.patch("atest_utils.get_modified_files")
    def test_is_match_file_patterns(self, mock_modified_files):
        """Test mathod is_match_file_patterns."""
        test_mapping_file = ''
        test_detail = {
            "name": "Test",
            "file_patterns": ["(/|^)test_fp1[^/]*\\.java",
                              "(/|^)test_fp2[^/]*\\.java"]
        }
        mock_modified_files.return_value = {'/a/b/test_fp122.java',
                                            '/a/b/c/d/test_fp222.java'}
        self.assertTrue(test_mapping.is_match_file_patterns(test_mapping_file,
                                                            test_detail))
        mock_modified_files.return_value = {}
        self.assertFalse(test_mapping.is_match_file_patterns(test_mapping_file,
                                                             test_detail))
        mock_modified_files.return_value = {'/a/b/test_fp3.java'}
        self.assertFalse(test_mapping.is_match_file_patterns(test_mapping_file,
                                                             test_detail))
        test_mapping_file = '/a/b/TEST_MAPPING'
        mock_modified_files.return_value = {'/a/b/test_fp3.java',
                                            '/a/b/TEST_MAPPING'}
        self.assertTrue(test_mapping.is_match_file_patterns(test_mapping_file,
                                                            test_detail))


if __name__ == '__main__':
    unittest.main()
