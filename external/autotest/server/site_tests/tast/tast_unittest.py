#!/usr/bin/python2
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import os
import shutil
import tempfile
import unittest
import yaml

import dateutil.parser

import common
import tast

from autotest_lib.client.common_lib import base_job
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server.hosts import host_info
from autotest_lib.server.hosts import servo_host


# Arbitrary base time to use in tests.
BASE_TIME = dateutil.parser.parse('2018-01-01T00:00:00Z')

# Arbitrary fixed time to use in place of time.time() when running tests.
NOW = BASE_TIME + datetime.timedelta(0, 60)


class TastTest(unittest.TestCase):
    """Tests the tast.tast Autotest server test.

    This unit test verifies interactions between the tast.py Autotest server
    test and the 'tast' executable that's actually responsible for running
    individual Tast tests and reporting their results. To do that, it sets up a
    fake environment in which it can run the Autotest test, including a fake
    implementation of the 'tast' executable provided by testdata/fake_tast.py.
    """

    # Arbitrary data to pass to the tast command.
    HOST = 'dut.example.net'
    PORT = 22
    TEST_PATTERNS = ['(bvt)']
    MAX_RUN_SEC = 300

    def setUp(self):
        self._temp_dir = tempfile.mkdtemp('.tast_unittest')

        def make_subdir(subdir):
            # pylint: disable=missing-docstring
            path = os.path.join(self._temp_dir, subdir)
            os.mkdir(path)
            return path

        self._job = FakeServerJob(make_subdir('job'), make_subdir('tmp'))
        self._bin_dir = make_subdir('bin')
        self._out_dir = make_subdir('out')
        self._root_dir = make_subdir('root')
        self._set_up_root()

        self._test = tast.tast(self._job, self._bin_dir, self._out_dir)
        self._host = FakeHost(self.HOST, self.PORT)

        self._test_patterns = []
        self._tast_commands = {}

    def tearDown(self):
        shutil.rmtree(self._temp_dir)

    def _get_path_in_root(self, orig_path):
        """Appends a path to self._root_dir (which stores Tast-related files).

        @param orig_path: Path to append, e.g. '/usr/bin/tast'.
        @return: Path within the root dir, e.g. '/path/to/root/usr/bin/tast'.
        """
        return os.path.join(self._root_dir, os.path.relpath(orig_path, '/'))

    def _set_up_root(self, ssp=False):
        """Creates Tast-related files and dirs within self._root_dir.

        @param ssp: If True, install files to locations used with Server-Side
            Packaging. Otherwise, install to locations used by Portage packages.
        """
        def create_file(orig_dest, src=None):
            """Creates a file under self._root_dir.

            @param orig_dest: Original absolute path, e.g. "/usr/bin/tast".
            @param src: Absolute path to file to copy, or none to create empty.
            @return: Absolute path to created file.
            """
            dest = self._get_path_in_root(orig_dest)
            if not os.path.exists(os.path.dirname(dest)):
                os.makedirs(os.path.dirname(dest))
            if src:
                shutil.copyfile(src, dest)
                shutil.copymode(src, dest)
            else:
                open(dest, 'a').close()
            return dest

        # Copy fake_tast.py to the usual location for the 'tast' executable.
        # The remote bundle dir and remote_test_runner just need to exist so
        # tast.py can find them; their contents don't matter since fake_tast.py
        # won't actually use them.
        self._tast_path = create_file(
                tast.tast._SSP_TAST_PATH if ssp else tast.tast._TAST_PATH,
                os.path.join(os.path.dirname(os.path.realpath(__file__)),
                             'testdata', 'fake_tast.py'))
        self._remote_bundle_dir = os.path.dirname(
                create_file(os.path.join(tast.tast._SSP_REMOTE_BUNDLE_DIR if ssp
                                         else tast.tast._REMOTE_BUNDLE_DIR,
                                         'fake')))
        self._remote_test_runner_path = create_file(
                tast.tast._SSP_REMOTE_TEST_RUNNER_PATH if ssp
                else tast.tast._REMOTE_TEST_RUNNER_PATH)

    def _init_tast_commands(self, tests, run_private_tests=False,
                            run_vars=[], run_varsfiles=None):
        """Sets fake_tast.py's behavior for 'list' and 'run' commands.

        @param tests: List of TestInfo objects.
        @param run_private_tests: Whether to run private tests.
        @param run_vars: List of string values that should be passed to 'run'
            via -var.
        @param run_varsfiles: filenames should be passed to 'run' via -varsfile.
        """
        list_args = [
            'build=False',
            'patterns=%s' % self.TEST_PATTERNS,
            'remotebundledir=%s' % self._remote_bundle_dir,
            'remoterunner=%s' % self._remote_test_runner_path,
            'sshretries=%d' % tast.tast._SSH_CONNECT_RETRIES,
            'target=%s:%d' % (self.HOST, self.PORT),
            'downloadprivatebundles=%s' % run_private_tests,
            'verbose=True',
        ]
        run_args = list_args + [
            'resultsdir=%s' % self._test.resultsdir,
            'continueafterfailure=True',
            'var=%s' % run_vars,
        ]
        if run_varsfiles:
            run_args.append('varsfile=%s' % run_varsfiles)

        test_list = json.dumps([t.test() for t in tests])
        run_files = {
            self._results_path(): ''.join(
                    [json.dumps(t.test_result()) + '\n'
                     for t in tests if t.start_time()]),
        }
        self._tast_commands = {
            'list': TastCommand(list_args, stdout=test_list),
            'run': TastCommand(run_args, files_to_write=run_files),
        }

    def _results_path(self):
        """Returns the path where "tast run" writes streamed results.

        @return Path to streamed results file.
        """
        return os.path.join(self._test.resultsdir,
                            tast.tast._STREAMED_RESULTS_FILENAME)

    def _run_test(self, ignore_test_failures=False, command_args=[],
                  run_private_tests=False, varsfiles=None):
        """Writes fake_tast.py's configuration and runs the test.

        @param ignore_test_failures: Passed as the identically-named arg to
            Tast.initialize().
        @param command_args: Passed as the identically-named arg to
            Tast.initialize().
        @param run_private_tests: Passed as the identically-named arg to
            Tast.initialize().
        @param varsfiles: list of names of yaml files containing variables set
             in |-varsfile| arguments.
        """
        self._test.initialize(self._host,
                              self.TEST_PATTERNS,
                              ignore_test_failures=ignore_test_failures,
                              max_run_sec=self.MAX_RUN_SEC,
                              command_args=command_args,
                              install_root=self._root_dir,
                              run_private_tests=run_private_tests,
                              varsfiles=varsfiles)
        self._test.set_fake_now_for_testing(
                (NOW - tast._UNIX_EPOCH).total_seconds())

        cfg = {}
        for name, cmd in self._tast_commands.iteritems():
            cfg[name] = vars(cmd)
        path = os.path.join(os.path.dirname(self._tast_path), 'config.json')
        with open(path, 'a') as f:
            json.dump(cfg, f)

        try:
            self._test.run_once()
        finally:
            if self._job.post_run_hook:
                self._job.post_run_hook()

    def _run_test_for_failure(self, failed, missing):
        """Calls _run_test and checks the resulting failure message.

        @param failed: List of TestInfo objects for expected-to-fail tests.
        @param missing: List of TestInfo objects for expected-missing tests.
        """
        with self.assertRaises(error.TestFail) as cm:
            self._run_test()

        msg = self._test._get_failure_message([t.name() for t in failed],
                                              [t.name() for t in missing])
        self.assertEqual(msg, str(cm.exception))

    def _load_job_keyvals(self):
        """Loads job keyvals.

        @return Keyvals as a str-to-str dict, or None if keyval file is missing.
        """
        if not os.path.exists(os.path.join(self._job.resultdir,
                                           'keyval')):
            return None
        return utils.read_keyval(self._job.resultdir)

    def testPassingTests(self):
        """Tests that passing tests are reported correctly."""
        tests = [TestInfo('pkg.Test1', 0, 2),
                 TestInfo('pkg.Test2', 3, 5),
                 TestInfo('pkg.Test3', 6, 8)]
        self._init_tast_commands(tests)
        self._run_test()
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertIs(self._load_job_keyvals(), None)

    def testFailingTests(self):
        """Tests that failing tests are reported correctly."""
        tests = [TestInfo('pkg.Test1', 0, 2, errors=[('failed', 1)]),
                 TestInfo('pkg.Test2', 3, 6),
                 TestInfo('pkg.Test2', 7, 8, errors=[('another', 7)])]
        self._init_tast_commands(tests)
        self._run_test_for_failure([tests[0], tests[2]], [])
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertIs(self._load_job_keyvals(), None)

    def testIgnoreTestFailures(self):
        """Tests that tast.tast can still pass with Tast test failures."""
        tests = [TestInfo('pkg.Test', 0, 2, errors=[('failed', 1)])]
        self._init_tast_commands(tests)
        self._run_test(ignore_test_failures=True)
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testSkippedTest(self):
        """Tests that skipped tests aren't reported."""
        tests = [TestInfo('pkg.Normal', 0, 1),
                 TestInfo('pkg.Skipped', 2, 2, skip_reason='missing deps')]
        self._init_tast_commands(tests)
        self._run_test()
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertIs(self._load_job_keyvals(), None)

    def testSkippedTestWithErrors(self):
        """Tests that skipped tests are reported if they also report errors."""
        tests = [TestInfo('pkg.Normal', 0, 1),
                 TestInfo('pkg.SkippedWithErrors', 2, 2, skip_reason='bad deps',
                          errors=[('bad deps', 2)])]
        self._init_tast_commands(tests)
        self._run_test_for_failure([tests[1]], [])
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertIs(self._load_job_keyvals(), None)

    def testMissingTests(self):
        """Tests that missing tests are reported when there's another test."""
        tests = [TestInfo('pkg.Test1', None, None),
                 TestInfo('pkg.Test2', 0, 2),
                 TestInfo('pkg.Test3', None, None)]
        self._init_tast_commands(tests)
        self._run_test_for_failure([], [tests[0], tests[2]])
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertEqual(self._load_job_keyvals(),
                         {'tast_missing_test.0': 'pkg.Test1',
                          'tast_missing_test.1': 'pkg.Test3'})

    def testNoTestsRun(self):
        """Tests that a missing test is reported when it's the only test."""
        tests = [TestInfo('pkg.Test', None, None)]
        self._init_tast_commands(tests)
        self._run_test_for_failure([], [tests[0]])
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))
        self.assertEqual(self._load_job_keyvals(),
                         {'tast_missing_test.0': 'pkg.Test'})

    def testHangingTest(self):
        """Tests that a not-finished test is reported."""
        tests = [TestInfo('pkg.Test1', 0, 2),
                 TestInfo('pkg.Test2', 3, None),
                 TestInfo('pkg.Test3', None, None)]
        self._init_tast_commands(tests)
        self._run_test_for_failure([tests[1]], [tests[2]])
        self.assertEqual(
                status_string(get_status_entries_from_tests(tests[:2])),
                status_string(self._job.status_entries))
        self.assertEqual(self._load_job_keyvals(),
                         {'tast_missing_test.0': 'pkg.Test3'})

    def testRunError(self):
        """Tests that a run error is reported for a non-finished test."""
        tests = [TestInfo('pkg.Test1', 0, 2),
                 TestInfo('pkg.Test2', 3, None),
                 TestInfo('pkg.Test3', None, None)]
        self._init_tast_commands(tests)

        # Simulate the run being aborted due to a lost SSH connection.
        path = os.path.join(self._test.resultsdir,
                            tast.tast._RUN_ERROR_FILENAME)
        msg = 'Lost SSH connection to DUT'
        self._tast_commands['run'].files_to_write[path] = msg
        self._tast_commands['run'].status = 1

        self._run_test_for_failure([tests[1]], [tests[2]])
        self.assertEqual(
                status_string(get_status_entries_from_tests(tests[:2], msg)),
                status_string(self._job.status_entries))
        self.assertEqual(self._load_job_keyvals(),
                         {'tast_missing_test.0': 'pkg.Test3'})

    def testNoTestsMatched(self):
        """Tests that an error is raised if no tests are matched."""
        self._init_tast_commands([])
        with self.assertRaises(error.TestFail) as _:
            self._run_test()

    def testListCommandFails(self):
        """Tests that an error is raised if the list command fails."""
        self._init_tast_commands([])

        # The list subcommand writes log messages to stderr on failure.
        FAILURE_MSG = "failed to connect"
        self._tast_commands['list'].status = 1
        self._tast_commands['list'].stdout = None
        self._tast_commands['list'].stderr = 'blah blah\n%s\n' % FAILURE_MSG

        # The first line of the exception should include the last line of output
        # from tast.
        with self.assertRaises(error.TestFail) as cm:
            self._run_test()
        first_line = str(cm.exception).split('\n')[0]
        self.assertTrue(FAILURE_MSG in first_line,
                        '"%s" not in "%s"' % (FAILURE_MSG, first_line))

    def testListCommandPrintsGarbage(self):
        """Tests that an error is raised if the list command prints bad data."""
        self._init_tast_commands([])
        self._tast_commands['list'].stdout = 'not valid JSON data'
        with self.assertRaises(error.TestFail) as _:
            self._run_test()

    def testRunCommandFails(self):
        """Tests that an error is raised if the run command fails."""
        tests = [TestInfo('pkg.Test1', 0, 1), TestInfo('pkg.Test2', 2, 3)]
        self._init_tast_commands(tests)
        FAILURE_MSG = "this is the failure"
        self._tast_commands['run'].status = 1
        self._tast_commands['run'].stdout = 'blah blah\n%s\n' % FAILURE_MSG

        with self.assertRaises(error.TestFail) as cm:
            self._run_test()
        self.assertTrue(FAILURE_MSG in str(cm.exception),
                        '"%s" not in "%s"' % (FAILURE_MSG, str(cm.exception)))
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testRunCommandWritesTrailingGarbage(self):
        """Tests that an error is raised if the run command prints bad data."""
        tests = [TestInfo('pkg.Test1', 0, 1), TestInfo('pkg.Test2', None, None)]
        self._init_tast_commands(tests)
        self._tast_commands['run'].files_to_write[self._results_path()] += \
                'not valid JSON data'
        with self.assertRaises(error.TestFail) as _:
            self._run_test()
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testNoResultsFile(self):
        """Tests that an error is raised if no results file is written."""
        tests = [TestInfo('pkg.Test1', None, None)]
        self._init_tast_commands(tests)
        self._tast_commands['run'].files_to_write = {}
        with self.assertRaises(error.TestFail) as _:
            self._run_test()
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testNoResultsFileAfterRunCommandFails(self):
        """Tests that stdout is included in error after missing results."""
        tests = [TestInfo('pkg.Test1', None, None)]
        self._init_tast_commands(tests)
        FAILURE_MSG = "this is the failure"
        self._tast_commands['run'].status = 1
        self._tast_commands['run'].files_to_write = {}
        self._tast_commands['run'].stdout = 'blah blah\n%s\n' % FAILURE_MSG

        # The first line of the exception should include the last line of output
        # from tast rather than a message about the missing results file.
        with self.assertRaises(error.TestFail) as cm:
            self._run_test()
        first_line = str(cm.exception).split('\n')[0]
        self.assertTrue(FAILURE_MSG in first_line,
                        '"%s" not in "%s"' % (FAILURE_MSG, first_line))
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testMissingTastExecutable(self):
        """Tests that an error is raised if the tast command isn't found."""
        os.remove(self._get_path_in_root(tast.tast._TAST_PATH))
        with self.assertRaises(error.TestFail) as _:
            self._run_test()

    def testMissingRemoteTestRunner(self):
        """Tests that an error is raised if remote_test_runner isn't found."""
        os.remove(self._get_path_in_root(tast.tast._REMOTE_TEST_RUNNER_PATH))
        with self.assertRaises(error.TestFail) as _:
            self._run_test()

    def testMissingRemoteBundleDir(self):
        """Tests that an error is raised if remote bundles aren't found."""
        shutil.rmtree(self._get_path_in_root(tast.tast._REMOTE_BUNDLE_DIR))
        with self.assertRaises(error.TestFail) as _:
            self._run_test()

    def testSspPaths(self):
        """Tests that files can be located at their alternate SSP locations."""
        for p in os.listdir(self._root_dir):
            shutil.rmtree(os.path.join(self._root_dir, p))
        self._set_up_root(ssp=True)

        tests = [TestInfo('pkg.Test', 0, 1)]
        self._init_tast_commands(tests)
        self._run_test()
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testFailureMessage(self):
        """Tests that appropriate failure messages are generated."""
        # Just do this to initialize the self._test.
        self._init_tast_commands([TestInfo('pkg.Test', 0, 0)])
        self._run_test()

        msg = lambda f, m: self._test._get_failure_message(f, m)
        self.assertEqual('', msg([], []))
        self.assertEqual('1 failed: t1', msg(['t1'], []))
        self.assertEqual('2 failed: t1 t2', msg(['t1', 't2'], []))
        self.assertEqual('1 missing: t1', msg([], ['t1']))
        self.assertEqual('1 failed: t1; 1 missing: t2', msg(['t1'], ['t2']))

    def testFailureMessageIgnoreTestFailures(self):
        """Tests that test failures are ignored in messages when requested."""
        # Just do this to initialize the self._test.
        self._init_tast_commands([TestInfo('pkg.Test', 0, 0)])
        self._run_test(ignore_test_failures=True)

        msg = lambda f, m: self._test._get_failure_message(f, m)
        self.assertEqual('', msg([], []))
        self.assertEqual('', msg(['t1'], []))
        self.assertEqual('1 missing: t1', msg([], ['t1']))
        self.assertEqual('1 missing: t2', msg(['t1'], ['t2']))

    def testNonAsciiFailureMessage(self):
        """Tests that non-ascii failure message should be handled correctly"""
        tests = [TestInfo('pkg.Test', 0, 2, errors=[('失敗', 1)])]
        self._init_tast_commands(tests)
        self._run_test(ignore_test_failures=True)
        self.assertEqual(status_string(get_status_entries_from_tests(tests)),
                         status_string(self._job.status_entries))

    def testRunPrivateTests(self):
        """Tests running private tests."""
        self._init_tast_commands([TestInfo('pkg.Test', 0, 0)],
                                 run_private_tests=True)
        self._run_test(ignore_test_failures=True, run_private_tests=True)

    def testServoFromCommandArgs(self):
        """Tests passing servo info via command-line arg."""
        SERVO_HOST = 'chromeos6-row2-rack21-labstation1.cros'
        SERVO_PORT = '9995'

        servo_var = 'servo=%s:%s' % (SERVO_HOST, SERVO_PORT)
        self._init_tast_commands([TestInfo('pkg.Test', 0, 0)],
                                 run_vars=[servo_var])

        # Simulate servo info being passed on the command line via --args.
        args = [
            '%s=%s' % (servo_host.SERVO_HOST_ATTR, SERVO_HOST),
            '%s=%s' % (servo_host.SERVO_PORT_ATTR, SERVO_PORT),
        ]
        self._run_test(command_args=args)

    def testServoFromHostInfoStore(self):
        """Tests getting servo info from the host."""
        SERVO_HOST = 'chromeos6-row2-rack21-labstation1.cros'
        SERVO_PORT = '9995'

        servo_var = 'servo=%s:%s' % (SERVO_HOST, SERVO_PORT)
        self._init_tast_commands([TestInfo('pkg.Test', 0, 0)],
                                 run_vars=[servo_var])

        # Simulate the host's servo info being stored in the Autotest DB.
        attr = {
            servo_host.SERVO_HOST_ATTR: SERVO_HOST,
            servo_host.SERVO_PORT_ATTR: SERVO_PORT,
        }
        self._host.host_info_store.commit(host_info.HostInfo(attributes=attr))
        self._run_test()

    def testVarsfileOption(self):
        with tempfile.NamedTemporaryFile(
                suffix='.yaml', dir=self._temp_dir) as temp_file:
            yaml.dump({"var1": "val1", "var2": "val2"}, stream=temp_file)
            varsfiles = [temp_file.name]
            self._init_tast_commands([TestInfo('pkg.Test', 0, 0)],
                                     run_varsfiles=varsfiles)
            self._run_test(varsfiles=varsfiles)


