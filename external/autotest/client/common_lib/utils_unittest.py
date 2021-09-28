#!/usr/bin/python2

# pylint: disable=missing-docstring

import StringIO
import errno
import itertools
import logging
import os
import select
import socket
import subprocess
import time
import unittest
import urllib2

import common
from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.test_utils import mock

# mock 1.0.0 (in site-packages/chromite/third_party/mock.py)
# which is an ancestor of Python's default library starting from Python 3.3.
# See https://docs.python.org/3/library/unittest.mock.html
import mock as pymock

metrics = utils.metrics_mock


class test_read_one_line(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils, "open")


    def tearDown(self):
        self.god.unstub_all()


    def test_ip_to_long(self):
        self.assertEqual(utils.ip_to_long('0.0.0.0'), 0)
        self.assertEqual(utils.ip_to_long('255.255.255.255'), 4294967295)
        self.assertEqual(utils.ip_to_long('192.168.0.1'), 3232235521)
        self.assertEqual(utils.ip_to_long('1.2.4.8'), 16909320)


    def test_long_to_ip(self):
        self.assertEqual(utils.long_to_ip(0), '0.0.0.0')
        self.assertEqual(utils.long_to_ip(4294967295), '255.255.255.255')
        self.assertEqual(utils.long_to_ip(3232235521), '192.168.0.1')
        self.assertEqual(utils.long_to_ip(16909320), '1.2.4.8')


    def test_create_subnet_mask(self):
        self.assertEqual(utils.create_subnet_mask(0), 0)
        self.assertEqual(utils.create_subnet_mask(32), 4294967295)
        self.assertEqual(utils.create_subnet_mask(25), 4294967168)


    def test_format_ip_with_mask(self):
        self.assertEqual(utils.format_ip_with_mask('192.168.0.1', 0),
                         '0.0.0.0/0')
        self.assertEqual(utils.format_ip_with_mask('192.168.0.1', 32),
                         '192.168.0.1/32')
        self.assertEqual(utils.format_ip_with_mask('192.168.0.1', 26),
                         '192.168.0.0/26')
        self.assertEqual(utils.format_ip_with_mask('192.168.0.255', 26),
                         '192.168.0.192/26')


    def create_test_file(self, contents):
        test_file = StringIO.StringIO(contents)
        utils.open.expect_call("filename", "r").and_return(test_file)


    def test_reads_one_line_file(self):
        self.create_test_file("abc\n")
        self.assertEqual("abc", utils.read_one_line("filename"))
        self.god.check_playback()


    def test_strips_read_lines(self):
        self.create_test_file("abc   \n")
        self.assertEqual("abc   ", utils.read_one_line("filename"))
        self.god.check_playback()


    def test_drops_extra_lines(self):
        self.create_test_file("line 1\nline 2\nline 3\n")
        self.assertEqual("line 1", utils.read_one_line("filename"))
        self.god.check_playback()


    def test_works_on_empty_file(self):
        self.create_test_file("")
        self.assertEqual("", utils.read_one_line("filename"))
        self.god.check_playback()


    def test_works_on_file_with_no_newlines(self):
        self.create_test_file("line but no newline")
        self.assertEqual("line but no newline",
                         utils.read_one_line("filename"))
        self.god.check_playback()


    def test_preserves_leading_whitespace(self):
        self.create_test_file("   has leading whitespace")
        self.assertEqual("   has leading whitespace",
                         utils.read_one_line("filename"))


class test_write_one_line(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils, "open")


    def tearDown(self):
        self.god.unstub_all()


    def get_write_one_line_output(self, content):
        test_file = mock.SaveDataAfterCloseStringIO()
        utils.open.expect_call("filename", "w").and_return(test_file)
        utils.write_one_line("filename", content)
        self.god.check_playback()
        return test_file.final_data


    def test_writes_one_line_file(self):
        self.assertEqual("abc\n", self.get_write_one_line_output("abc"))


    def test_preserves_existing_newline(self):
        self.assertEqual("abc\n", self.get_write_one_line_output("abc\n"))


    def test_preserves_leading_whitespace(self):
        self.assertEqual("   abc\n", self.get_write_one_line_output("   abc"))


    def test_preserves_trailing_whitespace(self):
        self.assertEqual("abc   \n", self.get_write_one_line_output("abc   "))


    def test_handles_empty_input(self):
        self.assertEqual("\n", self.get_write_one_line_output(""))


class test_open_write_close(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils, "open")


    def tearDown(self):
        self.god.unstub_all()


    def test_simple_functionality(self):
        data = "\n\nwhee\n"
        test_file = mock.SaveDataAfterCloseStringIO()
        utils.open.expect_call("filename", "w").and_return(test_file)
        utils.open_write_close("filename", data)
        self.god.check_playback()
        self.assertEqual(data, test_file.final_data)


