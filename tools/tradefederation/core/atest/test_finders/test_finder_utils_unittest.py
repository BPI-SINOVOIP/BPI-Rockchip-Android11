#!/usr/bin/env python
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

"""Unittests for test_finder_utils."""

import os
import unittest
import mock

# pylint: disable=import-error
import atest_error
import constants
import module_info
import unittest_constants as uc
import unittest_utils
from test_finders import test_finder_utils

CLASS_DIR = 'foo/bar/jank/src/android/jank/cts/ui'
OTHER_DIR = 'other/dir/'
OTHER_CLASS_NAME = 'test.java'
CLASS_NAME3 = 'test2'
INT_DIR1 = os.path.join(uc.TEST_DATA_DIR, 'integration_dir_testing/int_dir1')
INT_DIR2 = os.path.join(uc.TEST_DATA_DIR, 'integration_dir_testing/int_dir2')
INT_FILE_NAME = 'int_dir_testing'
FIND_TWO = uc.ROOT + 'other/dir/test.java\n' + uc.FIND_ONE
FIND_THREE = '/a/b/c.java\n/d/e/f.java\n/g/h/i.java'
FIND_THREE_LIST = ['/a/b/c.java', '/d/e/f.java', '/g/h/i.java']
VTS_XML = 'VtsAndroidTest.xml'
VTS_BITNESS_XML = 'VtsBitnessAndroidTest.xml'
VTS_PUSH_DIR = 'vts_push_files'
VTS_PLAN_DIR = 'vts_plan_files'
VTS_XML_TARGETS = {'VtsTestName',
                   'DATA/nativetest/vts_treble_vintf_test/vts_treble_vintf_test',
                   'DATA/nativetest64/vts_treble_vintf_test/vts_treble_vintf_test',
                   'DATA/lib/libhidl-gen-hash.so',
                   'DATA/lib64/libhidl-gen-hash.so',
                   'hal-hidl-hash/frameworks/hardware/interfaces/current.txt',
                   'hal-hidl-hash/hardware/interfaces/current.txt',
                   'hal-hidl-hash/system/hardware/interfaces/current.txt',
                   'hal-hidl-hash/system/libhidl/transport/current.txt',
                   'target_with_delim',
                   'out/dir/target',
                   'push_file1_target1',
                   'push_file1_target2',
                   'push_file2_target1',
                   'push_file2_target2',
                   'CtsDeviceInfo.apk',
                   'DATA/app/DeviceHealthTests/DeviceHealthTests.apk',
                   'DATA/app/sl4a/sl4a.apk'}
