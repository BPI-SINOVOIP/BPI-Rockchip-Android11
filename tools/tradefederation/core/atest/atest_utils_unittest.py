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

"""Unittests for atest_utils."""

import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
import mock

import atest_error
import atest_utils
import constants
import unittest_utils
from test_finders import test_info

if sys.version_info[0] == 2:
    from StringIO import StringIO
else:
    from io import StringIO

TEST_MODULE_NAME_A = 'ModuleNameA'
TEST_RUNNER_A = 'FakeTestRunnerA'
TEST_BUILD_TARGET_A = set(['bt1', 'bt2'])
TEST_DATA_A = {'test_data_a_1': 'a1',
               'test_data_a_2': 'a2'}
TEST_SUITE_A = 'FakeSuiteA'
TEST_MODULE_CLASS_A = 'FAKE_MODULE_CLASS_A'
TEST_INSTALL_LOC_A = set(['host', 'device'])
TEST_FINDER_A = 'MODULE'
TEST_INFO_A = test_info.TestInfo(TEST_MODULE_NAME_A, TEST_RUNNER_A,
                                 TEST_BUILD_TARGET_A, TEST_DATA_A,
                                 TEST_SUITE_A, TEST_MODULE_CLASS_A,
                                 TEST_INSTALL_LOC_A)
TEST_INFO_A.test_finder = TEST_FINDER_A

