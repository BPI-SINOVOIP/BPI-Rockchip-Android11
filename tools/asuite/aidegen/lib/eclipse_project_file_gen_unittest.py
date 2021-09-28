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

"""Unittests for generate project file of Eclipse."""

import os
import unittest
from unittest import mock

from aidegen import unittest_constants
from aidegen.lib import common_util
from aidegen.lib import eclipse_project_file_gen
from aidegen.lib import project_info


# pylint: disable=protected-access
class EclipseConfUnittests(unittest.TestCase):
    """Unit tests for generate_project_file.py"""
    _ROOT_PATH = '/android/root'
    _PROJECT_RELPATH = 'module/path'
    _PROJECT_ABSPATH = os.path.join(_ROOT_PATH, _PROJECT_RELPATH)
    _PROJECT_SAMPLE = os.path.join(unittest_constants.TEST_DATA_PATH,
                                   'eclipse.project')

    def setUp(self):
        """Prepare the EclipseConf object."""
        with mock.patch.object(project_info, 'ProjectInfo') as self.proj_info:
            self.proj_info.project_absolute_path = self._PROJECT_ABSPATH
            self.proj_info.project_relative_path = self._PROJECT_RELPATH
            self.proj_info.module_name = 'test'
            self.eclipse = eclipse_project_file_gen.EclipseConf(self.proj_info)

    def tearDown(self):
        """Clear the EclipseConf object."""
        self.eclipse = None

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_link(self, mock_get_root):
        """Test get_link return a correct link resource config."""
        mock_get_root.return_value = '/a'
        expected_result = ('                <link><name>dependencies/b/c</name>'
                           '<type>2</type><location>/a/b/c</location></link>\n')
        self.assertEqual(self.eclipse._gen_link('b/c'), expected_result)

    @mock.patch('os.path.exists')
    @mock.patch.object(common_util, 'get_android_out_dir')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_create_project_content(self, mock_get_root, mock_out_dir,
                                    mock_dir_exists):
        """Test _create_project_content."""
        mock_get_root.return_value = self._ROOT_PATH
        mock_out_dir.return_value = os.path.join(self._ROOT_PATH, 'out')
        mock_dir_exists.return_value = True
        self.eclipse.jar_module_paths = {
            'relative/path/to/file1.jar': self._PROJECT_RELPATH,
            'relative/path/to/file2.jar': 'source/folder/of/jar',
        }
        expected_content = common_util.read_file_content(self._PROJECT_SAMPLE)
        self.eclipse._create_project_content()
        self.assertEqual(self.eclipse.project_content, expected_content)

    @mock.patch.object(common_util, 'exist_android_bp')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_src_path_entries(self, mock_get_root, mock_exist_android_bp):
        """Test generate source folders' class path entries."""
        mock_get_root.return_value = self._ROOT_PATH
        self.eclipse.src_paths = set([
            'module/path/src',
            'module/path/test',
            'out/src',
        ])
        expected_result = [
            '    <classpathentry kind="src" path="dependencies/out/src"/>\n',
            '    <classpathentry kind="src" path="src"/>\n',
            '    <classpathentry kind="src" path="test"/>\n',
        ]
        mock_exist_android_bp.return_value = False
        generated_result = sorted(self.eclipse._gen_src_path_entries())
        self.assertEqual(generated_result, expected_result)

        mock_exist_android_bp.return_value = True
        expected_result = [
            '    <classpathentry excluding="Android.bp" kind="src" '
            'path="dependencies/out/src"/>\n',
            '    <classpathentry excluding="Android.bp" kind="src" '
            'path="src"/>\n',
            '    <classpathentry excluding="Android.bp" kind="src" '
            'path="test"/>\n',
        ]
        generated_result = sorted(self.eclipse._gen_src_path_entries())
        self.assertEqual(generated_result, expected_result)


    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_jar_path_entries(self, mock_get_root):
        """Test generate jar files' class path entries."""
        mock_get_root.return_value = self._ROOT_PATH
        self.eclipse.jar_module_paths = {
            '/abspath/to/the/file.jar': 'relpath/to/the/module',
        }
        expected_result = [
            ('    <classpathentry exported="true" kind="lib" '
             'path="/abspath/to/the/file.jar" '
             'sourcepath="dependencies/relpath/to/the/module"/>\n')
        ]
        self.assertEqual(self.eclipse._gen_jar_path_entries(), expected_result)

    def test_get_other_src_folders(self):
        """Test _get_other_src_folders."""
        self.eclipse.src_paths = set([
            'module/path/src',
            'module/path/test',
            'out/module/path/src',
        ])
        expected_result = ['out/module/path/src']
        self.assertEqual(self.eclipse._get_other_src_folders(), expected_result)

    @mock.patch('os.makedirs')
    @mock.patch('os.path.exists')
    def test_gen_bin_link(self, mock_exists, mock_mkdir):
        """Test _gen_bin_link."""
        mock_exists.return_value = True
        self.eclipse._gen_bin_link()
        self.assertFalse(mock_mkdir.called)
        mock_exists.return_value = False
        self.eclipse._gen_bin_link()
        self.assertTrue(mock_mkdir.called)

    def test_gen_r_path_entries(self):
        """Test _gen_r_path_entries."""
        self.eclipse.r_java_paths = ['a/b', 'c/d']
        expected_result = [
            '    <classpathentry kind="src" path="dependencies/a/b"/>\n',
            '    <classpathentry kind="src" path="dependencies/c/d"/>\n',
        ]
        self.assertEqual(self.eclipse._gen_r_path_entries(), expected_result)

    def test_gen_bin_dir_entry(self):
        """Test _gen_bin_dir_entry."""
        expected_result = ['    <classpathentry kind="src" path="bin"/>\n']
        self.assertEqual(self.eclipse._gen_bin_dir_entry(), expected_result)

    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_gen_jar_path_entries')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_gen_bin_dir_entry')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_gen_r_path_entries')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_gen_src_path_entries')
    def test_create_classpath_content(self, mock_gen_src, mock_gen_r,
                                      mock_gen_bin, mock_gen_jar):
        """Test _create_classpath_content."""
        self.eclipse._create_classpath_content()
        self.assertTrue(mock_gen_src.called)
        self.assertTrue(mock_gen_r.called)
        self.assertTrue(mock_gen_bin.called)
        self.assertTrue(mock_gen_jar.called)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_create_project_content')
    def test_generate_project_file(self, mock_gen_content, mock_gen_file):
        """Test generate_project_file."""
        self.eclipse.generate_project_file()
        self.assertTrue(mock_gen_content.called)
        self.assertTrue(mock_gen_file.called)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       '_create_classpath_content')
    def test_generate_classpath_file(self, mock_gen_content, mock_gen_file):
        """Test generate_classpath_file."""
        self.eclipse.generate_classpath_file()
        self.assertTrue(mock_gen_content.called)
        self.assertTrue(mock_gen_file.called)

    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       'generate_classpath_file')
    @mock.patch.object(eclipse_project_file_gen.EclipseConf,
                       'generate_project_file')
    def test_generate_ide_project_files(self, mock_gen_project,
                                        mock_gen_classpath):
        """Test generate_ide_project_files."""
        self.eclipse.generate_ide_project_files([self.proj_info])
        self.assertTrue(mock_gen_project.called)
        self.assertTrue(mock_gen_classpath.called)


if __name__ == '__main__':
    unittest.main()
