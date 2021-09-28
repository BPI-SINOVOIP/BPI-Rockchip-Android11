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

"""Unittests for native_module_info."""

import logging
import os
import unittest
from unittest import mock

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import module_info
from aidegen.lib import native_module_info

import atest

_PATH_TO_MULT_MODULES_WITH_MULTI_ARCH = 'shared/path/to/be/used2'
_TESTABLE_MODULES_WITH_SHARED_PATH = [
    'multiarch', 'multiarch1', 'multiarch2', 'multiarch3', 'multiarch3_32'
]
_REBUILD_TARGET1 = 'android.frameworks.bufferhub@1.0'
_NATIVE_INCLUDES1 = [
    'frameworks/native/include',
    'frameworks/native/libs/ui',
    'out/soong/.intermediates/' + _REBUILD_TARGET1 + '_genc++_headers/gen'
]
_CC_NAME_TO_MODULE_INFO = {
    'multiarch': {
        'path': [
            'shared/path/to/be/used2'
        ],
        'srcs': [
            'shared/path/to/be/used2/multiarch.cpp',
            'out/soong/.intermediates/shared/path/to/be/used2/gen/Iarch.cpp'
        ],
        'local_common_flags': {
            constant.KEY_HEADER: _NATIVE_INCLUDES1
        },
        'module_name': 'multiarch'
    },
    'multiarch1': {
        'path': [
            'shared/path/to/be/used2/arch1'
        ],
        'module_name': 'multiarch1'
    },
    'multiarch2': {
        'path': [
            'shared/path/to/be/used2/arch2'
        ],
        'module_name': 'multiarch2'
    },
    'multiarch3': {
        'path': [
            'shared/path/to/be/used2/arch3'
        ],
        'module_name': 'multiarch3'
    },
    'multiarch3_32': {
        'path': [
            'shared/path/to/be/used2/arch3_32'
        ],
        'module_name': 'multiarch3_32'
    },
    _REBUILD_TARGET1: {
        'path': [
            '/path/to/rebuild'
        ],
        'module_name': _REBUILD_TARGET1
    },
    'multiarch-eng': {
        'path': [
            'shared/path/to/be/used2-eng'
        ],
        'srcs': [
            'shared/path/to/be/used2/multiarch-eng.cpp',
            'out/soong/.intermediates/shared/path/to/be/used2/gen/Iarch-eng.cpp'
        ],
        'module_name': 'multiarch-eng'
    }
}
_CC_MODULE_INFO = {
    'clang': '${ANDROID_ROOT}/prebuilts/clang/host/linux-x86/bin/clang',
    'clang++': '${ANDROID_ROOT}/prebuilts/clang/host/linux-x86/bin/clang++',
    'modules': _CC_NAME_TO_MODULE_INFO
}


