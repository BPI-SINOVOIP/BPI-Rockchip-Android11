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

"""Unittests for common_util."""

import logging
import os
import unittest
from unittest import mock
from xml.etree import ElementTree

from aidegen import constant
from aidegen import unittest_constants
from aidegen.lib import common_util
from aidegen.lib import errors

from atest import module_info


# pylint: disable=too-many-arguments
# pylint: disable=protected-access
class AidegenCommonUtilUnittests(unittest.TestCase):
    """Unit tests for common_util.py"""

    _TEST_XML_CONTENT = """<application><component name="ProjectJdkTable">

    <jdk version="2">     <name value="JDK_OTHER" />
      <type value="JavaSDK" />    </jdk>  </component>
</application>
"""
    _SAMPLE_XML_CONTENT = """<application>
  <component name="ProjectJdkTable">
    <jdk version="2">
      <name value="JDK_OTHER"/>
      <type value="JavaSDK"/>
    </jdk>
  </component>
</application>"""

    @mock.patch('os.getcwd')
    @mock.patch('os.path.isabs')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_related_paths(self, mock_get_root, mock_is_abspath, mock_cwd):
        """Test get_related_paths with different conditions."""
        mod_info = mock.MagicMock()
        mod_info.is_module.return_value = True
        mod_info.get_paths.return_value = {}
        mock_is_abspath.return_value = False
        self.assertEqual((None, None),
                         common_util.get_related_paths(
                             mod_info, unittest_constants.TEST_MODULE))
        mock_get_root.return_value = unittest_constants.TEST_PATH
        mod_info.get_paths.return_value = [unittest_constants.TEST_MODULE]
        expected = (unittest_constants.TEST_MODULE, os.path.join(
            unittest_constants.TEST_PATH, unittest_constants.TEST_MODULE))
        self.assertEqual(
            expected, common_util.get_related_paths(
                mod_info, unittest_constants.TEST_MODULE))
        mod_info.is_module.return_value = False
        mod_info.get_module_names.return_value = True
        self.assertEqual(expected, common_util.get_related_paths(
            mod_info, unittest_constants.TEST_MODULE))
        self.assertEqual(('', unittest_constants.TEST_PATH),
                         common_util.get_related_paths(
                             mod_info, constant.WHOLE_ANDROID_TREE_TARGET))

        mod_info.is_module.return_value = False
        mod_info.get_module_names.return_value = False
        mock_is_abspath.return_value = True
        mock_get_root.return_value = '/a'
        self.assertEqual(('b/c', '/a/b/c'),
                         common_util.get_related_paths(mod_info, '/a/b/c'))

        mock_is_abspath.return_value = False
        mock_cwd.return_value = '/a'
        mock_get_root.return_value = '/a'
        self.assertEqual(('b/c', '/a/b/c'),
                         common_util.get_related_paths(mod_info, 'b/c'))


    @mock.patch('os.getcwd')
    @mock.patch.object(common_util, 'is_android_root')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_related_paths_2(
            self, mock_get_root, mock_is_root, mock_getcwd):
        """Test get_related_paths with different conditions."""

        mock_get_root.return_value = '/a'
        mod_info = mock.MagicMock()

        # Test get_module_names returns False, user inputs a relative path of
        # current directory.
        mod_info.is_mod.return_value = False
        rel_path = 'b/c/d'
        abs_path = '/a/b/c/d'
        mod_info.get_paths.return_value = [rel_path]
        mod_info.get_module_names.return_value = False
        mock_getcwd.return_value = '/a/b/c'
        input_target = 'd'
        # expected tuple: (rel_path, abs_path)
        expected = (rel_path, abs_path)
        result = common_util.get_related_paths(mod_info, input_target)
        self.assertEqual(expected, result)

        # Test user doesn't input target and current working directory is the
        # android root folder.
        mock_getcwd.return_value = '/a'
        mock_is_root.return_value = True
        expected = ('', '/a')
        result = common_util.get_related_paths(mod_info, target=None)
        self.assertEqual(expected, result)

        # Test user doesn't input target and current working directory is not
        # android root folder.
        mock_getcwd.return_value = '/a/b'
        mock_is_root.return_value = False
        expected = ('b', '/a/b')
        result = common_util.get_related_paths(mod_info, target=None)
        self.assertEqual(expected, result)
        result = common_util.get_related_paths(mod_info, target='.')
        self.assertEqual(expected, result)

    @mock.patch.object(common_util, 'is_android_root')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_is_target_android_root(self, mock_get_rel, mock_get_root):
        """Test is_target_android_root with different conditions."""
        mod_info = mock.MagicMock()
        mock_get_rel.return_value = None, unittest_constants.TEST_PATH
        mock_get_root.return_value = True
        self.assertTrue(
            common_util.is_target_android_root(
                mod_info, [unittest_constants.TEST_MODULE]))
        mock_get_rel.return_value = None, ''
        mock_get_root.return_value = False
        self.assertFalse(
            common_util.is_target_android_root(
                mod_info, [unittest_constants.TEST_MODULE]))

    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch.object(common_util, 'has_build_target')
    @mock.patch('os.path.isdir')
    @mock.patch.object(common_util, 'get_related_paths')
    def test_check_module(self, mock_get, mock_isdir, mock_has_target,
                          mock_get_root):
        """Test if _check_module raises errors with different conditions."""
        mod_info = mock.MagicMock()
        mock_get.return_value = None, None
        with self.assertRaises(errors.FakeModuleError) as ctx:
            common_util.check_module(mod_info, unittest_constants.TEST_MODULE)
            expected = common_util.FAKE_MODULE_ERROR.format(
                unittest_constants.TEST_MODULE)
            self.assertEqual(expected, str(ctx.exception))
        mock_get_root.return_value = unittest_constants.TEST_PATH
        mock_get.return_value = None, unittest_constants.TEST_MODULE
        with self.assertRaises(errors.ProjectOutsideAndroidRootError) as ctx:
            common_util.check_module(mod_info, unittest_constants.TEST_MODULE)
            expected = common_util.OUTSIDE_ROOT_ERROR.format(
                unittest_constants.TEST_MODULE)
            self.assertEqual(expected, str(ctx.exception))
        mock_get.return_value = None, unittest_constants.TEST_PATH
        mock_isdir.return_value = False
        with self.assertRaises(errors.ProjectPathNotExistError) as ctx:
            common_util.check_module(mod_info, unittest_constants.TEST_MODULE)
            expected = common_util.PATH_NOT_EXISTS_ERROR.format(
                unittest_constants.TEST_MODULE)
            self.assertEqual(expected, str(ctx.exception))
        mock_isdir.return_value = True
        mock_has_target.return_value = False
        mock_get.return_value = None, os.path.join(unittest_constants.TEST_PATH,
                                                   'test.jar')
        with self.assertRaises(errors.NoModuleDefinedInModuleInfoError) as ctx:
            common_util.check_module(mod_info, unittest_constants.TEST_MODULE)
            expected = common_util.NO_MODULE_DEFINED_ERROR.format(
                unittest_constants.TEST_MODULE)
            self.assertEqual(expected, str(ctx.exception))
        self.assertFalse(common_util.check_module(mod_info, '', False))
        self.assertFalse(common_util.check_module(mod_info, 'nothing', False))

    @mock.patch.object(common_util, 'check_module')
    def test_check_modules(self, mock_check):
        """Test _check_modules with different module lists."""
        mod_info = mock.MagicMock()
        common_util._check_modules(mod_info, [])
        self.assertEqual(mock_check.call_count, 0)
        common_util._check_modules(mod_info, ['module1', 'module2'])
        self.assertEqual(mock_check.call_count, 2)
        target = 'nothing'
        mock_check.return_value = False
        self.assertFalse(common_util._check_modules(mod_info, [target], False))

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_abs_path(self, mock_get_root):
        """Test get_abs_path handling."""
        mock_get_root.return_value = unittest_constants.TEST_DATA_PATH
        self.assertEqual(unittest_constants.TEST_DATA_PATH,
                         common_util.get_abs_path(''))
        test_path = os.path.join(unittest_constants.TEST_DATA_PATH, 'test.jar')
        self.assertEqual(test_path, common_util.get_abs_path(test_path))
        self.assertEqual(test_path, common_util.get_abs_path('test.jar'))

    def test_is_target(self):
        """Test is_target handling."""
        self.assertTrue(
            common_util.is_target('packages/apps/tests/test.a', ['.so', '.a']))
        self.assertTrue(
            common_util.is_target('packages/apps/tests/test.so', ['.so', '.a']))
        self.assertFalse(
            common_util.is_target(
                'packages/apps/tests/test.jar', ['.so', '.a']))

    @mock.patch.object(logging, 'basicConfig')
    def test_configure_logging(self, mock_log_config):
        """Test configure_logging with different arguments."""
        common_util.configure_logging(True)
        log_format = common_util._LOG_FORMAT
        datefmt = common_util._DATE_FORMAT
        level = common_util.logging.DEBUG
        self.assertTrue(
            mock_log_config.called_with(
                level=level, format=log_format, datefmt=datefmt))
        common_util.configure_logging(False)
        level = common_util.logging.INFO
        self.assertTrue(
            mock_log_config.called_with(
                level=level, format=log_format, datefmt=datefmt))

    @mock.patch.object(common_util, '_check_modules')
    @mock.patch.object(module_info, 'ModuleInfo')
    def test_get_atest_module_info(self, mock_modinfo, mock_check_modules):
        """Test get_atest_module_info handling."""
        common_util.get_atest_module_info()
        self.assertEqual(mock_modinfo.call_count, 1)
        mock_modinfo.reset_mock()
        mock_check_modules.return_value = False
        common_util.get_atest_module_info(['nothing'])
        self.assertEqual(mock_modinfo.call_count, 2)

    @mock.patch('builtins.open', create=True)
    def test_read_file_content(self, mock_open):
        """Test read_file_content handling."""
        expacted_data1 = 'Data1'
        file_a = 'fileA'
        mock_open.side_effect = [
            mock.mock_open(read_data=expacted_data1).return_value
        ]
        self.assertEqual(expacted_data1, common_util.read_file_content(file_a))
        mock_open.assert_called_once_with(file_a, encoding='utf8')

    @mock.patch('os.getenv')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_android_out_dir(self, mock_get_android_root_dir, mock_getenv):
        """Test get_android_out_dir handling."""
        root = 'my/path-to-root/master'
        default_root = 'out'
        android_out_root = 'eng_out'
        mock_get_android_root_dir.return_value = root
        mock_getenv.side_effect = ['', '']
        self.assertEqual(default_root, common_util.get_android_out_dir())
        mock_getenv.side_effect = [android_out_root, '']
        self.assertEqual(android_out_root, common_util.get_android_out_dir())
        mock_getenv.side_effect = ['', default_root]
        self.assertEqual(os.path.join(default_root, os.path.basename(root)),
                         common_util.get_android_out_dir())
        mock_getenv.side_effect = [android_out_root, default_root]
        self.assertEqual(android_out_root, common_util.get_android_out_dir())

    def test_has_build_target(self):
        """Test has_build_target handling."""
        mod_info = mock.MagicMock()
        mod_info.path_to_module_info = {'a/b/c': {}}
        rel_path = 'a/b'
        self.assertTrue(common_util.has_build_target(mod_info, rel_path))
        rel_path = 'd/e'
        self.assertFalse(common_util.has_build_target(mod_info, rel_path))

    @mock.patch('os.path.expanduser')
    def test_remove_user_home_path(self, mock_expanduser):
        """ Test replace the user home path to a constant string."""
        mock_expanduser.return_value = '/usr/home/a'
        test_string = '/usr/home/a/test/dir'
        expect_string = '$USER_HOME$/test/dir'
        result_path = common_util.remove_user_home_path(test_string)
        self.assertEqual(result_path, expect_string)

    def test_io_error_handle(self):
        """Test io_error_handle handling."""
        err = "It's an IO error."
        def some_io_error_func():
            raise IOError(err)
        with self.assertRaises(IOError) as context:
            decorator = common_util.io_error_handle(some_io_error_func)
            decorator()
            self.assertTrue(err in context.exception)

    @mock.patch.object(common_util, '_show_env_setup_msg_and_exit')
    @mock.patch('os.environ.get')
    def test_get_android_root_dir(self, mock_get_env, mock_show_msg):
        """Test get_android_root_dir handling."""
        root = 'root'
        mock_get_env.return_value = root
        expected = common_util.get_android_root_dir()
        self.assertEqual(root, expected)
        root = ''
        mock_get_env.return_value = root
        common_util.get_android_root_dir()
        self.assertTrue(mock_show_msg.called)

    # pylint: disable=no-value-for-parameter
    def test_check_args(self):
        """Test check_args handling."""
        with self.assertRaises(TypeError):
            decorator = common_util.check_args(name=str, text=str)
            decorator(parse_rule(None, 'text'))
        with self.assertRaises(TypeError):
            decorator = common_util.check_args(name=str, text=str)
            decorator(parse_rule('Paul', ''))
        with self.assertRaises(TypeError):
            decorator = common_util.check_args(name=str, text=str)
            decorator(parse_rule(1, 2))

    @mock.patch.object(common_util, 'get_blueprint_json_path')
    @mock.patch.object(common_util, 'get_android_out_dir')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_get_blueprint_json_files_relative_dict(
            self, mock_get_root, mock_get_out, mock_get_path):
        """Test get_blueprint_json_files_relative_dict function,"""
        mock_get_root.return_value = 'a/b'
        mock_get_out.return_value = 'out'
        mock_get_path.return_value = 'out/soong/bp_java_file'
        path_compdb = os.path.join('a/b', 'out', 'soong',
                                   constant.RELATIVE_COMPDB_PATH,
                                   constant.COMPDB_JSONFILE_NAME)
        data = {
            constant.GEN_JAVA_DEPS: 'a/b/out/soong/bp_java_file',
            constant.GEN_CC_DEPS: 'a/b/out/soong/bp_java_file',
            constant.GEN_COMPDB: path_compdb
        }
        self.assertEqual(
            data, common_util.get_blueprint_json_files_relative_dict())

    @mock.patch('os.environ.get')
    def test_get_lunch_target(self, mock_get_env):
        """Test get_lunch_target."""
        mock_get_env.return_value = "test"
        self.assertEqual(
            common_util.get_lunch_target(), '{"lunch target": "test-test"}')

    def test_to_pretty_xml(self):
        """Test to_pretty_xml."""
        root = ElementTree.fromstring(self._TEST_XML_CONTENT)
        pretty_xml = common_util.to_pretty_xml(root)
        self.assertEqual(pretty_xml, self._SAMPLE_XML_CONTENT)

    def test_to_to_boolean(self):
        """Test to_boolean function with conditions."""
        self.assertTrue(common_util.to_boolean('True'))
        self.assertTrue(common_util.to_boolean('true'))
        self.assertTrue(common_util.to_boolean('T'))
        self.assertTrue(common_util.to_boolean('t'))
        self.assertTrue(common_util.to_boolean('1'))
        self.assertFalse(common_util.to_boolean('False'))
        self.assertFalse(common_util.to_boolean('false'))
        self.assertFalse(common_util.to_boolean('F'))
        self.assertFalse(common_util.to_boolean('f'))
        self.assertFalse(common_util.to_boolean('0'))
        self.assertFalse(common_util.to_boolean(''))

    @mock.patch.object(os.path, 'exists')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_find_git_root(self, mock_get_root, mock_exist):
        """Test find_git_root."""
        mock_get_root.return_value = '/a/b'
        mock_exist.return_value = True
        self.assertEqual(common_util.find_git_root('c/d'), '/a/b/c/d')
        mock_exist.return_value = False
        self.assertEqual(common_util.find_git_root('c/d'), None)


# pylint: disable=unused-argument
def parse_rule(self, name, text):
    """A test function for test_check_args."""


if __name__ == '__main__':
    unittest.main()
