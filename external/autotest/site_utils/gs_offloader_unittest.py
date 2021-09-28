#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import __builtin__
import Queue
import json
import logging
import os
import shutil
import signal
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest

import mock
import mox

import common
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib import utils
#For unittest without cloud_client.proto compiled.
try:
    from autotest_lib.site_utils import cloud_console_client
except ImportError:
    cloud_console_client = None
from autotest_lib.site_utils import gs_offloader
from autotest_lib.site_utils import job_directories
from autotest_lib.site_utils import job_directories_unittest as jd_test
from autotest_lib.tko import models
from autotest_lib.utils import gslib
from autotest_lib.site_utils import pubsub_utils
from chromite.lib import timeout_util

# Test value to use for `days_old`, if nothing else is required.
_TEST_EXPIRATION_AGE = 7


def _get_options(argv):
    """Helper function to exercise command line parsing.

    @param argv Value of sys.argv to be parsed.

    """
    sys.argv = ['bogus.py'] + argv
    return gs_offloader.parse_options()


def is_fifo(path):
  """Determines whether a path is a fifo.

  @param path: fifo path string.
  """
  return stat.S_ISFIFO(os.lstat(path).st_mode)


def _get_fake_process():
  return FakeProcess()


class FakeProcess(object):
    """Fake process object."""

    def __init__(self):
        self.returncode = 0


    def wait(self):
        return True


class OffloaderOptionsTests(mox.MoxTestBase):
    """Tests for the `Offloader` constructor.

    Tests that offloader instance fields are set as expected
    for given command line options.

    """

    _REGULAR_ONLY = {job_directories.SwarmingJobDirectory,
                     job_directories.RegularJobDirectory}
    _SPECIAL_ONLY = {job_directories.SwarmingJobDirectory,
                     job_directories.SpecialJobDirectory}
    _BOTH = _REGULAR_ONLY | _SPECIAL_ONLY


    def setUp(self):
        super(OffloaderOptionsTests, self).setUp()
        self.mox.StubOutWithMock(utils, 'get_offload_gsuri')
        gs_offloader.GS_OFFLOADING_ENABLED = True
        gs_offloader.GS_OFFLOADER_MULTIPROCESSING = False


    def _mock_get_sub_offloader(self, is_moblab, multiprocessing=False,
                               console_client=None, delete_age=0):
        """Mock the process of getting the offload_dir function."""
        if is_moblab:
            expected_gsuri = '%sresults/%s/%s/' % (
                    global_config.global_config.get_config_value(
                            'CROS', 'image_storage_server'),
                    'Fa:ke:ma:c0:12:34', 'rand0m-uu1d')
        else:
            expected_gsuri = utils.DEFAULT_OFFLOAD_GSURI
        utils.get_offload_gsuri().AndReturn(expected_gsuri)
        sub_offloader = gs_offloader.GSOffloader(expected_gsuri,
            multiprocessing, delete_age, console_client)
        self.mox.StubOutWithMock(gs_offloader, 'GSOffloader')
        if cloud_console_client:
            self.mox.StubOutWithMock(cloud_console_client,
                    'is_cloud_notification_enabled')
        if console_client:
            cloud_console_client.is_cloud_notification_enabled().AndReturn(True)
            gs_offloader.GSOffloader(
                    expected_gsuri, multiprocessing, delete_age,
                    mox.IsA(cloud_console_client.PubSubBasedClient)).AndReturn(
                        sub_offloader)
        else:
            if cloud_console_client:
                cloud_console_client.is_cloud_notification_enabled().AndReturn(
                        False)
            gs_offloader.GSOffloader(
                expected_gsuri, multiprocessing, delete_age, None).AndReturn(
                    sub_offloader)
        self.mox.ReplayAll()
        return sub_offloader


    def test_process_no_options(self):
        """Test default offloader options."""
        sub_offloader = self._mock_get_sub_offloader(False)
        offloader = gs_offloader.Offloader(_get_options([]))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._REGULAR_ONLY)
        self.assertEqual(offloader._processes, 1)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_process_all_option(self):
        """Test offloader handling for the --all option."""
        sub_offloader = self._mock_get_sub_offloader(False)
        offloader = gs_offloader.Offloader(_get_options(['--all']))
        self.assertEqual(set(offloader._jobdir_classes), self._BOTH)
        self.assertEqual(offloader._processes, 1)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_process_hosts_option(self):
        """Test offloader handling for the --hosts option."""
        sub_offloader = self._mock_get_sub_offloader(False)
        offloader = gs_offloader.Offloader(
                _get_options(['--hosts']))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._SPECIAL_ONLY)
        self.assertEqual(offloader._processes, 1)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_parallelism_option(self):
        """Test offloader handling for the --parallelism option."""
        sub_offloader = self._mock_get_sub_offloader(False)
        offloader = gs_offloader.Offloader(
                _get_options(['--parallelism', '2']))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._REGULAR_ONLY)
        self.assertEqual(offloader._processes, 2)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_delete_only_option(self):
        """Test offloader handling for the --delete_only option."""
        offloader = gs_offloader.Offloader(
                _get_options(['--delete_only']))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._REGULAR_ONLY)
        self.assertEqual(offloader._processes, 1)
        self.assertIsInstance(offloader._gs_offloader,
                              gs_offloader.FakeGSOffloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_days_old_option(self):
        """Test offloader handling for the --days_old option."""
        sub_offloader = self._mock_get_sub_offloader(False, delete_age=7)
        offloader = gs_offloader.Offloader(
                _get_options(['--days_old', '7']))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._REGULAR_ONLY)
        self.assertEqual(offloader._processes, 1)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 7)
        self.assertEqual(offloader._delete_age_limit, 7)


    def test_moblab_gsuri_generation(self):
        """Test offloader construction for Moblab."""
        sub_offloader = self._mock_get_sub_offloader(True)
        offloader = gs_offloader.Offloader(_get_options([]))
        self.assertEqual(set(offloader._jobdir_classes),
                         self._REGULAR_ONLY)
        self.assertEqual(offloader._processes, 1)
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.assertEqual(offloader._upload_age_limit, 0)
        self.assertEqual(offloader._delete_age_limit, 0)


    def test_globalconfig_offloading_flag(self):
        """Test enabling of --delete_only via global_config."""
        gs_offloader.GS_OFFLOADING_ENABLED = False
        offloader = gs_offloader.Offloader(
                _get_options([]))
        self.assertIsInstance(offloader._gs_offloader,
                             gs_offloader.FakeGSOffloader)

    def test_offloader_multiprocessing_flag_set(self):
        """Test multiprocessing is set."""
        sub_offloader = self._mock_get_sub_offloader(True, True)
        offloader = gs_offloader.Offloader(_get_options(['-m']))
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.mox.VerifyAll()

    def test_offloader_multiprocessing_flag_not_set_default_false(self):
        """Test multiprocessing is set."""
        gs_offloader.GS_OFFLOADER_MULTIPROCESSING = False
        sub_offloader = self._mock_get_sub_offloader(True, False)
        offloader = gs_offloader.Offloader(_get_options([]))
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.mox.VerifyAll()

    def test_offloader_multiprocessing_flag_not_set_default_true(self):
        """Test multiprocessing is set."""
        gs_offloader.GS_OFFLOADER_MULTIPROCESSING = True
        sub_offloader = self._mock_get_sub_offloader(True, True)
        offloader = gs_offloader.Offloader(_get_options([]))
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.mox.VerifyAll()


    def test_offloader_pubsub_enabled(self):
        """Test multiprocessing is set."""
        if not cloud_console_client:
            return
        self.mox.StubOutWithMock(pubsub_utils, "PubSubClient")
        sub_offloader = self._mock_get_sub_offloader(True, False,
                cloud_console_client.PubSubBasedClient())
        offloader = gs_offloader.Offloader(_get_options([]))
        self.assertEqual(offloader._gs_offloader,
                         sub_offloader)
        self.mox.VerifyAll()


