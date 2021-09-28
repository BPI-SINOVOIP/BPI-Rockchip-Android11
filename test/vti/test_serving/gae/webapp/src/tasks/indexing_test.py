#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src.proto import model
from webapp.src.tasks import indexing
from webapp.src.testing import unittest_base


class IndexingHandlerTest(unittest_base.UnitTestBase):
    """Tests for IndexingHandler.

    Attributes:
        testbed: A Testbed instance which provides local unit testing.
        indexing_handler: A mock IndexingHandler instance.
    """

    def setUp(self):
        """Initializes test"""
        super(IndexingHandlerTest, self).setUp()
        # Mocking IndexingHandler.
        self.indexing_handler = indexing.IndexingHandler(mock.Mock())
        self.indexing_handler.request = mock.Mock()

    def testSingleJobReindexing(self):
        """Asserts re-indexing links job and schedule successfully."""

        print("\nCreating a single schedule...")
        schedule = self.GenerateScheduleModel()
        schedule.put()

        schedules = model.ScheduleModel.query().fetch()
        self.assertEqual(1, len(schedules))

        print("Creating a job for stored schedule...")
        for schedule in schedules:
            job = model.JobModel()
            job.priority = schedule.priority
            job.test_name = schedule.test_name
            job.period = schedule.period
            job.build_storage_type = schedule.build_storage_type
            job.manifest_branch = schedule.manifest_branch
            job.build_target = schedule.build_target
            job.pab_account_id = schedule.device_pab_account_id
            job.shards = schedule.shards
            job.retry_count = schedule.retry_count
            job.gsi_storage_type = schedule.gsi_storage_type
            job.gsi_branch = schedule.gsi_branch
            job.gsi_build_target = schedule.gsi_build_target
            job.gsi_pab_account_id = schedule.gsi_pab_account_id
            job.gsi_vendor_version = schedule.gsi_vendor_version
            job.test_storage_type = schedule.test_storage_type
            job.test_branch = schedule.test_branch
            job.test_build_target = schedule.test_build_target
            job.test_pab_account_id = schedule.test_pab_account_id
            job.put()

        jobs = model.JobModel.query().fetch()
        self.assertEqual(1, len(jobs))

        print("Seeking children jobs before re-indexing...")
        jobs = model.JobModel.query().fetch()
        for job in jobs:
            parent_key = job.parent_schedule
            self.assertIsNone(parent_key)

        print("Seeking children jobs after re-indexing...")
        self.indexing_handler.request.get = mock.MagicMock(return_value="job")
        self.indexing_handler.post()
        jobs = model.JobModel.query().fetch()
        for job in jobs:
            parent_key = job.parent_schedule
            parent_schedule = parent_key.get()
            self.assertEqual(
                True,
                ((parent_schedule.priority == job.priority) and
                 (parent_schedule.test_name == job.test_name) and
                 (parent_schedule.period == job.period) and
                 (parent_schedule.build_storage_type == job.build_storage_type)
                 and (parent_schedule.manifest_branch == job.manifest_branch)
                 and (parent_schedule.build_target == job.build_target) and
                 (parent_schedule.device_pab_account_id == job.pab_account_id)
                 and (parent_schedule.shards == job.shards) and
                 (parent_schedule.retry_count == job.retry_count) and
                 (parent_schedule.gsi_storage_type == job.gsi_storage_type) and
                 (parent_schedule.gsi_branch == job.gsi_branch) and
                 (parent_schedule.gsi_build_target == job.gsi_build_target) and
                 (parent_schedule.gsi_pab_account_id == job.gsi_pab_account_id)
                 and
                 (parent_schedule.test_storage_type == job.test_storage_type)
                 and (parent_schedule.test_branch == job.test_branch) and
                 (parent_schedule.test_build_target == job.test_build_target)
                 and (parent_schedule.test_pab_account_id ==
                      job.test_pab_account_id)))

    def testMultiJobReindexing(self):
        """Asserts re-indexing links job and schedule successfully."""
        print("\nCreating four schedules...")
        for num in xrange(4):
            schedule = self.GenerateScheduleModel(test_name=str(num + 1))
            schedule.put()
            schedule.put()

        schedules = model.ScheduleModel.query().fetch()
        self.assertEqual(4, len(schedules))

        print("Creating jobs as number of test_name...")
        for schedule in schedules:
            for _ in xrange(int(schedule.test_name)):
                job = model.JobModel()
                job.priority = schedule.priority
                job.test_name = schedule.test_name
                job.period = schedule.period
                job.build_storage_type = schedule.build_storage_type
                job.manifest_branch = schedule.manifest_branch
                job.build_target = schedule.build_target
                job.pab_account_id = schedule.device_pab_account_id
                job.shards = schedule.shards
                job.retry_count = schedule.retry_count
                job.gsi_storage_type = schedule.gsi_storage_type
                job.gsi_branch = schedule.gsi_branch
                job.gsi_build_target = schedule.gsi_build_target
                job.gsi_pab_account_id = schedule.gsi_pab_account_id
                job.gsi_vendor_version = schedule.gsi_vendor_version
                job.test_storage_type = schedule.test_storage_type
                job.test_branch = schedule.test_branch
                job.test_build_target = schedule.test_build_target
                job.test_pab_account_id = schedule.test_pab_account_id
                job.put()

        jobs = model.JobModel.query().fetch()
        self.assertEqual(10, len(jobs))

        print("Seeking children jobs before re-indexing...")
        jobs = model.JobModel.query().fetch()
        for job in jobs:
            parent_key = job.parent_schedule
            self.assertIsNone(parent_key)

        print("Seeking children jobs after re-indexing...")
        self.indexing_handler.request.get = mock.MagicMock(return_value="job")
        self.indexing_handler.post()
        jobs = model.JobModel.query().fetch()
        for job in jobs:
            parent_key = job.parent_schedule
            parent_schedule = parent_key.get()
            self.assertEqual(
                True,
                ((parent_schedule.priority == job.priority) and
                 (parent_schedule.test_name == job.test_name) and
                 (parent_schedule.period == job.period) and
                 (parent_schedule.build_storage_type == job.build_storage_type)
                 and (parent_schedule.manifest_branch == job.manifest_branch)
                 and (parent_schedule.build_target == job.build_target) and
                 (parent_schedule.device_pab_account_id == job.pab_account_id)
                 and (parent_schedule.shards == job.shards) and
                 (parent_schedule.retry_count == job.retry_count) and
                 (parent_schedule.gsi_storage_type == job.gsi_storage_type) and
                 (parent_schedule.gsi_branch == job.gsi_branch) and
                 (parent_schedule.gsi_build_target == job.gsi_build_target) and
                 (parent_schedule.gsi_pab_account_id == job.gsi_pab_account_id)
                 and
                 (parent_schedule.test_storage_type == job.test_storage_type)
                 and (parent_schedule.test_branch == job.test_branch) and
                 (parent_schedule.test_build_target == job.test_build_target)
                 and (parent_schedule.test_pab_account_id ==
                      job.test_pab_account_id)))


if __name__ == "__main__":
    unittest.main()