class TestInfo:
    """Wraps information about a Tast test.

    This struct is used to:
    - get test definitions printed by fake_tast.py's 'list' command
    - get test results written by fake_tast.py's 'run' command
    - get expected base_job.status_log_entry objects that unit tests compare
      against what tast.Tast actually recorded
    """
    def __init__(self, name, start_offset, end_offset, errors=None,
                 skip_reason=None, attr=None, timeout_ns=0):
        """
        @param name: Name of the test, e.g. 'ui.ChromeLogin'.
        @param start_offset: Start time as int seconds offset from BASE_TIME,
            or None to indicate that tast didn't report a result for this test.
        @param end_offset: End time as int seconds offset from BASE_TIME, or
            None to indicate that tast reported that this test started but not
            that it finished.
        @param errors: List of (string, int) tuples containing reasons and
            seconds offsets of errors encountered while running the test, or
            None if no errors were encountered.
        @param skip_reason: Human-readable reason that the test was skipped, or
            None to indicate that it wasn't skipped.
        @param attr: List of string test attributes assigned to the test, or
            None if no attributes are assigned.
        @param timeout_ns: Test timeout in nanoseconds.
        """
        def from_offset(offset):
            """Returns an offset from BASE_TIME.

            @param offset: Offset as integer seconds.
            @return: datetime.datetime object.
            """
            if offset is None:
                return None
            return BASE_TIME + datetime.timedelta(seconds=offset)

        self._name = name
        self._start_time = from_offset(start_offset)
        self._end_time = from_offset(end_offset)
        self._errors = (
                [(e[0], from_offset(e[1])) for e in errors] if errors else [])
        self._skip_reason = skip_reason
        self._attr = list(attr) if attr else []
        self._timeout_ns = timeout_ns

    def name(self):
        # pylint: disable=missing-docstring
        return self._name

    def start_time(self):
        # pylint: disable=missing-docstring
        return self._start_time

    def test(self):
        """Returns a test dict printed by the 'list' command.

        @return: dict representing a Tast testing.Test struct.
        """
        return {
            'name': self._name,
            'attr': self._attr,
            'timeout': self._timeout_ns,
        }

    def test_result(self):
        """Returns a dict representing a result written by the 'run' command.

        @return: dict representing a Tast TestResult struct.
        """
        return {
            'name': self._name,
            'start': to_rfc3339(self._start_time),
            'end': to_rfc3339(self._end_time),
            'errors': [{'reason': e[0], 'time': to_rfc3339(e[1])}
                       for e in self._errors],
            'skipReason': self._skip_reason,
            'attr': self._attr,
            'timeout': self._timeout_ns,
        }

    def status_entries(self, run_error_msg=None):
        """Returns expected base_job.status_log_entry objects for this test.

        @param run_error_msg: String containing run error message, or None if no
            run error was encountered.
        @return: List of Autotest base_job.status_log_entry objects.
        """
        # Deliberately-skipped tests shouldn't have status entries unless errors
        # were also reported.
        if self._skip_reason and not self._errors:
            return []

        # Tests that weren't even started (e.g. because of an earlier issue)
        # shouldn't have status entries.
        if not self._start_time:
            return []

        def make(status_code, dt, msg=''):
            """Makes a base_job.status_log_entry.

            @param status_code: String status code.
            @param dt: datetime.datetime object containing entry time.
            @param msg: String message (typically only set for errors).
            @return: base_job.status_log_entry object.
            """
            timestamp = int((dt - tast._UNIX_EPOCH).total_seconds())
            return base_job.status_log_entry(
                    status_code, None,
                    tast.tast._TEST_NAME_PREFIX + self._name, msg, None,
                    timestamp=timestamp)

        entries = [make(tast.tast._JOB_STATUS_START, self._start_time)]

        if self._end_time and not self._errors:
            entries.append(make(tast.tast._JOB_STATUS_GOOD, self._end_time))
            entries.append(make(tast.tast._JOB_STATUS_END_GOOD, self._end_time))
        else:
            for e in self._errors:
                entries.append(make(tast.tast._JOB_STATUS_FAIL, e[1], e[0]))
            if not self._end_time:
                # If the test didn't finish, the run error (if any) should be
                # included.
                if run_error_msg:
                    entries.append(make(tast.tast._JOB_STATUS_FAIL,
                                        self._start_time, run_error_msg))
                entries.append(make(tast.tast._JOB_STATUS_FAIL,
                                    self._start_time,
                                    tast.tast._TEST_DID_NOT_FINISH_MSG))
            entries.append(make(tast.tast._JOB_STATUS_END_FAIL,
                                self._end_time or self._start_time or NOW))

        return entries