#pylint: disable=protected-access
class AtestUtilsUnittests(unittest.TestCase):
    """Unit tests for atest_utils.py"""

    def test_capture_fail_section_has_fail_section(self):
        """Test capture_fail_section when has fail section."""
        test_list = ['AAAAAA', 'FAILED: Error1', '^\n', 'Error2\n',
                     '[  6% 191/2997] BBBBBB\n', 'CCCCC',
                     '[  20% 322/2997] DDDDDD\n', 'EEEEE']
        want_list = ['FAILED: Error1', '^\n', 'Error2\n']
        self.assertEqual(want_list,
                         atest_utils._capture_fail_section(test_list))

    def test_capture_fail_section_no_fail_section(self):
        """Test capture_fail_section when no fail section."""
        test_list = ['[ 6% 191/2997] XXXXX', 'YYYYY: ZZZZZ']
        want_list = []
        self.assertEqual(want_list,
                         atest_utils._capture_fail_section(test_list))

    def test_is_test_mapping(self):
        """Test method is_test_mapping."""
        tm_option_attributes = [
            'test_mapping',
            'include_subdirs'
        ]
        for attr_to_test in tm_option_attributes:
            args = mock.Mock()
            for attr in tm_option_attributes:
                setattr(args, attr, attr == attr_to_test)
            args.tests = []
            self.assertTrue(
                atest_utils.is_test_mapping(args),
                'Failed to validate option %s' % attr_to_test)

        args = mock.Mock()
        for attr in tm_option_attributes:
            setattr(args, attr, False)
        args.tests = [':group_name']
        self.assertTrue(atest_utils.is_test_mapping(args))

        args = mock.Mock()
        for attr in tm_option_attributes:
            setattr(args, attr, False)
        args.tests = [':test1', 'test2']
        self.assertFalse(atest_utils.is_test_mapping(args))

        args = mock.Mock()
        for attr in tm_option_attributes:
            setattr(args, attr, False)
        args.tests = ['test2']
        self.assertFalse(atest_utils.is_test_mapping(args))

    @mock.patch('curses.tigetnum')
    def test_has_colors(self, mock_curses_tigetnum):
        """Test method _has_colors."""
        # stream is file I/O
        stream = open('/tmp/test_has_colors.txt', 'wb')
        self.assertFalse(atest_utils._has_colors(stream))
        stream.close()

        # stream is not a tty(terminal).
        stream = mock.Mock()
        stream.isatty.return_value = False
        self.assertFalse(atest_utils._has_colors(stream))

        # stream is a tty(terminal) and colors < 2.
        stream = mock.Mock()
        stream.isatty.return_value = True
        mock_curses_tigetnum.return_value = 1
        self.assertFalse(atest_utils._has_colors(stream))

        # stream is a tty(terminal) and colors > 2.
        stream = mock.Mock()
        stream.isatty.return_value = True
        mock_curses_tigetnum.return_value = 256
        self.assertTrue(atest_utils._has_colors(stream))


    @mock.patch('atest_utils._has_colors')
    def test_colorize(self, mock_has_colors):
        """Test method colorize."""
        original_str = "test string"
        green_no = 2

        # _has_colors() return False.
        mock_has_colors.return_value = False
        converted_str = atest_utils.colorize(original_str, green_no,
                                             highlight=True)
        self.assertEqual(original_str, converted_str)

        # Green with highlight.
        mock_has_colors.return_value = True
        converted_str = atest_utils.colorize(original_str, green_no,
                                             highlight=True)
        green_highlight_string = '\x1b[1;42m%s\x1b[0m' % original_str
        self.assertEqual(green_highlight_string, converted_str)

        # Green, no highlight.
        mock_has_colors.return_value = True
        converted_str = atest_utils.colorize(original_str, green_no,
                                             highlight=False)
        green_no_highlight_string = '\x1b[1;32m%s\x1b[0m' % original_str
        self.assertEqual(green_no_highlight_string, converted_str)


    @mock.patch('atest_utils._has_colors')
    def test_colorful_print(self, mock_has_colors):
        """Test method colorful_print."""
        testing_str = "color_print_test"
        green_no = 2

        # _has_colors() return False.
        mock_has_colors.return_value = False
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.colorful_print(testing_str, green_no, highlight=True,
                                   auto_wrap=False)
        sys.stdout = sys.__stdout__
        uncolored_string = testing_str
        self.assertEqual(capture_output.getvalue(), uncolored_string)

        # Green with highlight, but no wrap.
        mock_has_colors.return_value = True
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.colorful_print(testing_str, green_no, highlight=True,
                                   auto_wrap=False)
        sys.stdout = sys.__stdout__
        green_highlight_no_wrap_string = '\x1b[1;42m%s\x1b[0m' % testing_str
        self.assertEqual(capture_output.getvalue(),
                         green_highlight_no_wrap_string)

        # Green, no highlight, no wrap.
        mock_has_colors.return_value = True
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.colorful_print(testing_str, green_no, highlight=False,
                                   auto_wrap=False)
        sys.stdout = sys.__stdout__
        green_no_high_no_wrap_string = '\x1b[1;32m%s\x1b[0m' % testing_str
        self.assertEqual(capture_output.getvalue(),
                         green_no_high_no_wrap_string)

        # Green with highlight and wrap.
        mock_has_colors.return_value = True
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.colorful_print(testing_str, green_no, highlight=True,
                                   auto_wrap=True)
        sys.stdout = sys.__stdout__
        green_highlight_wrap_string = '\x1b[1;42m%s\x1b[0m\n' % testing_str
        self.assertEqual(capture_output.getvalue(), green_highlight_wrap_string)

        # Green with wrap, but no highlight.
        mock_has_colors.return_value = True
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.colorful_print(testing_str, green_no, highlight=False,
                                   auto_wrap=True)
        sys.stdout = sys.__stdout__
        green_wrap_no_highlight_string = '\x1b[1;32m%s\x1b[0m\n' % testing_str
        self.assertEqual(capture_output.getvalue(),
                         green_wrap_no_highlight_string)

    @mock.patch('socket.gethostname')
    @mock.patch('subprocess.check_output')
    def test_is_external_run(self, mock_output, mock_hostname):
        """Test method is_external_run."""
        mock_output.return_value = ''
        mock_hostname.return_value = ''
        self.assertTrue(atest_utils.is_external_run())

        mock_output.return_value = 'test@other.com'
        mock_hostname.return_value = 'abc.com'
        self.assertTrue(atest_utils.is_external_run())

        mock_output.return_value = 'test@other.com'
        mock_hostname.return_value = 'abc.google.com'
        self.assertFalse(atest_utils.is_external_run())

        mock_output.return_value = 'test@other.com'
        mock_hostname.return_value = 'abc.google.def.com'
        self.assertTrue(atest_utils.is_external_run())

        mock_output.return_value = 'test@google.com'
        self.assertFalse(atest_utils.is_external_run())

        mock_output.return_value = 'test@other.com'
        mock_hostname.return_value = 'c.googlers.com'
        self.assertFalse(atest_utils.is_external_run())

        mock_output.return_value = 'test@other.com'
        mock_hostname.return_value = 'a.googlers.com'
        self.assertTrue(atest_utils.is_external_run())

        mock_output.side_effect = OSError()
        self.assertTrue(atest_utils.is_external_run())

        mock_output.side_effect = subprocess.CalledProcessError(1, 'cmd')
        self.assertTrue(atest_utils.is_external_run())

    @mock.patch('metrics.metrics_base.get_user_type')
    def test_print_data_collection_notice(self, mock_get_user_type):
        """Test method print_data_collection_notice."""

        # get_user_type return 1(external).
        mock_get_user_type.return_value = 1
        notice_str = ('\n==================\nNotice:\n'
                      '  We collect anonymous usage statistics'
                      ' in accordance with our'
                      ' Content Licenses (https://source.android.com/setup/start/licenses),'
                      ' Contributor License Agreement (https://opensource.google.com/docs/cla/),'
                      ' Privacy Policy (https://policies.google.com/privacy) and'
                      ' Terms of Service (https://policies.google.com/terms).'
                      '\n==================\n\n')
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.print_data_collection_notice()
        sys.stdout = sys.__stdout__
        uncolored_string = notice_str
        self.assertEqual(capture_output.getvalue(), uncolored_string)

        # get_user_type return 0(internal).
        mock_get_user_type.return_value = 0
        notice_str = ('\n==================\nNotice:\n'
                      '  We collect usage statistics'
                      ' in accordance with our'
                      ' Content Licenses (https://source.android.com/setup/start/licenses),'
                      ' Contributor License Agreement (https://cla.developers.google.com/),'
                      ' Privacy Policy (https://policies.google.com/privacy) and'
                      ' Terms of Service (https://policies.google.com/terms).'
                      '\n==================\n\n')
        capture_output = StringIO()
        sys.stdout = capture_output
        atest_utils.print_data_collection_notice()
        sys.stdout = sys.__stdout__
        uncolored_string = notice_str
        self.assertEqual(capture_output.getvalue(), uncolored_string)

    @mock.patch('__builtin__.raw_input')
    @mock.patch('json.load')
    def test_update_test_runner_cmd(self, mock_json_load_data, mock_raw_input):
        """Test method handle_test_runner_cmd without enable do_verification."""
        former_cmd_str = 'Former cmds ='
        write_result_str = 'Save result mapping to test_result'
        tmp_file = tempfile.NamedTemporaryFile()
        input_cmd = 'atest_args'
        runner_cmds = ['cmd1', 'cmd2']
        capture_output = StringIO()
        sys.stdout = capture_output
        # Previous data is empty. Should not enter strtobool.
        # If entered, exception will be raised cause test fail.
        mock_json_load_data.return_value = {}
        atest_utils.handle_test_runner_cmd(input_cmd,
                                           runner_cmds,
                                           do_verification=False,
                                           result_path=tmp_file.name)
        sys.stdout = sys.__stdout__
        self.assertEqual(capture_output.getvalue().find(former_cmd_str), -1)
        # Previous data is the same as the new input. Should not enter strtobool.
        # If entered, exception will be raised cause test fail
        capture_output = StringIO()
        sys.stdout = capture_output
        mock_json_load_data.return_value = {input_cmd:runner_cmds}
        atest_utils.handle_test_runner_cmd(input_cmd,
                                           runner_cmds,
                                           do_verification=False,
                                           result_path=tmp_file.name)
        sys.stdout = sys.__stdout__
        self.assertEqual(capture_output.getvalue().find(former_cmd_str), -1)
        self.assertEqual(capture_output.getvalue().find(write_result_str), -1)
        # Previous data has different cmds. Should enter strtobool not update,
        # should not find write_result_str.
        prev_cmds = ['cmd1']
        mock_raw_input.return_value = 'n'
        capture_output = StringIO()
        sys.stdout = capture_output
        mock_json_load_data.return_value = {input_cmd:prev_cmds}
        atest_utils.handle_test_runner_cmd(input_cmd,
                                           runner_cmds,
                                           do_verification=False,
                                           result_path=tmp_file.name)
        sys.stdout = sys.__stdout__
        self.assertEqual(capture_output.getvalue().find(write_result_str), -1)

    @mock.patch('json.load')
    def test_verify_test_runner_cmd(self, mock_json_load_data):
        """Test method handle_test_runner_cmd without enable update_result."""
        tmp_file = tempfile.NamedTemporaryFile()
        input_cmd = 'atest_args'
        runner_cmds = ['cmd1', 'cmd2']
        # Previous data is the same as the new input. Should not raise exception.
        mock_json_load_data.return_value = {input_cmd:runner_cmds}
        atest_utils.handle_test_runner_cmd(input_cmd,
                                           runner_cmds,
                                           do_verification=True,
                                           result_path=tmp_file.name)
        # Previous data has different cmds. Should enter strtobool and hit
        # exception.
        prev_cmds = ['cmd1']
        mock_json_load_data.return_value = {input_cmd:prev_cmds}
        self.assertRaises(atest_error.DryRunVerificationError,
                          atest_utils.handle_test_runner_cmd,
                          input_cmd,
                          runner_cmds,
                          do_verification=True,
                          result_path=tmp_file.name)

    def test_get_test_info_cache_path(self):
        """Test method get_test_info_cache_path."""
        input_file_name = 'mytest_name'
        cache_root = '/a/b/c'
        expect_hashed_name = ('%s.cache' % hashlib.md5(str(input_file_name).
                                                       encode()).hexdigest())
        self.assertEqual(os.path.join(cache_root, expect_hashed_name),
                         atest_utils.get_test_info_cache_path(input_file_name,
                                                              cache_root))

    def test_get_and_load_cache(self):
        """Test method update_test_info_cache and load_test_info_cache."""
        test_reference = 'myTestRefA'
        test_cache_dir = tempfile.mkdtemp()
        atest_utils.update_test_info_cache(test_reference, [TEST_INFO_A],
                                           test_cache_dir)
        unittest_utils.assert_equal_testinfo_sets(
            self, set([TEST_INFO_A]),
            atest_utils.load_test_info_cache(test_reference, test_cache_dir))

    @mock.patch('os.getcwd')
    def test_get_build_cmd(self, mock_cwd):
        """Test method get_build_cmd."""
        build_top = '/home/a/b/c'
        rel_path = 'd/e'
        mock_cwd.return_value = os.path.join(build_top, rel_path)
        os_environ_mock = {constants.ANDROID_BUILD_TOP: build_top}
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            expected_cmd = ['../../build/soong/soong_ui.bash', '--make-mode']
            self.assertEqual(expected_cmd, atest_utils.get_build_cmd())

    @mock.patch('subprocess.check_output')
    def test_get_modified_files(self, mock_co):
        """Test method get_modified_files"""
        mock_co.side_effect = ['/a/b/',
                               '\n',
                               'test_fp1.java\nc/test_fp2.java']
        self.assertEqual({'/a/b/test_fp1.java', '/a/b/c/test_fp2.java'},
                         atest_utils.get_modified_files(''))
        mock_co.side_effect = ['/a/b/',
                               'test_fp4',
                               '/test_fp3.java']
        self.assertEqual({'/a/b/test_fp4', '/a/b/test_fp3.java'},
                         atest_utils.get_modified_files(''))


if __name__ == "__main__":
    unittest.main()
