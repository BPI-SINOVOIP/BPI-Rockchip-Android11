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

"""Unittests for project_info."""

import logging
import os
import shutil
import tempfile
import unittest
from unittest import mock

from aidegen import constant
from aidegen import unittest_constants
from aidegen.lib import common_util
from aidegen.lib import project_info
from aidegen.lib import project_config

_MODULE_INFO = {
    'm1': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m2', 'm6'],
        'path': ['m1']
    },
    'm2': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m3', 'm4']
    },
    'm3': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': []
    },
    'm4': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m6']
    },
    'm5': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': []
    },
    'm6': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m2']
    },
}
_EXPECT_DEPENDENT_MODULES = {
    'm1': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m2', 'm6'],
        'path': ['m1'],
        'depth': 0
    },
    'm2': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m3', 'm4'],
        'depth': 1
    },
    'm3': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': [],
        'depth': 2
    },
    'm4': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m6'],
        'depth': 2
    },
    'm6': {
        'class': ['JAVA_LIBRARIES'],
        'dependencies': ['m2'],
        'depth': 1
    },
}


# pylint: disable=protected-access
# pylint: disable=invalid-name
class ProjectInfoUnittests(unittest.TestCase):
    """Unit tests for project_info.py"""

    def setUp(self):
        """Initialize arguments for ProjectInfo."""
        self.args = mock.MagicMock()
        self.args.module_name = 'm1'
        self.args.project_path = ''
        self.args.ide = ['j']
        self.args.no_launch = True
        self.args.depth = 0
        self.args.android_tree = False
        self.args.skip_build = True
        self.args.targets = ['m1']
        self.args.verbose = False
        self.args.ide_installed_path = None
        self.args.config_reset = False

    @mock.patch('atest.module_info.ModuleInfo')
    def test_get_dep_modules(self, mock_module_info):
        """Test get_dep_modules recursively find dependent modules."""
        mock_module_info.name_to_module_info = _MODULE_INFO
        mock_module_info.is_module.return_value = True
        mock_module_info.get_paths.return_value = ['m1']
        mock_module_info.get_module_names.return_value = ['m1']
        project_info.ProjectInfo.modules_info = mock_module_info
        proj_info = project_info.ProjectInfo(self.args.module_name, False)
        self.assertEqual(proj_info.dep_modules, _EXPECT_DEPENDENT_MODULES)

    @mock.patch.object(project_info.ProjectInfo,
                       '_get_modules_under_project_path')
    @mock.patch.object(project_info.ProjectInfo, 'get_dep_modules')
    def test_init(self, mock_get_deps, mock_get_sub_modules):
        """Test init."""
        project_info.ProjectInfo(constant.FRAMEWORK_ALL, False)
        self.assertTrue(mock_get_deps.called)
        self.assertFalse(mock_get_sub_modules.called)

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_target_name(self, mock_get_root):
        """Test get_target_name with different conditions."""
        mock_get_root.return_value = unittest_constants.TEST_DATA_PATH
        self.assertEqual(
            project_info.ProjectInfo.get_target_name(
                unittest_constants.TEST_MODULE,
                unittest_constants.TEST_DATA_PATH),
            os.path.basename(unittest_constants.TEST_DATA_PATH))
        self.assertEqual(
            project_info.ProjectInfo.get_target_name(
                unittest_constants.TEST_MODULE, unittest_constants.TEST_PATH),
            unittest_constants.TEST_MODULE)

    # pylint: disable=too-many-locals
    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch('atest.module_info.ModuleInfo')
    @mock.patch('atest.atest_utils.build')
    def test_locate_source(self, mock_atest_utils_build, mock_module_info,
                           mock_get_root):
        """Test locate_source handling."""
        mock_atest_utils_build.build.return_value = True
        test_root_path = os.path.join(tempfile.mkdtemp(), 'test')
        shutil.copytree(unittest_constants.TEST_DATA_PATH, test_root_path)
        mock_get_root.return_value = test_root_path
        generated_jar = ('out/soong/.intermediates/packages/apps/test/test/'
                         'android_common/generated.jar')
        locate_module_info = dict(unittest_constants.MODULE_INFO)
        locate_module_info['installed'] = [generated_jar]
        mock_module_info.is_module.return_value = True
        mock_module_info.get_paths.return_value = [
            unittest_constants.MODULE_PATH
        ]
        mock_module_info.get_module_names.return_value = [
            unittest_constants.TEST_MODULE
        ]
        project_config.ProjectConfig(self.args)
        project_info_obj = project_info.ProjectInfo(
            mock_module_info.get_paths()[0])
        project_info_obj.dep_modules = {
            unittest_constants.TEST_MODULE: locate_module_info
        }
        project_info_obj._init_source_path()
        # Show warning when the jar not exists after build the module.
        result_jar = set()
        project_info_obj.locate_source()
        self.assertEqual(project_info_obj.source_path['jar_path'], result_jar)

        # Test collects source and test folders.
        result_source = set(['packages/apps/test/src/main/java'])
        result_test = set(['packages/apps/test/tests'])
        self.assertEqual(project_info_obj.source_path['source_folder_path'],
                         result_source)
        self.assertEqual(project_info_obj.source_path['test_folder_path'],
                         result_test)

    @mock.patch.object(project_info, 'batch_build_dependencies')
    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch('atest.module_info.ModuleInfo')
    @mock.patch('atest.atest_utils.build')
    def test_locate_source_with_skip_build(self, mock_atest_utils_build,
                                           mock_module_info, mock_get_root,
                                           mock_batch):
        """Test locate_source handling."""
        mock_atest_utils_build.build.return_value = True
        test_root_path = os.path.join(tempfile.mkdtemp(), 'test')
        shutil.copytree(unittest_constants.TEST_DATA_PATH, test_root_path)
        mock_get_root.return_value = test_root_path
        generated_jar = ('out/soong/.intermediates/packages/apps/test/test/'
                         'android_common/generated.jar')
        locate_module_info = dict(unittest_constants.MODULE_INFO)
        locate_module_info['installed'] = [generated_jar]
        mock_module_info.is_module.return_value = True
        mock_module_info.get_paths.return_value = [
            unittest_constants.MODULE_PATH
        ]
        mock_module_info.get_module_names.return_value = [
            unittest_constants.TEST_MODULE
        ]
        args = mock.MagicMock()
        args.module_name = 'm1'
        args.project_path = ''
        args.ide = ['j']
        args.no_launch = True
        args.depth = 0
        args.android_tree = False
        args.skip_build = True
        args.targets = ['m1']
        args.verbose = False
        args.ide_installed_path = None
        args.config_reset = False
        project_config.ProjectConfig(args)
        project_info_obj = project_info.ProjectInfo(
            mock_module_info.get_paths()[0])
        project_info_obj.dep_modules = {
            unittest_constants.TEST_MODULE: locate_module_info
        }
        project_info_obj._init_source_path()
        project_info_obj.locate_source()
        self.assertFalse(mock_batch.called)

        args.ide = ['v']
        args.skip_build = False
        project_config.ProjectConfig(args)
        project_info_obj = project_info.ProjectInfo(
            mock_module_info.get_paths()[0])
        project_info_obj.dep_modules = {
            unittest_constants.TEST_MODULE: locate_module_info
        }
        project_info_obj._init_source_path()
        project_info_obj.locate_source()
        self.assertFalse(mock_batch.called)

    def test_separate_build_target(self):
        """Test separate_build_target."""
        test_list = ['1', '22', '333', '4444', '55555', '1', '7777777']
        targets = []
        sample = [['1', '22', '333'], ['4444'], ['55555', '1'], ['7777777']]
        for start, end in iter(
                project_info._separate_build_targets(test_list, 9)):
            targets.append(test_list[start:end])
        self.assertEqual(targets, sample)

    def test_separate_build_target_with_length_short(self):
        """Test separate_build_target with length short."""
        test_list = ['1']
        sample = [['1']]
        targets = []
        for start, end in iter(
                project_info._separate_build_targets(test_list, 9)):
            targets.append(test_list[start:end])
        self.assertEqual(targets, sample)

    @mock.patch.object(project_info.ProjectInfo, 'locate_source')
    @mock.patch('atest.module_info.ModuleInfo')
    def test_rebuild_jar_once(self, mock_module_info, mock_locate_source):
        """Test rebuild the jar/srcjar only one time."""
        mock_module_info.get_paths.return_value = ['m1']
        project_info.ProjectInfo.modules_info = mock_module_info
        proj_info = project_info.ProjectInfo(self.args.module_name, False)
        proj_info.locate_source(build=False)
        self.assertEqual(mock_locate_source.call_count, 1)
        proj_info.locate_source(build=True)
        self.assertEqual(mock_locate_source.call_count, 2)

    @mock.patch('builtins.print')
    @mock.patch('builtins.format')
    @mock.patch('atest.atest_utils.build')
    def test_build_target(self, mock_build, mock_format, mock_print):
        """Test _build_target."""
        build_argument = ['-k', 'j']
        test_targets = ['mod_1', 'mod_2']
        build_argument.extend(test_targets)
        mock_build.return_value = False
        project_info._build_target(test_targets)
        self.assertTrue(mock_build.called_with((build_argument, True)))
        self.assertTrue(mock_format.called_with('\n'.join(test_targets)))
        self.assertTrue(mock_print.called)
        mock_print.reset_mock()
        mock_format.reset_mock()
        mock_build.reset_mock()

        mock_build.return_value = True
        project_info._build_target(test_targets)
        self.assertTrue(mock_build.called_with((build_argument, True)))
        self.assertFalse(mock_format.called)
        self.assertFalse(mock_print.called)
        mock_print.reset_mock()
        mock_format.reset_mock()
        mock_build.reset_mock()

    @mock.patch('builtins.print')
    @mock.patch.object(project_info.ProjectInfo, '_search_android_make_files')
    @mock.patch('atest.module_info.ModuleInfo')
    def test_display_convert_make_files_message(
            self, mock_module_info, mock_search, mock_print):
        """Test _display_convert_make_files_message with conditions."""
        mock_search.return_value = []
        mock_module_info.get_paths.return_value = ['m1']
        project_info.ProjectInfo.modules_info = mock_module_info
        proj_info = project_info.ProjectInfo(self.args.module_name)
        proj_info._display_convert_make_files_message()
        self.assertFalse(mock_print.called)

        mock_print.mock_reset()
        mock_search.return_value = ['a/b/path/to/target.mk']
        proj_info = project_info.ProjectInfo(self.args.module_name)
        proj_info._display_convert_make_files_message()
        self.assertTrue(mock_print.called)

    @mock.patch.object(project_info, '_build_target')
    @mock.patch.object(project_info, '_separate_build_targets')
    @mock.patch.object(logging, 'info')
    def test_batch_build_dependencies(self, mock_log, mock_sep, mock_build):
        """Test batch_build_dependencies."""
        mock_sep.return_value = [(0, 1)]
        project_info.batch_build_dependencies({'m1', 'm2'})
        self.assertTrue(mock_log.called)
        self.assertTrue(mock_sep.called)
        self.assertEqual(mock_build.call_count, 1)


class MultiProjectsInfoUnittests(unittest.TestCase):
    """Unit tests for MultiProjectsInfo class."""

    @mock.patch.object(project_info.ProjectInfo, '__init__')
    @mock.patch.object(project_info.ProjectInfo, 'get_dep_modules')
    @mock.patch.object(project_info.ProjectInfo,
                       '_get_robolectric_dep_module')
    @mock.patch.object(project_info.ProjectInfo,
                       '_get_modules_under_project_path')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_collect_all_dep_modules(self, mock_relpath, mock_sub_modules_path,
                                     mock_robo_module, mock_get_dep_modules,
                                     mock_init):
        """Test _collect_all_dep_modules."""
        mock_init.return_value = None
        mock_relpath.return_value = ('path/to/sub/module', '')
        mock_sub_modules_path.return_value = 'sub_module'
        mock_robo_module.return_value = 'robo_module'
        expected = set(project_info._CORE_MODULES)
        expected.update({'sub_module', 'robo_module'})
        proj = project_info.MultiProjectsInfo(['a'])
        proj.collect_all_dep_modules()
        self.assertTrue(mock_get_dep_modules.called_with(expected))


if __name__ == '__main__':
    unittest.main()