VTS_PLAN_TARGETS = {os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-staging-default.xml'),
                    os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-aa.xml'),
                    os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-bb.xml'),
                    os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-cc.xml'),
                    os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-dd.xml')}
XML_TARGETS = {'CtsJankDeviceTestCases', 'perf-setup.sh', 'cts-tradefed',
               'GtsEmptyTestApp'}
PATH_TO_MODULE_INFO_WITH_AUTOGEN = {
    'foo/bar/jank' : [{'auto_test_config' : True}]}
PATH_TO_MODULE_INFO_WITH_MULTI_AUTOGEN = {
    'foo/bar/jank' : [{'auto_test_config' : True},
                      {'auto_test_config' : True}]}
PATH_TO_MODULE_INFO_WITH_MULTI_AUTOGEN_AND_ROBO = {
    'foo/bar' : [{'auto_test_config' : True},
                 {'auto_test_config' : True}],
    'foo/bar/jank': [{constants.MODULE_CLASS : [constants.MODULE_CLASS_ROBOLECTRIC]}]}

#pylint: disable=protected-access
class TestFinderUtilsUnittests(unittest.TestCase):
    """Unit tests for test_finder_utils.py"""

    def test_split_methods(self):
        """Test _split_methods method."""
        # Class
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('Class.Name'),
            ('Class.Name', set()))
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('Class.Name#Method'),
            ('Class.Name', {'Method'}))
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('Class.Name#Method,Method2'),
            ('Class.Name', {'Method', 'Method2'}))
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('Class.Name#Method,Method2'),
            ('Class.Name', {'Method', 'Method2'}))
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('Class.Name#Method,Method2'),
            ('Class.Name', {'Method', 'Method2'}))
        self.assertRaises(
            atest_error.TooManyMethodsError, test_finder_utils.split_methods,
            'class.name#Method,class.name.2#method')
        # Path
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('foo/bar/class.java'),
            ('foo/bar/class.java', set()))
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.split_methods('foo/bar/class.java#Method'),
            ('foo/bar/class.java', {'Method'}))

    @mock.patch.object(test_finder_utils, 'has_method_in_file',
                       return_value=False)
    @mock.patch('__builtin__.raw_input', return_value='1')
    def test_extract_test_path(self, _, has_method):
        """Test extract_test_dir method."""
        paths = [os.path.join(uc.ROOT, CLASS_DIR, uc.CLASS_NAME + '.java')]
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(uc.FIND_ONE), paths)
        paths = [os.path.join(uc.ROOT, CLASS_DIR, uc.CLASS_NAME + '.java')]
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(FIND_TWO), paths)
        has_method.return_value = True
        paths = [os.path.join(uc.ROOT, CLASS_DIR, uc.CLASS_NAME + '.java')]
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(uc.FIND_ONE, 'method'), paths)

    def test_has_method_in_file(self):
        """Test has_method_in_file method."""
        test_path = os.path.join(uc.TEST_DATA_DIR, 'class_file_path_testing',
                                 'hello_world_test.cc')
        self.assertTrue(test_finder_utils.has_method_in_file(
            test_path, frozenset(['PrintHelloWorld'])))
        self.assertFalse(test_finder_utils.has_method_in_file(
            test_path, frozenset(['PrintHelloWorld1'])))
        test_path = os.path.join(uc.TEST_DATA_DIR, 'class_file_path_testing',
                                 'hello_world_test.java')
        self.assertTrue(test_finder_utils.has_method_in_file(
            test_path, frozenset(['testMethod1'])))
        test_path = os.path.join(uc.TEST_DATA_DIR, 'class_file_path_testing',
                                 'hello_world_test.java')
        self.assertTrue(test_finder_utils.has_method_in_file(
            test_path, frozenset(['testMethod', 'testMethod2'])))
        test_path = os.path.join(uc.TEST_DATA_DIR, 'class_file_path_testing',
                                 'hello_world_test.java')
        self.assertFalse(test_finder_utils.has_method_in_file(
            test_path, frozenset(['testMethod'])))

    @mock.patch('__builtin__.raw_input', return_value='1')
    def test_extract_test_from_tests(self, mock_input):
        """Test method extract_test_from_tests method."""
        tests = []
        self.assertEquals(test_finder_utils.extract_test_from_tests(tests), None)
        paths = [os.path.join(uc.ROOT, CLASS_DIR, uc.CLASS_NAME + '.java')]
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(uc.FIND_ONE), paths)
        paths = [os.path.join(uc.ROOT, OTHER_DIR, OTHER_CLASS_NAME)]
        mock_input.return_value = '0'
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(FIND_TWO), paths)
        # Test inputing out-of-range integer or a string
        mock_input.return_value = '100'
        self.assertEquals(test_finder_utils.extract_test_from_tests(
            uc.CLASS_NAME), [])
        mock_input.return_value = 'lOO'
        self.assertEquals(test_finder_utils.extract_test_from_tests(
            uc.CLASS_NAME), [])

    @mock.patch('__builtin__.raw_input', return_value='1')
    def test_extract_test_from_multiselect(self, mock_input):
        """Test method extract_test_from_tests method."""
        # selecting 'All'
        paths = ['/a/b/c.java', '/d/e/f.java', '/g/h/i.java']
        mock_input.return_value = '3'
        unittest_utils.assert_strict_equal(
            self, sorted(test_finder_utils.extract_test_from_tests(
                FIND_THREE_LIST)), sorted(paths))
        # multi-select
        paths = ['/a/b/c.java', '/g/h/i.java']
        mock_input.return_value = '0,2'
        unittest_utils.assert_strict_equal(
            self, sorted(test_finder_utils.extract_test_from_tests(
                FIND_THREE_LIST)), sorted(paths))
        # selecting a range
        paths = ['/d/e/f.java', '/g/h/i.java']
        mock_input.return_value = '1-2'
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_from_tests(FIND_THREE_LIST), paths)
        # mixed formats
        paths = ['/a/b/c.java', '/d/e/f.java', '/g/h/i.java']
        mock_input.return_value = '0,1-2'
        unittest_utils.assert_strict_equal(
            self, sorted(test_finder_utils.extract_test_from_tests(
                FIND_THREE_LIST)), sorted(paths))
        # input unsupported formats, return empty
        paths = []
        mock_input.return_value = '?/#'
        unittest_utils.assert_strict_equal(
            self, test_finder_utils.extract_test_path(FIND_THREE), paths)

    @mock.patch('os.path.isdir')
    def test_is_equal_or_sub_dir(self, mock_isdir):
        """Test is_equal_or_sub_dir method."""
        self.assertTrue(test_finder_utils.is_equal_or_sub_dir('/a/b/c', '/'))
        self.assertTrue(test_finder_utils.is_equal_or_sub_dir('/a/b/c', '/a'))
        self.assertTrue(test_finder_utils.is_equal_or_sub_dir('/a/b/c',
                                                              '/a/b/c'))
        self.assertFalse(test_finder_utils.is_equal_or_sub_dir('/a/b',
                                                               '/a/b/c'))
        self.assertFalse(test_finder_utils.is_equal_or_sub_dir('/a', '/f'))
        mock_isdir.return_value = False
        self.assertFalse(test_finder_utils.is_equal_or_sub_dir('/a/b', '/a'))

    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile',
                side_effect=unittest_utils.isfile_side_effect)
    def test_find_parent_module_dir(self, _isfile, _isdir):
        """Test _find_parent_module_dir method."""
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.path_to_module_info = {}
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            uc.MODULE_DIR)

    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', return_value=False)
    def test_find_parent_module_dir_with_autogen_config(self, _isfile, _isdir):
        """Test _find_parent_module_dir method."""
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.path_to_module_info = PATH_TO_MODULE_INFO_WITH_AUTOGEN
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            uc.MODULE_DIR)

    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', side_effect=[False] * 5 + [True])
    def test_find_parent_module_dir_with_autogen_subconfig(self, _isfile, _isdir):
        """Test _find_parent_module_dir method.

        This case is testing when the auto generated config is in a
        sub-directory of a larger test that contains a test config in a parent
        directory.
        """
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.path_to_module_info = (
            PATH_TO_MODULE_INFO_WITH_MULTI_AUTOGEN)
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            uc.MODULE_DIR)

    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', return_value=False)
    def test_find_parent_module_dir_with_multi_autogens(self, _isfile, _isdir):
        """Test _find_parent_module_dir method.

        This case returns folders with multiple autogenerated configs defined.
        """
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.path_to_module_info = (
            PATH_TO_MODULE_INFO_WITH_MULTI_AUTOGEN)
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            uc.MODULE_DIR)

    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', return_value=False)
    def test_find_parent_module_dir_with_robo_and_autogens(self, _isfile,
                                                           _isdir):
        """Test _find_parent_module_dir method.

        This case returns folders with multiple autogenerated configs defined
        with a Robo test above them, which is the expected result.
        """
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.path_to_module_info = (
            PATH_TO_MODULE_INFO_WITH_MULTI_AUTOGEN_AND_ROBO)
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            uc.MODULE_DIR)


    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', return_value=False)
    def test_find_parent_module_dir_robo(self, _isfile, _isdir):
        """Test _find_parent_module_dir method.

        Make sure we behave as expected when we encounter a robo module path.
        """
        abs_class_dir = '/%s' % CLASS_DIR
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.is_robolectric_module.return_value = True
        rel_class_dir_path = os.path.relpath(abs_class_dir, uc.ROOT)
        mock_module_info.path_to_module_info = {rel_class_dir_path: [{}]}
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.find_parent_module_dir(uc.ROOT,
                                                     abs_class_dir,
                                                     mock_module_info),
            rel_class_dir_path)

    def test_get_targets_from_xml(self):
        """Test get_targets_from_xml method."""
        # Mocking Etree is near impossible, so use a real file, but mocking
        # ModuleInfo is still fine. Just have it return False when it finds a
        # module that states it's not a module.
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.is_module.side_effect = lambda module: (
            not module == 'is_not_module')
        xml_file = os.path.join(uc.TEST_DATA_DIR, constants.MODULE_CONFIG)
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.get_targets_from_xml(xml_file, mock_module_info),
            XML_TARGETS)

    @mock.patch.object(test_finder_utils, '_VTS_PUSH_DIR',
                       os.path.join(uc.TEST_DATA_DIR, VTS_PUSH_DIR))
    def test_get_targets_from_vts_xml(self):
        """Test get_targets_from_xml method."""
        # Mocking Etree is near impossible, so use a real file, but mock out
        # ModuleInfo,
        mock_module_info = mock.Mock(spec=module_info.ModuleInfo)
        mock_module_info.is_module.return_value = True
        xml_file = os.path.join(uc.TEST_DATA_DIR, VTS_XML)
        unittest_utils.assert_strict_equal(
            self,
            test_finder_utils.get_targets_from_vts_xml(xml_file, '',
                                                       mock_module_info),
            VTS_XML_TARGETS)

    @mock.patch('subprocess.check_output')
    def test_get_ignored_dirs(self, _mock_check_output):
        """Test _get_ignored_dirs method."""

        # Clean cached value for test.
        test_finder_utils._get_ignored_dirs.cached_ignore_dirs = []

        build_top = '/a/b'
        _mock_check_output.return_value = ('/a/b/c/.find-ignore\n'
                                           '/a/b/out/.out-dir\n'
                                           '/a/b/d/.out-dir\n\n')
        # Case 1: $OUT_DIR = ''. No customized out dir.
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: ''}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            correct_ignore_dirs = ['/a/b/c', '/a/b/out', '/a/b/d']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, correct_ignore_dirs)
        # Case 2: $OUT_DIR = 'out2'
        test_finder_utils._get_ignored_dirs.cached_ignore_dirs = []
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: 'out2'}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            correct_ignore_dirs = ['/a/b/c', '/a/b/out', '/a/b/d', '/a/b/out2']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, correct_ignore_dirs)
        # Case 3: The $OUT_DIR is abs dir but not under $ANDROID_BUILD_TOP
        test_finder_utils._get_ignored_dirs.cached_ignore_dirs = []
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: '/x/y/e/g'}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            correct_ignore_dirs = ['/a/b/c', '/a/b/out', '/a/b/d']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, correct_ignore_dirs)
        # Case 4: The $OUT_DIR is abs dir and under $ANDROID_BUILD_TOP
        test_finder_utils._get_ignored_dirs.cached_ignore_dirs = []
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: '/a/b/e/g'}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            correct_ignore_dirs = ['/a/b/c', '/a/b/out', '/a/b/d', '/a/b/e/g']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, correct_ignore_dirs)
        # Case 5: There is a file of '.out-dir' under $OUT_DIR.
        test_finder_utils._get_ignored_dirs.cached_ignore_dirs = []
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: 'out'}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            correct_ignore_dirs = ['/a/b/c', '/a/b/out', '/a/b/d']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, correct_ignore_dirs)
        # Case 6: Testing cache. All of the changes are useless.
        _mock_check_output.return_value = ('/a/b/X/.find-ignore\n'
                                           '/a/b/YY/.out-dir\n'
                                           '/a/b/d/.out-dir\n\n')
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top,
                           constants.ANDROID_OUT_DIR: 'new'}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            cached_answer = ['/a/b/c', '/a/b/out', '/a/b/d']
            none_cached_answer = ['/a/b/X', '/a/b/YY', '/a/b/d', 'a/b/new']
            ignore_dirs = test_finder_utils._get_ignored_dirs()
            self.assertEqual(ignore_dirs, cached_answer)
            self.assertNotEqual(ignore_dirs, none_cached_answer)

    @mock.patch('__builtin__.raw_input', return_value='0')
    def test_search_integration_dirs(self, mock_input):
        """Test search_integration_dirs."""
        mock_input.return_value = '0'
        paths = [os.path.join(uc.ROOT, INT_DIR1, INT_FILE_NAME+'.xml')]
        int_dirs = [INT_DIR1]
        test_result = test_finder_utils.search_integration_dirs(INT_FILE_NAME, int_dirs)
        unittest_utils.assert_strict_equal(self, test_result, paths)
        int_dirs = [INT_DIR1, INT_DIR2]
        test_result = test_finder_utils.search_integration_dirs(INT_FILE_NAME, int_dirs)
        unittest_utils.assert_strict_equal(self, test_result, paths)

    @mock.patch('os.path.isfile', return_value=False)
    @mock.patch('os.environ.get', return_value=uc.TEST_CONFIG_DATA_DIR)
    @mock.patch('__builtin__.raw_input', return_value='0')
    # pylint: disable=too-many-statements
    def test_find_class_file(self, mock_input, _mock_env, _mock_isfile):
        """Test find_class_file."""
        # 1. Java class(find).
        java_tmp_test_result = []
        mock_input.return_value = '0'
        java_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_TESTCASE_JAVA + '.java')
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      uc.FIND_PATH_TESTCASE_JAVA))
        mock_input.return_value = '1'
        kt_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_TESTCASE_JAVA + '.kt')
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      uc.FIND_PATH_TESTCASE_JAVA))
        self.assertTrue(java_class in java_tmp_test_result)
        self.assertTrue(kt_class in java_tmp_test_result)

        # 2. Java class(read index).
        del java_tmp_test_result[:]
        mock_input.return_value = '0'
        _mock_isfile = True
        test_finder_utils.FIND_INDEXES['CLASS'] = uc.CLASS_INDEX
        java_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_TESTCASE_JAVA + '.java')
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      uc.FIND_PATH_TESTCASE_JAVA))
        mock_input.return_value = '1'
        kt_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_TESTCASE_JAVA + '.kt')
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      uc.FIND_PATH_TESTCASE_JAVA))
        self.assertTrue(java_class in java_tmp_test_result)
        self.assertTrue(kt_class in java_tmp_test_result)

        # 3. Qualified Java class(find).
        del java_tmp_test_result[:]
        mock_input.return_value = '0'
        _mock_isfile = False
        java_qualified_class = '{0}.{1}'.format(uc.FIND_PATH_FOLDER, uc.FIND_PATH_TESTCASE_JAVA)
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      java_qualified_class))
        mock_input.return_value = '1'
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      java_qualified_class))
        self.assertTrue(java_class in java_tmp_test_result)
        self.assertTrue(kt_class in java_tmp_test_result)

        # 4. Qualified Java class(read index).
        del java_tmp_test_result[:]
        mock_input.return_value = '0'
        _mock_isfile = True
        test_finder_utils.FIND_INDEXES['QUALIFIED_CLASS'] = uc.QCLASS_INDEX
        java_qualified_class = '{0}.{1}'.format(uc.FIND_PATH_FOLDER, uc.FIND_PATH_TESTCASE_JAVA)
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      java_qualified_class))
        mock_input.return_value = '1'
        java_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                      java_qualified_class))
        self.assertTrue(java_class in java_tmp_test_result)
        self.assertTrue(kt_class in java_tmp_test_result)

        # 5. CC class(find).
        cc_tmp_test_result = []
        _mock_isfile = False
        mock_input.return_value = '0'
        cpp_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_FILENAME_CC + '.cpp')
        cc_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                    uc.FIND_PATH_TESTCASE_CC,
                                                                    True))
        mock_input.return_value = '1'
        cc_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_FILENAME_CC + '.cc')
        cc_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                    uc.FIND_PATH_TESTCASE_CC,
                                                                    True))
        self.assertTrue(cpp_class in cc_tmp_test_result)
        self.assertTrue(cc_class in cc_tmp_test_result)

        # 6. CC class(read index).
        del cc_tmp_test_result[:]
        mock_input.return_value = '0'
        _mock_isfile = True
        test_finder_utils.FIND_INDEXES['CC_CLASS'] = uc.CC_CLASS_INDEX
        cpp_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_FILENAME_CC + '.cpp')
        cc_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                    uc.FIND_PATH_TESTCASE_CC,
                                                                    True))
        mock_input.return_value = '1'
        cc_class = os.path.join(uc.FIND_PATH, uc.FIND_PATH_FILENAME_CC + '.cc')
        cc_tmp_test_result.extend(test_finder_utils.find_class_file(uc.FIND_PATH,
                                                                    uc.FIND_PATH_TESTCASE_CC,
                                                                    True))
        self.assertTrue(cpp_class in cc_tmp_test_result)
        self.assertTrue(cc_class in cc_tmp_test_result)

    @mock.patch('__builtin__.raw_input', return_value='0')
    @mock.patch.object(test_finder_utils, 'get_dir_path_and_filename')
    @mock.patch('os.path.exists', return_value=True)
    def test_get_int_dir_from_path(self, _exists, _find, mock_input):
        """Test get_int_dir_from_path."""
        mock_input.return_value = '0'
        int_dirs = [INT_DIR1]
        path = os.path.join(uc.ROOT, INT_DIR1, INT_FILE_NAME+'.xml')
        _find.return_value = (INT_DIR1, INT_FILE_NAME+'.xml')
        test_result = test_finder_utils.get_int_dir_from_path(path, int_dirs)
        unittest_utils.assert_strict_equal(self, test_result, INT_DIR1)
        _find.return_value = (INT_DIR1, None)
        test_result = test_finder_utils.get_int_dir_from_path(path, int_dirs)
        unittest_utils.assert_strict_equal(self, test_result, None)
        int_dirs = [INT_DIR1, INT_DIR2]
        _find.return_value = (INT_DIR1, INT_FILE_NAME+'.xml')
        test_result = test_finder_utils.get_int_dir_from_path(path, int_dirs)
        unittest_utils.assert_strict_equal(self, test_result, INT_DIR1)

    def test_get_install_locations(self):
        """Test get_install_locations."""
        host_installed_paths = ["out/host/a/b"]
        host_expect = set(['host'])
        self.assertEqual(test_finder_utils.get_install_locations(host_installed_paths),
                         host_expect)
        device_installed_paths = ["out/target/c/d"]
        device_expect = set(['device'])
        self.assertEqual(test_finder_utils.get_install_locations(device_installed_paths),
                         device_expect)
        both_installed_paths = ["out/host/e", "out/target/f"]
        both_expect = set(['host', 'device'])
        self.assertEqual(test_finder_utils.get_install_locations(both_installed_paths),
                         both_expect)
        no_installed_paths = []
        no_expect = set()
        self.assertEqual(test_finder_utils.get_install_locations(no_installed_paths),
                         no_expect)

    def test_get_plans_from_vts_xml(self):
        """Test get_plans_from_vts_xml method."""
        xml_path = os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'vts-staging-default.xml')
        self.assertEqual(
            test_finder_utils.get_plans_from_vts_xml(xml_path),
            VTS_PLAN_TARGETS)
        xml_path = os.path.join(uc.TEST_DATA_DIR, VTS_PLAN_DIR, 'NotExist.xml')
        self.assertRaises(atest_error.XmlNotExistError,
                          test_finder_utils.get_plans_from_vts_xml, xml_path)

    def test_get_levenshtein_distance(self):
        """Test get_levenshetine distance module correctly returns distance."""
        self.assertEqual(test_finder_utils.get_levenshtein_distance(uc.MOD1, uc.FUZZY_MOD1), 1)
        self.assertEqual(test_finder_utils.get_levenshtein_distance(uc.MOD2, uc.FUZZY_MOD2,
                                                                    dir_costs=(1, 2, 3)), 3)
        self.assertEqual(test_finder_utils.get_levenshtein_distance(uc.MOD3, uc.FUZZY_MOD3,
                                                                    dir_costs=(1, 2, 1)), 8)


if __name__ == '__main__':
    unittest.main()
