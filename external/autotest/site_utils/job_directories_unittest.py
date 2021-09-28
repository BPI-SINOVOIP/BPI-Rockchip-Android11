"""Tests for job_directories."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import datetime
import mox
import os
import shutil
import tempfile
import unittest

import common
from autotest_lib.site_utils import job_directories
from autotest_lib.client.common_lib import time_utils


class SwarmingJobDirectoryTestCase(unittest.TestCase):
    """Tests SwarmingJobDirectory."""

    def test_get_job_directories_legacy(self):
        with _change_to_tempdir():
            os.makedirs("swarming-3e4391423c3a4311/b")
            os.mkdir("not-a-swarming-dir")
            results = job_directories.SwarmingJobDirectory.get_job_directories()
            self.assertEqual(set(results), {"swarming-3e4391423c3a4311"})

    def test_get_job_directories(self):
        with _change_to_tempdir():
            os.makedirs("swarming-3e4391423c3a4310/1")
            os.makedirs("swarming-3e4391423c3a4310/0")
            os.makedirs("swarming-3e4391423c3a4310/a")
            os.mkdir("not-a-swarming-dir")
            results = job_directories.SwarmingJobDirectory.get_job_directories()
            self.assertEqual(set(results),
                             {"swarming-3e4391423c3a4310/1",
                              "swarming-3e4391423c3a4310/a"})


class GetJobIDOrTaskID(unittest.TestCase):
    """Tests get_job_id_or_task_id."""

    def test_legacy_swarming_path(self):
        self.assertEqual(
                "3e4391423c3a4311",
                job_directories.get_job_id_or_task_id(
                        "/autotest/results/swarming-3e4391423c3a4311"),
        )
        self.assertEqual(
                "3e4391423c3a4311",
                job_directories.get_job_id_or_task_id(
                        "swarming-3e4391423c3a4311"),
        )

    def test_swarming_path(self):
        self.assertEqual(
                "3e4391423c3a4311",
                job_directories.get_job_id_or_task_id(
                        "/autotest/results/swarming-3e4391423c3a4310/1"),
        )
        self.assertEqual(
                "3e4391423c3a431f",
                job_directories.get_job_id_or_task_id(
                        "swarming-3e4391423c3a4310/f"),
        )



class JobDirectorySubclassTests(mox.MoxTestBase):
    """Test specific to RegularJobDirectory and SpecialJobDirectory.

    This provides coverage for the implementation in both
    RegularJobDirectory and SpecialJobDirectory.

    """

    def setUp(self):
        super(JobDirectorySubclassTests, self).setUp()
        self.mox.StubOutWithMock(job_directories, '_AFE')


    def test_regular_job_fields(self):
        """Test the constructor for `RegularJobDirectory`.

        Construct a regular job, and assert that the `dirname`
        and `_id` attributes are set as expected.

        """
        resultsdir = '118-fubar'
        job = job_directories.RegularJobDirectory(resultsdir)
        self.assertEqual(job.dirname, resultsdir)
        self.assertEqual(job._id, '118')


    def test_special_job_fields(self):
        """Test the constructor for `SpecialJobDirectory`.

        Construct a special job, and assert that the `dirname`
        and `_id` attributes are set as expected.

        """
        destdir = 'hosts/host1'
        resultsdir = destdir + '/118-reset'
        job = job_directories.SpecialJobDirectory(resultsdir)
        self.assertEqual(job.dirname, resultsdir)
        self.assertEqual(job._id, '118')


    def _check_finished_job(self, jobtime, hqetimes, expected):
        """Mock and test behavior of a finished job.

        Initialize the mocks for a call to
        `get_timestamp_if_finished()`, then simulate one call.
        Assert that the returned timestamp matches the passed
        in expected value.

        @param jobtime Time used to construct a _MockJob object.
        @param hqetimes List of times used to construct
                        _MockHostQueueEntry objects.
        @param expected Expected time to be returned by
                        get_timestamp_if_finished

        """
        job = job_directories.RegularJobDirectory('118-fubar')
        job_directories._AFE.get_jobs(
                id=job._id, finished=True).AndReturn(
                        [_MockJob(jobtime)])
        job_directories._AFE.get_host_queue_entries(
                finished_on__isnull=False,
                job_id=job._id).AndReturn(
                        [_MockHostQueueEntry(t) for t in hqetimes])
        self.mox.ReplayAll()
        self.assertEqual(expected, job.get_timestamp_if_finished())
        self.mox.VerifyAll()


    def test_finished_regular_job(self):
        """Test getting the timestamp for a finished regular job.

        Tests the return value for
        `RegularJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is finished.

        """
        created_timestamp = make_timestamp(1, True)
        hqe_timestamp = make_timestamp(0, True)
        self._check_finished_job(created_timestamp,
                                 [hqe_timestamp],
                                 hqe_timestamp)


    def test_finished_regular_job_multiple_hqes(self):
        """Test getting the timestamp for a regular job with multiple hqes.

        Tests the return value for
        `RegularJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is finished and the job has multiple host
        queue entries.

        Tests that the returned timestamp is the latest timestamp in
        the list of HQEs, regardless of the returned order.

        """
        created_timestamp = make_timestamp(2, True)
        older_hqe_timestamp = make_timestamp(1, True)
        newer_hqe_timestamp = make_timestamp(0, True)
        hqe_list = [older_hqe_timestamp,
                    newer_hqe_timestamp]
        self._check_finished_job(created_timestamp,
                                 hqe_list,
                                 newer_hqe_timestamp)
        self.mox.ResetAll()
        hqe_list.reverse()
        self._check_finished_job(created_timestamp,
                                 hqe_list,
                                 newer_hqe_timestamp)


    def test_finished_regular_job_null_finished_times(self):
        """Test getting the timestamp for an aborted regular job.

        Tests the return value for
        `RegularJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is finished and the job has aborted host
        queue entries.

        """
        timestamp = make_timestamp(0, True)
        self._check_finished_job(timestamp, [], timestamp)


    def test_unfinished_regular_job(self):
        """Test getting the timestamp for an unfinished regular job.

        Tests the return value for
        `RegularJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is not finished.

        """
        job = job_directories.RegularJobDirectory('118-fubar')
        job_directories._AFE.get_jobs(
                id=job._id, finished=True).AndReturn([])
        self.mox.ReplayAll()
        self.assertIsNone(job.get_timestamp_if_finished())
        self.mox.VerifyAll()


    def test_finished_special_job(self):
        """Test getting the timestamp for a finished special job.

        Tests the return value for
        `SpecialJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is finished.

        """
        job = job_directories.SpecialJobDirectory(
                'hosts/host1/118-reset')
        timestamp = make_timestamp(0, True)
        job_directories._AFE.get_special_tasks(
                id=job._id, is_complete=True).AndReturn(
                    [_MockSpecialTask(timestamp)])
        self.mox.ReplayAll()
        self.assertEqual(timestamp,
                         job.get_timestamp_if_finished())
        self.mox.VerifyAll()


    def test_unfinished_special_job(self):
        """Test getting the timestamp for an unfinished special job.

        Tests the return value for
        `SpecialJobDirectory.get_timestamp_if_finished()` when
        the AFE indicates the job is not finished.

        """
        job = job_directories.SpecialJobDirectory(
                'hosts/host1/118-reset')
        job_directories._AFE.get_special_tasks(
                id=job._id, is_complete=True).AndReturn([])
        self.mox.ReplayAll()
        self.assertIsNone(job.get_timestamp_if_finished())
        self.mox.VerifyAll()


class JobExpirationTests(unittest.TestCase):
    """Tests to exercise `job_directories.is_job_expired()`."""

    def test_expired(self):
        """Test detection of an expired job."""
        timestamp = make_timestamp(_TEST_EXPIRATION_AGE, True)
        self.assertTrue(
            job_directories.is_job_expired(
                _TEST_EXPIRATION_AGE, timestamp))


    def test_alive(self):
        """Test detection of a job that's not expired."""
        # N.B.  This test may fail if its run time exceeds more than
        # about _MARGIN_SECS seconds.
        timestamp = make_timestamp(_TEST_EXPIRATION_AGE, False)
        self.assertFalse(
            job_directories.is_job_expired(
                _TEST_EXPIRATION_AGE, timestamp))


# When constructing sample time values for testing expiration,
# allow this many seconds between the expiration time and the
# current time.
_MARGIN_SECS = 10.0
# Test value to use for `days_old`, if nothing else is required.
_TEST_EXPIRATION_AGE = 7


class _MockJob(object):
    """Class to mock the return value of `AFE.get_jobs()`."""
    def __init__(self, created):
        self.created_on = created


class _MockHostQueueEntry(object):
    """Class to mock the return value of `AFE.get_host_queue_entries()`."""
    def __init__(self, finished):
        self.finished_on = finished


class _MockSpecialTask(object):
    """Class to mock the return value of `AFE.get_special_tasks()`."""
    def __init__(self, finished):
        self.time_finished = finished


@contextlib.contextmanager
def _change_to_tempdir():
    old_dir = os.getcwd()
    tempdir = tempfile.mkdtemp('job_directories_unittest')
    try:
        os.chdir(tempdir)
        yield
    finally:
        os.chdir(old_dir)
        shutil.rmtree(tempdir)


def make_timestamp(age_limit, is_expired):
    """Create a timestamp for use by `job_directories.is_job_expired()`.

    The timestamp will meet the syntactic requirements for
    timestamps used as input to `is_job_expired()`.  If
    `is_expired` is true, the timestamp will be older than
    `age_limit` days before the current time; otherwise, the
    date will be younger.

    @param age_limit    The number of days before expiration of the
                        target timestamp.
    @param is_expired   Whether the timestamp should be expired
                        relative to `age_limit`.

    """
    seconds = -_MARGIN_SECS
    if is_expired:
        seconds = -seconds
    delta = datetime.timedelta(days=age_limit, seconds=seconds)
    reference_time = datetime.datetime.now() - delta
    return reference_time.strftime(time_utils.TIME_FMT)


if __name__ == '__main__':
    unittest.main()