class test_read_keyval(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils, "open")
        self.god.stub_function(os.path, "isdir")
        self.god.stub_function(os.path, "exists")


    def tearDown(self):
        self.god.unstub_all()


    def create_test_file(self, filename, contents):
        test_file = StringIO.StringIO(contents)
        os.path.exists.expect_call(filename).and_return(True)
        utils.open.expect_call(filename).and_return(test_file)


    def read_keyval(self, contents):
        os.path.isdir.expect_call("file").and_return(False)
        self.create_test_file("file", contents)
        keyval = utils.read_keyval("file")
        self.god.check_playback()
        return keyval


    def test_returns_empty_when_file_doesnt_exist(self):
        os.path.isdir.expect_call("file").and_return(False)
        os.path.exists.expect_call("file").and_return(False)
        self.assertEqual({}, utils.read_keyval("file"))
        self.god.check_playback()


    def test_accesses_files_directly(self):
        os.path.isdir.expect_call("file").and_return(False)
        self.create_test_file("file", "")
        utils.read_keyval("file")
        self.god.check_playback()


    def test_accesses_directories_through_keyval_file(self):
        os.path.isdir.expect_call("dir").and_return(True)
        self.create_test_file("dir/keyval", "")
        utils.read_keyval("dir")
        self.god.check_playback()


    def test_values_are_rstripped(self):
        keyval = self.read_keyval("a=b   \n")
        self.assertEquals(keyval, {"a": "b"})


    def test_comments_are_ignored(self):
        keyval = self.read_keyval("a=b # a comment\n")
        self.assertEquals(keyval, {"a": "b"})


    def test_integers_become_ints(self):
        keyval = self.read_keyval("a=1\n")
        self.assertEquals(keyval, {"a": 1})
        self.assertEquals(int, type(keyval["a"]))


    def test_float_values_become_floats(self):
        keyval = self.read_keyval("a=1.5\n")
        self.assertEquals(keyval, {"a": 1.5})
        self.assertEquals(float, type(keyval["a"]))


    def test_multiple_lines(self):
        keyval = self.read_keyval("a=one\nb=two\n")
        self.assertEquals(keyval, {"a": "one", "b": "two"})


    def test_the_last_duplicate_line_is_used(self):
        keyval = self.read_keyval("a=one\nb=two\na=three\n")
        self.assertEquals(keyval, {"a": "three", "b": "two"})


    def test_extra_equals_are_included_in_values(self):
        keyval = self.read_keyval("a=b=c\n")
        self.assertEquals(keyval, {"a": "b=c"})


    def test_non_alphanumeric_keynames_are_rejected(self):
        self.assertRaises(ValueError, self.read_keyval, "a$=one\n")


    def test_underscores_are_allowed_in_key_names(self):
        keyval = self.read_keyval("a_b=value\n")
        self.assertEquals(keyval, {"a_b": "value"})


    def test_dashes_are_allowed_in_key_names(self):
        keyval = self.read_keyval("a-b=value\n")
        self.assertEquals(keyval, {"a-b": "value"})

    def test_empty_value_is_allowed(self):
        keyval = self.read_keyval("a=\n")
        self.assertEquals(keyval, {"a": ""})


class test_write_keyval(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils, "open")
        self.god.stub_function(os.path, "isdir")


    def tearDown(self):
        self.god.unstub_all()


    def assertHasLines(self, value, lines):
        vlines = value.splitlines()
        vlines.sort()
        self.assertEquals(vlines, sorted(lines))


    def write_keyval(self, filename, dictionary, expected_filename=None,
                     type_tag=None):
        if expected_filename is None:
            expected_filename = filename
        test_file = StringIO.StringIO()
        self.god.stub_function(test_file, "close")
        utils.open.expect_call(expected_filename, "a").and_return(test_file)
        test_file.close.expect_call()
        if type_tag is None:
            utils.write_keyval(filename, dictionary)
        else:
            utils.write_keyval(filename, dictionary, type_tag)
        return test_file.getvalue()


    def write_keyval_file(self, dictionary, type_tag=None):
        os.path.isdir.expect_call("file").and_return(False)
        return self.write_keyval("file", dictionary, type_tag=type_tag)


    def test_accesses_files_directly(self):
        os.path.isdir.expect_call("file").and_return(False)
        result = self.write_keyval("file", {"a": "1"})
        self.assertEquals(result, "a=1\n")


    def test_accesses_directories_through_keyval_file(self):
        os.path.isdir.expect_call("dir").and_return(True)
        result = self.write_keyval("dir", {"b": "2"}, "dir/keyval")
        self.assertEquals(result, "b=2\n")


    def test_numbers_are_stringified(self):
        result = self.write_keyval_file({"c": 3})
        self.assertEquals(result, "c=3\n")


    def test_type_tags_are_excluded_by_default(self):
        result = self.write_keyval_file({"d": "a string"})
        self.assertEquals(result, "d=a string\n")
        self.assertRaises(ValueError, self.write_keyval_file,
                          {"d{perf}": "a string"})


    def test_perf_tags_are_allowed(self):
        result = self.write_keyval_file({"a{perf}": 1, "b{perf}": 2},
                                        type_tag="perf")
        self.assertHasLines(result, ["a{perf}=1", "b{perf}=2"])
        self.assertRaises(ValueError, self.write_keyval_file,
                          {"a": 1, "b": 2}, type_tag="perf")


    def test_non_alphanumeric_keynames_are_rejected(self):
        self.assertRaises(ValueError, self.write_keyval_file, {"x$": 0})


    def test_underscores_are_allowed_in_key_names(self):
        result = self.write_keyval_file({"a_b": "value"})
        self.assertEquals(result, "a_b=value\n")


    def test_dashes_are_allowed_in_key_names(self):
        result = self.write_keyval_file({"a-b": "value"})
        self.assertEquals(result, "a-b=value\n")


class test_is_url(unittest.TestCase):
    def test_accepts_http(self):
        self.assertTrue(utils.is_url("http://example.com"))


    def test_accepts_ftp(self):
        self.assertTrue(utils.is_url("ftp://ftp.example.com"))


    def test_rejects_local_path(self):
        self.assertFalse(utils.is_url("/home/username/file"))


    def test_rejects_local_filename(self):
        self.assertFalse(utils.is_url("filename"))


    def test_rejects_relative_local_path(self):
        self.assertFalse(utils.is_url("somedir/somesubdir/file"))


    def test_rejects_local_path_containing_url(self):
        self.assertFalse(utils.is_url("somedir/http://path/file"))


class test_urlopen(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)


    def tearDown(self):
        self.god.unstub_all()


    def stub_urlopen_with_timeout_comparison(self, test_func, expected_return,
                                             *expected_args):
        expected_args += (None,) * (2 - len(expected_args))
        def urlopen(url, data=None):
            self.assertEquals(expected_args, (url,data))
            test_func(socket.getdefaulttimeout())
            return expected_return
        self.god.stub_with(urllib2, "urlopen", urlopen)


    def stub_urlopen_with_timeout_check(self, expected_timeout,
                                        expected_return, *expected_args):
        def test_func(timeout):
            self.assertEquals(timeout, expected_timeout)
        self.stub_urlopen_with_timeout_comparison(test_func, expected_return,
                                                  *expected_args)


    def test_timeout_set_during_call(self):
        self.stub_urlopen_with_timeout_check(30, "retval", "url")
        retval = utils.urlopen("url", timeout=30)
        self.assertEquals(retval, "retval")


    def test_timeout_reset_after_call(self):
        old_timeout = socket.getdefaulttimeout()
        self.stub_urlopen_with_timeout_check(30, None, "url")
        try:
            socket.setdefaulttimeout(1234)
            utils.urlopen("url", timeout=30)
            self.assertEquals(1234, socket.getdefaulttimeout())
        finally:
            socket.setdefaulttimeout(old_timeout)


    def test_timeout_set_by_default(self):
        def test_func(timeout):
            self.assertTrue(timeout is not None)
        self.stub_urlopen_with_timeout_comparison(test_func, None, "url")
        utils.urlopen("url")


    def test_args_are_untouched(self):
        self.stub_urlopen_with_timeout_check(30, None, "http://url",
                                             "POST data")
        utils.urlopen("http://url", timeout=30, data="POST data")


