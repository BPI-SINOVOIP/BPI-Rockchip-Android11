#!/usr/bin/env python3
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

"""Unittests for module_info_utils."""

import copy
import logging
import os.path
import unittest
from unittest import mock

from aidegen import aidegen_main
from aidegen import unittest_constants
from aidegen.lib import common_util
from aidegen.lib import errors
from aidegen.lib import module_info_util
from aidegen.lib import project_config

from atest import atest_utils

_TEST_CLASS_DICT = {'class': ['JAVA_LIBRARIES']}
_TEST_SRCS_BAR_DICT = {'srcs': ['Bar']}
_TEST_SRCS_BAZ_DICT = {'srcs': ['Baz']}
_TEST_DEP_FOO_DIST = {'dependencies': ['Foo']}
_TEST_DEP_SRC_DICT = {'dependencies': ['Foo'], 'srcs': ['Bar']}
_TEST_DEP_SRC_MORE_DICT = {'dependencies': ['Foo'], 'srcs': ['Baz', 'Bar']}
_TEST_MODULE_A_DICT = {
    'module_a': {
        'class': ['JAVA_LIBRARIES'],
        'path': ['path_a'],
        'installed': ['out/path_a/a.jar'],
        'dependencies': ['Foo'],
        'srcs': ['Bar'],
        'compatibility_suites': ['null-suite'],
        'module_name': ['ltp_fstat03_64']
    }
}
_TEST_MODULE_A_DICT_HAS_NONEED_ITEMS = {
    'module_a': {
        'class': ['JAVA_LIBRARIES'],
        'path': ['path_a'],
        'installed': ['out/path_a/a.jar'],
        'dependencies': ['Foo'],
        'srcs': ['Bar'],
        'compatibility_suites': ['null-suite'],
        'module_name': ['ltp_fstat03_64']
    }
}
_TEST_MODULE_A_JOIN_PATH_DICT = {
    'module_a': {
        'class': ['JAVA_LIBRARIES'],
        'path': ['path_a'],
        'installed': ['out/path_a/a.jar'],
        'dependencies': ['Foo'],
        'srcs': ['path_a/Bar']
    }
}


