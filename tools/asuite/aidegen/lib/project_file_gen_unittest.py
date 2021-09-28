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

"""Unittests for project_file_gen."""

import logging
import os
import shutil
import unittest
from unittest import mock

from aidegen import unittest_constants
from aidegen.idea import xml_gen
from aidegen.lib import common_util
from aidegen.lib import config
from aidegen.lib import project_config
from aidegen.lib import project_file_gen
from aidegen.lib import project_info
from aidegen.project import source_splitter


# pylint: disable=protected-access
# pylint: disable-msg=too-many-arguments
class AidegenProjectFileGenUnittest(unittest.TestCase):
    """Unit tests for project_file_gen.py."""

    maxDiff = None
    _TEST_DATA_PATH = unittest_constants.TEST_DATA_PATH
    _ANDROID_PROJECT_PATH = unittest_constants.ANDROID_PROJECT_PATH
    _PROJECT_PATH = os.path.join(_TEST_DATA_PATH, 'project')
    _ANDROID_FACET_SAMPLE = os.path.join(_TEST_DATA_PATH, 'android_facet.iml')
    _PROJECT_FACET_SAMPLE = os.path.join(_TEST_DATA_PATH, 'project_facet.iml')
    _MODULE_DEP_SAMPLE = os.path.join(_TEST_DATA_PATH, 'module_dependency.iml')
    _IML_SAMPLE = os.path.join(_TEST_DATA_PATH, 'test.iml')
    _DEPENDENCIES_IML_SAMPLE = os.path.join(_TEST_DATA_PATH, 'dependencies.iml')
    _MODULE_XML_SAMPLE = os.path.join(_TEST_DATA_PATH, 'modules.xml')
    _MAIN_MODULE_XML_SAMPLE = os.path.join(_TEST_DATA_PATH,
                                           'modules_only_self_module.xml')
    _ENABLE_DEBUGGER_MODULE_SAMPLE = os.path.join(
        _TEST_DATA_PATH, 'modules_with_enable_debugger.xml')
    _IML_PATH = os.path.join(_ANDROID_PROJECT_PATH, 'android_project.iml')
    _DEPENDENCIES_IML_PATH = os.path.join(_ANDROID_PROJECT_PATH,
                                          'dependencies.iml')
    _IDEA_PATH = os.path.join(_ANDROID_PROJECT_PATH, '.idea')
    _MODULE_PATH = os.path.join(_IDEA_PATH, 'modules.xml')
    _SOURCE_SAMPLE = os.path.join(_TEST_DATA_PATH, 'source.iml')
    _SRCJAR_SAMPLE = os.path.join(_TEST_DATA_PATH, 'srcjar.iml')
    _AOSP_FOLDER = '/aosp'
    _TEST_SOURCE_LIST = [
        'a/b/c/d', 'a/b/c/d/e', 'a/b/c/d/e/f', 'a/b/c/d/f', 'e/f/a', 'e/f/b/c',
        'e/f/g/h'
    ]
    _ANDROID_SOURCE_RELATIVE_PATH = 'test_data/project'
    _SAMPLE_CONTENT_LIST = ['a/b/c/d', 'e/f']
    _SAMPLE_TRIMMED_SOURCE_LIST = ['a/b/c/d', 'e/f/a', 'e/f/b/c', 'e/f/g/h']

    def _init_project_config(self, args):
        """Initialize project configurations."""
        self.assertIsNotNone(args)
        pconfig = project_config.ProjectConfig(args)
        pconfig.init_environment()

    @mock.patch.object(project_config, 'ProjectConfig')
    @mock.patch.object(project_info, 'ProjectInfo')
    def test_generate_modules_xml(self, mock_project, mock_config):
        """Test _generate_modules_xml."""
        mock_config.is_launch_ide = True
        mock_project.project_absolute_path = self._ANDROID_PROJECT_PATH
        pfile_gen = project_file_gen.ProjectFileGenerator(mock_project)
        # Test for main project.
        try:
            pfile_gen._generate_modules_xml([])
            project_file_gen.update_enable_debugger(self._ANDROID_PROJECT_PATH)
            test_module = common_util.read_file_content(self._MODULE_PATH)
        finally:
            shutil.rmtree(self._IDEA_PATH)
        sample_module = common_util.read_file_content(self._MODULE_XML_SAMPLE)
        self.assertEqual(test_module, sample_module)

        # Test for sub projects which only has self module.
        try:
            pfile_gen._generate_modules_xml()
            project_file_gen.update_enable_debugger(self._ANDROID_PROJECT_PATH)
            test_module = common_util.read_file_content(self._MODULE_PATH)
        finally:
            shutil.rmtree(self._IDEA_PATH)
        sample_module = common_util.read_file_content(
            self._MAIN_MODULE_XML_SAMPLE)
        self.assertEqual(test_module, sample_module)

    @mock.patch.object(project_file_gen, '_get_all_git_path')
    @mock.patch.object(xml_gen, 'write_ignore_git_dirs_file')
    @mock.patch.object(xml_gen, 'gen_vcs_xml')
    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch.object(common_util, 'find_git_root')
    @mock.patch.object(project_info, 'ProjectInfo')
    def test_merge_project_vcs_xmls(self, mock_project, mock_get_git_root,
                                    mock_get_root, mock_write, mock_ignore_git,
                                    mock_all_git_path):
        """Test _merge_project_vcs_xmls."""
        mock_get_root.return_value = '/a/b'
        mock_project.project_absolute_path = '/a/b/c'
        mock_project.project_relative_path = 'c'
        mock_get_git_root.return_value = '/a/b/c'
        project_file_gen._merge_project_vcs_xmls([mock_project])
        self.assertTrue(mock_write.called_with('/a/b/c', '/a/b/c'))
        mock_project.project_absolute_path = '/a/b'
        mock_project.project_relative_path = None
        mock_get_git_root.return_value = None
        mock_all_git_path.return_value = ['/a', '/b']
        project_file_gen._merge_project_vcs_xmls([mock_project])
        self.assertTrue(mock_write.called_with('/a/b', [None]))
        self.assertTrue(mock_ignore_git.called_with('/a/b', ['/a', '/b']))

    @mock.patch.object(project_info, 'ProjectInfo')
    def test_copy_project_files(self, mock_project):
        """Test _copy_constant_project_files."""
        mock_project.project_absolute_path = self._ANDROID_PROJECT_PATH
        project_file_gen.ProjectFileGenerator(
            mock_project)._copy_constant_project_files()
        self.assertTrue(
            os.path.isfile(
                os.path.join(self._IDEA_PATH,
                             project_file_gen._CODE_STYLE_FOLDER,
                             'codeStyleConfig.xml')))
        self.assertTrue(
            os.path.isfile(
                os.path.join(self._IDEA_PATH,
                             project_file_gen._COPYRIGHT_FOLDER,
                             'Apache_2.xml')))
        self.assertTrue(
            os.path.isfile(
                os.path.join(self._IDEA_PATH,
                             project_file_gen._COPYRIGHT_FOLDER,
                             'profiles_settings.xml')))
        shutil.rmtree(self._IDEA_PATH)

    @mock.patch.object(logging, 'error')
    @mock.patch.object(os, 'symlink')
    @mock.patch.object(os.path, 'exists')
    def test_generate_git_ignore(self, mock_path_exist, mock_link,
                                 mock_loggin_error):
        """Test _generate_git_ignore."""
        mock_path_exist.return_value = True
        project_file_gen._generate_git_ignore(
            common_util.get_aidegen_root_dir())
        self.assertFalse(mock_link.called)

        # Test for creating symlink exception.
        mock_path_exist.return_value = False
        mock_link.side_effect = OSError()
        project_file_gen._generate_git_ignore(
            common_util.get_aidegen_root_dir())
        self.assertTrue(mock_loggin_error.called)

    def test_filter_out_source_paths(self):
        """Test _filter_out_source_paths."""
        test_set = {'a/a.java', 'b/b.java', 'c/c.java'}
        module_relpath = {'a', 'c'}
        expected_result = {'b/b.java'}
        result_set = project_file_gen._filter_out_source_paths(test_set,
                                                               module_relpath)
        self.assertEqual(result_set, expected_result)

    @mock.patch.object(project_config, 'ProjectConfig')
    @mock.patch.object(project_info, 'ProjectInfo')
    def test_update_enable_debugger(self, mock_project, mock_config):
        """Test update_enable_debugger."""
        mock_config.is_launch_ide = True
        enable_debugger_iml = '/path/to/enable_debugger/enable_debugger.iml'
        sample_module = common_util.read_file_content(
            self._ENABLE_DEBUGGER_MODULE_SAMPLE)
        mock_project.project_absolute_path = self._ANDROID_PROJECT_PATH
        pfile_gen = project_file_gen.ProjectFileGenerator(mock_project)
        try:
            pfile_gen._generate_modules_xml([])
            project_file_gen.update_enable_debugger(self._ANDROID_PROJECT_PATH,
                                                    enable_debugger_iml)
            test_module = common_util.read_file_content(self._MODULE_PATH)
            self.assertEqual(test_module, sample_module)
        finally:
            shutil.rmtree(self._IDEA_PATH)

    @mock.patch.object(common_util, 'find_git_root')
    @mock.patch.object(project_file_gen.ProjectFileGenerator,
                       '_generate_modules_xml')
    @mock.patch.object(project_info, 'ProjectInfo')
    def test_generate_intellij_project_file(self, mock_project,
                                            mock_gen_xml, mock_get_git_path):
        """Test generate_intellij_project_file."""
        mock_project.project_absolute_path = self._ANDROID_PROJECT_PATH
        mock_get_git_path.return_value = 'git/path'
        project_gen = project_file_gen.ProjectFileGenerator(mock_project)
        project_gen.project_info.is_main_project = False
        project_gen.generate_intellij_project_file()
        self.assertFalse(mock_gen_xml.called)
        project_gen.project_info.is_main_project = True
        project_gen.generate_intellij_project_file()
        self.assertTrue(mock_gen_xml.called)

    @mock.patch.object(os, 'walk')
    def test_get_all_git_path(self, mock_os_walk):
        """Test _get_all_git_path."""
        # Test .git folder exists.
        mock_os_walk.return_value = [('/root', ['.git', 'a'], None)]
        test_result = list(project_file_gen._get_all_git_path('/root'))
        expected_result = ['/root']
        self.assertEqual(test_result, expected_result)

        # Test .git folder does not exist.
        mock_os_walk.return_value = [('/root', ['a'], None)]
        test_result = list(project_file_gen._get_all_git_path('/root'))
        expected_result = []
        self.assertEqual(test_result, expected_result)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(os.path, 'isfile')
    def test_generate_test_mapping_schema(self, mock_is_file,
                                          mock_file_generate):
        """Test _generate_test_mapping_schema."""
        mock_is_file.return_value = False
        project_file_gen._generate_test_mapping_schema('')
        self.assertFalse(mock_file_generate.called)
        mock_is_file.return_value = True
        project_file_gen._generate_test_mapping_schema('')
        self.assertTrue(mock_file_generate.called)

    @mock.patch.object(project_file_gen, 'update_enable_debugger')
    @mock.patch.object(config.AidegenConfig, 'create_enable_debugger_module')
    def test_gen_enable_debugger_module(self, mock_create_module,
                                        mock_update_module):
        """Test gen_enable_debugger_module."""
        android_sdk_path = None
        project_file_gen.gen_enable_debugger_module('a', android_sdk_path)
        self.assertFalse(mock_create_module.called)
        mock_create_module.return_value = False
        project_file_gen.gen_enable_debugger_module('a', 'b')
        self.assertFalse(mock_update_module.called)
        mock_create_module.return_value = True
        project_file_gen.gen_enable_debugger_module('a', 'b')
        self.assertTrue(mock_update_module.called)

    @mock.patch.object(project_config.ProjectConfig, 'get_instance')
    @mock.patch.object(project_file_gen, '_merge_project_vcs_xmls')
    @mock.patch.object(project_file_gen.ProjectFileGenerator,
                       'generate_intellij_project_file')
    @mock.patch.object(source_splitter.ProjectSplitter, 'gen_projects_iml')
    @mock.patch.object(source_splitter.ProjectSplitter,
                       'gen_framework_srcjars_iml')
    @mock.patch.object(source_splitter.ProjectSplitter, 'revise_source_folders')
    @mock.patch.object(source_splitter.ProjectSplitter, 'get_dependencies')
    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch.object(project_info, 'ProjectInfo')
    def test_generate_ide_project_files(self, mock_project, mock_get_root,
                                        mock_get_dep, mock_revise_src,
                                        mock_gen_framework_srcjars,
                                        mock_gen_projects_iml, mock_gen_file,
                                        mock_merge_vcs, mock_project_config):
        """Test generate_ide_project_files."""
        mock_get_root.return_value = '/aosp'
        mock_project.project_absolute_path = '/aosp'
        mock_project.project_relative_path = ''
        project_cfg = mock.Mock()
        mock_project_config.return_value = project_cfg
        project_cfg.full_repo = True
        gen_proj = project_file_gen.ProjectFileGenerator
        gen_proj.generate_ide_project_files([mock_project])
        self.assertTrue(mock_get_dep.called)
        self.assertTrue(mock_revise_src.called)
        self.assertTrue(mock_gen_framework_srcjars.called)
        self.assertTrue(mock_gen_projects_iml.called)
        self.assertTrue(mock_gen_file.called)
        self.assertTrue(mock_merge_vcs.called)


if __name__ == '__main__':
    unittest.main()