class test_urlretrieve(unittest.TestCase):
    def setUp(self):
        self.god = mock.mock_god(ut=self)


    def tearDown(self):
        self.god.unstub_all()


    def test_urlopen_passed_arguments(self):
        self.god.stub_function(utils, "urlopen")
        self.god.stub_function(utils.shutil, "copyfileobj")
        self.god.stub_function(utils, "open")

        url = "url"
        dest = "somefile"
        data = object()
        timeout = 10

        src_file = self.god.create_mock_class(file, "file")
        dest_file = self.god.create_mock_class(file, "file")

        (utils.urlopen.expect_call(url, data=data, timeout=timeout)
                .and_return(src_file))
        utils.open.expect_call(dest, "wb").and_return(dest_file)
        utils.shutil.copyfileobj.expect_call(src_file, dest_file)
        dest_file.close.expect_call()
        src_file.close.expect_call()

        utils.urlretrieve(url, dest, data=data, timeout=timeout)
        self.god.check_playback()


class test_merge_trees(unittest.TestCase):
    # a some path-handling helper functions
    def src(self, *path_segments):
        return os.path.join(self.src_tree.name, *path_segments)


    def dest(self, *path_segments):
        return os.path.join(self.dest_tree.name, *path_segments)


    def paths(self, *path_segments):
        return self.src(*path_segments), self.dest(*path_segments)


    def assertFileEqual(self, *path_segments):
        src, dest = self.paths(*path_segments)
        self.assertEqual(True, os.path.isfile(src))
        self.assertEqual(True, os.path.isfile(dest))
        self.assertEqual(os.path.getsize(src), os.path.getsize(dest))
        self.assertEqual(open(src).read(), open(dest).read())


    def assertFileContents(self, contents, *path_segments):
        dest = self.dest(*path_segments)
        self.assertEqual(True, os.path.isfile(dest))
        self.assertEqual(os.path.getsize(dest), len(contents))
        self.assertEqual(contents, open(dest).read())


    def setUp(self):
        self.src_tree = autotemp.tempdir(unique_id='utilsrc')
        self.dest_tree = autotemp.tempdir(unique_id='utilsdest')

        # empty subdirs
        os.mkdir(self.src("empty"))
        os.mkdir(self.dest("empty"))


    def tearDown(self):
        self.src_tree.clean()
        self.dest_tree.clean()


    def test_both_dont_exist(self):
        utils.merge_trees(*self.paths("empty"))


    def test_file_only_at_src(self):
        print >> open(self.src("src_only"), "w"), "line 1"
        utils.merge_trees(*self.paths("src_only"))
        self.assertFileEqual("src_only")


    def test_file_only_at_dest(self):
        print >> open(self.dest("dest_only"), "w"), "line 1"
        utils.merge_trees(*self.paths("dest_only"))
        self.assertEqual(False, os.path.exists(self.src("dest_only")))
        self.assertFileContents("line 1\n", "dest_only")


    def test_file_at_both(self):
        print >> open(self.dest("in_both"), "w"), "line 1"
        print >> open(self.src("in_both"), "w"), "line 2"
        utils.merge_trees(*self.paths("in_both"))
        self.assertFileContents("line 1\nline 2\n", "in_both")


    def test_directory_with_files_in_both(self):
        print >> open(self.dest("in_both"), "w"), "line 1"
        print >> open(self.src("in_both"), "w"), "line 3"
        utils.merge_trees(*self.paths())
        self.assertFileContents("line 1\nline 3\n", "in_both")


    def test_directory_with_mix_of_files(self):
        print >> open(self.dest("in_dest"), "w"), "dest line"
        print >> open(self.src("in_src"), "w"), "src line"
        utils.merge_trees(*self.paths())
        self.assertFileContents("dest line\n", "in_dest")
        self.assertFileContents("src line\n", "in_src")


    def test_directory_with_subdirectories(self):
        os.mkdir(self.src("src_subdir"))
        print >> open(self.src("src_subdir", "subfile"), "w"), "subdir line"
        os.mkdir(self.src("both_subdir"))
        os.mkdir(self.dest("both_subdir"))
        print >> open(self.src("both_subdir", "subfile"), "w"), "src line"
        print >> open(self.dest("both_subdir", "subfile"), "w"), "dest line"
        utils.merge_trees(*self.paths())
        self.assertFileContents("subdir line\n", "src_subdir", "subfile")
        self.assertFileContents("dest line\nsrc line\n", "both_subdir",
                                "subfile")


class test_get_relative_path(unittest.TestCase):
    def test_not_absolute(self):
        self.assertRaises(AssertionError, utils.get_relative_path, "a", "b")

    def test_same_dir(self):
        self.assertEqual(utils.get_relative_path("/a/b/c", "/a/b"), "c")

    def test_forward_dir(self):
        self.assertEqual(utils.get_relative_path("/a/b/c/d", "/a/b"), "c/d")

    def test_previous_dir(self):
        self.assertEqual(utils.get_relative_path("/a/b", "/a/b/c/d"), "../..")

    def test_parallel_dir(self):
        self.assertEqual(utils.get_relative_path("/a/c/d", "/a/b/c/d"),
                         "../../../c/d")