# pylint: disable=invalid-name
# pylint: disable=protected-access
# ptlint: disable=too-many-format-args
class AidegenModuleInfoUtilUnittests(unittest.TestCase):
    """Unit tests for module_info_utils.py"""

    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_of_build_bp_info_reuse_jsons(self, mock_json, mock_isfile,
                                          mock_log, mock_time):
        """Test of _build_bp_info to well reuse existing files."""
        gen_files = ['file1', 'file_a', 'file_b']
        mock_json.return_value = gen_files
        # Test of the well reuse existing files.
        mock_isfile.side_effect = (True, True, True)
        mod_info = mock.MagicMock()
        module_info_util._build_bp_info(mod_info, skip_build=True)
        self.assertTrue(mock_json.called)
        self.assertTrue(mock_log.called)
        self.assertFalse(mock_time.called)

    # pylint: disable=too-many-arguments
    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_of_build_bp_info_rebuild_jsons(self, mock_json, mock_isfile,
                                            mock_log, mock_time, mock_build,
                                            mock_reuse, mock_fail):
        """Test of _build_bp_info on rebuilding jsons."""
        gen_files = ['file2', 'file_a', 'file_b']
        mock_json.return_value = gen_files
        mod_info = mock.MagicMock()
        # Test of the existing files can't reuse.
        mock_json.return_value = gen_files
        mock_isfile.side_effect = (True, False, True)
        mock_time.side_effect = (None, None, None)
        mock_build.side_effect = (True, True)
        module_info_util._build_bp_info(mod_info, skip_build=True)
        self.assertTrue(mock_json.called)
        self.assertTrue(mock_log.called)

        # Test of the well rebuild case.
        action_pass = '\nGenerate blueprint json successfully.'
        self.assertTrue(mock_build.called)
        self.assertTrue(mock_log.called_with(action_pass))
        self.assertFalse(mock_reuse.called)
        self.assertFalse(mock_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_of_build_bp_info_show_build_fail(self, mock_json, mock_isfile,
                                              mock_log, mock_time, mock_build,
                                              mock_judge, mock_reuse,
                                              mock_fail):
        """Test of _build_bp_info to show build failed message."""
        gen_files = ['file3', 'file_a', 'file_b']
        mock_json.return_value = gen_files
        mod_info = mock.MagicMock()
        # Test rebuild failed case - show build fail message.
        mock_json.return_value = gen_files
        test_prj = 'main'
        mock_isfile.side_effect = (True, False, True)
        mock_time.side_effect = (None, None, None)
        mock_build.side_effect = (True, False)
        mock_judge.side_effect = [True, False, False]
        module_info_util._build_bp_info(mod_info, main_project=test_prj,
                                        skip_build=False)
        self.assertTrue(mock_json.called)
        self.assertTrue(mock_log.called)
        self.assertTrue(mock_build.called)
        self.assertFalse(mock_reuse.called)
        self.assertTrue(mock_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_of_build_bp_info_rebuild_and_reuse(self, mock_json, mock_isfile,
                                                mock_log, mock_time, mock_build,
                                                mock_judge, mock_reuse,
                                                mock_fail):
        """Test of _build_bp_info to reuse existing jsons."""
        gen_files = ['file4', 'file_a', 'file_b']
        mock_json.return_value = gen_files
        mod_info = mock.MagicMock()
        mock_json.return_value = gen_files
        test_prj = 'main'
        # Test rebuild failed case - show reuse message.
        mock_isfile.side_effect = (True, True, True)
        mock_time.side_effect = (None, None, None)
        mock_build.side_effect = (True, False)
        mock_judge.side_effect = [True, False, True]
        module_info_util._build_bp_info(
            mod_info, main_project=test_prj, skip_build=False)
        self.assertTrue(mock_log.called)
        self.assertTrue(mock_reuse.called)
        self.assertFalse(mock_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_of_build_bp_info_reuse_pass(self, mock_json, mock_isfile, mock_log,
                                         mock_time, mock_build, mock_judge,
                                         mock_reuse, mock_fail):
        """Test of _build_bp_info reuse pass."""
        gen_files = ['file5', 'file_a', 'file_b']
        mock_json.return_value = gen_files
        test_prj = 'main'
        mod_info = mock.MagicMock()
        # Test rebuild failed case - show nothing.
        mock_isfile.side_effect = (False, True, True)
        mock_time.side_effect = (None, None, None)
        mock_build.side_effect = (True, False)
        mock_judge.side_effect = [True, True, True]
        module_info_util._build_bp_info(mod_info, main_project=test_prj,
                                        skip_build=False)
        self.assertTrue(mock_log.called)
        self.assertFalse(mock_reuse.called)
        self.assertFalse(mock_fail.called)

    # pylint: enable=too-many-arguments
    def test_merge_module_keys_with_empty_dict(self):
        """Test _merge_module_keys with an empty dictionary."""
        test_b_dict = {}
        test_m_dict = copy.deepcopy(_TEST_DEP_SRC_DICT)
        module_info_util._merge_module_keys(test_m_dict, test_b_dict)
        self.assertEqual(_TEST_DEP_SRC_DICT, test_m_dict)

    def test_merge_module_keys_with_key_not_in_orginial_dict(self):
        """Test _merge_module_keys with the key does not exist in the dictionary
        to be merged into.
        """
        test_b_dict = _TEST_SRCS_BAR_DICT
        test_m_dict = copy.deepcopy(_TEST_DEP_FOO_DIST)
        module_info_util._merge_module_keys(test_m_dict, test_b_dict)
        self.assertEqual(_TEST_DEP_SRC_DICT, test_m_dict)

    def test_merge_module_keys_with_key_in_orginial_dict(self):
        """Test _merge_module_keys with with the key exists in the dictionary
        to be merged into.
        """
        test_b_dict = _TEST_SRCS_BAZ_DICT
        test_m_dict = copy.deepcopy(_TEST_DEP_SRC_DICT)
        module_info_util._merge_module_keys(test_m_dict, test_b_dict)
        self.assertEqual(
            set(_TEST_DEP_SRC_MORE_DICT['srcs']), set(test_m_dict['srcs']))
        self.assertEqual(
            set(_TEST_DEP_SRC_MORE_DICT['dependencies']),
            set(test_m_dict['dependencies']))

    def test_merge_module_keys_with_duplicated_item_dict(self):
        """Test _merge_module_keys with with the key exists in the dictionary
        to be merged into.
        """
        test_b_dict = _TEST_CLASS_DICT
        test_m_dict = copy.deepcopy(_TEST_CLASS_DICT)
        module_info_util._merge_module_keys(test_m_dict, test_b_dict)
        self.assertEqual(_TEST_CLASS_DICT, test_m_dict)

    def test_copy_needed_items_from_empty_dict(self):
        """Test _copy_needed_items_from an empty dictionary."""
        test_mk_dict = {}
        want_dict = {}
        self.assertEqual(want_dict,
                         module_info_util._copy_needed_items_from(test_mk_dict))

    def test_copy_needed_items_from_all_needed_items_dict(self):
        """Test _copy_needed_items_from a dictionary with all needed items."""
        self.assertEqual(
            _TEST_MODULE_A_DICT,
            module_info_util._copy_needed_items_from(_TEST_MODULE_A_DICT))

    def test_copy_needed_items_from_some_needed_items_dict(self):
        """Test _copy_needed_items_from a dictionary with some needed items."""
        self.assertEqual(
            _TEST_MODULE_A_DICT,
            module_info_util._copy_needed_items_from(
                _TEST_MODULE_A_DICT_HAS_NONEED_ITEMS))

    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch('os.path.isfile')
    def test_build_bp_info_normal(self, mock_isfile, mock_build, mock_time):
        """Test _build_target with verbose true and false."""
        amodule_info = mock.MagicMock()
        skip = True
        mock_isfile.return_value = True
        mock_build.return_value = True
        module_info_util._build_bp_info(amodule_info, unittest_constants.
                                        TEST_MODULE, False, skip)
        self.assertFalse(mock_build.called)
        skip = False
        mock_time.return_value = float()
        module_info_util._build_bp_info(amodule_info, unittest_constants.
                                        TEST_MODULE, False, skip)
        self.assertTrue(mock_time.called)
        self.assertEqual(mock_build.call_count, 2)

    @mock.patch('os.path.getmtime')
    @mock.patch('os.path.isfile')
    def test_is_new_json_file_generated(self, mock_isfile, mock_getmtime):
        """Test _is_new_json_file_generated with different situations."""
        jfile = 'path/test.json'
        mock_isfile.return_value = False
        self.assertFalse(
            module_info_util._is_new_json_file_generated(jfile, None))
        mock_isfile.return_value = True
        self.assertTrue(
            module_info_util._is_new_json_file_generated(jfile, None))
        original_file_mtime = 1000
        mock_getmtime.return_value = original_file_mtime
        self.assertFalse(
            module_info_util._is_new_json_file_generated(
                jfile, original_file_mtime))
        mock_getmtime.return_value = 1001
        self.assertTrue(
            module_info_util._is_new_json_file_generated(
                jfile, original_file_mtime))

    @mock.patch('builtins.input')
    @mock.patch('glob.glob')
    def test_build_failed_handle(self, mock_glob, mock_input):
        """Test _build_failed_handle with different situations."""
        mock_glob.return_value = ['project/file.iml']
        mock_input.return_value = 'N'
        with self.assertRaises(SystemExit) as cm:
            module_info_util._build_failed_handle(
                unittest_constants.TEST_MODULE)
        self.assertEqual(cm.exception.code, 1)
        mock_glob.return_value = []
        with self.assertRaises(errors.BuildFailureError):
            module_info_util._build_failed_handle(
                unittest_constants.TEST_MODULE)

    @mock.patch.object(project_config.ProjectConfig, 'get_instance')
    @mock.patch.object(module_info_util, '_merge_dict')
    @mock.patch.object(common_util, 'get_json_dict')
    @mock.patch.object(module_info_util, '_build_bp_info')
    def test_generate_merged_module_info(
            self, mock_build, mock_get_soong, mock_merge_dict, mock_get_inst):
        """Test generate_merged_module_info function."""
        config = mock.MagicMock()
        config.atest_module_info = mock.MagicMock()
        main_project = 'tradefed'
        args = aidegen_main._parse_args([main_project, '-n', '-v'])
        config.targets = args.targets
        config.verbose = args.verbose
        config.is_skip_build = args.skip_build
        module_info_util.generate_merged_module_info()
        self.assertTrue(mock_get_inst.called)
        self.assertTrue(mock_build.called)
        self.assertTrue(mock_get_soong.called)
        self.assertTrue(mock_merge_dict.called)

    @mock.patch.object(common_util, 'get_blueprint_json_files_relative_dict')
    def test_get_generated_json_files(self, mock_get_bp_dict):
        """Test _get_generated_json_files function with condictions,"""
        a_env = 'GEN_A'
        b_env = 'GEN_B'
        a_file_path = 'a/b/path/to/a_file'
        b_file_path = 'a/b/path/to/b_file'
        file_paths = [a_file_path, b_file_path]
        env_on = {a_env: 'true', b_env: 'true'}
        mock_get_bp_dict.return_value = {a_env: a_file_path, b_env: b_file_path}
        result_paths = file_paths
        self.assertEqual(
            result_paths, module_info_util._get_generated_json_files(env_on))
        result_paths = []
        env_on = {a_env: 'false', b_env: 'false'}
        self.assertEqual(
            result_paths, module_info_util._get_generated_json_files(env_on))
        c_env = 'GEN_C'
        d_env = 'GEN_D'
        c_file_path = 'a/b/path/to/d_file'
        d_file_path = 'a/b/path/to/d_file'
        env_on = {a_env: 'true', b_env: 'true'}
        mock_get_bp_dict.return_value = {c_env: c_file_path, d_env: d_file_path}
        result_paths = []
        self.assertEqual(
            result_paths, module_info_util._get_generated_json_files(env_on))

    @mock.patch.object(common_util, 'get_related_paths')
    @mock.patch.object(module_info_util, '_build_failed_handle')
    def test_show_build_failed_message(
            self, mock_handle, mock_relpath):
        """Test _show_build_failed_message with different conditions."""
        main_project = ''
        mock_relpath.return_value = 'c/d', 'a/b/c/d'
        module_info_util._show_build_failed_message({}, main_project)
        self.assertFalse(mock_handle.called)
        main_project = 'tradefed'
        module_info_util._show_build_failed_message({}, main_project)
        self.assertTrue(mock_handle.called)

    # pylint: disable=too-many-arguments
    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_build_bp_info_skip(self, mock_gen_jsons, mock_isfile,
                                mock_log_info, mock_get_mtimes, mock_build,
                                mock_news, mock_warn, mock_show_reuse,
                                mock_build_fail):
        """Test _build_bp_info function with skip build."""
        mock_gen_jsons.return_value = [
            'a/b/out/soong/module_bp_java_deps.json',
            'a/b/out/soong/module_bp_cc_deps.json'
        ]
        mock_isfile.return_value = True
        module_info_util._build_bp_info(
            module_info=mock.Mock(), skip_build=True)
        self.assertTrue(mock_log_info.called)
        self.assertFalse(mock_get_mtimes.called)
        self.assertFalse(mock_build.called)
        self.assertFalse(mock_news.called)
        self.assertFalse(mock_warn.called)
        self.assertFalse(mock_show_reuse.called)
        self.assertFalse(mock_build_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_build_bp_info_no_skip(self, mock_gen_jsons, mock_isfile,
                                   mock_log_info, mock_get_mtimes, mock_build,
                                   mock_news, mock_warn, mock_show_reuse,
                                   mock_build_fail):
        """Test _build_bp_info function without skip build."""
        mock_gen_jsons.return_value = [
            'a/b/out/soong/module_bp_java_deps.json',
            'a/b/out/soong/module_bp_cc_deps.json'
        ]
        mock_isfile.return_value = True
        mock_build.return_value = True
        module_info_util._build_bp_info(
            module_info=mock.Mock(), skip_build=False)
        self.assertTrue(mock_log_info.called)
        self.assertTrue(mock_get_mtimes.called)
        self.assertTrue(mock_build.called)
        self.assertFalse(mock_news.called)
        self.assertTrue(mock_warn.called)
        self.assertFalse(mock_show_reuse.called)
        self.assertFalse(mock_build_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_build_bp_info_failed(self, mock_gen_jsons, mock_isfile,
                                  mock_log_info, mock_get_mtimes, mock_build,
                                  mock_news, mock_warn, mock_show_reuse,
                                  mock_build_fail):
        """Test _build_bp_info function with build failed."""
        mock_gen_jsons.return_value = [
            'a/b/out/soong/module_bp_java_deps.json',
            'a/b/out/soong/module_bp_cc_deps.json'
        ]
        mock_isfile.return_value = True
        mock_build.return_value = False
        mock_news.return_value = False
        module_info_util._build_bp_info(
            module_info=mock.Mock(), skip_build=False)
        self.assertFalse(mock_log_info.called)
        self.assertTrue(mock_get_mtimes.called)
        self.assertTrue(mock_build.called)
        self.assertTrue(mock_news.called)
        self.assertTrue(mock_warn.called)
        self.assertTrue(mock_show_reuse.called)
        self.assertFalse(mock_build_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_build_bp_info_failed_not_exist(
            self, mock_gen_jsons, mock_isfile, mock_log_info,
            mock_get_mtimes, mock_build, mock_news, mock_warn, mock_show_reuse,
            mock_build_fail):
        """Test _build_bp_info function with build failed files not exist."""
        mock_gen_jsons.return_value = [
            'a/b/out/soong/module_bp_java_deps.json',
            'a/b/out/soong/module_bp_cc_deps.json'
        ]
        mock_isfile.return_value = False
        mock_build.return_value = False
        mock_news.return_value = False
        module_info_util._build_bp_info(
            module_info=mock.Mock(), skip_build=False)
        self.assertFalse(mock_log_info.called)
        self.assertFalse(mock_get_mtimes.called)
        self.assertTrue(mock_build.called)
        self.assertTrue(mock_news.called)
        self.assertTrue(mock_warn.called)
        self.assertFalse(mock_show_reuse.called)
        self.assertTrue(mock_build_fail.called)

    @mock.patch.object(module_info_util, '_show_build_failed_message')
    @mock.patch.object(module_info_util, '_show_files_reuse_message')
    @mock.patch.object(logging, 'warning')
    @mock.patch.object(module_info_util, '_is_new_json_file_generated')
    @mock.patch.object(atest_utils, 'build')
    @mock.patch.object(os.path, 'getmtime')
    @mock.patch.object(logging, 'info')
    @mock.patch.object(os.path, 'isfile')
    @mock.patch.object(module_info_util, '_get_generated_json_files')
    def test_build_bp_info_failed_files_exist(
            self, mock_gen_jsons, mock_isfile, mock_log_info,
            mock_get_mtimes, mock_build, mock_news, mock_warn, mock_show_reuse,
            mock_build_fail):
        """Test _build_bp_info function with build failed files not exist."""
        mock_gen_jsons.return_value = [
            'a/b/out/soong/module_bp_java_deps.json',
            'a/b/out/soong/module_bp_cc_deps.json'
        ]
        mock_isfile.return_value = False
        mock_build.return_value = False
        mock_news.return_value = True
        module_info_util._build_bp_info(
            module_info=mock.Mock(), skip_build=False)
        self.assertFalse(mock_log_info.called)
        self.assertFalse(mock_get_mtimes.called)
        self.assertTrue(mock_build.called)
        self.assertTrue(mock_news.called)
        self.assertTrue(mock_warn.called)
        self.assertFalse(mock_show_reuse.called)
        self.assertFalse(mock_build_fail.called)


if __name__ == '__main__':
    unittest.main()