class FakeServerJob:
    """Fake implementation of server_job from server/server_job.py."""
    def __init__(self, result_dir, tmp_dir):
        self.pkgmgr = None
        self.autodir = None
        self.resultdir = result_dir
        self.tmpdir = tmp_dir
        self.post_run_hook = None
        self.status_entries = []

    def add_post_run_hook(self, hook):
        """Stub implementation of server_job.add_post_run_hook."""
        self.post_run_hook = hook

    def record_entry(self, entry, log_in_subdir=True):
        """Stub implementation of server_job.record_entry."""
        assert(not log_in_subdir)
        self.status_entries.append(entry)


class FakeHost:
    """Fake implementation of AbstractSSHHost from server/hosts/abstract_ssh.py.
    """
    def __init__(self, hostname, port):
        self.hostname = hostname
        self.port = port
        self.host_info_store = host_info.InMemoryHostInfoStore(None)


class TastCommand(object):
    """Args and behavior for fake_tast.py for a given command, e.g. "list"."""

    def __init__(self, required_args, status=0, stdout=None, stderr=None,
                 files_to_write=None):
        """
        @param required_args: List of required args, each specified as
            'name=value'. Names correspond to argparse-provided names in
            fake_tast.py (typically just the flag name, e.g. 'build' or
            'resultsdir'). Values correspond to str() representations of the
            argparse-provided values.
        @param status: Status code for fake_tast.py to return.
        @param stdout: Data to write to stdout.
        @param stderr: Data to write to stderr.
        @param files_to_write: Dict mapping from paths of files to write to
            their contents, or None to not write any files.
        """
        self.required_args = required_args
        self.status = status
        self.stdout = stdout
        self.stderr = stderr
        self.files_to_write = files_to_write if files_to_write else {}


