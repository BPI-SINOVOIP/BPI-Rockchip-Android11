#!/usr/bin/env python3
#
# Copyright 2020 - The Android Open Source Project
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

"""Unittests for source_splitter."""

import os
import shutil
import tempfile
import unittest
from unittest import mock

from aidegen import unittest_constants
from aidegen.idea import iml
from aidegen.lib import common_util
from aidegen.lib import project_config
from aidegen.lib import project_info
from aidegen.project import source_splitter


# pylint: disable=protected-access
class ProjectSplitterUnittest(unittest.TestCase):
    """Unit tests for ProjectSplitter class."""

    _TEST_DIR = None
    _TEST_PATH = unittest_constants.TEST_DATA_PATH
    _SAMPLE_EXCLUDE_FOLDERS = [
        '\n            <excludeFolder url="file://%s/.idea" />' % _TEST_PATH,
        '\n            <excludeFolder url="file://%s/out" />' % _TEST_PATH,
    ]

    def setUp(self):
        """Prepare the testdata related data."""
        projects = []
        targets = ['a', 'b', 'c', 'framework']
        ProjectSplitterUnittest._TEST_DIR = tempfile.mkdtemp()
        for i, target in enumerate(targets):
            with mock.patch.object(project_info, 'ProjectInfo') as proj_info:
                projects.append(proj_info(target, i == 0))
        projects[0].project_relative_path = 'src1'
        projects[0].source_path = {
            'source_folder_path': {'src1', 'src2', 'other1'},
            'test_folder_path': {'src1/tests'},
            'jar_path': {'jar1.jar'},
            'jar_module_path': dict(),
            'r_java_path': set(),
            'srcjar_path': {'srcjar1.srcjar'}
        }
        projects[1].project_relative_path = 'src2'
        projects[1].source_path = {
            'source_folder_path': {'src2', 'src2/src3', 'src2/lib', 'other2'},
            'test_folder_path': {'src2/tests'},
            'jar_path': set(),
            'jar_module_path': dict(),
            'r_java_path': set(),
            'srcjar_path': {'srcjar2.srcjar'}
        }
        projects[2].project_relative_path = 'src2/src3'
        projects[2].source_path = {
            'source_folder_path': {'src2/src3', 'src2/lib'},
            'test_folder_path': {'src2/src3/tests'},
            'jar_path': {'jar3.jar'},
            'jar_module_path': dict(),
            'r_java_path': set(),
            'srcjar_path': {'srcjar3.srcjar'}
        }
        projects[3].project_relative_path = 'frameworks/base'
        projects[3].source_path = {
            'source_folder_path': set(),
            'test_folder_path': set(),
            'jar_path': set(),
            'jar_module_path': dict(),
            'r_java_path': set(),
            'srcjar_path': {'framework.srcjar', 'other.srcjar'}
        }
        with mock.patch.object(project_config.ProjectConfig,
                               'get_instance') as proj_cfg:
            config = mock.Mock()
            config.full_repo = False
            proj_cfg.return_value = config
            self.split_projs = source_splitter.ProjectSplitter(projects)

    def tearDown(self):
        """Clear the testdata related path."""
        self.split_projs = None
        shutil.rmtree(ProjectSplitterUnittest._TEST_DIR)
        iml.IMLGenerator.USED_NAME_CACHE.clear()

    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch.object(project_config.ProjectConfig, 'get_instance')
    @mock.patch('builtins.any')
    def test_init(self, mock_any, mock_project, mock_root):
        """Test initialize the attributes."""
        self.assertEqual(len(self.split_projs._projects), 4)
        mock_any.return_value = False
        mock_root.return_value = ProjectSplitterUnittest._TEST_DIR
        with mock.patch.object(project_info, 'ProjectInfo') as proj_info:
            config = mock.Mock()
            config.full_repo = False
            mock_project.return_value = config
            project = source_splitter.ProjectSplitter(proj_info(['a'], True))
            self.assertFalse(project._framework_exist)
            config.full_repo = True
            project = source_splitter.ProjectSplitter(proj_info(['a'], True))
            self.assertEqual(project._full_repo_iml,
                             os.path.basename(
                                 ProjectSplitterUnittest._TEST_DIR))

    @mock.patch.object(source_splitter.ProjectSplitter,
                       '_remove_duplicate_sources')
    @mock.patch.object(source_splitter.ProjectSplitter,
                       '_keep_local_sources')
    @mock.patch.object(source_splitter.ProjectSplitter,
                       '_collect_all_srcs')
    def test_revise_source_folders(self, mock_copy_srcs, mock_keep_srcs,
                                   mock_remove_srcs):
        """Test revise_source_folders."""
        self.split_projs.revise_source_folders()
        self.assertTrue(mock_copy_srcs.called)
        self.assertTrue(mock_keep_srcs.called)
        self.assertTrue(mock_remove_srcs.called)

    def test_collect_all_srcs(self):
        """Test _collect_all_srcs."""
        self.split_projs._collect_all_srcs()
        sources = self.split_projs._all_srcs
        expected_srcs = {'src1', 'src2', 'src2/src3', 'src2/lib', 'other1',
                         'other2'}
        self.assertEqual(sources['source_folder_path'], expected_srcs)
        expected_tests = {'src1/tests', 'src2/tests', 'src2/src3/tests'}
        self.assertEqual(sources['test_folder_path'], expected_tests)

    def test_keep_local_sources(self):
        """Test _keep_local_sources."""
        self.split_projs._collect_all_srcs()
        self.split_projs._keep_local_sources()
        srcs1 = self.split_projs._projects[0].source_path
        srcs2 = self.split_projs._projects[1].source_path
        srcs3 = self.split_projs._projects[2].source_path
        all_srcs = self.split_projs._all_srcs
        expected_srcs1 = {'src1'}
        expected_srcs2 = {'src2', 'src2/src3', 'src2/lib'}
        expected_srcs3 = {'src2/src3'}
        expected_all_srcs = {'other1', 'other2'}
        expected_all_tests = set()
        self.assertEqual(srcs1['source_folder_path'], expected_srcs1)
        self.assertEqual(srcs2['source_folder_path'], expected_srcs2)
        self.assertEqual(srcs3['source_folder_path'], expected_srcs3)
        self.assertEqual(all_srcs['source_folder_path'], expected_all_srcs)
        self.assertEqual(all_srcs['test_folder_path'], expected_all_tests)

    def test_remove_duplicate_sources(self):
        """Test _remove_duplicate_sources."""
        self.split_projs._collect_all_srcs()
        self.split_projs._keep_local_sources()
        self.split_projs._remove_duplicate_sources()
        srcs2 = self.split_projs._projects[1].source_path
        srcs3 = self.split_projs._projects[2].source_path
        expected_srcs2 = {'src2', 'src2/lib'}
        expected_srcs3 = {'src2/src3'}
        self.assertEqual(srcs2['source_folder_path'], expected_srcs2)
        self.assertEqual(srcs3['source_folder_path'], expected_srcs3)

    def test_get_dependencies(self):
        """Test get_dependencies."""
        iml.IMLGenerator.USED_NAME_CACHE.clear()
        self.split_projs.get_dependencies()
        dep1 = ['framework_srcjars', 'base', 'src2', 'dependencies']
        dep2 = ['framework_srcjars', 'base', 'dependencies']
        dep3 = ['framework_srcjars', 'base', 'src2', 'dependencies']
        self.assertEqual(self.split_projs._projects[0].dependencies, dep1)
        self.assertEqual(self.split_projs._projects[1].dependencies, dep2)
        self.assertEqual(self.split_projs._projects[2].dependencies, dep3)

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_framework_srcjars_iml(self, mock_root):
        """Test gen_framework_srcjars_iml."""
        mock_root.return_value = self._TEST_DIR
        self.split_projs._projects[0].dep_modules = {
            'framework-all': {
                'module_name': 'framework-all',
                'path': ['frameworks/base'],
                'srcjars': ['framework.srcjar'],
                'iml_name': 'framework_srcjars'
            }
        }
        self.split_projs._framework_exist = False
        self.split_projs.gen_framework_srcjars_iml()
        expected_srcjars = [
            'other.srcjar',
            'srcjar1.srcjar',
            'srcjar2.srcjar',
            'srcjar3.srcjar',
        ]
        expected_path = os.path.join(self._TEST_DIR,
                                     'frameworks/base/framework_srcjars.iml')
        self.split_projs._framework_exist = True
        self.split_projs.revise_source_folders()
        iml_path = self.split_projs.gen_framework_srcjars_iml()
        srcjars = self.split_projs._all_srcs['srcjar_path']
        self.assertEqual(sorted(list(srcjars)), expected_srcjars)
        self.assertEqual(iml_path, expected_path)

    @mock.patch.object(iml.IMLGenerator, 'create')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_dependencies_iml(self, mock_root, mock_create_iml):
        """Test _gen_dependencies_iml."""
        mock_root.return_value = self._TEST_DIR
        self.split_projs.revise_source_folders()
        self.split_projs._framework_exist = False
        self.split_projs._gen_dependencies_iml()
        self.split_projs._framework_exist = True
        self.split_projs._gen_dependencies_iml()
        self.assertTrue(mock_create_iml.called)

    @mock.patch.object(source_splitter, 'get_exclude_content')
    @mock.patch.object(project_config.ProjectConfig, 'get_instance')
    @mock.patch.object(iml.IMLGenerator, 'create')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_projects_iml(self, mock_root, mock_create_iml, mock_project,
                              mock_get_excludes):
        """Test gen_projects_iml."""
        mock_root.return_value = self._TEST_DIR
        config = mock.Mock()
        mock_project.return_value = config
        config.exclude_paths = []
        self.split_projs.revise_source_folders()
        self.split_projs.gen_projects_iml()
        self.assertTrue(mock_create_iml.called)
        self.assertFalse(mock_get_excludes.called)
        config.exclude_paths = ['a']
        self.split_projs.gen_projects_iml()
        self.assertTrue(mock_get_excludes.called)

    def test_get_exclude_content(self):
        """Test get_exclude_content."""
        exclude_folders = source_splitter.get_exclude_content(self._TEST_PATH)
        self.assertEqual(self._SAMPLE_EXCLUDE_FOLDERS, exclude_folders)


if __name__ == '__main__':
    unittest.main()
