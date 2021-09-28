# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import datetime
import unittest

import common
# import has side-effects, must appear before any django imports.
from autotest_lib.frontend import setup_django_environment

from autotest_lib.frontend import setup_test_environment
from autotest_lib.frontend.afe import models


class ShardHeartbeatTest(unittest.TestCase):
    def setUp(self):
        self._tag_creator = _TagCreator('ShardHeartbeatTest')
        setup_test_environment.set_up()


    def tearDown(self):
        setup_test_environment.tear_down()


    def testJobsWithDepsIsFilteredByShardLabel(self):
        label = self._create_label()
        shard = self._create_shard(label)
        job = self._create_job_with_label(label)
        # Should not be assigned.
        self._create_job_with_label(self._create_label())

        assigned = models.Job.assign_to_shard(shard, [])
        self.assertEqual(set(assigned), {job})


    def testJobsForHostsIsFilteredByShardLabel(self):
        label = self._create_label()
        shard = self._create_shard(label)
        job = self._create_job_for_host(self._create_host(label))
        # Should not be assigned.
        self._create_job_for_host(self._create_host(self._create_label()))

        assigned = models.Job.assign_to_shard(shard, [])
        self.assertEqual(set(assigned), {job})


    def testJobsWithKnownIDsIsIgnored(self):
        label = self._create_label()
        shard = self._create_shard(label)
        known_job = self._create_job_with_label(label)
        new_job = self._create_job_with_label(label)
        assigned_jobs = models.Job.assign_to_shard(shard, [known_job.id])
        self.assertEqual(set(assigned_jobs), {new_job})


    def testOldJobsAreIgnoredWhenOptionEnabled(self):
        with self._ignore_jobs_older_than(2):
            label = self._create_label()
            shard = self._create_shard(label)
            job = self._create_job_with_label(label)
            # Should not be assigned.
            self._create_job_with_label(label, datetime.timedelta(hours=3))
            assigned = models.Job.assign_to_shard(shard, [])
            self.assertEqual(set(assigned), {job})


    def testOldJobsAreNotIgnoredWhenOptionDisabled(self):
        with self._ignore_jobs_older_than(0):
            label = self._create_label()
            shard = self._create_shard(label)
            job = self._create_job_with_label(label,
                                              datetime.timedelta(hours=3))
            assigned = models.Job.assign_to_shard(shard, [])
            self.assertEqual(set(assigned), {job})


    @contextlib.contextmanager
    def _ignore_jobs_older_than(self, value):
        old = models.Job.SKIP_JOBS_CREATED_BEFORE
        try:
            models.Job.SKIP_JOBS_CREATED_BEFORE = value
            yield
        finally:
            models.Job.SKIP_JOBS_CREATED_BEFORE = old


    def _create_job_for_host(self, host, pending_age=None):
        """Create a job for the given host created pending_age ago.

        @param host: A models.Host object.
        @param pending_age: A datetime.datetime object.
        @return: A models.Job object.
        """
        job = models.Job.objects.create(
            name=self._tag_creator.next(),
            created_on=_past_timestamp(pending_age),
        )
        hqe = models.HostQueueEntry.objects.create(
            job=job,
            host_id=host.id,
            status=models.HostQueueEntry.Status.QUEUED,
        )
        return job


    def _create_job_with_label(self, label, pending_age=None):
        """Create a job for the given label created pending_age ago.

        @param host: A models.Label object.
        @param pending_age: A datetime.datetime object.
        @return: A models.Job object.
        """
        job = models.Job.objects.create(
            name=self._tag_creator.next(),
            created_on=_past_timestamp(pending_age),
        )
        job.dependency_labels.add(label)
        hqe = models.HostQueueEntry.objects.create(
            job=job,
            meta_host_id=label.id,
            status=models.HostQueueEntry.Status.QUEUED,
        )
        return job


    def _create_host(self, label):
        """Create a models.Host with the given models.Label."""
        host = models.Host.objects.create(hostname=self._tag_creator.next())
        host.labels.add(label)
        return host


    def _create_label(self):
        """Create a models.Label."""
        return models.Label.objects.create(name=self._tag_creator.next())


    def _create_shard(self, label):
        """Create a models.Shard with the givem models.Label."""
        shard = models.Shard.objects.create(hostname=self._tag_creator.next())
        shard.labels.add(label)
        return shard


class _TagCreator(object):
    """Create unique but deterministic str tags by calling next()."""
    def __init__(self, prefix):
        self._prefix = prefix
        self._count = 0

    def next(self):
        self._count += 1
        return self._prefix + str(self._count)


def _past_timestamp(delta=None):
    """Compute datetime.datetime given timedelta in the past.

    @param delta: A datetime.timedelta object.
    @return: A datetime.datetime object.
    """
    now = datetime.datetime.now()
    if delta is None:
        return now
    return now - delta


if __name__ == '__main__':
    unittest.main()