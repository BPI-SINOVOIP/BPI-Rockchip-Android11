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

import datetime
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src import vtslab_status as Status
from webapp.src.proto import model
from webapp.src.scheduler import schedule_worker
from webapp.src.testing import unittest_base
from webapp.src.utils import model_util


class ModelTest(unittest_base.UnitTestBase):
    """Tests for PeriodicJobHeartBeat cron class."""

    def testJobAndScheduleModel(self):
        """Asserts JobModel and ScheduleModel.

        When JobModel's status is changed, ScheduleModel's error_count is
        changed based on the status. This should not be applied before JobModel
        entity is updated to Datastore.
        """
        period = 360

        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab, period=period)
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        # Mocking ScheduleHandler and essential methods.
        scheduler = schedule_worker.ScheduleHandler(mock.Mock())
        scheduler.response = mock.Mock()
        scheduler.response.write = mock.Mock()
        scheduler.request.get = mock.MagicMock(return_value="")

        print("\nCreating a job...")
        scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(1, len(jobs))

        print("Occurring infra error...")
        job = jobs[0]
        job.status = Status.JOB_STATUS_DICT["infra-err"]
        parent_schedule = job.parent_schedule.get()
        parent_from_db = model.ScheduleModel.query().fetch()[0]

        # in test error_count could be None but in real there will be no None.
        self.assertNotEqual(1, parent_schedule.error_count)
        self.assertNotEqual(1, parent_from_db.error_count)

        # error count should be changed after put
        job.put()
        model_util.UpdateParentSchedule(job, job.status)
        self.assertEqual(1, parent_schedule.error_count)
        self.assertEqual(1, parent_from_db.error_count)

        print("Suspending a job...")
        for num in xrange(2):
            jobs = model.JobModel.query().fetch()
            for job in jobs:
                job.timestamp = datetime.datetime.now() - datetime.timedelta(
                    minutes=(period + 10))
                job.put()

            parent_from_db = model.ScheduleModel.query().fetch()[0]
            self.assertEqual(1 + num, parent_schedule.error_count)
            self.assertEqual(1 + num, parent_from_db.error_count)

            # reset a device manually to re-schedule
            device = model.DeviceModel.query().fetch()[0]
            device.status = Status.DEVICE_STATUS_DICT["fastboot"]
            device.scheduling_status = (
                Status.DEVICE_SCHEDULING_STATUS_DICT["free"])
            device.timestamp = datetime.datetime.now()
            device.put()

            scheduler.post()
            jobs = model.JobModel.query().fetch()
            self.assertEqual(2 + num, len(jobs))

            ready_jobs = model.JobModel.query(
                model.JobModel.status == Status.JOB_STATUS_DICT[
                    "ready"]).fetch()
            self.assertEqual(1, len(ready_jobs))

            ready_job = ready_jobs[0]
            ready_job.status = Status.JOB_STATUS_DICT["infra-err"]
            parent_schedule = ready_job.parent_schedule.get()
            parent_from_db = model.ScheduleModel.query().fetch()[0]
            self.assertEqual(1 + num, parent_schedule.error_count)
            self.assertEqual(1 + num, parent_from_db.error_count)

            # # error count should be changed after put
            ready_job.put()
            model_util.UpdateParentSchedule(ready_job, ready_job.status)
            self.assertEqual(2 + num, parent_schedule.error_count)
            self.assertEqual(2 + num, parent_from_db.error_count)

        print("Asserting a schedule's suspend status...")
        # after three errors the schedule should be suspended.
        schedule_from_db = model.ScheduleModel.query().fetch()[0]
        schedule_from_db.put()
        self.assertEqual(3, schedule_from_db.error_count)
        self.assertEqual(True, schedule_from_db.suspended)

        # reset a device manually to re-schedule
        device = model.DeviceModel.query().fetch()[0]
        device.status = Status.DEVICE_STATUS_DICT["fastboot"]
        device.scheduling_status = (
            Status.DEVICE_SCHEDULING_STATUS_DICT["free"])
        device.timestamp = datetime.datetime.now()
        device.put()

        print("Asserting that job creation is blocked...")
        jobs = model.JobModel.query().fetch()
        self.assertEqual(3, len(jobs))

        for job in jobs:
            job.timestamp = datetime.datetime.now() - datetime.timedelta(
                minutes=(period + 10))
            job.put()

        scheduler.post()

        # a job should not be created.
        jobs = model.JobModel.query().fetch()
        self.assertEqual(3, len(jobs))

        print("Asserting that job creation is allowed after resuming...")
        schedule_from_db = model.ScheduleModel.query().fetch()[0]
        schedule_from_db.suspended = False
        schedule_from_db.put()

        scheduler.post()

        jobs = model.JobModel.query().fetch()
        self.assertEqual(4, len(jobs))


if __name__ == "__main__":
    unittest.main()
