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
from webapp.src.endpoint import job_queue
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class JobQueueTest(unittest_base.UnitTestBase):
    """A class to test job_queue endpoint API."""

    def setUp(self):
        """Initializes test"""
        super(JobQueueTest, self).setUp()

    def testGetJobModel(self):
        """Asserts job_queue/get API receives a job lease request."""
        test_values = {
            "test_type": Status.TEST_TYPE_DICT[Status.TEST_TYPE_TOT],
            "hostname": self.GetRandomString(),
            "priority": self.GetRandomString(),
            "test_name": self.GetRandomString(),
            "require_signed_device_build": False,
            "has_bootloader_img": True,
            "has_radio_img": False,
            "device": self.GetRandomString(),
            "serial": ["serial01", "serial02"],
            "build_storage_type": Status.STORAGE_TYPE_DICT["GCS"],
            "manifest_branch": self.GetRandomString(),
            "build_target": self.GetRandomString(),
            "build_id": self.GetRandomString(),
            "pab_account_id": self.GetRandomString(),
            "shards": 1,
            "param": [""],
            "status": Status.JOB_STATUS_DICT["ready"],
            "period": 360,
            "gsi_storage_type": Status.STORAGE_TYPE_DICT["GCS"],
            "gsi_branch": self.GetRandomString(),
            "gsi_build_target": self.GetRandomString(),
            "gsi_build_id": self.GetRandomString(),
            "gsi_pab_account_id": self.GetRandomString(),
            "gsi_vendor_version": self.GetRandomString(),
            "test_storage_type": Status.STORAGE_TYPE_DICT["GCS"],
            "test_branch": self.GetRandomString(),
            "test_build_target": self.GetRandomString(),
            "test_build_id": self.GetRandomString(),
            "test_pab_account_id": self.GetRandomString(),
            "retry_count": 2,
            "infra_log_url": self.GetRandomString(),
            "image_package_repo_base": self.GetRandomString(),
            "report_bucket": [self.GetRandomString()],
            "report_spreadsheet_id": [self.GetRandomString()],
            "report_persistent_url": [self.GetRandomString()],
            "report_reference_url": [self.GetRandomString()],
        }

        for serial in test_values["serial"]:
            self.GenerateDeviceModel(serial=serial).put()

        job = model.JobModel()
        for key in test_values:
            setattr(job, key, test_values[key])
        job.timestamp = datetime.datetime.now()
        job.put()

        container = (job_queue.JOB_QUEUE_RESOURCE.combined_message_class(
            hostname=test_values["hostname"]))
        api = job_queue.JobQueueApi()
        response = api.lease(container)

        self.assertEqual(response.return_code,
                          model.ReturnCodeMessage.SUCCESS)
        self.assertEqual(len(response.jobs), 1)
        for key in test_values:
            if key is "status":
                self.assertEqual(
                    getattr(response.jobs[0], key),
                    Status.JOB_STATUS_DICT["leased"])
            else:
                self.assertEqual(
                    getattr(response.jobs[0], key), test_values[key])

        devices = model.DeviceModel.query().fetch()
        for device in devices:
            self.assertEqual(device.scheduling_status,
                              Status.DEVICE_SCHEDULING_STATUS_DICT["use"])

        # test job heartbeat api
        container = (job_queue.JOB_QUEUE_RESOURCE.combined_message_class(
            hostname=response.jobs[0].hostname,
            manifest_branch=response.jobs[0].manifest_branch,
            build_target=response.jobs[0].build_target,
            test_name=response.jobs[0].test_name,
            serial=response.jobs[0].serial,
            status=response.jobs[0].status,
        ))
        api = job_queue.JobQueueApi()
        response = api.heartbeat(container)
        self.assertEqual(response.return_code,
                          model.ReturnCodeMessage.SUCCESS)

        jobs = model.JobModel.query().fetch()
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0].status, Status.JOB_STATUS_DICT["leased"])
        self.assertTrue(datetime.datetime.now() - jobs[0].heartbeat_stamp <
                        datetime.timedelta(seconds=1))

        # test job heartbeat api to complete the job
        container = (job_queue.JOB_QUEUE_RESOURCE.combined_message_class(
            hostname=response.jobs[0].hostname,
            manifest_branch=response.jobs[0].manifest_branch,
            build_target=response.jobs[0].build_target,
            test_name=response.jobs[0].test_name,
            serial=response.jobs[0].serial,
            status=Status.JOB_STATUS_DICT["complete"],
        ))
        api = job_queue.JobQueueApi()
        response = api.heartbeat(container)
        self.assertEqual(response.return_code,
                          model.ReturnCodeMessage.SUCCESS)

        jobs = model.JobModel.query().fetch()
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0].status, Status.JOB_STATUS_DICT["complete"])
        self.assertTrue(datetime.datetime.now() - jobs[0].heartbeat_stamp <
                        datetime.timedelta(seconds=1))

        devices = model.DeviceModel.query().fetch()
        for device in devices:
            self.assertEqual(device.scheduling_status,
                              Status.DEVICE_SCHEDULING_STATUS_DICT["free"])


if __name__ == "__main__":
    unittest.main()
