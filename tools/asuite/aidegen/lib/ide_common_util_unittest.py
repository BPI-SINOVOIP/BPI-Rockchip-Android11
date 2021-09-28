#!/usr/bin/env python3
#
# Copyright 2019 - The Android Open Source Project
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


"""Unittests for ide_common_util."""

import os
import unittest

from unittest import mock

from aidegen import aidegen_main
from aidegen import constant
from aidegen import unittest_constants
from aidegen.lib import ide_common_util
from aidegen.lib import ide_util
from aidegen.lib import project_config


# pylint: disable=protected-access
class IdeUtilCommonUnittests(unittest.TestCase):
    """Unit tests for ide_util.py."""

    _TEST_PRJ_PATH1 = ''
    _TEST_PRJ_PATH2 = ''
    _TEST_PRJ_PATH3 = ''
    _TEST_PRJ_PATH4 = ''

    def setUp(self):
        """Prepare the testdata related path."""
        IdeUtilCommonUnittests._TEST_PRJ_PATH1 = os.path.join(
            unittest_constants.TEST_DATA_PATH, 'android_facet.iml')
        IdeUtilCommonUnittests._TEST_PRJ_PATH2 = os.path.join(
            unittest_constants.TEST_DATA_PATH, 'project/test.java')
        test_path = unittest_constants.TEST_DATA_PATH
        IdeUtilCommonUnittests._TEST_PRJ_PATH3 = test_path
        IdeUtilCommonUnittests._TEST_PRJ_PATH4 = os.path.join(
            unittest_constants.TEST_DATA_PATH, '.idea')

    def tearDown(self):
        """Clear the testdata related path."""
        IdeUtilCommonUnittests._TEST_PRJ_PATH1 = ''
        IdeUtilCommonUnittests._TEST_PRJ_PATH2 = ''
        IdeUtilCommonUnittests._TEST_PRJ_PATH3 = ''
        IdeUtilCommonUnittests._TEST_PRJ_PATH4 = ''

    def test_is_intellij_project(self):
        """Test _is_intellij_project."""
        self.assertFalse(
            ide_common_util.is_intellij_project(
                IdeUtilCommonUnittests._TEST_PRJ_PATH2))
        self.assertTrue(
            ide_common_util.is_intellij_project(
                IdeUtilCommonUnittests._TEST_PRJ_PATH1))
        self.assertTrue(
            ide_common_util.is_intellij_project(
                IdeUtilCommonUnittests._TEST_PRJ_PATH3))
        self.assertFalse(
            ide_common_util.is_intellij_project(
                IdeUtilCommonUnittests._TEST_PRJ_PATH4))

    @mock.patch('glob.glob', return_value=unittest_constants.IDEA_SH_FIND_NONE)
    def test_get_intellij_sh_none(self, mock_glob):
        """Test with the cmd return none, test result should be None."""
        mock_glob.return_value = unittest_constants.IDEA_SH_FIND_NONE
        args = aidegen_main._parse_args(['tradefed'])
        project_config.ProjectConfig(args)
        self.assertEqual(
            None,
            ide_common_util.get_intellij_version_path(
                ide_util.IdeLinuxIntelliJ()._ls_ce_path))
        self.assertEqual(
            None,
            ide_common_util.get_intellij_version_path(
                ide_util.IdeLinuxIntelliJ()._ls_ue_path))

    @mock.patch('builtins.input')
    @mock.patch('glob.glob', return_value=unittest_constants.IDEA_SH_FIND)
    def test_ask_preference(self, mock_glob, mock_input):
        """Ask users' preference, the result should be equal to test data."""
        mock_glob.return_value = unittest_constants.IDEA_SH_FIND
        mock_input.return_value = '1'
        self.assertEqual(
            ide_common_util.ask_preference(unittest_constants.IDEA_SH_FIND,
                                           constant.IDE_INTELLIJ),
            unittest_constants.IDEA_SH_FIND[0])
        mock_input.return_value = '2'
        self.assertEqual(
            ide_common_util.ask_preference(unittest_constants.IDEA_SH_FIND,
                                           constant.IDE_INTELLIJ),
            unittest_constants.IDEA_SH_FIND[1])

    def test_get_run_ide_cmd(self):
        """Test get_run_ide_cmd."""
        test_script_path = 'a/b/c/d.sh'
        test_project_path = 'xyz/.idea'
        test_result = ' '.join([
            constant.NOHUP, test_script_path, test_project_path,
            constant.IGNORE_STD_OUT_ERR_CMD
        ])
        self.assertEqual(test_result, ide_common_util.get_run_ide_cmd(
            test_script_path, test_project_path))

    @mock.patch('builtins.sorted')
    @mock.patch('glob.glob')
    def test_get_scripts_from_file_path(self, mock_list, mock_sort):
        """Test _get_scripts_from_file_path."""
        test_file = 'a/b/c/d.e'
        ide_common_util._get_scripts_from_file_path(test_file, 'd.e')
        mock_list.return_value = [test_file]
        self.assertTrue(mock_sort.called)
        mock_list.return_value = None
        self.assertEqual(ide_common_util._get_scripts_from_file_path(
            test_file, 'd.e'), None)

    @mock.patch('builtins.sorted')
    @mock.patch('builtins.list')
    def test_get_scripts_from_dir_path(self, mock_list, mock_sort):
        """Test get_scripts_from_dir_path."""
        test_path = 'a/b/c/d.e'
        mock_list.return_value = ['a', 'b', 'c']
        ide_common_util.get_scripts_from_dir_path(test_path, 'd.e')
        self.assertTrue(mock_sort.called)
        mock_list.return_value = []
        self.assertEqual(ide_common_util.get_scripts_from_dir_path(
            test_path, 'd.e'), None)


if __name__ == '__main__':
    unittest.main()