class test_sh_escape(unittest.TestCase):
    def _test_in_shell(self, text):
        escaped_text = utils.sh_escape(text)
        proc = subprocess.Popen('echo "%s"' % escaped_text, shell=True,
                                stdin=open(os.devnull, 'r'),
                                stdout=subprocess.PIPE,
                                stderr=open(os.devnull, 'w'))
        stdout, _ = proc.communicate()
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(stdout[:-1], text)


    def test_normal_string(self):
        self._test_in_shell('abcd')


    def test_spaced_string(self):
        self._test_in_shell('abcd efgh')


    def test_dollar(self):
        self._test_in_shell('$')


    def test_single_quote(self):
        self._test_in_shell('\'')


    def test_single_quoted_string(self):
        self._test_in_shell('\'efgh\'')


    def test_string_with_single_quote(self):
        self._test_in_shell("a'b")


    def test_string_with_escaped_single_quote(self):
        self._test_in_shell(r"a\'b")


    def test_double_quote(self):
        self._test_in_shell('"')


    def test_double_quoted_string(self):
        self._test_in_shell('"abcd"')


    def test_backtick(self):
        self._test_in_shell('`')


    def test_backticked_string(self):
        self._test_in_shell('`jklm`')


    def test_backslash(self):
        self._test_in_shell('\\')


    def test_backslashed_special_characters(self):
        self._test_in_shell('\\$')
        self._test_in_shell('\\"')
        self._test_in_shell('\\\'')
        self._test_in_shell('\\`')


    def test_backslash_codes(self):
        self._test_in_shell('\\n')
        self._test_in_shell('\\r')
        self._test_in_shell('\\t')
        self._test_in_shell('\\v')
        self._test_in_shell('\\b')
        self._test_in_shell('\\a')
        self._test_in_shell('\\000')

    def test_real_newline(self):
        self._test_in_shell('\n')
        self._test_in_shell('\\\n')


class test_sh_quote_word(test_sh_escape):
    """Run tests on sh_quote_word.

    Inherit from test_sh_escape to get the same tests to run on both.
    """

    def _test_in_shell(self, text):
        quoted_word = utils.sh_quote_word(text)
        echoed_value = subprocess.check_output('echo %s' % quoted_word,
                                               shell=True)
        self.assertEqual(echoed_value, text + '\n')


class test_nested_sh_quote_word(test_sh_quote_word):
    """Run nested tests on sh_quote_word.

    Inherit from test_sh_quote_word to get the same tests to run on both.
    """

    def _test_in_shell(self, text):
        command = 'echo ' + utils.sh_quote_word(text)
        nested_command = 'echo ' + utils.sh_quote_word(command)
        produced_command = subprocess.check_output(nested_command, shell=True)
        echoed_value = subprocess.check_output(produced_command, shell=True)
        self.assertEqual(echoed_value, text + '\n')


class test_run(unittest.TestCase):
    """
    Test the utils.run() function.

    Note: This test runs simple external commands to test the utils.run()
    API without assuming implementation details.
    """

    # Log levels in ascending severity.
    LOG_LEVELS = [logging.DEBUG, logging.INFO, logging.WARNING, logging.ERROR,
                  logging.CRITICAL]


    def setUp(self):
        self.god = mock.mock_god(ut=self)
        self.god.stub_function(utils.logging, 'warning')
        self.god.stub_function(utils.logging, 'debug')

        # Log level -> StringIO.StringIO.
        self.logs = {}
        for level in self.LOG_LEVELS:
            self.logs[level] = StringIO.StringIO()

        # Override logging_manager.LoggingFile to return buffers.
        def logging_file(level=None, prefix=None):
            return self.logs[level]
        self.god.stub_with(utils.logging_manager, 'LoggingFile', logging_file)

    def tearDown(self):
        self.god.unstub_all()


    def __check_result(self, result, command, exit_status=0, stdout='',
                       stderr=''):
        self.assertEquals(result.command, command)
        self.assertEquals(result.exit_status, exit_status)
        self.assertEquals(result.stdout, stdout)
        self.assertEquals(result.stderr, stderr)


    def __get_logs(self):
        """Returns contents of log buffers at all levels.

            @return: 5-element list of strings corresponding to logged messages
                at the levels in self.LOG_LEVELS.
        """
        return [self.logs[v].getvalue() for v in self.LOG_LEVELS]


    def test_default_simple(self):
        cmd = 'echo "hello world"'
        # expect some king of logging.debug() call but don't care about args
        utils.logging.debug.expect_any_call()
        self.__check_result(utils.run(cmd), cmd, stdout='hello world\n')


    def test_default_failure(self):
        cmd = 'exit 11'
        try:
            utils.run(cmd, verbose=False)
        except utils.error.CmdError, err:
            self.__check_result(err.result_obj, cmd, exit_status=11)


    def test_ignore_status(self):
        cmd = 'echo error >&2 && exit 11'
        self.__check_result(utils.run(cmd, ignore_status=True, verbose=False),
                            cmd, exit_status=11, stderr='error\n')


    def test_timeout(self):
        # we expect a logging.warning() message, don't care about the contents
        utils.logging.warning.expect_any_call()
        try:
            utils.run('echo -n output && sleep 10', timeout=1, verbose=False)
        except utils.error.CmdError, err:
            self.assertEquals(err.result_obj.stdout, 'output')


    def test_stdout_stderr_tee(self):
        cmd = 'echo output && echo error >&2'
        stdout_tee = StringIO.StringIO()
        stderr_tee = StringIO.StringIO()

        self.__check_result(utils.run(
                cmd, stdout_tee=stdout_tee, stderr_tee=stderr_tee,
                verbose=False), cmd, stdout='output\n', stderr='error\n')
        self.assertEqual(stdout_tee.getvalue(), 'output\n')
        self.assertEqual(stderr_tee.getvalue(), 'error\n')


    def test_stdin_string(self):
        cmd = 'cat'
        self.__check_result(utils.run(cmd, verbose=False, stdin='hi!\n'),
                            cmd, stdout='hi!\n')


    def test_stdout_tee_to_logs_info(self):
        """Test logging stdout at the info level."""
        utils.run('echo output', stdout_tee=utils.TEE_TO_LOGS,
                  stdout_level=logging.INFO, verbose=False)
        self.assertEqual(self.__get_logs(), ['', 'output\n', '', '', ''])


    def test_stdout_tee_to_logs_warning(self):
        """Test logging stdout at the warning level."""
        utils.run('echo output', stdout_tee=utils.TEE_TO_LOGS,
                  stdout_level=logging.WARNING, verbose=False)
        self.assertEqual(self.__get_logs(), ['', '', 'output\n', '', ''])


    def test_stdout_and_stderr_tee_to_logs(self):
        """Test simultaneous stdout and stderr log levels."""
        utils.run('echo output && echo error >&2', stdout_tee=utils.TEE_TO_LOGS,
                  stderr_tee=utils.TEE_TO_LOGS, stdout_level=logging.INFO,
                  stderr_level=logging.ERROR, verbose=False)
        self.assertEqual(self.__get_logs(), ['', 'output\n', '', 'error\n', ''])


    def test_default_expected_stderr_log_level(self):
        """Test default expected stderr log level.

        stderr should be logged at the same level as stdout when
        stderr_is_expected is true and stderr_level isn't passed.
        """
        utils.run('echo output && echo error >&2', stdout_tee=utils.TEE_TO_LOGS,
                  stderr_tee=utils.TEE_TO_LOGS, stdout_level=logging.INFO,
                  stderr_is_expected=True, verbose=False)
        self.assertEqual(self.__get_logs(), ['', 'output\nerror\n', '', '', ''])


    def test_safe_args(self):
        # NOTE: The string in expected_quoted_cmd depends on the internal
        # implementation of shell quoting which is used by utils.run(),
        # in this case, sh_quote_word().
        expected_quoted_cmd = "echo 'hello \"world' again"
        self.__check_result(utils.run(
                'echo', verbose=False, args=('hello "world', 'again')),
                expected_quoted_cmd, stdout='hello "world again\n')


    def test_safe_args_given_string(self):
        self.assertRaises(TypeError, utils.run, 'echo', args='hello')


    def test_wait_interrupt(self):
        """Test that we actually select twice if the first one returns EINTR."""
        utils.logging.debug.expect_any_call()

        bg_job = utils.BgJob('echo "hello world"')
        bg_job.result.exit_status = 0
        self.god.stub_function(utils.select, 'select')

        utils.select.select.expect_any_call().and_raises(
                select.error(errno.EINTR, 'Select interrupted'))
        utils.logging.warning.expect_any_call()

        utils.select.select.expect_any_call().and_return(
                ([bg_job.sp.stdout, bg_job.sp.stderr], [], None))
        utils.logging.warning.expect_any_call()

        self.assertFalse(
                utils._wait_for_commands([bg_job], time.time(), None))