# pylint: disable=protected-access
class NativeModuleInfoUnittests(unittest.TestCase):
    """Unit tests for module_info.py"""

    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_get_module_names_in_targets_paths(self, mock_relpath, mock_load):
        """Test get_module_names_in_targets_paths handling."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        mock_relpath.return_value = (_PATH_TO_MULT_MODULES_WITH_MULTI_ARCH, '')
        result = mod_info.get_module_names_in_targets_paths(['multiarch'])
        self.assertEqual(_TESTABLE_MODULES_WITH_SHARED_PATH, result)

    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_get_module_includes_empty(self, mock_load):
        """Test get_module_includes without include paths."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        result = mod_info.get_module_includes('multiarch1')
        self.assertEqual(set(), result)

    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_get_module_includes_not_empty(self, mock_load):
        """Test get_module_includes with include paths."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        result = mod_info.get_module_includes('multiarch')
        self.assertEqual(set(_NATIVE_INCLUDES1), result)

    @mock.patch.object(logging, 'warning')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_module_need_build_without_mod_info(self, mock_load, mock_warn):
        """Test get_module_includes without module info."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        self.assertFalse(mod_info.is_module_need_build('test_multiarch'))
        self.assertTrue(mock_warn.called)

    @mock.patch.object(logging, 'warning')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_module_need_build_with_mod_info(self, mock_load, mock_warn):
        """Test get_module_includes with module info."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        self.assertFalse(mod_info.is_module_need_build('multiarch1'))
        self.assertFalse(mock_warn.called)

    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_include_need_build')
    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_source_need_build')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_module_need_build_with_src_needs(
            self, mock_load, mock_warn, mock_src_need, mock_inc_need):
        """Test get_module_includes with needed build source modules."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        mock_src_need.return_value = True
        mock_inc_need.return_value = False
        self.assertTrue(mod_info.is_module_need_build('multiarch'))
        self.assertFalse(mock_warn.called)

    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_include_need_build')
    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_source_need_build')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_module_need_build_with_inc_needs(
            self, mock_load, mock_warn, mock_src_need, mock_inc_need):
        """Test get_module_includes with needed build include modules."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        mock_src_need.return_value = False
        mock_inc_need.return_value = True
        self.assertTrue(mod_info.is_module_need_build('multiarch'))
        self.assertFalse(mock_warn.called)

    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_include_need_build')
    @mock.patch.object(native_module_info.NativeModuleInfo,
                       '_is_source_need_build')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_module_need_build_without_needs(
            self, mock_load, mock_warn, mock_src_need, mock_inc_need):
        """Test get_module_includes without needs."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        mock_src_need.return_value = False
        mock_inc_need.return_value = False
        self.assertFalse(mod_info.is_module_need_build('multiarch1'))
        self.assertFalse(mock_warn.called)

    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_source_need_build_return_true(self, mock_load, mock_isfile):
        """Test _is_source_need_build with source paths or not existing."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        mock_isfile.return_value = False
        self.assertTrue(mod_info._is_source_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch']))

    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_source_need_build_return_false(self, mock_load, mock_isfile):
        """Test _is_source_need_build without source paths or paths exist."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        self.assertFalse(mod_info._is_source_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch1']))
        mock_isfile.return_value = True
        self.assertFalse(mod_info._is_source_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch']))

    @mock.patch.object(os.path, 'isdir')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_include_need_build_return_true(self, mock_load, mock_isdir):
        """Test _is_include_need_build with include paths and not existing."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mock_isdir.return_value = False
        mod_info = native_module_info.NativeModuleInfo()
        self.assertTrue(mod_info._is_include_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch']))

    @mock.patch.object(os.path, 'isdir')
    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_is_include_need_build_return_false(self, mock_load, mock_isdir):
        """Test _is_include_need_build without include paths or paths exist."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        self.assertFalse(mod_info._is_include_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch1']))
        mock_isdir.return_value = True
        self.assertFalse(mod_info._is_include_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch']))
        mock_isdir.return_value = True
        self.assertFalse(mod_info._is_include_need_build(
            _CC_NAME_TO_MODULE_INFO['multiarch']))

    def test_not_implemented_methods(self):
        """Test not implemented methods."""
        mod_info = native_module_info.NativeModuleInfo()
        target = 'test'
        with self.assertRaises(NotImplementedError):
            mod_info.get_testable_modules()
        with self.assertRaises(NotImplementedError):
            mod_info.is_testable_module(mock.Mock())
        with self.assertRaises(NotImplementedError):
            mod_info.has_test_config(mock.Mock())
        with self.assertRaises(NotImplementedError):
            mod_info.get_robolectric_test_name(target)
        with self.assertRaises(NotImplementedError):
            mod_info.is_robolectric_test(target)
        with self.assertRaises(NotImplementedError):
            mod_info.is_auto_gen_test_config(target)
        with self.assertRaises(NotImplementedError):
            mod_info.is_robolectric_module(mock.Mock())
        with self.assertRaises(NotImplementedError):
            mod_info.is_native_test(target)

    @mock.patch.object(
        native_module_info.NativeModuleInfo, '_load_module_info_file')
    def test_not_implement_methods_check(self, mock_load):
        """Test for all not implemented methods which should raise error."""
        mock_load.return_value = None, _CC_NAME_TO_MODULE_INFO
        mod_info = native_module_info.NativeModuleInfo()
        with self.assertRaises(NotImplementedError):
            suite_name = 'test'
            test_data = {'a': 'test'}
            mod_info.is_suite_in_compatibility_suites(suite_name, test_data)

        with self.assertRaises(NotImplementedError):
            mod_info.get_testable_modules()

        with self.assertRaises(NotImplementedError):
            test_data = {'a': 'test'}
            mod_info.is_testable_module(test_data)

        with self.assertRaises(NotImplementedError):
            test_data = {'a': 'test'}
            mod_info.has_test_config(test_data)

        with self.assertRaises(NotImplementedError):
            test_mod = 'mod_a'
            mod_info.get_robolectric_test_name(test_mod)

        with self.assertRaises(NotImplementedError):
            test_mod = 'mod_a'
            mod_info.is_robolectric_test(test_mod)

        with self.assertRaises(NotImplementedError):
            test_mod = 'a'
            mod_info.is_auto_gen_test_config(test_mod)

        with self.assertRaises(NotImplementedError):
            test_data = {'a': 'test'}
            mod_info.is_robolectric_module(test_data)

        with self.assertRaises(NotImplementedError):
            test_mod = 'a'
            mod_info.is_native_test(test_mod)

    @mock.patch.object(atest.module_info.ModuleInfo, '_load_module_info_file')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(common_util, 'get_json_dict')
    @mock.patch.object(module_info.AidegenModuleInfo,
                       '_discover_mod_file_and_target')
    @mock.patch.object(common_util, 'get_blueprint_json_path')
    def test_load_module_info_file(self, mock_get_json, mock_load, mock_dict,
                                   mock_isfile, mock_base_load):
        """Test _load_module_info_file."""
        force_build = True
        mod_file = '/test/path'
        mock_get_json.return_value = mod_file
        native_module_info.NativeModuleInfo(force_build)
        self.assertTrue(mock_get_json.called_with(
            constant.BLUEPRINT_CC_JSONFILE_NAME))
        mock_load.return_value = mod_file, mod_file
        self.assertTrue(mock_load.called_with(True))
        mock_dict.return_value = _CC_MODULE_INFO
        self.assertTrue(mock_dict.called_with(mod_file))
        self.assertFalse(mock_base_load.called)
        mock_base_load.reset_mock()
        mock_get_json.reset_mock()
        mock_load.reset_mock()
        mock_dict.reset_mock()

        native_module_info.NativeModuleInfo(force_build, mod_file)
        self.assertFalse(mock_get_json.called)
        mock_load.return_value = None, mod_file
        self.assertTrue(mock_load.called_with(True))
        self.assertTrue(mock_dict.called_with(mod_file))
        self.assertFalse(mock_base_load.called)
        mock_base_load.reset_mock()
        mock_get_json.reset_mock()
        mock_load.reset_mock()
        mock_dict.reset_mock()

        force_build = False
        mock_get_json.return_value = mod_file
        native_module_info.NativeModuleInfo(force_build, mod_file)
        self.assertFalse(mock_get_json.called)
        mock_isfile.return_value = True
        self.assertFalse(mock_load.called)
        mock_dict.return_value = _CC_MODULE_INFO
        self.assertTrue(mock_dict.called_with(mod_file))
        self.assertFalse(mock_base_load.called)
        mock_base_load.reset_mock()
        mock_get_json.reset_mock()
        mock_load.reset_mock()
        mock_dict.reset_mock()


if __name__ == '__main__':
    unittest.main()
