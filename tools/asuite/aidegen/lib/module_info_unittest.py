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

"""Unittests for module_info."""

import os
import unittest

from unittest import mock

from aidegen.lib import common_util
from aidegen.lib import module_info
from aidegen.lib import module_info_util

# pylint: disable=protected-access
#pylint: disable=invalid-name
class AidegenModuleInfoUnittests(unittest.TestCase):
    """Unit tests for module_info.py"""

    def test_is_target_module(self):
        """Test is_target_module with different conditions."""
        self.assertFalse(module_info.AidegenModuleInfo.is_target_module({}))
        self.assertFalse(module_info.AidegenModuleInfo.is_target_module(
            {'path': ''}))
        self.assertFalse(module_info.AidegenModuleInfo.is_target_module(
            {'class': ''}))
        self.assertTrue(module_info.AidegenModuleInfo.is_target_module(
            {'class': ['APPS']}))
        self.assertTrue(
            module_info.AidegenModuleInfo.is_target_module(
                {'class': ['JAVA_LIBRARIES']}))
        self.assertTrue(
            module_info.AidegenModuleInfo.is_target_module(
                {'class': ['ROBOLECTRIC']}))

    def test_is_project_path_relative_module(self):
        """Test is_project_path_relative_module handling."""
        mod_info = {'class':['APPS']}
        self.assertFalse(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, ''))
        mod_info = {'class':['APPS'], 'path':['path_to_a']}
        self.assertTrue(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, ''))
        self.assertFalse(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, 'test'))
        mod_info = {'path':['path_to_a']}
        self.assertFalse(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, 'test'))
        self.assertFalse(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, 'path_to_a'))
        mod_info = {'class':['APPS'], 'path':['test/path_to_a']}
        self.assertTrue(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, 'test'))
        self.assertFalse(
            module_info.AidegenModuleInfo.is_project_path_relative_module(
                mod_info, 'tes'))

    @mock.patch.object(common_util, 'dump_json_dict')
    @mock.patch('logging.debug')
    @mock.patch.object(module_info_util, 'generate_merged_module_info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch('os.remove')
    def test_discover_mod_file_and_target(self, mock_remove, mock_is_file,
                                          mock_generate, mock_log, mock_dump):
        """Test _discover_mod_file_and_target with conditions."""
        # Test not force build case.
        mock_generate.return_value = None
        force_build = False
        mock_is_file.return_value = True
        module_info.AidegenModuleInfo._discover_mod_file_and_target(force_build)
        self.assertFalse(mock_remove.called)
        self.assertFalse(mock_log.called)
        self.assertTrue(mock_generate.called)
        self.assertTrue(mock_dump.called)

        # Test force_build case.
        force_build = True
        self.assertTrue(mock_is_file.called)
        mock_is_file.return_value = False
        module_info.AidegenModuleInfo._discover_mod_file_and_target(force_build)
        self.assertFalse(mock_remove.called)
        self.assertTrue(mock_log.called)
        self.assertTrue(mock_dump.called)

        mock_is_file.return_value = True
        module_info.AidegenModuleInfo._discover_mod_file_and_target(force_build)
        self.assertTrue(mock_remove.called)
        self.assertTrue(mock_dump.called)


if __name__ == '__main__':
    unittest.main()
