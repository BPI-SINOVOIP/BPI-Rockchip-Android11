#!/usr/bin/env python3
#
# Copyright 2020, The Android Open Source Project
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

"""Unittests for AndroidSDK class."""

import unittest
from unittest import mock

from aidegen.lib import common_util
from aidegen.sdk import android_sdk


# pylint: disable=protected-access
class AndroidSDKUnittests(unittest.TestCase):
    """Unit tests for AndroidSDK class."""

    def setUp(self):
        """Prepare the testdata related path."""
        self.sdk = android_sdk.AndroidSDK()

    def tearDown(self):
        """Clear the testdata related path."""
        self.sdk = None

    def test_init(self):
        """Test initialize the attributes."""
        self.assertEqual(self.sdk.max_api_level, 0)
        self.assertEqual(self.sdk.max_code_name, None)
        self.assertEqual(self.sdk.platform_mapping, {})
        self.assertEqual(self.sdk.android_sdk_path, None)

    def test_parse_max_api_level(self):
        """Test _parse_max_api_level."""
        self.sdk._platform_mapping = {
            'android-29': {
                'api_level': 29,
                'code_name': '29',
            },
            'android-28': {
                'api_level': 28,
                'code_name': '28',
            },
        }
        api_level = self.sdk._parse_max_api_level()
        self.assertEqual(api_level, 29)

        self.sdk._platform_mapping = {
            'android-28': {
                'api_level': 28,
                'code_name': '28',
            },
            'android-Q': {
                'api_level': 29,
                'code_name': 'Q',
            },
        }
        api_level = self.sdk._parse_max_api_level()
        self.assertEqual(api_level, 29)

    def test_parse_max_code_name(self):
        """Test _parse_max_code_name."""
        self.sdk._max_api_level = 29
        self.sdk._platform_mapping = {
            'android-29': {
                'api_level': 29,
                'code_name': '29',
            },
            'android-28': {
                'api_level': 28,
                'code_name': '28',
            },
        }
        code_name = self.sdk._parse_max_code_name()
        self.assertEqual(code_name, '29')

        self.sdk._platform_mapping = {
            'android-29': {
                'api_level': 29,
                'code_name': '29',
            },
            'android-Q': {
                'api_level': 29,
                'code_name': 'Q',
            },
        }
        code_name = self.sdk._parse_max_code_name()
        self.assertEqual(code_name, 'Q')

    @mock.patch.object(common_util, 'read_file_content')
    def test_parse_api_info(self, mock_read_file):
        """Test _parse_api_info."""
        mock_read_file.return_value = '\nAndroidVersion.ApiLevel=29\n'
        expected_result = '29', '29'
        self.assertEqual(self.sdk._parse_api_info(''), expected_result)

        mock_read_file.return_value = ('\nAndroidVersion.ApiLevel=29\n'
                                       'AndroidVersion.CodeName=Q\n')
        expected_result = '29', 'Q'
        self.assertEqual(self.sdk._parse_api_info(''), expected_result)

        mock_read_file.return_value = ''
        expected_result = 0, 0
        self.assertEqual(self.sdk._parse_api_info(''), expected_result)

    @mock.patch.object(android_sdk.AndroidSDK, '_parse_api_info')
    @mock.patch('glob.glob')
    def test_gen_platform_mapping(self, mock_glob, mock_parse_api_info):
        """Test _gen_platform_mapping."""
        mock_glob.return_value = ['/sdk/platforms/android-29/source.properties']
        mock_parse_api_info.return_value = 0, 0
        test_result = self.sdk._gen_platform_mapping('')
        expected_result = {}
        self.assertEqual(test_result, False)
        self.assertEqual(self.sdk._platform_mapping, expected_result)

        mock_parse_api_info.return_value = '29', '29'
        test_result = self.sdk._gen_platform_mapping('')
        expected_result = {
            'android-29': {
                'api_level': 29,
                'code_name': '29',
            }
        }
        self.assertEqual(test_result, True)
        self.assertEqual(self.sdk._platform_mapping, expected_result)

    @mock.patch.object(android_sdk.AndroidSDK, '_gen_platform_mapping')
    def test_is_android_sdk_path(self, mock_gen_platform_mapping):
        """Test is_android_sdk_path."""
        self.sdk._platform_mapping = {
            'android-29': {
                'api_level': 29,
                'code_name': '29',
            },
        }
        mock_gen_platform_mapping.return_value = True
        self.assertEqual(self.sdk.is_android_sdk_path('a/b'), True)
        self.assertEqual(self.sdk.android_sdk_path, 'a/b')
        self.assertEqual(self.sdk.max_api_level, 29)

        mock_gen_platform_mapping.return_value = False
        self.assertEqual(self.sdk.is_android_sdk_path('a/b'), False)

    @mock.patch('builtins.input')
    @mock.patch.object(android_sdk.AndroidSDK, 'is_android_sdk_path')
    def test_path_analysis(self, mock_is_sdk_path, mock_input):
        """Test path_analysis."""
        mock_is_sdk_path.return_value = True
        self.assertEqual(self.sdk.path_analysis('a/b'), True)

        mock_is_sdk_path.return_value = False
        mock_input.return_value = ''
        self.assertEqual(self.sdk.path_analysis('a/b'), False)

        mock_is_sdk_path.return_value = False
        mock_input.return_value = 'a/b'
        self.assertEqual(self.sdk.path_analysis('a/b'), False)

        self.sdk._INPUT_QUERY_TIMES = 0
        self.assertEqual(self.sdk.path_analysis('a/b'), False)


if __name__ == '__main__':
    unittest.main()
