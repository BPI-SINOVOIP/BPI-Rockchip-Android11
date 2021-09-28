#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable-msg=C0111

import os, unittest
import mox
import common
import shutil
import tempfile
import types
from autotest_lib.client.common_lib import control_data
from autotest_lib.server.cros.dynamic_suite import control_file_getter
from autotest_lib.server.cros.dynamic_suite import suite as suite_module
from autotest_lib.server.hosts import host_info
from autotest_lib.site_utils import test_runner_utils


class StartsWithList(mox.Comparator):
    def __init__(self, start_of_list):
        """Mox comparator which returns True if the argument
        to the mocked function is a list that begins with the elements
        in start_of_list.
        """
        self._lhs = start_of_list

    def equals(self, rhs):
        if len(rhs)<len(self._lhs):
            return False
        for (x, y) in zip(self._lhs, rhs):
            if x != y:
                return False
        return True


class ContainsSublist(mox.Comparator):
    def __init__(self, sublist):
        """Mox comparator which returns True if the argument
        to the mocked function is a list that contains sublist
        as a sub-list.
        """
        self._sublist = sublist

    def equals(self, rhs):
        n = len(self._sublist)
        if len(rhs)<n:
            return False
        return any((self._sublist == rhs[i:i+n])
                   for i in xrange(len(rhs) - n + 1))

class DummyJob(object):
    def __init__(self, id=1):
        self.id = id