class test_compare_versions(unittest.TestCase):
    def test_zerofill(self):
        self.assertEqual(utils.compare_versions('1.7', '1.10'), -1)
        self.assertEqual(utils.compare_versions('1.222', '1.3'), 1)
        self.assertEqual(utils.compare_versions('1.03', '1.3'), 0)


    def test_unequal_len(self):
        self.assertEqual(utils.compare_versions('1.3', '1.3.4'), -1)
        self.assertEqual(utils.compare_versions('1.3.1', '1.3'), 1)


    def test_dash_delimited(self):
        self.assertEqual(utils.compare_versions('1-2-3', '1-5-1'), -1)
        self.assertEqual(utils.compare_versions('1-2-1', '1-1-1'), 1)
        self.assertEqual(utils.compare_versions('1-2-4', '1-2-4'), 0)


    def test_alphabets(self):
        self.assertEqual(utils.compare_versions('m.l.b', 'n.b.a'), -1)
        self.assertEqual(utils.compare_versions('n.b.a', 'm.l.b'), 1)
        self.assertEqual(utils.compare_versions('abc.e', 'abc.e'), 0)


    def test_mix_symbols(self):
        self.assertEqual(utils.compare_versions('k-320.1', 'k-320.3'), -1)
        self.assertEqual(utils.compare_versions('k-231.5', 'k-231.1'), 1)
        self.assertEqual(utils.compare_versions('k-231.1', 'k-231.1'), 0)

        self.assertEqual(utils.compare_versions('k.320-1', 'k.320-3'), -1)
        self.assertEqual(utils.compare_versions('k.231-5', 'k.231-1'), 1)
        self.assertEqual(utils.compare_versions('k.231-1', 'k.231-1'), 0)


class test_args_to_dict(unittest.TestCase):
    def test_no_args(self):
        result = utils.args_to_dict([])
        self.assertEqual({}, result)


    def test_matches(self):
        result = utils.args_to_dict(['aBc:DeF', 'SyS=DEf', 'XY_Z:',
                                     'F__o0O=', 'B8r:=:=', '_bAZ_=:=:'])
        self.assertEqual(result, {'abc':'DeF', 'sys':'DEf', 'xy_z':'',
                                  'f__o0o':'', 'b8r':'=:=', '_baz_':':=:'})


    def test_unmatches(self):
        # Temporarily shut warning messages from args_to_dict() when an argument
        # doesn't match its pattern.
        logger = logging.getLogger()
        saved_level = logger.level
        logger.setLevel(logging.ERROR)

        try:
            result = utils.args_to_dict(['ab-c:DeF', '--SyS=DEf', 'a*=b', 'a*b',
                                         ':VAL', '=VVV', 'WORD'])
            self.assertEqual({}, result)
        finally:
            # Restore level.
            logger.setLevel(saved_level)


