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
from webapp.src.scheduler import job_heartbeat
from webapp.src.scheduler import schedule_worker
from webapp.src.testing import unittest_base


class JobHeartbeatTest(unittest_base.UnitTestBase):
    """Tests for PeriodicJobHeartBeat cron class.

    Attributes:
        testbed: A Testbed instance which provides local unit testing.
        job_heartbeat: A mock job_heartbeat.PeriodicJobHeartBeat instance.
    """

    def setUp(self):
        """Initializes test"""
        super(JobHeartbeatTest, self).setUp()
        # Mocking PeriodicJobHeartBeat and essential methods.
        self.job_heartbeat = job_heartbeat.PeriodicJobHeartBeat(mock.Mock())
        self.job_heartbeat.response = mock.Mock()
        self.job_heartbeat.response.write = mock.Mock()

    def testJobHearbeat(self):
        """Asserts job heartbeat detects unavailable jobs."""
        num_of_devices = 2
        shards = 2

        lab = self.GenerateLabModel()
        lab.put()

        devices = []
        for _ in range(num_of_devices):
            for i in range(shards):
                device = self.GenerateDeviceModel(
                    hostname=lab.hostname, product="product{}".format(i))
                device.put()
                devices.append(device)

        schedules = []
        for device in devices:
            schedule = self.GenerateScheduleModel(
                lab_model=lab, device_model=device, shards=shards)
            schedule.put()
            schedules.append(schedule)

        for schedule in schedules:
            build_dict = self.GenerateBuildModel(schedule)
            for key in build_dict:
                build_dict[key].put()

        # Mocking ScheduleHandler and essential methods.
        scheduler = schedule_worker.ScheduleHandler(mock.Mock())
        scheduler.response = mock.Mock()
        scheduler.response.write = mock.Mock()
        scheduler.request.get = mock.MagicMock(return_value="")

        # Creating jobs.
        scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(2, len(jobs))

        # jobs[0] will get old enough so it will be timed out.
        jobs[0].status = Status.JOB_STATUS_DICT["leased"]
        jobs[0].timestamp = (datetime.datetime.now() - datetime.timedelta(
            seconds=job_heartbeat.JOB_RESPONSE_TIMEOUT_SECONDS + 5))
        jobs[0].heartbeat_stamp = (
            datetime.datetime.now() - datetime.timedelta(
                seconds=job_heartbeat.JOB_RESPONSE_TIMEOUT_SECONDS + 5))
        jobs[0].put()

        # jobs[1] will not exceed the timeout time.
        jobs[1].status = Status.JOB_STATUS_DICT["leased"]
        jobs[1].timestamp = (datetime.datetime.now() - datetime.timedelta(
            seconds=job_heartbeat.JOB_RESPONSE_TIMEOUT_SECONDS - 5))
        jobs[1].heartbeat_stamp = (
            datetime.datetime.now() - datetime.timedelta(
                seconds=job_heartbeat.JOB_RESPONSE_TIMEOUT_SECONDS - 5))
        jobs[1].put()

        # Creating jobs.
        self.job_heartbeat.get()

        # One job(job[0]) should be changed to infra-err status.
        jobs = model.JobModel.query().fetch()
        infra_error_jobs = [
            x for x in jobs if x.status == Status.JOB_STATUS_DICT["infra-err"]
        ]
        self.assertEqual(len(infra_error_jobs), 1)

        # job[0]'s devices should be changed to free scheduling status.
        serials = infra_error_jobs[0].serial
        devices = model.DeviceModel.query(
            model.DeviceModel.serial.IN(serials)).fetch()
        for device in devices:
            self.assertEqual(device.scheduling_status,
                              Status.DEVICE_SCHEDULING_STATUS_DICT["free"])


if __name__ == "__main__":
    unittest.main()
