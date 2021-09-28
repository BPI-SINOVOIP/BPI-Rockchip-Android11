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

"""Unittests for project_config."""

from __future__ import print_function

import os
import unittest
from unittest import mock

from aidegen import aidegen_main
from aidegen import constant
from aidegen.lib import errors
from aidegen import unittest_constants
from aidegen.lib import project_config
from aidegen.lib import common_util


# pylint: disable=protected-access
class AidegenProjectConfigUnittests(unittest.TestCase):
    """Unit tests for project_config.py"""

    @mock.patch.object(os, 'getcwd')
    def test_is_whole_android_tree(self, mock_getcwd):
        """Test _is_whole_android_tree with different conditions."""
        self.assertTrue(project_config.is_whole_android_tree(['a'], True))
        mock_getcwd.return_value = common_util.get_android_root_dir()
        self.assertTrue(project_config.is_whole_android_tree([''], False))
        self.assertFalse(project_config.is_whole_android_tree(['a'], False))

    @mock.patch.object(common_util, 'is_android_root')
    def test_check_whole_android_tree(self, mock_is_android_root):
        """Test _check_whole_android_tree with different conditions."""
        mock_is_android_root.return_value = True
        exspected = [constant.WHOLE_ANDROID_TREE_TARGET]
        self.assertEqual(
            exspected, project_config._check_whole_android_tree([''], True))
        self.assertEqual(
            exspected, project_config._check_whole_android_tree([''], False))
        exspected = constant.WHOLE_ANDROID_TREE_TARGET
        self.assertEqual(
            exspected, project_config._check_whole_android_tree(['a'], True)[0])
        exspected = ['a']
        self.assertEqual(
            exspected, project_config._check_whole_android_tree(['a'], False))

    def test_init_with_no_launch_ide(self):
        """Test __init__ method without launching IDE."""
        args = aidegen_main._parse_args(['a', '-n', '-s'])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.ide_name, constant.IDE_INTELLIJ)
        self.assertFalse(config.is_launch_ide)
        self.assertEqual(config.depth, 0)
        self.assertFalse(config.full_repo)
        self.assertTrue(config.is_skip_build)
        self.assertEqual(config.targets, ['a'])
        self.assertFalse(config.verbose)
        self.assertEqual(config.ide_installed_path, None)
        self.assertFalse(config.config_reset)
        self.assertEqual(config.exclude_paths, None)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.ide_name, constant.IDE_INTELLIJ)
        self.assertFalse(config_obj.is_launch_ide)
        self.assertEqual(config_obj.depth, 0)
        self.assertFalse(config_obj.full_repo)
        self.assertTrue(config_obj.is_skip_build)
        self.assertEqual(config_obj.targets, ['a'])
        self.assertFalse(config_obj.verbose)
        self.assertEqual(config_obj.ide_installed_path, None)
        self.assertFalse(config_obj.config_reset)
        self.assertEqual(config.exclude_paths, None)

    def test_init_with_diff_arguments(self):
        """Test __init__ method with different arguments."""
        args = aidegen_main._parse_args([])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.ide_name, constant.IDE_INTELLIJ)
        self.assertEqual(config.targets, [''])
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.targets, [''])
        self.assertEqual(config_obj.ide_name, constant.IDE_INTELLIJ)
        target = 'tradefed'
        args = aidegen_main._parse_args([target])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.targets, [target])
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.targets, [target])
        depth = '2'
        args = aidegen_main._parse_args(['-d', depth])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.depth, int(depth))
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.depth, int(depth))

    def test_init_with_other_arguments(self):
        """Test __init__ method with other arguments."""
        args = aidegen_main._parse_args(['-v'])
        config = project_config.ProjectConfig(args)
        self.assertTrue(config.verbose)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertTrue(config_obj.verbose)
        args = aidegen_main._parse_args(['-i', 's'])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.ide_name, constant.IDE_ANDROID_STUDIO)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.ide_name, constant.IDE_ANDROID_STUDIO)
        args = aidegen_main._parse_args(['-i', 'e'])
        config = project_config.ProjectConfig(args)
        self.assertEqual(config.ide_name, constant.IDE_ECLIPSE)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(config_obj.ide_name, constant.IDE_ECLIPSE)
        args = aidegen_main._parse_args(['-p', unittest_constants.TEST_MODULE])
        config = project_config.ProjectConfig(args)
        self.assertEqual(
            config.ide_installed_path, unittest_constants.TEST_MODULE)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertEqual(
            config_obj.ide_installed_path, unittest_constants.TEST_MODULE)
        args = aidegen_main._parse_args(['-n'])
        config = project_config.ProjectConfig(args)
        self.assertFalse(config.is_launch_ide)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertFalse(config_obj.is_launch_ide)
        args = aidegen_main._parse_args(['-r'])
        config = project_config.ProjectConfig(args)
        self.assertTrue(config.config_reset)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertTrue(config_obj.config_reset)
        args = aidegen_main._parse_args(['-s'])
        config = project_config.ProjectConfig(args)
        self.assertTrue(config.is_skip_build)
        config_obj = project_config.ProjectConfig.get_instance()
        self.assertTrue(config_obj.is_skip_build)

    @mock.patch.object(common_util, 'get_related_paths')
    def test_transform_exclusive_paths(self, mock_get_rel):
        """Test _transform_exclusive_paths with conditions."""
        result = project_config._transform_exclusive_paths(mock.Mock(), [])
        self.assertFalse(mock_get_rel.called)
        self.assertEqual(None, result)
        rel_path = 'to/path'
        abs_path = 'a/b/to/path'
        mock_get_rel.reset_mock()
        mock_get_rel.return_value = rel_path, abs_path
        result = project_config._transform_exclusive_paths(
            mock.Mock(), [abs_path])
        self.assertEqual([rel_path], result)

    @mock.patch.object(project_config, '_check_whole_android_tree')
    @mock.patch.object(project_config, '_transform_exclusive_paths')
    @mock.patch.object(common_util, 'get_atest_module_info')
    @mock.patch.object(project_config.ProjectConfig, '_show_skip_build_msg')
    def test_init_environment(self, mock_show_skip, mock_get_atest, mock_trans,
                              mock_check_whole):
        """Test init_environment method."""
        args = aidegen_main._parse_args(['-v'])
        config = project_config.ProjectConfig(args)
        config.init_environment()
        self.assertTrue(mock_show_skip.called)
        self.assertTrue(mock_get_atest.called)
        self.assertTrue(mock_trans.called)
        self.assertTrue(mock_check_whole.called)

    @mock.patch('builtins.print')
    def test_show_skip_build_msg_with_skip(self, mock_print):
        """Test _show_skip_build_msg method with skip build."""
        args = aidegen_main._parse_args(['-s'])
        config = project_config.ProjectConfig(args)
        config._show_skip_build_msg()
        self.assertTrue(mock_print.called_with(
            '\n{} {}\n'.format(common_util.COLORED_INFO('Warning:'),
                               project_config._SKIP_BUILD_WARN)))

    @mock.patch('builtins.print')
    def test_show_skip_build_msg_without_skip(self, mock_print):
        """Test _show_skip_build_msg method without skip build."""
        targets = ['Settings']
        args = aidegen_main._parse_args(targets)
        config = project_config.ProjectConfig(args)
        config._show_skip_build_msg()
        msg = project_config.SKIP_BUILD_INFO.format(common_util.COLORED_INFO(
            project_config._SKIP_BUILD_CMD.format(' '.join(config.targets))))
        self.assertTrue(mock_print.called_with(
            '\n{} {}\n'.format(common_util.COLORED_INFO('INFO:'), msg)))

    def test_get_instance_without_instance(self):
        """Test get_instance method without initialize an instance."""
        project_config.ProjectConfig._instance = None
        with self.assertRaises(errors.InstanceNotExistError):
            project_config.ProjectConfig.get_instance()


if __name__ == '__main__':
    unittest.main()