class test_get_random_port(unittest.TestCase):
    def do_bind(self, port, socket_type, socket_proto):
        s = socket.socket(socket.AF_INET, socket_type, socket_proto)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('', port))
        return s


    def test_get_port(self):
        for _ in xrange(100):
            p = utils.get_unused_port()
            s = self.do_bind(p, socket.SOCK_STREAM, socket.IPPROTO_TCP)
            self.assert_(s.getsockname())
            s = self.do_bind(p, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            self.assert_(s.getsockname())


def test_function(arg1, arg2, arg3, arg4=4, arg5=5, arg6=6):
    """Test global function.
    """


class TestClass(object):
    """Test class.
    """

    def test_instance_function(self, arg1, arg2, arg3, arg4=4, arg5=5, arg6=6):
        """Test instance function.
        """


    @classmethod
    def test_class_function(cls, arg1, arg2, arg3, arg4=4, arg5=5, arg6=6):
        """Test class function.
        """


    @staticmethod
    def test_static_function(arg1, arg2, arg3, arg4=4, arg5=5, arg6=6):
        """Test static function.
        """


class GetFunctionArgUnittest(unittest.TestCase):
    """Tests for method get_function_arg_value."""

    def run_test(self, func, insert_arg):
        """Run test.

        @param func: Function being called with given arguments.
        @param insert_arg: Set to True to insert an object in the argument list.
                           This is to mock instance/class object.
        """
        if insert_arg:
            args = (None, 1, 2, 3)
        else:
            args = (1, 2, 3)
        for i in range(1, 7):
            self.assertEquals(utils.get_function_arg_value(
                    func, 'arg%d'%i, args, {}), i)

        self.assertEquals(utils.get_function_arg_value(
                func, 'arg7', args, {'arg7': 7}), 7)
        self.assertRaises(
                KeyError, utils.get_function_arg_value,
                func, 'arg3', args[:-1], {})


    def test_global_function(self):
        """Test global function.
        """
        self.run_test(test_function, False)


    def test_instance_function(self):
        """Test instance function.
        """
        self.run_test(TestClass().test_instance_function, True)


    def test_class_function(self):
        """Test class function.
        """
        self.run_test(TestClass.test_class_function, True)


    def test_static_function(self):
        """Test static function.
        """
        self.run_test(TestClass.test_static_function, False)


class IsInSameSubnetUnittest(unittest.TestCase):
    """Test is_in_same_subnet function."""

    def test_is_in_same_subnet(self):
        """Test is_in_same_subnet function."""
        self.assertTrue(utils.is_in_same_subnet('192.168.0.0', '192.168.1.2',
                                                23))
        self.assertFalse(utils.is_in_same_subnet('192.168.0.0', '192.168.1.2',
                                                24))
        self.assertTrue(utils.is_in_same_subnet('192.168.0.0', '192.168.0.255',
                                                24))
        self.assertFalse(utils.is_in_same_subnet('191.168.0.0', '192.168.0.0',
                                                24))


class GetWirelessSsidUnittest(unittest.TestCase):
    """Test get_wireless_ssid function."""

    DEFAULT_SSID = 'default'
    SSID_1 = 'ssid_1'
    SSID_2 = 'ssid_2'
    SSID_3 = 'ssid_3'

    def test_get_wireless_ssid(self):
        """Test is_in_same_subnet function."""
        god = mock.mock_god()
        god.stub_function_to_return(utils.CONFIG, 'get_config_value',
                                    self.DEFAULT_SSID)
        god.stub_function_to_return(utils.CONFIG, 'get_config_value_regex',
                                    {'wireless_ssid_1.2.3.4/24': self.SSID_1,
                                     'wireless_ssid_4.3.2.1/16': self.SSID_2,
                                     'wireless_ssid_4.3.2.111/32': self.SSID_3})
        self.assertEqual(self.SSID_1, utils.get_wireless_ssid('1.2.3.100'))
        self.assertEqual(self.SSID_2, utils.get_wireless_ssid('4.3.2.100'))
        self.assertEqual(self.SSID_3, utils.get_wireless_ssid('4.3.2.111'))
        self.assertEqual(self.DEFAULT_SSID,
                         utils.get_wireless_ssid('100.0.0.100'))


class LaunchControlBuildParseUnittest(unittest.TestCase):
    """Test various parsing functions related to Launch Control builds and
    devices.
    """

    def test_parse_launch_control_target(self):
        """Test parse_launch_control_target function."""
        target_tests = {
                ('shamu', 'userdebug'): 'shamu-userdebug',
                ('shamu', 'eng'): 'shamu-eng',
                ('shamu-board', 'eng'): 'shamu-board-eng',
                (None, None): 'bad_target',
                (None, None): 'target'}
        for result, target in target_tests.items():
            self.assertEqual(result, utils.parse_launch_control_target(target))


class GetOffloaderUriTest(unittest.TestCase):
    """Test get_offload_gsuri function."""
    _IMAGE_STORAGE_SERVER = 'gs://test_image_bucket'

    def setUp(self):
        self.god = mock.mock_god()

    def tearDown(self):
        self.god.unstub_all()

    def test_get_default_lab_offload_gsuri(self):
        """Test default lab offload gsuri ."""
        self.god.mock_up(utils.CONFIG, 'CONFIG')
        self.god.stub_function_to_return(utils, 'is_moblab', False)
        self.assertEqual(utils.DEFAULT_OFFLOAD_GSURI,
                utils.get_offload_gsuri())

        self.god.check_playback()

    def test_get_default_moblab_offload_gsuri(self):
        self.god.mock_up(utils.CONFIG, 'CONFIG')
        self.god.stub_function_to_return(utils, 'is_moblab', True)
        utils.CONFIG.get_config_value.expect_call(
                'CROS', 'image_storage_server').and_return(
                        self._IMAGE_STORAGE_SERVER)
        self.god.stub_function_to_return(utils,
                'get_moblab_serial_number', 'test_serial_number')
        self.god.stub_function_to_return(utils, 'get_moblab_id', 'test_id')
        expected_gsuri = '%sresults/%s/%s/' % (
                self._IMAGE_STORAGE_SERVER, 'test_serial_number', 'test_id')
        cached_gsuri = utils.DEFAULT_OFFLOAD_GSURI
        utils.DEFAULT_OFFLOAD_GSURI = None
        gsuri = utils.get_offload_gsuri()
        utils.DEFAULT_OFFLOAD_GSURI = cached_gsuri
        self.assertEqual(expected_gsuri, gsuri)

        self.god.check_playback()

    def test_get_moblab_offload_gsuri(self):
        """Test default lab offload gsuri ."""
        self.god.mock_up(utils.CONFIG, 'CONFIG')
        self.god.stub_function_to_return(utils, 'is_moblab', True)
        self.god.stub_function_to_return(utils,
                'get_moblab_serial_number', 'test_serial_number')
        self.god.stub_function_to_return(utils, 'get_moblab_id', 'test_id')
        gsuri = '%s%s/%s/' % (
                utils.DEFAULT_OFFLOAD_GSURI, 'test_serial_number', 'test_id')
        self.assertEqual(gsuri, utils.get_offload_gsuri())

        self.god.check_playback()



class  MockMetricsTest(unittest.TestCase):
    """Test metrics mock class can handle various metrics calls."""

    def test_Counter(self):
        """Test the mock class can create an instance and call any method.
        """
        c = metrics.Counter('counter')
        c.increment(fields={'key': 1})


    def test_Context(self):
        """Test the mock class can handle context class.
        """
        test_value = None
        with metrics.SecondsTimer('context') as t:
            test_value = 'called_in_context'
            t['random_key'] = 'pass'
        self.assertEqual('called_in_context', test_value)


    def test_decorator(self):
        """Test the mock class can handle decorator.
        """
        class TestClass(object):

            def __init__(self):
                self.value = None

        test_value = TestClass()
        test_value.value = None
        @metrics.SecondsTimerDecorator('decorator')
        def test(arg):
            arg.value = 'called_in_decorator'

        test(test_value)
        self.assertEqual('called_in_decorator', test_value.value)


    def test_setitem(self):
        """Test the mock class can handle set item call.
        """
        timer = metrics.SecondsTimer('name')
        timer['random_key'] = 'pass'


class test_background_sample(unittest.TestCase):
    """Test that the background sample can sample as desired.
    """

    def test_can_sample(self):
        """Test that a simple sample will work with no other complications.
        """
        should_be_sampled = 'name'

        def sample_function():
            """Return value of variable stored in method."""
            return should_be_sampled
        still_sampling = True

        t = utils.background_sample_until_condition(
                function=sample_function,
                condition=lambda: still_sampling,
                timeout=5,
                sleep_interval=0.1)
        result = t.finish()
        self.assertIn(should_be_sampled, result)


    def test_samples_multiple_values(self):
        """Test that a sample will work and actually samples at the necessary
        intervals, such that it will pick up changes.
        """
        should_be_sampled = 'name'

        def sample_function():
            """Return value of variable stored in method."""
            return should_be_sampled
        still_sampling = True

        t = utils.background_sample_until_condition(
                function=sample_function,
                condition=lambda: still_sampling,
                timeout=5,
                sleep_interval=0.1)
        # Let it sample values some with the initial value.
        time.sleep(2.5)
        # It should also sample some with the new value.
        should_be_sampled = 'noname'
        result = t.finish()
        self.assertIn('name', result)
        self.assertIn('noname', result)


class FakeTime(object):
    """Provides time() and sleep() for faking time module.
    """

    def __init__(self, start_time):
        self._time = start_time


    def time(self):
        return self._time


    def sleep(self, interval):
        self._time += interval


class TimeModuleMockTestCase(unittest.TestCase):
    """Mocks up utils.time with a FakeTime.

    It substitudes time.time() and time.sleep() with FakeTime.time()
    and FakeTime.sleep(), respectively.
    """

    def setUp(self):
        self.fake_time_begin = 10
        self.fake_time = FakeTime(self.fake_time_begin)
        self.patcher = pymock.patch(
            'autotest_lib.client.common_lib.utils.time')
        self.time_mock = self.patcher.start()
        self.addCleanup(self.patcher.stop)
        self.time_mock.time.side_effect = self.fake_time.time
        self.time_mock.sleep.side_effect = self.fake_time.sleep


def always_raise():
    """A function that raises an exception."""
    raise Exception('always raise')


def fail_n_times(count):
    """Creates a function that returns False for the first count-th calls.

    @return a function returns False for the first count-th calls and True
            afterwards.
    """
    counter = itertools.count(count, -1)
    return lambda: next(counter) <= 0


class test_poll_for_condition(TimeModuleMockTestCase):
    """Test poll_for_condition.
    """

    def test_ok(self):
        """Test polling condition that returns True.
        """
        self.assertTrue(utils.poll_for_condition(lambda: True))


    def test_ok_evaluated_as_true(self):
        """Test polling condition which's return value is evaluated as True.
        """
        self.assertEqual(1, utils.poll_for_condition(lambda: 1))

        self.assertEqual('something',
                         utils.poll_for_condition(lambda: 'something'))


    def test_fail(self):
        """Test polling condition that returns False.

        Expect TimeoutError exception as neither customized exception nor
        exception raised from condition().
        """
        with self.assertRaises(utils.TimeoutError):
            utils.poll_for_condition(lambda: False, timeout=3, sleep_interval=1)
        self.assertEqual(3, self.time_mock.sleep.call_count)


    def test_fail_evaluated_as_false(self):
        """Test polling condition which's return value is evaluated as False.

        Expect TimeoutError exception as neither customized exception nor
        exception raised from condition().
        """
        with self.assertRaises(utils.TimeoutError):
            utils.poll_for_condition(lambda: 0, timeout=3, sleep_interval=1)
        self.assertEqual(3, self.time_mock.sleep.call_count)

        with self.assertRaises(utils.TimeoutError):
            utils.poll_for_condition(lambda: None, timeout=3, sleep_interval=1)


    def test_exception_arg(self):
        """Test polling condition always fails.

        Expect exception raised by 'exception' args.
        """
        with self.assertRaisesRegexp(Exception, 'from args'):
            utils.poll_for_condition(lambda: False,
                                     exception=Exception('from args'),
                                     timeout=3, sleep_interval=1)
        self.assertEqual(3, self.time_mock.sleep.call_count)


    def test_exception_from_condition(self):
        """Test polling condition always fails.

        Expect exception raised by condition().
        """
        with self.assertRaisesRegexp(Exception, 'always raise'):
            utils.poll_for_condition(always_raise,
                                     exception=Exception('from args'),
                                     timeout=3, sleep_interval=1)
        # For poll_for_condition, if condition() raises exception, it raises
        # immidiately without retry. So sleep() should not be called.
        self.time_mock.sleep.assert_not_called()


    def test_ok_after_retry(self):
        """Test polling a condition which is success after retry twice.
        """
        self.assertTrue(utils.poll_for_condition(fail_n_times(2), timeout=3,
                                                 sleep_interval=1))


    def test_cannot_wait(self):
        """Test polling a condition which fails till timeout.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'Timed out waiting for unnamed condition'):
            utils.poll_for_condition(fail_n_times(4), timeout=3,
                                     sleep_interval=1)
        self.assertEqual(3, self.time_mock.sleep.call_count)


class test_poll_for_condition_ex(TimeModuleMockTestCase):
    """Test poll_for_condition_ex.
    """

    def test_ok(self):
        """Test polling condition that returns True.
        """
        self.assertTrue(utils.poll_for_condition_ex(lambda: True))


    def test_ok_evaluated_as_true(self):
        """Test polling condition which's return value is evaluated as True.
        """
        self.assertEqual(1, utils.poll_for_condition_ex(lambda: 1))

        self.assertEqual('something',
                         utils.poll_for_condition_ex(lambda: 'something'))


    def test_fail(self):
        """Test polling condition that returns False.

        Expect TimeoutError raised.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'Timed out waiting for unamed condition'):
            utils.poll_for_condition_ex(lambda: False, timeout=3,
                                        sleep_interval=1)
        self.assertEqual(2, self.time_mock.sleep.call_count)


    def test_fail_evaluated_as_false(self):
        """Test polling condition which's return value is evaluated as False.

        Expect TimeoutError raised.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'Timed out waiting for unamed condition'):
            utils.poll_for_condition_ex(lambda: 0, timeout=3,
                                        sleep_interval=1)
        self.assertEqual(2, self.time_mock.sleep.call_count)

        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'Timed out waiting for unamed condition'):
            utils.poll_for_condition_ex(lambda: None, timeout=3,
                                        sleep_interval=1)


    def test_desc_arg(self):
        """Test polling condition always fails with desc.

        Expect TimeoutError with condition description embedded.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'Timed out waiting for always false condition'):
            utils.poll_for_condition_ex(lambda: False,
                                        desc='always false condition',
                                        timeout=3, sleep_interval=1)
        self.assertEqual(2, self.time_mock.sleep.call_count)


    def test_exception(self):
        """Test polling condition that raises.

        Expect TimeoutError with condition raised exception embedded.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                "Reason: Exception\('always raise',\)"):
            utils.poll_for_condition_ex(always_raise, timeout=3,
                                        sleep_interval=1)
        self.assertEqual(2, self.time_mock.sleep.call_count)


    def test_ok_after_retry(self):
        """Test polling a condition which is success after retry twice.
        """
        self.assertTrue(utils.poll_for_condition_ex(fail_n_times(2), timeout=3,
                                                    sleep_interval=1))


    def test_cannot_wait(self):
        """Test polling a condition which fails till timeout.
        """
        with self.assertRaisesRegexp(
                utils.TimeoutError,
                'condition evaluted as false'):
            utils.poll_for_condition_ex(fail_n_times(3), timeout=3,
                                        sleep_interval=1)
        self.assertEqual(2, self.time_mock.sleep.call_count)


class test_timer(TimeModuleMockTestCase):
    """Test Timer.
    """

    def test_zero_timeout(self):
        """Test Timer with zero timeout.

        Only the first timer.sleep(0) is True.
        """
        timer = utils.Timer(0)
        self.assertTrue(timer.sleep(0))
        self.assertFalse(timer.sleep(0))
        self.time_mock.sleep.assert_not_called()


    def test_sleep(self):
        """Test Timer.sleep()
        """
        timeout = 3
        sleep_interval = 2
        timer = utils.Timer(timeout)

        # Kicks off timer.
        self.assertTrue(timer.sleep(sleep_interval))
        self.assertEqual(self.fake_time_begin + timeout, timer.deadline)
        self.assertTrue(timer.sleep(sleep_interval))
        # now: 12. 12 + 2 > 13, unable to sleep
        self.assertFalse(timer.sleep(sleep_interval))

        self.time_mock.sleep.assert_has_calls([pymock.call(sleep_interval)])


class test_timeout_error(unittest.TestCase):
    """Test TimeoutError.

    Test TimeoutError with three invocations format.
    """

    def test_no_args(self):
        """Create TimeoutError without arguments.
        """
        e = utils.TimeoutError()
        self.assertEqual('', str(e))
        self.assertEqual('TimeoutError()', repr(e))


    def test_with_message(self):
        """Create TimeoutError with text message.
        """
        e = utils.TimeoutError(message='Waiting for condition')
        self.assertEqual('Waiting for condition', str(e))
        self.assertEqual("TimeoutError('Waiting for condition',)", repr(e))

        # Positional message argument for backward compatibility.
        e = utils.TimeoutError('Waiting for condition')
        self.assertEqual('Waiting for condition', str(e))
        self.assertEqual("TimeoutError('Waiting for condition',)", repr(e))



    def test_with_reason(self):
        """Create TimeoutError with reason only.
        """
        e = utils.TimeoutError(reason='illegal input')
        self.assertEqual("Reason: 'illegal input'", str(e))
        self.assertEqual("TimeoutError(\"Reason: 'illegal input'\",)", repr(e))
        self.assertEqual('illegal input', e.reason)


    def test_with_message_reason(self):
        """Create TimeoutError with text message and reason.
        """
        e = utils.TimeoutError(message='Waiting for condition',
                               reason='illegal input')
        self.assertEqual("Waiting for condition. Reason: 'illegal input'",
                         str(e))
        self.assertEqual('illegal input', e.reason)

        # Positional message argument for backward compatibility.
        e = utils.TimeoutError('Waiting for condition', reason='illegal input')
        self.assertEqual("Waiting for condition. Reason: 'illegal input'",
                         str(e))
        self.assertEqual('illegal input', e.reason)


    def test_with_message_reason_object(self):
        """Create TimeoutError with text message and reason as exception object.
        """
        e = utils.TimeoutError(message='Waiting for condition',
                               reason=Exception('illegal input'))
        self.assertEqual(
            "Waiting for condition. Reason: Exception('illegal input',)",
            str(e))
        self.assertIsInstance(e.reason, Exception)
        self.assertEqual('illegal input', e.reason.message)

        # Positional message argument for backward compatibility.
        e = utils.TimeoutError('Waiting for condition',
                               reason=Exception('illegal input'))
        self.assertEqual(
            "Waiting for condition. Reason: Exception('illegal input',)",
            str(e))
        self.assertIsInstance(e.reason, Exception)
        self.assertEqual('illegal input', e.reason.message)



if __name__ == "__main__":
    unittest.main()