class _MockJobDirectory(job_directories._JobDirectory):
    """Subclass of `_JobDirectory` used as a helper for tests."""

    GLOB_PATTERN = '[0-9]*-*'


    def __init__(self, resultsdir):
        """Create new job in initial state."""
        super(_MockJobDirectory, self).__init__(resultsdir)
        self._timestamp = None
        self.queue_args = [resultsdir, os.path.dirname(resultsdir), self._timestamp]


    def get_timestamp_if_finished(self):
        return self._timestamp


    def set_finished(self, days_old):
        """Make this job appear to be finished.

        After calling this function, calls to `enqueue_offload()`
        will find this job as finished, but not expired and ready
        for offload.  Note that when `days_old` is 0,
        `enqueue_offload()` will treat a finished job as eligible
        for offload.

        @param days_old The value of the `days_old` parameter that
                        will be passed to `enqueue_offload()` for
                        testing.

        """
        self._timestamp = jd_test.make_timestamp(days_old, False)
        self.queue_args[2] = self._timestamp


    def set_expired(self, days_old):
        """Make this job eligible to be offloaded.

        After calling this function, calls to `offload` will attempt
        to offload this job.

        @param days_old The value of the `days_old` parameter that
                        will be passed to `enqueue_offload()` for
                        testing.

        """
        self._timestamp = jd_test.make_timestamp(days_old, True)
        self.queue_args[2] = self._timestamp


    def set_incomplete(self):
        """Make this job appear to have failed offload just once."""
        self.offload_count += 1
        self.first_offload_start = time.time()
        if not os.path.isdir(self.dirname):
            os.mkdir(self.dirname)


    def set_reportable(self):
        """Make this job be reportable."""
        self.set_incomplete()
        self.offload_count += 1


    def set_complete(self):
        """Make this job be completed."""
        self.offload_count += 1
        if os.path.isdir(self.dirname):
            os.rmdir(self.dirname)


    def process_gs_instructions(self):
        """Always still offload the job directory."""
        return True


class CommandListTests(unittest.TestCase):
    """Tests for `_get_cmd_list()`."""

    def _command_list_assertions(self, job, use_rsync=True, multi=False):
        """Call `_get_cmd_list()` and check the return value.

        Check the following assertions:
          * The command name (argv[0]) is 'gsutil'.
          * '-m' option (argv[1]) is on when the argument, multi, is True.
          * The arguments contain the 'cp' subcommand.
          * The next-to-last argument (the source directory) is the
            job's `queue_args[0]`.
          * The last argument (the destination URL) is the job's
            'queue_args[1]'.

        @param job A job with properly calculated arguments to
                   `_get_cmd_list()`
        @param use_rsync True when using 'rsync'. False when using 'cp'.
        @param multi True when using '-m' option for gsutil.

        """
        test_bucket_uri = 'gs://a-test-bucket'

        gs_offloader.USE_RSYNC_ENABLED = use_rsync

        gs_path = os.path.join(test_bucket_uri, job.queue_args[1])

        command = gs_offloader._get_cmd_list(
                multi, job.queue_args[0], gs_path)

        self.assertEqual(command[0], 'gsutil')
        if multi:
            self.assertEqual(command[1], '-m')
        self.assertEqual(command[-2], job.queue_args[0])

        if use_rsync:
            self.assertTrue('rsync' in command)
            self.assertEqual(command[-1],
                             os.path.join(test_bucket_uri, job.queue_args[0]))
        else:
            self.assertTrue('cp' in command)
            self.assertEqual(command[-1],
                             os.path.join(test_bucket_uri, job.queue_args[1]))

        finish_command = gs_offloader._get_finish_cmd_list(gs_path)
        self.assertEqual(finish_command[0], 'gsutil')
        self.assertEqual(finish_command[1], 'cp')
        self.assertEqual(finish_command[2], '/dev/null')
        self.assertEqual(finish_command[3],
                         os.path.join(gs_path, '.finished_offload'))


    def test__get_cmd_list_regular(self):
        """Test `_get_cmd_list()` as for a regular job."""
        job = _MockJobDirectory('118-debug')
        self._command_list_assertions(job)


    def test__get_cmd_list_special(self):
        """Test `_get_cmd_list()` as for a special job."""
        job = _MockJobDirectory('hosts/host1/118-reset')
        self._command_list_assertions(job)


    def test_get_cmd_list_regular_no_rsync(self):
        """Test `_get_cmd_list()` as for a regular job."""
        job = _MockJobDirectory('118-debug')
        self._command_list_assertions(job, use_rsync=False)


    def test_get_cmd_list_special_no_rsync(self):
        """Test `_get_cmd_list()` as for a special job."""
        job = _MockJobDirectory('hosts/host1/118-reset')
        self._command_list_assertions(job, use_rsync=False)


    def test_get_cmd_list_regular_multi(self):
        """Test `_get_cmd_list()` as for a regular job with True multi."""
        job = _MockJobDirectory('118-debug')
        self._command_list_assertions(job, multi=True)


    def test__get_cmd_list_special_multi(self):
        """Test `_get_cmd_list()` as for a special job with True multi."""
        job = _MockJobDirectory('hosts/host1/118-reset')
        self._command_list_assertions(job, multi=True)


class _TempResultsDirTestCase(unittest.TestCase):
    """Mixin class for tests using a temporary results directory."""

    REGULAR_JOBLIST = [
        '111-fubar', '112-fubar', '113-fubar', '114-snafu']
    HOST_LIST = ['host1', 'host2', 'host3']
    SPECIAL_JOBLIST = [
        'hosts/host1/333-reset', 'hosts/host1/334-reset',
        'hosts/host2/444-reset', 'hosts/host3/555-reset']


    def setUp(self):
        super(_TempResultsDirTestCase, self).setUp()
        self._resultsroot = tempfile.mkdtemp()
        self._cwd = os.getcwd()
        os.chdir(self._resultsroot)


    def tearDown(self):
        os.chdir(self._cwd)
        shutil.rmtree(self._resultsroot)
        super(_TempResultsDirTestCase, self).tearDown()


    def make_job(self, jobdir):
        """Create a job with results in `self._resultsroot`.

        @param jobdir Name of the subdirectory to be created in
                      `self._resultsroot`.

        """
        os.makedirs(jobdir)
        return _MockJobDirectory(jobdir)


    def make_job_hierarchy(self):
        """Create a sample hierarchy of job directories.

        `self.REGULAR_JOBLIST` is a list of directories for regular
        jobs to be created; `self.SPECIAL_JOBLIST` is a list of
        directories for special jobs to be created.

        """
        for d in self.REGULAR_JOBLIST:
            os.mkdir(d)
        hostsdir = 'hosts'
        os.mkdir(hostsdir)
        for host in self.HOST_LIST:
            os.mkdir(os.path.join(hostsdir, host))
        for d in self.SPECIAL_JOBLIST:
            os.mkdir(d)


