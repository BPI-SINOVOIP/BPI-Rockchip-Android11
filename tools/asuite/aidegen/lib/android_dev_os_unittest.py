#!/usr/bin/env python3
#
# Copyright 2018 - The Android Open Source Project
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

"""Unittests for android_dev_os."""

import unittest
from unittest import mock

from aidegen.lib.android_dev_os import AndroidDevOS


class IdeUtilUnittests(unittest.TestCase):
    """Unit tests for android_dev_os."""

    @mock.patch('platform.system', return_value='Darwin')
    def test_get_os_info_mac(self, mock_system):
        """Test with running in Mac"""
        mock_system.return_value = 'Darwin'
        self.assertTrue(AndroidDevOS.MAC == AndroidDevOS.get_os_type())
        self.assertFalse(AndroidDevOS.LINUX == AndroidDevOS.get_os_type())
        self.assertFalse(AndroidDevOS.WINDOWS == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.get_os_name() == 'MAC')

    @mock.patch('platform.system', return_value='Linux')
    def test_get_os_info_linux(self, mock_system):
        """Test with running in Linux"""
        mock_system.return_value = 'Linux'
        self.assertFalse(AndroidDevOS.MAC == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.LINUX == AndroidDevOS.get_os_type())
        self.assertFalse(AndroidDevOS.WINDOWS == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.get_os_name() == 'LINUX')

    @mock.patch('platform.system', return_value='Windows')
    def test_get_os_info_windows(self, mock_system):
        """Test with running in Windows"""
        mock_system.return_value = 'Windows'
        self.assertFalse(AndroidDevOS.MAC == AndroidDevOS.get_os_type())
        self.assertFalse(AndroidDevOS.LINUX == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.WINDOWS == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.get_os_name() == 'WINDOWS')

    @mock.patch('platform.system', return_value='None')
    def test_get_os_info_default(self, mock_system):
        """Test with running in unknown"""
        mock_system.return_value = 'None'
        self.assertFalse(AndroidDevOS.MAC == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.LINUX == AndroidDevOS.get_os_type())
        self.assertFalse(AndroidDevOS.WINDOWS == AndroidDevOS.get_os_type())
        self.assertTrue(AndroidDevOS.get_os_name() == 'LINUX')


if __name__ == '__main__':
    unittest.main()
