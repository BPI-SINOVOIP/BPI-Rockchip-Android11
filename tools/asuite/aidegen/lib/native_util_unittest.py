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

"""Unittests for native_util."""

import os
import unittest
from unittest import mock

from aidegen import unittest_constants
from aidegen.lib import common_util
from aidegen.lib import native_module_info
from aidegen.lib import native_util


# pylint: disable=protected-access
# pylint: disable=invalid-name
class AidegenNativeUtilUnittests(unittest.TestCase):
    """Unit tests for native_util.py"""

    @mock.patch.object(native_util, '_check_native_project_exists')
    @mock.patch.object(native_util, '_check_java_file_exists')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_analyze_native_and_java_projects(
            self, mock_get_related, mock_check_java, mock_check_native):
        """Test analyze_native_and_java_projects function."""
        mock_get_related.return_value = None, None
        mock_check_java.return_value = True
        mock_check_native.return_value = True
        targets = ['a']
        self.assertEqual((targets, targets),
                         native_util._analyze_native_and_java_projects(
                             None, None, targets))
        mock_check_native.return_value = False
        self.assertEqual((targets, []),
                         native_util._analyze_native_and_java_projects(
                             None, None, targets))
        mock_check_java.return_value = False
        mock_check_native.return_value = True
        self.assertEqual(([], targets),
                         native_util._analyze_native_and_java_projects(
                             None, None, targets))

    def test_check_native_project_exists(self):
        """Test _check_native_project_exists function."""
        rel_path = 'a/b'
        path_to_module_info = {'a/b/c': {}}
        self.assertTrue(
            native_util._check_native_project_exists(path_to_module_info,
                                                     rel_path))
        rel_path = 'a/b/c/d'
        self.assertFalse(
            native_util._check_native_project_exists(path_to_module_info,
                                                     rel_path))

    def test_find_parent(self):
        """Test _find_parent function with conditions."""
        current_parent = None
        abs_path = 'a/b/c/d'
        expected = abs_path
        result = native_util._find_parent(abs_path, current_parent)
        self.assertEqual(result, expected)
        current_parent = 'a/b/c/d/e'
        result = native_util._find_parent(abs_path, current_parent)
        self.assertEqual(result, expected)
        current_parent = 'a/b/c'
        expected = current_parent
        result = native_util._find_parent(abs_path, current_parent)
        self.assertEqual(result, expected)
        current_parent = 'a/b/f'
        expected = 'a/b'
        result = native_util._find_parent(abs_path, current_parent)
        self.assertEqual(result, expected)

    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_load_module_info_file')
    @mock.patch.object(native_util, '_find_parent')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_get_merged_native_target_is_module(
            self, mock_get_related, mock_find_parent, mock_load_info):
        """Test _get_merged_native_target function if the target is a module."""
        mock_get_related.return_value = 'c/d', 'a/b/c/d'
        parent = 'a/b'
        mock_find_parent.return_value = parent
        targets = ['multiarch']
        expected = (parent, targets)
        mock_load_info.return_value = (
            None, unittest_constants.CC_NAME_TO_MODULE_INFO)
        cc_mod_info = native_module_info.NativeModuleInfo()
        new_parent, new_targets = native_util._get_merged_native_target(
            cc_mod_info, targets)
        result = (new_parent, new_targets)
        self.assertEqual(result, expected)

    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_load_module_info_file')
    @mock.patch.object(native_util, '_find_parent')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_get_merged_native_target_is_path(self, mock_get_related,
                                              mock_find_parent, mock_load_info):
        """Test _get_merged_native_target function if the target is a path."""
        parent = 'a/b'
        rel_path = 'shared/path/to/be/used2'
        mock_get_related.return_value = rel_path, os.path.join(parent, rel_path)
        mock_find_parent.return_value = parent
        mock_load_info.return_value = (
            None, unittest_constants.CC_NAME_TO_MODULE_INFO)
        targets = [rel_path]
        result_targets = unittest_constants.TESTABLE_MODULES_WITH_SHARED_PATH
        expected = (parent, result_targets)
        cc_mod_info = native_module_info.NativeModuleInfo()
        new_parent, new_targets = native_util._get_merged_native_target(
            cc_mod_info, targets)
        result = (new_parent, new_targets)
        self.assertEqual(result, expected)


    def test_filter_out_modules(self):
        """Test _filter_out_modules with conditions."""
        targets = ['shared/path/to/be/used2']
        result = ([], targets)
        self.assertEqual(
            result, native_util._filter_out_modules(targets, lambda x: False))
        targets = ['multiarch']
        result = (targets, [])
        self.assertEqual(
            result, native_util._filter_out_modules(targets, lambda x: True))

    @mock.patch.object(native_util, '_analyze_native_and_java_projects')
    @mock.patch.object(native_util, '_filter_out_modules')
    def test_get_native_and_java_projects(self, mock_fil, mock_ana):
        """Test get_native_and_java_projects handling."""
        targets = ['multiarch']
        mock_fil.return_value = [], targets
        cc_mod_info = mock.Mock()
        cc_mod_info.is_module = mock.Mock()
        cc_mod_info.is_module.return_value = True
        at_mod_info = mock.Mock()
        at_mod_info.is_module = mock.Mock()
        at_mod_info.is_module.return_value = True
        mock_ana.return_value = [], targets
        native_util.get_native_and_java_projects(
            at_mod_info, cc_mod_info, targets)
        self.assertEqual(mock_fil.call_count, 2)
        self.assertEqual(mock_ana.call_count, 1)

    @mock.patch('os.walk')
    def test_check_java_file_exists(self, mock_walk):
        """Test _check_java_file_exists with conditions."""
        root_dir = 'a/path/to/dir'
        folder = 'path/to/dir'
        target = 'test.java'
        abs_path = os.path.join(root_dir, folder)
        mock_walk.return_value = [(root_dir, [folder], [target])]
        self.assertTrue(native_util._check_java_file_exists(abs_path))
        target = 'test.cpp'
        mock_walk.return_value = [(root_dir, [folder], [target])]
        self.assertFalse(native_util._check_java_file_exists(abs_path))


if __name__ == '__main__':
    unittest.main()