class _TempResultsDirTestBase(_TempResultsDirTestCase, mox.MoxTestBase):
    """Base Mox test class for tests using a temporary results directory."""


class FailedOffloadsLogTest(_TempResultsDirTestBase):
    """Test the formatting of failed offloads log file."""
    # Below is partial sample of a failed offload log file.  This text is
    # deliberately hard-coded and then parsed to create the test data; the idea
    # is to make sure the actual text format will be reviewed by a human being.
    #
    # first offload      count  directory
    # --+----1----+----  ----+ ----+----1----+----2----+----3
    _SAMPLE_DIRECTORIES_REPORT = '''\
    =================== ======  ==============================
    2014-03-14 15:09:26      1  118-fubar
    2014-03-14 15:19:23      2  117-fubar
    2014-03-14 15:29:20      6  116-fubar
    2014-03-14 15:39:17     24  115-fubar
    2014-03-14 15:49:14    120  114-fubar
    2014-03-14 15:59:11    720  113-fubar
    2014-03-14 16:09:08   5040  112-fubar
    2014-03-14 16:19:05  40320  111-fubar
    '''

    def setUp(self):
        super(FailedOffloadsLogTest, self).setUp()
        self._offloader = gs_offloader.Offloader(_get_options([]))
        self._joblist = []
        for line in self._SAMPLE_DIRECTORIES_REPORT.split('\n')[1 : -1]:
            date_, time_, count, dir_ = line.split()
            job = _MockJobDirectory(dir_)
            job.offload_count = int(count)
            timestruct = time.strptime("%s %s" % (date_, time_),
                                       gs_offloader.FAILED_OFFLOADS_TIME_FORMAT)
            job.first_offload_start = time.mktime(timestruct)
            # enter the jobs in reverse order, to make sure we
            # test that the output will be sorted.
            self._joblist.insert(0, job)


    def assert_report_well_formatted(self, report_file):
        """Assert that report file is well formatted.

        @param report_file: Path to report file
        """
        with open(report_file, 'r') as f:
            report_lines = f.read().split()

        for end_of_header_index in range(len(report_lines)):
            if report_lines[end_of_header_index].startswith('=='):
                break
        self.assertLess(end_of_header_index, len(report_lines),
                        'Failed to find end-of-header marker in the report')

        relevant_lines = report_lines[end_of_header_index:]
        expected_lines = self._SAMPLE_DIRECTORIES_REPORT.split()
        self.assertListEqual(relevant_lines, expected_lines)


    def test_failed_offload_log_format(self):
        """Trigger an e-mail report and check its contents."""
        log_file = os.path.join(self._resultsroot, 'failed_log')
        report = self._offloader._log_failed_jobs_locally(self._joblist,
                                                          log_file=log_file)
        self.assert_report_well_formatted(log_file)


    def test_failed_offload_file_overwrite(self):
        """Verify that we can saefly overwrite the log file."""
        log_file = os.path.join(self._resultsroot, 'failed_log')
        with open(log_file, 'w') as f:
            f.write('boohoohoo')
        report = self._offloader._log_failed_jobs_locally(self._joblist,
                                                          log_file=log_file)
        self.assert_report_well_formatted(log_file)