def to_rfc3339(t):
    """Returns an RFC3339 timestamp.

    @param t: UTC datetime.datetime object or None for the zero time.
    @return: String RFC3339 time, e.g. '2018-01-02T02:34:28Z'.
    """
    if t is None:
        return '0001-01-01T00:00:00Z'
    assert(not t.utcoffset())
    return t.strftime('%Y-%m-%dT%H:%M:%SZ')


def get_status_entries_from_tests(tests, run_error_msg=None):
    """Returns a flattened list of status entries from TestInfo objects.

    @param tests: List of TestInfo objects.
    @param run_error_msg: String containing run error message, or None if no
        run error was encountered.
    @return: Flattened list of base_job.status_log_entry objects produced by
        calling status_entries() on each TestInfo object.
    """
    return sum([t.status_entries(run_error_msg) for t in tests], [])


def status_string(entries):
    """Returns a string describing a list of base_job.status_log_entry objects.

    @param entries: List of base_job.status_log_entry objects.
    @return: String containing space-separated representations of entries.
    """
    strings = []
    for entry in entries:
        timestamp = entry.fields[base_job.status_log_entry.TIMESTAMP_FIELD]
        s = '[%s %s %s %s]' % (timestamp, entry.operation, entry.status_code,
                               repr(str(entry.message)))
        strings.append(s)

    return ' '.join(strings)


if __name__ == '__main__':
    unittest.main()