class TestRunnerUnittests(mox.MoxTestBase):

    def setUp(self):
        mox.MoxTestBase.setUp(self)


    def test_fetch_local_suite(self):
        # Deferred until fetch_local_suite knows about non-local builds.
        pass


    def _results_directory_from_results_list(self, results_list):
        """Generate a temp directory filled with provided test results.

        @param results_list: List of results, each result is a tuple of strings
                             (test_name, test_status_message).
        @returns: Absolute path to the results directory.
        """
        global_dir = tempfile.mkdtemp()
        for index, (test_name, test_status_message) in enumerate(results_list):
            dir_name = '-'.join(['results',
                                 "%02.f" % (index + 1),
                                 test_name])
            local_dir = os.path.join(global_dir, dir_name)
            os.mkdir(local_dir)
            os.mkdir('%s/debug' % local_dir)
            with open("%s/status.log" % local_dir, mode='w+') as status:
                status.write(test_status_message)
                status.flush()
        return global_dir


    def test_handle_local_result_for_good_test(self):
        getter = self.mox.CreateMock(control_file_getter.DevServerGetter)
        getter.get_control_file_list(suite_name=mox.IgnoreArg()).AndReturn([])
        job = DummyJob()
        test = self.mox.CreateMock(control_data.ControlData)
        test.job_retries = 5
        self.mox.StubOutWithMock(test_runner_utils.LocalSuite,
                                 '_retry_local_result')
        self.mox.ReplayAll()
        suite = test_runner_utils.LocalSuite([], "tag", [], None, getter,
                                             job_retry=True)
        suite._retry_handler = suite_module.RetryHandler({job.id: test})

        #No calls, should not be retried
        directory = self._results_directory_from_results_list([
            ("dummy_Good", "GOOD: nonexistent test completed successfully")])
        new_id = suite.handle_local_result(
            job.id, directory,
            lambda log_entry, log_in_subdir=False: None)
        self.assertIsNone(new_id)
        shutil.rmtree(directory)


    def test_handle_local_result_for_bad_test(self):
        getter = self.mox.CreateMock(control_file_getter.DevServerGetter)
        getter.get_control_file_list(suite_name=mox.IgnoreArg()).AndReturn([])
        job = DummyJob()
        test = self.mox.CreateMock(control_data.ControlData)
        test.job_retries = 5
        self.mox.StubOutWithMock(test_runner_utils.LocalSuite,
                                 '_retry_local_result')
        test_runner_utils.LocalSuite._retry_local_result(
            job.id, mox.IgnoreArg()).AndReturn(42)
        self.mox.ReplayAll()
        suite = test_runner_utils.LocalSuite([], "tag", [], None, getter,
                                             job_retry=True)
        suite._retry_handler = suite_module.RetryHandler({job.id: test})

        directory = self._results_directory_from_results_list([
            ("dummy_Bad", "FAIL")])
        new_id = suite.handle_local_result(
            job.id, directory,
            lambda log_entry, log_in_subdir=False: None)
        self.assertIsNotNone(new_id)
        shutil.rmtree(directory)


    def test_generate_report_status_code_success_with_retries(self):
        global_dir = self._results_directory_from_results_list([
            ("dummy_Flaky", "FAIL"),
            ("dummy_Flaky", "GOOD: nonexistent test completed successfully")])
        status_code = test_runner_utils.generate_report(
            global_dir, just_status_code=True)
        self.assertEquals(status_code, 0)
        shutil.rmtree(global_dir)


    def test_generate_report_status_code_failure_with_retries(self):
        global_dir = self._results_directory_from_results_list([
            ("dummy_Good", "GOOD: nonexistent test completed successfully"),
            ("dummy_Bad", "FAIL"),
            ("dummy_Bad", "FAIL")])
        status_code = test_runner_utils.generate_report(
            global_dir, just_status_code=True)
        self.assertNotEquals(status_code, 0)
        shutil.rmtree(global_dir)


    def test_get_predicate_for_test_arg(self):
        # Assert the type signature of get_predicate_for_test(...)
        # Because control.test_utils_wrapper calls this function,
        # it is imperative for backwards compatilbility that
        # the return type of the tested function does not change.
        tests = ['dummy_test', 'e:name_expression', 'f:expression',
                 'suite:suitename']
        for test in tests:
            pred, desc = test_runner_utils.get_predicate_for_test_arg(test)
            self.assertTrue(isinstance(pred, types.FunctionType))
            self.assertTrue(isinstance(desc, str))

    def test_perform_local_run(self):
        afe = test_runner_utils.setup_local_afe()
        autotest_path = 'ottotest_path'
        suite_name = 'sweet_name'
        test_arg = 'suite:' + suite_name
        remote = 'remoat'
        build = 'bild'
        board = 'bored'
        fast_mode = False
        suite_control_files = ['c1', 'c2', 'c3', 'c4']
        results_dir = '/tmp/test_that_results_fake'
        id_digits = 1
        ssh_verbosity = 2
        ssh_options = '-F /dev/null -i /dev/null'
        args = 'matey'
        ignore_deps = False
        retry = True

        # Fake suite objects that will be returned by fetch_local_suite
        class fake_suite(object):
            def __init__(self, suite_control_files, hosts):
                self._suite_control_files = suite_control_files
                self._hosts = hosts
                self._jobs = []
                self._jobs_to_tests = {}
                self.retry_hack = True

            def schedule(self, *args, **kwargs):
                for control_file in self._suite_control_files:
                    job_id = afe.create_job(control_file, hosts=self._hosts)
                    self._jobs.append(job_id)
                    self._jobs_to_tests[job_id] = control_file

            def handle_local_result(self, job_id, results_dir, logger,
                                    **kwargs):
                if results_dir == "success_directory":
                    return None
                retries = True
                if 'retries' in kwargs:
                    retries = kwargs['retries']
                if retries and self.retry_hack:
                    self.retry_hack = False
                else:
                    return None
                control_file = self._jobs_to_tests.get(job_id)
                job_id = afe.create_job(control_file, hosts=self._hosts)
                self._jobs.append(job_id)
                self._jobs_to_tests[job_id] = control_file
                return job_id

            @property
            def jobs(self):
                return self._jobs

            def test_name_from_job(self, id):
                return ""

        # Mock out scheduling of suite and running of jobs.
        self.mox.StubOutWithMock(test_runner_utils, 'fetch_local_suite')
        test_runner_utils.fetch_local_suite(autotest_path, mox.IgnoreArg(),
                afe, test_arg=test_arg, remote=remote, build=build,
                board=board, results_directory=results_dir,
                no_experimental=False,
                ignore_deps=ignore_deps,
                job_retry=retry
                ).AndReturn(fake_suite(suite_control_files, [remote]))
        self.mox.StubOutWithMock(test_runner_utils, 'run_job')
        self.mox.StubOutWithMock(test_runner_utils, 'run_provisioning_job')
        self.mox.StubOutWithMock(test_runner_utils, '_auto_detect_labels')

        test_runner_utils._auto_detect_labels(afe, remote).AndReturn([])
        # Test perform_local_run. Enforce that run_provisioning_job,
        # run_job and _auto_detect_labels are called correctly.
        test_runner_utils.run_provisioning_job(
                'cros-version:' + build,
                remote,
                mox.IsA(host_info.HostInfo),
                autotest_path,
                results_dir,
                fast_mode,
                ssh_verbosity,
                ssh_options,
                False,
                False,
        )

        for control_file in suite_control_files:
            test_runner_utils.run_job(
                    mox.ContainsAttributeValue('control_file', control_file),
                    remote,
                    mox.IsA(host_info.HostInfo),
                    autotest_path,
                    results_dir,
                    fast_mode,
                    id_digits,
                    ssh_verbosity,
                    ssh_options,
                    mox.StrContains(args),
                    False,
                    False,
            ).AndReturn((0, '/fake/dir'))
        self.mox.ReplayAll()
        test_runner_utils.perform_local_run(
                afe, autotest_path, ['suite:'+suite_name], remote, fast_mode,
                build=build, board=board, ignore_deps=False,
                ssh_verbosity=ssh_verbosity, ssh_options=ssh_options,
                args=args, results_directory=results_dir, job_retry=retry)


if __name__ == '__main__':
    unittest.main()