class OffloadDirectoryTests(_TempResultsDirTestBase):
    """Tests for `offload_dir()`."""

    def setUp(self):
        super(OffloadDirectoryTests, self).setUp()
        # offload_dir() logs messages; silence them.
        self._saved_loglevel = logging.getLogger().getEffectiveLevel()
        logging.getLogger().setLevel(logging.CRITICAL+1)
        self._job = self.make_job(self.REGULAR_JOBLIST[0])
        self.mox.StubOutWithMock(gs_offloader, '_get_cmd_list')
        alarm = mock.patch('signal.alarm', return_value=0)
        alarm.start()
        self.addCleanup(alarm.stop)
        self.mox.StubOutWithMock(models.test, 'parse_job_keyval')
        self.should_remove_sarming_req_dir = False


    def tearDown(self):
        logging.getLogger().setLevel(self._saved_loglevel)
        super(OffloadDirectoryTests, self).tearDown()

    def _mock__upload_cts_testresult(self):
        self.mox.StubOutWithMock(gs_offloader, '_upload_cts_testresult')
        gs_offloader._upload_cts_testresult(
                mox.IgnoreArg(),mox.IgnoreArg()).AndReturn(None)

    def _mock_create_marker_file(self):
        self.mox.StubOutWithMock(__builtin__, 'open')
        open(mox.IgnoreArg(), 'a').AndReturn(mock.MagicMock())


    def _mock_offload_dir_calls(self, command, queue_args,
                                marker_initially_exists=False):
        """Mock out the calls needed by `offload_dir()`.

        This covers only the calls made when there is no timeout.

        @param command Command list to be returned by the mocked
                       call to `_get_cmd_list()`.

        """
        self.mox.StubOutWithMock(os.path, 'isfile')
        os.path.isfile(mox.IgnoreArg()).AndReturn(marker_initially_exists)
        command.append(queue_args[0])
        gs_offloader._get_cmd_list(
                False, queue_args[0],
                '%s%s' % (utils.DEFAULT_OFFLOAD_GSURI,
                          queue_args[1])).AndReturn(command)
        self._mock__upload_cts_testresult()


    def _run_offload_dir(self, should_succeed, delete_age):
        """Make one call to `offload_dir()`.

        The caller ensures all mocks are set up already.

        @param should_succeed True iff the call to `offload_dir()`
                              is expected to succeed and remove the
                              offloaded job directory.

        """
        self.mox.ReplayAll()
        gs_offloader.GSOffloader(
                utils.DEFAULT_OFFLOAD_GSURI, False, delete_age).offload(
                        self._job.queue_args[0],
                        self._job.queue_args[1],
                        self._job.queue_args[2])
        self.mox.VerifyAll()
        self.assertEqual(not should_succeed,
                         os.path.isdir(self._job.queue_args[0]))
        swarming_req_dir = gs_offloader._get_swarming_req_dir(
                self._job.queue_args[0])
        if swarming_req_dir:
            self.assertEqual(not self.should_remove_sarming_req_dir,
                             os.path.exists(swarming_req_dir))


    def test_offload_success(self):
        """Test that `offload_dir()` can succeed correctly."""
        self._mock_offload_dir_calls(['test', '-d'],
                                     self._job.queue_args)
        os.path.isfile(mox.IgnoreArg()).AndReturn(True)
        self._mock_create_marker_file()
        self._run_offload_dir(True, 0)


    def test_offload_failure(self):
        """Test that `offload_dir()` can fail correctly."""
        self._mock_offload_dir_calls(['test', '!', '-d'],
                                     self._job.queue_args)
        self._run_offload_dir(False, 0)


    def test_offload_swarming_req_dir_remove(self):
        """Test that `offload_dir()` can prune the empty swarming task dir."""
        should_remove = os.path.join('results', 'swarming-123abc0')
        self._job = self.make_job(os.path.join(should_remove, '1'))
        self._mock_offload_dir_calls(['test', '-d'],
                                     self._job.queue_args)

        os.path.isfile(mox.IgnoreArg()).AndReturn(True)
        self.should_remove_sarming_req_dir = True
        self._mock_create_marker_file()
        self._run_offload_dir(True, 0)


    def test_offload_swarming_req_dir_exist(self):
        """Test that `offload_dir()` keeps the non-empty swarming task dir."""
        should_not_remove = os.path.join('results', 'swarming-456edf0')
        self._job = self.make_job(os.path.join(should_not_remove, '1'))
        self.make_job(os.path.join(should_not_remove, '2'))
        self._mock_offload_dir_calls(['test', '-d'],
                                     self._job.queue_args)

        os.path.isfile(mox.IgnoreArg()).AndReturn(True)
        self.should_remove_sarming_req_dir = False
        self._mock_create_marker_file()
        self._run_offload_dir(True, 0)


    def test_sanitize_dir(self):
        """Test that folder/file name with invalid character can be corrected.
        """
        results_folder = tempfile.mkdtemp()
        invalid_chars = '_'.join(['[', ']', '*', '?', '#'])
        invalid_files = []
        invalid_folder_name = 'invalid_name_folder_%s' % invalid_chars
        invalid_folder = os.path.join(
                results_folder,
                invalid_folder_name)
        invalid_files.append(os.path.join(
                invalid_folder,
                'invalid_name_file_%s' % invalid_chars))
        good_folder =  os.path.join(results_folder, 'valid_name_folder')
        good_file = os.path.join(good_folder, 'valid_name_file')
        for folder in [invalid_folder, good_folder]:
            os.makedirs(folder)
        for f in invalid_files + [good_file]:
            with open(f, 'w'):
                pass
        # check that broken symlinks don't break sanitization
        symlink = os.path.join(invalid_folder, 'broken-link')
        os.symlink(os.path.join(results_folder, 'no-such-file'),
                   symlink)
        fifo1 = os.path.join(results_folder, 'test_fifo1')
        fifo2 = os.path.join(good_folder, 'test_fifo2')
        fifo3 = os.path.join(invalid_folder, 'test_fifo3')
        invalid_fifo4_name = 'test_fifo4_%s' % invalid_chars
        fifo4 = os.path.join(invalid_folder, invalid_fifo4_name)
        os.mkfifo(fifo1)
        os.mkfifo(fifo2)
        os.mkfifo(fifo3)
        os.mkfifo(fifo4)
        gs_offloader.sanitize_dir(results_folder)
        for _, dirs, files in os.walk(results_folder):
            for name in dirs + files:
                self.assertEqual(name, gslib.escape(name))
                for c in name:
                    self.assertFalse(c in ['[', ']', '*', '?', '#'])
        self.assertTrue(os.path.exists(good_file))

        self.assertTrue(os.path.exists(fifo1))
        self.assertFalse(is_fifo(fifo1))
        self.assertTrue(os.path.exists(fifo2))
        self.assertFalse(is_fifo(fifo2))
        corrected_folder = os.path.join(
                results_folder, gslib.escape(invalid_folder_name))
        corrected_fifo3 = os.path.join(
                corrected_folder,
                'test_fifo3')
        self.assertFalse(os.path.exists(fifo3))
        self.assertTrue(os.path.exists(corrected_fifo3))
        self.assertFalse(is_fifo(corrected_fifo3))
        corrected_fifo4 = os.path.join(
                corrected_folder, gslib.escape(invalid_fifo4_name))
        self.assertFalse(os.path.exists(fifo4))
        self.assertTrue(os.path.exists(corrected_fifo4))
        self.assertFalse(is_fifo(corrected_fifo4))

        corrected_symlink = os.path.join(
                corrected_folder,
                'broken-link')
        self.assertFalse(os.path.lexists(symlink))
        self.assertTrue(os.path.exists(corrected_symlink))
        self.assertFalse(os.path.islink(corrected_symlink))
        shutil.rmtree(results_folder)


    def check_limit_file_count(self, is_test_job=True):
        """Test that folder with too many files can be compressed.

        @param is_test_job: True to check the method with test job result
                            folder. Set to False for special task folder.
        """
        results_folder = tempfile.mkdtemp()
        host_folder = os.path.join(
                results_folder,
                'lab1-host1' if is_test_job else 'hosts/lab1-host1/1-repair')
        debug_folder = os.path.join(host_folder, 'debug')
        sysinfo_folder = os.path.join(host_folder, 'sysinfo')
        for folder in [debug_folder, sysinfo_folder]:
            os.makedirs(folder)
            for i in range(10):
                with open(os.path.join(folder, str(i)), 'w') as f:
                    f.write('test')

        gs_offloader._MAX_FILE_COUNT = 100
        gs_offloader.limit_file_count(
                results_folder if is_test_job else host_folder)
        self.assertTrue(os.path.exists(sysinfo_folder))

        gs_offloader._MAX_FILE_COUNT = 10
        gs_offloader.limit_file_count(
                results_folder if is_test_job else host_folder)
        self.assertFalse(os.path.exists(sysinfo_folder))
        self.assertTrue(os.path.exists(sysinfo_folder + '.tgz'))
        self.assertTrue(os.path.exists(debug_folder))

        shutil.rmtree(results_folder)


    def test_limit_file_count(self):
        """Test that folder with too many files can be compressed.
        """
        self.check_limit_file_count(is_test_job=True)
        self.check_limit_file_count(is_test_job=False)


    def test_is_valid_result(self):
        """Test _is_valid_result."""
        release_build = 'veyron_minnie-cheets-release/R52-8248.0.0'
        pfq_build = 'cyan-cheets-android-pfq/R54-8623.0.0-rc1'
        trybot_build = 'trybot-samus-release/R54-8640.0.0-b5092'
        trybot_2_build = 'trybot-samus-pfq/R54-8640.0.0-b5092'
        release_2_build = 'test-trybot-release/R54-8640.0.0-b5092'
        self.assertTrue(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))
        self.assertTrue(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, 'test_that_wrapper'))
        self.assertTrue(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, 'cros_test_platform'))
        self.assertTrue(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, 'bvt-arc'))
        self.assertFalse(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, 'bvt-cq'))
        self.assertTrue(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_V2_RESULT_PATTERN, 'arc-gts'))
        self.assertFalse(gs_offloader._is_valid_result(
            None, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))
        self.assertFalse(gs_offloader._is_valid_result(
            release_build, gs_offloader.CTS_RESULT_PATTERN, None))
        self.assertFalse(gs_offloader._is_valid_result(
            pfq_build, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))
        self.assertFalse(gs_offloader._is_valid_result(
            trybot_build, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))
        self.assertFalse(gs_offloader._is_valid_result(
            trybot_2_build, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))
        self.assertTrue(gs_offloader._is_valid_result(
            release_2_build, gs_offloader.CTS_RESULT_PATTERN, 'arc-cts'))


    def create_results_folder(self):
        """Create CTS/GTS results folders."""
        results_folder = tempfile.mkdtemp()
        host_folder = os.path.join(results_folder, 'chromeos4-row9-rack11-host22')
        debug_folder = os.path.join(host_folder, 'debug')
        sysinfo_folder = os.path.join(host_folder, 'sysinfo')
        cts_result_folder = os.path.join(
                host_folder, 'cheets_CTS.android.dpi', 'results', 'cts-results')
        cts_v2_result_folder = os.path.join(host_folder,
                'cheets_CTS_N.CtsGraphicsTestCases', 'results', 'android-cts')
        gts_result_folder = os.path.join(
                host_folder, 'cheets_GTS.google.admin', 'results', 'android-gts')
        timestamp_str = '2016.04.28_01.41.44'
        timestamp_cts_folder = os.path.join(cts_result_folder, timestamp_str)
        timestamp_cts_v2_folder = os.path.join(cts_v2_result_folder, timestamp_str)
        timestamp_gts_folder = os.path.join(gts_result_folder, timestamp_str)

        # Build host keyvals set to parse model info.
        host_info_path = os.path.join(host_folder, 'host_keyvals')
        dir_to_create = '/'
        for tdir in host_info_path.split('/'):
            dir_to_create = os.path.join(dir_to_create, tdir)
            if not os.path.exists(dir_to_create):
                os.mkdir(dir_to_create)
        with open(os.path.join(host_info_path, 'chromeos4-row9-rack11-host22'), 'w') as store_file:
            store_file.write('labels=board%3Acoral,hw_video_acc_vp9,cros,'+
                             'hw_jpeg_acc_dec,bluetooth,model%3Arobo360,'+
                             'accel%3Acros-ec,'+
                             'sku%3Arobo360_IntelR_CeleronR_CPU_N3450_1_10GHz_4Gb')

        # .autoserv_execute file is needed for the test results package to look
        # legit.
        autoserve_path = os.path.join(host_folder, '.autoserv_execute')
        with open(autoserve_path, 'w') as temp_file:
            temp_file.write(' ')

        # Test results in cts_result_folder with a different time-stamp.
        timestamp_str_2 = '2016.04.28_10.41.44'
        timestamp_cts_folder_2 = os.path.join(cts_result_folder, timestamp_str_2)

        for folder in [debug_folder, sysinfo_folder, cts_result_folder,
                       timestamp_cts_folder, timestamp_cts_folder_2,
                       timestamp_cts_v2_folder, timestamp_gts_folder]:
            os.makedirs(folder)

        path_pattern_pair = [(timestamp_cts_folder, gs_offloader.CTS_RESULT_PATTERN),
                             (timestamp_cts_folder_2, gs_offloader.CTS_RESULT_PATTERN),
                             (timestamp_cts_folder_2, gs_offloader.CTS_COMPRESSED_RESULT_PATTERN),
                             (timestamp_cts_v2_folder, gs_offloader.CTS_V2_RESULT_PATTERN),
                             (timestamp_cts_v2_folder, gs_offloader.CTS_V2_COMPRESSED_RESULT_PATTERN),
                             (timestamp_gts_folder, gs_offloader.CTS_V2_RESULT_PATTERN)]

        # Create timestamp.zip file_path.
        cts_zip_file = os.path.join(cts_result_folder, timestamp_str + '.zip')
        cts_zip_file_2 = os.path.join(cts_result_folder, timestamp_str_2 + '.zip')
        cts_v2_zip_file = os.path.join(cts_v2_result_folder, timestamp_str + '.zip')
        gts_zip_file = os.path.join(gts_result_folder, timestamp_str + '.zip')

        # Create xml file_path.
        cts_result_file = os.path.join(timestamp_cts_folder, 'testResult.xml')
        cts_result_file_2 = os.path.join(timestamp_cts_folder_2,
                                         'testResult.xml')
        cts_result_compressed_file_2 = os.path.join(timestamp_cts_folder_2,
                                                     'testResult.xml.tgz')
        gts_result_file = os.path.join(timestamp_gts_folder, 'test_result.xml')
        cts_v2_result_file = os.path.join(timestamp_cts_v2_folder,
                                         'test_result.xml')
        cts_v2_result_compressed_file = os.path.join(timestamp_cts_v2_folder,
                                         'test_result.xml.tgz')

        for file_path in [cts_zip_file, cts_zip_file_2, cts_v2_zip_file,
                          gts_zip_file, cts_result_file, cts_result_file_2,
                          cts_result_compressed_file_2, gts_result_file,
                          cts_v2_result_file, cts_v2_result_compressed_file]:
          if file_path.endswith('tgz'):
              test_result_file = gs_offloader.CTS_COMPRESSED_RESULT_TYPES[
                      os.path.basename(file_path)]
              with open(test_result_file, 'w') as f:
                  f.write('test')
              with tarfile.open(file_path, 'w:gz') as tar_file:
                  tar_file.add(test_result_file)
              os.remove(test_result_file)
          else:
              with open(file_path, 'w') as f:
                  f.write('test')

        return (results_folder, host_folder, path_pattern_pair)


    def test__upload_cts_testresult(self):
        """Test _upload_cts_testresult."""
        results_folder, host_folder, path_pattern_pair = self.create_results_folder()

        self.mox.StubOutWithMock(gs_offloader, '_upload_files')
        gs_offloader._upload_files(
            mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg(), False,
                mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(
                ['test', '-d', host_folder])
        gs_offloader._upload_files(
            mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg(), False,
                mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(
                ['test', '-d', host_folder])
        gs_offloader._upload_files(
            mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg(), False,
                mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(
                ['test', '-d', host_folder])

        self.mox.ReplayAll()
        gs_offloader._upload_cts_testresult(results_folder, False)
        self.mox.VerifyAll()
        shutil.rmtree(results_folder)


    def test_parse_cts_job_results_file_path(self):
        # A autotest path
        path = ('/317739475-chromeos-test/chromeos4-row9-rack11-host22/'
                'cheets_CTS.android.dpi/results/cts-results/'
                '2016.04.28_01.41.44')
        job_id, package, timestamp = \
            gs_offloader._parse_cts_job_results_file_path(path)
        self.assertEqual('317739475-chromeos-test', job_id)
        self.assertEqual('cheets_CTS.android.dpi', package)
        self.assertEqual('2016.04.28_01.41.44', timestamp)


        # A skylab path
        path = ('/swarming-458e3a3a7fc6f210/1/autoserv_test/'
                'cheets_CTS.android.dpi/results/cts-results/'
                '2016.04.28_01.41.44')
        job_id, package, timestamp = \
            gs_offloader._parse_cts_job_results_file_path(path)
        self.assertEqual('swarming-458e3a3a7fc6f210-1', job_id)
        self.assertEqual('cheets_CTS.android.dpi', package)
        self.assertEqual('2016.04.28_01.41.44', timestamp)


    def test_upload_files(self):
        """Test upload_files"""
        results_folder, host_folder, path_pattern_pair = self.create_results_folder()

        for path, pattern in path_pattern_pair:
            models.test.parse_job_keyval(mox.IgnoreArg()).AndReturn({
                'build': 'veyron_minnie-cheets-release/R52-8248.0.0',
                'hostname': 'chromeos4-row9-rack11-host22',
                'parent_job_id': 'p_id',
                'suite': 'arc-cts'
            })

            gs_offloader._get_cmd_list(
                False, mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(
                    ['test', '-d', path])
            gs_offloader._get_cmd_list(
                False, mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(
                    ['test', '-d', path])

            self.mox.ReplayAll()
            gs_offloader._upload_files(host_folder, path, pattern, False,
                                       'gs://a-test-bucket/',
                                       'gs://a-test-apfe-bucket/')
            self.mox.VerifyAll()
            self.mox.ResetAll()

        shutil.rmtree(results_folder)


    def test_get_metrics_fields(self):
        """Test method _get_metrics_fields."""
        results_folder, host_folder, _ = self.create_results_folder()
        models.test.parse_job_keyval(mox.IgnoreArg()).AndReturn({
                'build': 'veyron_minnie-cheets-release/R52-8248.0.0',
                'parent_job_id': 'p_id',
                'suite': 'arc-cts'
            })
        try:
            self.mox.ReplayAll()
            self.assertEqual({'board': 'veyron_minnie-cheets',
                              'milestone': 'R52'},
                             gs_offloader._get_metrics_fields(host_folder))
            self.mox.VerifyAll()
        finally:
            shutil.rmtree(results_folder)


class OffladerConfigTests(_TempResultsDirTestBase):
    """Tests for the `Offloader` to follow side_effect config."""

    def setUp(self):
        super(OffladerConfigTests, self).setUp()
        gs_offloader.GS_OFFLOADING_ENABLED = True
        gs_offloader.GS_OFFLOADER_MULTIPROCESSING = True
        self.dest_path = '/results'
        self.mox.StubOutWithMock(gs_offloader, '_get_metrics_fields')
        self.mox.StubOutWithMock(gs_offloader, '_OffloadError')
        self.mox.StubOutWithMock(gs_offloader, '_upload_cts_testresult')
        self.mox.StubOutWithMock(gs_offloader, '_emit_offload_metrics')
        self.mox.StubOutWithMock(gs_offloader, '_get_cmd_list')
        self.mox.StubOutWithMock(subprocess, 'Popen')
        self.mox.StubOutWithMock(gs_offloader, '_emit_gs_returncode_metric')


    def _run(self, results_dir, gs_bucket, expect_dest, cts_enabled):
        stdout = os.path.join(results_dir, 'std.log')
        stderr = os.path.join(results_dir, 'std.err')
        config = {
            'tko': {
                'proxy_socket': '/file-system/foo-socket',
                'mysql_user': 'foo-user',
                'mysql_password_file': '/file-system/foo-password-file'
            },
            'google_storage': {
                'bucket': gs_bucket,
                'credentials_file': '/foo-creds'
            },
            'cts': {
                'enabled': cts_enabled,
            },
            'this_field_is_ignored': True
        }
        path = os.path.join(results_dir, 'side_effects_config.json')
        with open(path, 'w') as f:
            f.write(json.dumps(config))
        gs_offloader._get_metrics_fields(results_dir)
        if cts_enabled:
            gs_offloader._upload_cts_testresult(results_dir, True)
        gs_offloader._get_cmd_list(
            True,
            mox.IgnoreArg(),
            expect_dest).AndReturn(['test', '-d', expect_dest])
        subprocess.Popen(mox.IgnoreArg(),
                         stdout=stdout,
                         stderr=stderr).AndReturn(_get_fake_process())
        gs_offloader._OffloadError(mox.IgnoreArg())
        gs_offloader._emit_gs_returncode_metric(mox.IgnoreArg()).AndReturn(True)
        gs_offloader._emit_offload_metrics(mox.IgnoreArg()).AndReturn(True)
        sub_offloader = gs_offloader.GSOffloader(results_dir, True, 0, None)
        subprocess.Popen(mox.IgnoreArg(),
                         stdout=stdout,
                         stderr=stderr).AndReturn(_get_fake_process())
        self.mox.ReplayAll()
        sub_offloader._try_offload(results_dir, self.dest_path, stdout, stderr)
        self.mox.VerifyAll()
        self.mox.ResetAll()
        shutil.rmtree(results_dir)


    def test_upload_files_to_dev(self):
        """Test upload results to dev gs bucket and skip cts uploading."""
        res = tempfile.mkdtemp()
        gs_bucket = 'dev-bucket'
        expect_dest = 'gs://' + gs_bucket + self.dest_path
        self._run(res, gs_bucket, expect_dest, False)


    def test_upload_files_prod(self):
        """Test upload results to the prod gs bucket and also upload to cts."""
        res = tempfile.mkdtemp()
        gs_bucket = 'prod-bucket'
        expect_dest = 'gs://' + gs_bucket + self.dest_path
        self._run(res, gs_bucket, expect_dest, True)


    def test_skip_gs_prefix(self):
        """Test skip the 'gs://' prefix if already presented."""
        res = tempfile.mkdtemp()
        gs_bucket = 'gs://prod-bucket'
        expect_dest = gs_bucket + self.dest_path
        self._run(res, gs_bucket, expect_dest, True)


class JobDirectoryOffloadTests(_TempResultsDirTestBase):
    """Tests for `_JobDirectory.enqueue_offload()`.

    When testing with a `days_old` parameter of 0, we use
    `set_finished()` instead of `set_expired()`.  This causes the
    job's timestamp to be set in the future.  This is done so as
    to test that when `days_old` is 0, the job is always treated
    as eligible for offload, regardless of the timestamp's value.

    Testing covers the following assertions:
     A. Each time `enqueue_offload()` is called, a message that
        includes the job's directory name will be logged using
        `logging.debug()`, regardless of whether the job was
        enqueued.  Nothing else is allowed to be logged.
     B. If the job is not eligible to be offloaded,
        `first_offload_start` and `offload_count` are 0.
     C. If the job is not eligible for offload, nothing is
        enqueued in `queue`.
     D. When the job is offloaded, `offload_count` increments
        each time.
     E. When the job is offloaded, the appropriate parameters are
        enqueued exactly once.
     F. The first time a job is offloaded, `first_offload_start` is
        set to the current time.
     G. `first_offload_start` only changes the first time that the
        job is offloaded.

    The test cases below are designed to exercise all of the
    meaningful state transitions at least once.

    """

    def setUp(self):
        super(JobDirectoryOffloadTests, self).setUp()
        self._job = self.make_job(self.REGULAR_JOBLIST[0])
        self._queue = Queue.Queue()


    def _offload_unexpired_job(self, days_old):
        """Make calls to `enqueue_offload()` for an unexpired job.

        This method tests assertions B and C that calling
        `enqueue_offload()` has no effect.

        """
        self.assertEqual(self._job.offload_count, 0)
        self.assertEqual(self._job.first_offload_start, 0)
        gs_offloader._enqueue_offload(self._job, self._queue, days_old)
        gs_offloader._enqueue_offload(self._job, self._queue, days_old)
        self.assertTrue(self._queue.empty())
        self.assertEqual(self._job.offload_count, 0)
        self.assertEqual(self._job.first_offload_start, 0)


    def _offload_expired_once(self, days_old, count):
        """Make one call to `enqueue_offload()` for an expired job.

        This method tests assertions D and E regarding side-effects
        expected when a job is offloaded.

        """
        gs_offloader._enqueue_offload(self._job, self._queue, days_old)
        self.assertEqual(self._job.offload_count, count)
        self.assertFalse(self._queue.empty())
        v = self._queue.get_nowait()
        self.assertTrue(self._queue.empty())
        self.assertEqual(v, self._job.queue_args)


    def _offload_expired_job(self, days_old):
        """Make calls to `enqueue_offload()` for a just-expired job.

        This method directly tests assertions F and G regarding
        side-effects on `first_offload_start`.

        """
        t0 = time.time()
        self._offload_expired_once(days_old, 1)
        t1 = self._job.first_offload_start
        self.assertLessEqual(t1, time.time())
        self.assertGreaterEqual(t1, t0)
        self._offload_expired_once(days_old, 2)
        self.assertEqual(self._job.first_offload_start, t1)
        self._offload_expired_once(days_old, 3)
        self.assertEqual(self._job.first_offload_start, t1)


    def test_case_1_no_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` of 0.

        This tests that offload works as expected if calls are
        made both before and after the job becomes expired.

        """
        self._offload_unexpired_job(0)
        self._job.set_finished(0)
        self._offload_expired_job(0)


    def test_case_2_no_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` of 0.

        This tests that offload works as expected if calls are made
        only after the job becomes expired.

        """
        self._job.set_finished(0)
        self._offload_expired_job(0)


    def test_case_1_with_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` non-zero.

        This tests that offload works as expected if calls are made
        before the job finishes, before the job expires, and after
        the job expires.

        """
        self._offload_unexpired_job(_TEST_EXPIRATION_AGE)
        self._job.set_finished(_TEST_EXPIRATION_AGE)
        self._offload_unexpired_job(_TEST_EXPIRATION_AGE)
        self._job.set_expired(_TEST_EXPIRATION_AGE)
        self._offload_expired_job(_TEST_EXPIRATION_AGE)


    def test_case_2_with_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` non-zero.

        This tests that offload works as expected if calls are made
        between finishing and expiration, and after the job expires.

        """
        self._job.set_finished(_TEST_EXPIRATION_AGE)
        self._offload_unexpired_job(_TEST_EXPIRATION_AGE)
        self._job.set_expired(_TEST_EXPIRATION_AGE)
        self._offload_expired_job(_TEST_EXPIRATION_AGE)


    def test_case_3_with_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` non-zero.

        This tests that offload works as expected if calls are made
        only before finishing and after expiration.

        """
        self._offload_unexpired_job(_TEST_EXPIRATION_AGE)
        self._job.set_expired(_TEST_EXPIRATION_AGE)
        self._offload_expired_job(_TEST_EXPIRATION_AGE)


    def test_case_4_with_expiration(self):
        """Test a series of `enqueue_offload()` calls with `days_old` non-zero.

        This tests that offload works as expected if calls are made
        only after expiration.

        """
        self._job.set_expired(_TEST_EXPIRATION_AGE)
        self._offload_expired_job(_TEST_EXPIRATION_AGE)


class GetJobDirectoriesTests(_TempResultsDirTestBase):
    """Tests for `_JobDirectory.get_job_directories()`."""

    def setUp(self):
        super(GetJobDirectoriesTests, self).setUp()
        self.make_job_hierarchy()
        os.mkdir('not-a-job')
        open('not-a-dir', 'w').close()


    def _run_get_directories(self, cls, expected_list):
        """Test `get_job_directories()` for the given class.

        Calls the method, and asserts that the returned list of
        directories matches the expected return value.

        @param expected_list Expected return value from the call.
        """
        dirlist = cls.get_job_directories()
        self.assertEqual(set(dirlist), set(expected_list))


    def test_get_regular_jobs(self):
        """Test `RegularJobDirectory.get_job_directories()`."""
        self._run_get_directories(job_directories.RegularJobDirectory,
                                  self.REGULAR_JOBLIST)


    def test_get_special_jobs(self):
        """Test `SpecialJobDirectory.get_job_directories()`."""
        self._run_get_directories(job_directories.SpecialJobDirectory,
                                  self.SPECIAL_JOBLIST)


class AddJobsTests(_TempResultsDirTestBase):
    """Tests for `Offloader._add_new_jobs()`."""

    MOREJOBS = ['115-fubar', '116-fubar', '117-fubar', '118-snafu']

    def setUp(self):
        super(AddJobsTests, self).setUp()
        self._initial_job_names = (
            set(self.REGULAR_JOBLIST) | set(self.SPECIAL_JOBLIST))
        self.make_job_hierarchy()
        self._offloader = gs_offloader.Offloader(_get_options(['-a']))
        self.mox.StubOutWithMock(logging, 'debug')


    def _run_add_new_jobs(self, expected_key_set):
        """Basic test assertions for `_add_new_jobs()`.

        Asserts the following:
          * The keys in the offloader's `_open_jobs` dictionary
            matches the expected set of keys.
          * For every job in `_open_jobs`, the job has the expected
            directory name.

        """
        count = len(expected_key_set) - len(self._offloader._open_jobs)
        logging.debug(mox.IgnoreArg(), count)
        self.mox.ReplayAll()
        self._offloader._add_new_jobs()
        self.assertEqual(expected_key_set,
                         set(self._offloader._open_jobs.keys()))
        for jobkey, job in self._offloader._open_jobs.items():
            self.assertEqual(jobkey, job.dirname)
        self.mox.VerifyAll()
        self.mox.ResetAll()


    def test_add_jobs_empty(self):
        """Test adding jobs to an empty dictionary.

        Calls the offloader's `_add_new_jobs()`, then perform
        the assertions of `self._check_open_jobs()`.

        """
        self._run_add_new_jobs(self._initial_job_names)


    def test_add_jobs_non_empty(self):
        """Test adding jobs to a non-empty dictionary.

        Calls the offloader's `_add_new_jobs()` twice; once from
        initial conditions, and then again after adding more
        directories.  After the second call, perform the assertions
        of `self._check_open_jobs()`.  Additionally, assert that
        keys added by the first call still map to their original
        job object after the second call.

        """
        self._run_add_new_jobs(self._initial_job_names)
        jobs_copy = self._offloader._open_jobs.copy()
        for d in self.MOREJOBS:
            os.mkdir(d)
        self._run_add_new_jobs(self._initial_job_names |
                                 set(self.MOREJOBS))
        for key in jobs_copy.keys():
            self.assertIs(jobs_copy[key],
                          self._offloader._open_jobs[key])


class ReportingTests(_TempResultsDirTestBase):
    """Tests for `Offloader._report_failed_jobs()`."""

    def setUp(self):
        super(ReportingTests, self).setUp()
        self._offloader = gs_offloader.Offloader(_get_options([]))
        self.mox.StubOutWithMock(self._offloader, '_log_failed_jobs_locally')
        self.mox.StubOutWithMock(logging, 'debug')


    def _add_job(self, jobdir):
        """Add a job to the dictionary of unfinished jobs."""
        j = self.make_job(jobdir)
        self._offloader._open_jobs[j.dirname] = j
        return j


    def _expect_log_message(self, new_open_jobs, with_failures):
        """Mock expected logging calls.

        `_report_failed_jobs()` logs one message with the number
        of jobs removed from the open job set and the number of jobs
        still remaining.  Additionally, if there are reportable
        jobs, then it logs the number of jobs that haven't yet
        offloaded.

        This sets up the logging calls using `new_open_jobs` to
        figure the job counts.  If `with_failures` is true, then
        the log message is set up assuming that all jobs in
        `new_open_jobs` have offload failures.

        @param new_open_jobs New job set for calculating counts
                             in the messages.
        @param with_failures Whether the log message with a
                             failure count is expected.

        """
        count = len(self._offloader._open_jobs) - len(new_open_jobs)
        logging.debug(mox.IgnoreArg(), count, len(new_open_jobs))
        if with_failures:
            logging.debug(mox.IgnoreArg(), len(new_open_jobs))


    def _run_update(self, new_open_jobs):
        """Call `_report_failed_jobs()`.

        Initial conditions are set up by the caller.  This calls
        `_report_failed_jobs()` once, and then checks these
        assertions:
          * The offloader's new `_open_jobs` field contains only
            the entries in `new_open_jobs`.

        @param new_open_jobs A dictionary representing the expected
                             new value of the offloader's
                             `_open_jobs` field.
        """
        self.mox.ReplayAll()
        self._offloader._report_failed_jobs()
        self._offloader._remove_offloaded_jobs()
        self.assertEqual(self._offloader._open_jobs, new_open_jobs)
        self.mox.VerifyAll()
        self.mox.ResetAll()


    def _expect_failed_jobs(self, failed_jobs):
        """Mock expected call to log the failed jobs on local disk.

        TODO(crbug.com/686904): The fact that we have to mock an internal
        function for this test is evidence that we need to pull out the local
        file formatter in its own object in a future CL.

        @param failed_jobs: The list of jobs being logged as failed.
        """
        self._offloader._log_failed_jobs_locally(failed_jobs)


    def test_no_jobs(self):
        """Test `_report_failed_jobs()` with no open jobs.

        Initial conditions are an empty `_open_jobs` list.
        Expected result is an empty `_open_jobs` list.

        """
        self._expect_log_message({}, False)
        self._expect_failed_jobs([])
        self._run_update({})


    def test_all_completed(self):
        """Test `_report_failed_jobs()` with only complete jobs.

        Initial conditions are an `_open_jobs` list consisting of only completed
        jobs.
        Expected result is an empty `_open_jobs` list.

        """
        for d in self.REGULAR_JOBLIST:
            self._add_job(d).set_complete()
        self._expect_log_message({}, False)
        self._expect_failed_jobs([])
        self._run_update({})


    def test_none_finished(self):
        """Test `_report_failed_jobs()` with only unfinished jobs.

        Initial conditions are an `_open_jobs` list consisting of only
        unfinished jobs.
        Expected result is no change to the `_open_jobs` list.

        """
        for d in self.REGULAR_JOBLIST:
            self._add_job(d)
        new_jobs = self._offloader._open_jobs.copy()
        self._expect_log_message(new_jobs, False)
        self._expect_failed_jobs([])
        self._run_update(new_jobs)


class GsOffloaderMockTests(_TempResultsDirTestCase):
    """Tests using mock instead of mox."""

    def setUp(self):
        super(GsOffloaderMockTests, self).setUp()
        alarm = mock.patch('signal.alarm', return_value=0)
        alarm.start()
        self.addCleanup(alarm.stop)

        self._saved_loglevel = logging.getLogger().getEffectiveLevel()
        logging.getLogger().setLevel(logging.CRITICAL + 1)

        self._job = self.make_job(self.REGULAR_JOBLIST[0])


    def test_offload_timeout_early(self):
        """Test that `offload_dir()` times out correctly.

        This test triggers timeout at the earliest possible moment,
        at the first call to set the timeout alarm.

        """
        signal.alarm.side_effect = [0, timeout_util.TimeoutError('fubar')]
        with mock.patch.object(gs_offloader, '_upload_cts_testresult',
                               autospec=True) as upload:
            upload.return_value = None
            gs_offloader.GSOffloader(
                    utils.DEFAULT_OFFLOAD_GSURI, False, 0).offload(
                            self._job.queue_args[0],
                            self._job.queue_args[1],
                            self._job.queue_args[2])
            self.assertTrue(os.path.isdir(self._job.queue_args[0]))


    # TODO(ayatane): This tests passes when run locally, but it fails
    # when run on trybot.  I have no idea why, but the assert isdir
    # fails.
    #
    # This test is also kind of redundant since we are using the timeout
    # from chromite which has its own tests.
    @unittest.skip('This fails on trybot')
    def test_offload_timeout_late(self):
        """Test that `offload_dir()` times out correctly.

        This test triggers timeout at the latest possible moment, at
        the call to clear the timeout alarm.

        """
        signal.alarm.side_effect = [0, 0, timeout_util.TimeoutError('fubar')]
        with mock.patch.object(gs_offloader, '_upload_cts_testresult',
                               autospec=True) as upload, \
             mock.patch.object(gs_offloader, '_get_cmd_list',
                               autospec=True) as get_cmd_list:
            upload.return_value = None
            get_cmd_list.return_value = ['test', '-d', self._job.queue_args[0]]
            gs_offloader.GSOffloader(
                    utils.DEFAULT_OFFLOAD_GSURI, False, 0).offload(
                            self._job.queue_args[0],
                            self._job.queue_args[1],
                            self._job.queue_args[2])
            self.assertTrue(os.path.isdir(self._job.queue_args[0]))



if __name__ == '__main__':
    unittest.main()
