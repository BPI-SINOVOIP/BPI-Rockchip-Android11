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


class ScheduleHandlerTest(unittest_base.UnitTestBase):
    """Tests for ScheduleHandler.

    Attributes:
        scheduler: A mock schedule_worker.ScheduleHandler.
    """

    def setUp(self):
        """Initializes test"""
        super(ScheduleHandlerTest, self).setUp()
        # Mocking ScheduleHandler and essential methods.
        self.scheduler = schedule_worker.ScheduleHandler(mock.Mock())
        self.scheduler.response = mock.Mock()
        self.scheduler.response.write = mock.Mock()
        self.scheduler.request.get = mock.MagicMock(return_value="")

    def testSimpleJobCreation(self):
        """Asserts a job is created.

        This test defines that each model only has a single entity, and asserts
        that a job is created.
        """
        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab)
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        self.scheduler.post()
        self.assertEqual(1, len(model.JobModel.query().fetch()))
        print("A job is created successfully.")

        device_query = model.DeviceModel.query(
            model.DeviceModel.serial == device.serial)
        device = device_query.fetch()[0]
        self.assertEqual(Status.DEVICE_SCHEDULING_STATUS_DICT["reserved"],
                         device.scheduling_status)
        print("A device is reserved successfully.")

    def testPriorityScheduling(self):
        """Asserts job creation with priority scheduling."""
        product = "product"
        high_priority_schedule_test_name = "high_test"
        medium_priority_schedule_test_name = "medium_test"

        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(
            hostname=lab.hostname, product=product)
        device.put()

        schedule_high = self.GenerateScheduleModel(
            device_model=device,
            lab_model=lab,
            priority="high",
            test_name=high_priority_schedule_test_name)
        schedule_high.put()

        schedule_medium = self.GenerateScheduleModel(
            device_model=device,
            lab_model=lab,
            priority="medium",
            test_name=medium_priority_schedule_test_name)
        schedule_medium.put()

        build_dict = self.GenerateBuildModel(schedule_high)
        for key in build_dict:
            build_dict[key].put()

        self.scheduler.post()
        schedules = model.ScheduleModel.query().fetch()
        self.assertEqual(schedules[0].test_name,
                         high_priority_schedule_test_name)

    def testPrioritySchedulingWithAging(self):
        """Asserts job creation with priority scheduling with aging."""
        product = "product"
        high_priority_schedule_test_name = "high_test"
        medium_priority_schedule_test_name = "medium_test"
        schedule_period_minute = 100

        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(
            hostname=lab.hostname, product=product)
        device.put()

        schedules = []
        schedule_high = self.GenerateScheduleModel(
            device_model=device,
            lab_model=lab,
            test_name=high_priority_schedule_test_name,
            period=schedule_period_minute,
            priority="high")
        schedule_high.put()
        schedules.append(schedule_high)

        schedule_medium = self.GenerateScheduleModel(
            device_model=device,
            lab_model=lab,
            test_name=medium_priority_schedule_test_name,
            period=schedule_period_minute,
            priority="medium")
        schedule_medium.put()
        schedules.append(schedule_medium)

        for schedule in schedules:
            build_dict = self.GenerateBuildModel(schedule)
            for key in build_dict:
                build_dict[key].put()

        high_original_priority_value = schedule_high.priority_value
        medium_original_priority_value = schedule_medium.priority_value

        # On first attempt, "high" priority will create a job.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(jobs[0].test_name, high_priority_schedule_test_name)

        # medium priority schedule's priority value will be decreased.
        self.assertEqual(medium_original_priority_value - 1,
                         schedule_medium.priority_value)

        self.PassTime(minutes=schedule_period_minute + 1)
        self.ResetDevices()

        # On second attempt, "high" priority will create a job.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        jobs.sort(key=lambda x: x.timestamp, reverse=True)  # latest first
        self.assertEqual(jobs[0].test_name, high_priority_schedule_test_name)

        # medium priority schedule's priority value will be decreased again.
        self.assertEqual(medium_original_priority_value - 2,
                         schedule_medium.priority_value)

        while schedule_medium.priority_value >= high_original_priority_value:
            self.PassTime(minutes=schedule_period_minute + 1)
            self.ResetDevices()
            self.scheduler.post()

        # at last, medium priority schedule should be able to create a job.
        self.PassTime(minutes=schedule_period_minute + 1)
        self.ResetDevices()
        self.scheduler.post()

        jobs = model.JobModel.query().fetch()
        jobs.sort(key=lambda x: x.timestamp, reverse=True)  # latest first
        self.assertEqual(jobs[0].test_name, medium_priority_schedule_test_name)

        # after a job is created, its priority value should be restored.
        self.assertEqual(schedule_medium.priority_value,
                         medium_original_priority_value)

    def testPrioritySchedulingWithAgingForMultiDevices(self):
        """Asserts job creation with priority scheduling for multi devices."""
        product1 = "product1"
        product2 = "product2"
        schedule_period_minute = 360

        lab = self.GenerateLabModel()
        lab.put()

        device1 = self.GenerateDeviceModel(
            hostname=lab.hostname, product=product1)
        device1.put()

        device2 = self.GenerateDeviceModel(
            hostname=lab.hostname, product=product2)
        device2.put()

        schedule1_l = self.GenerateScheduleModel(
            device_model=device1,
            lab_model=lab,
            priority="low",
            period=schedule_period_minute)
        schedule1_l.put()

        schedule1_h = self.GenerateScheduleModel(
            device_model=device1,
            lab_model=lab,
            priority="high",
            period=schedule_period_minute)
        schedule1_h.put()

        schedule2_m = self.GenerateScheduleModel(
            device_model=device2,
            lab_model=lab,
            priority="medium",
            period=schedule_period_minute)
        schedule2_m.put()

        schedule2_h = self.GenerateScheduleModel(
            device_model=device2,
            lab_model=lab,
            priority="high",
            period=schedule_period_minute)
        schedule2_h.put()

        schedule1_l_original_priority_value = schedule1_l.priority_value
        schedule2_m_original_priority_value = schedule2_m.priority_value

        for schedule in [schedule2_m, schedule2_h]:
            build_dict = self.GenerateBuildModel(schedule)
            for key in build_dict:
                build_dict[key].put()

        # create jobs
        self.scheduler.post()

        # schedule2_m will not get a change to create a job.
        jobs = model.JobModel.query().fetch()
        self.assertTrue(
            any([job.test_name == schedule2_h.test_name for job in jobs]))
        self.assertFalse(
            any([job.test_name == schedule2_m.test_name for job in jobs]))

        # schedule2_m's priority value should be decreased.
        self.assertTrue(schedule2_m_original_priority_value - 1,
                        schedule2_m.priority_value)

        # schedule1_l's priority value should not be changed because all other
        # schedules for device1 were also failed to created a job.
        self.assertTrue(schedule1_l_original_priority_value,
                        schedule1_l.priority_value)

        for num in range(3):
            self.assertTrue(schedule2_m_original_priority_value - 1 - num,
                            schedule2_m.priority_value)
            self.PassTime(minutes=schedule_period_minute + 1)
            self.ResetDevices()
            self.scheduler.post()
            self.assertFalse(
                any([job.test_name == schedule2_m.test_name for job in jobs]))
            self.assertTrue(schedule1_l_original_priority_value,
                            schedule1_l.priority_value)

        # device1 is ready for scheduling.
        for schedule in [schedule1_l, schedule1_h]:
            build_dict = self.GenerateBuildModel(schedule)
            for key in build_dict:
                build_dict[key].put()

        # after 4 times of failure, now schedule2_m can create a job.
        self.PassTime(minutes=schedule_period_minute + 1)
        self.ResetDevices()
        self.scheduler.post()

        jobs = model.JobModel.query().fetch()
        self.assertTrue(
            any([job.test_name == schedule2_m.test_name for job in jobs]))

        # now schedule_1's priority value should be changed.
        self.assertEqual(schedule1_l_original_priority_value - 1,
                          schedule1_l.priority_value)

    def testRetryAfterBootupError(self):
        """Asserts a schedule's period is shortened after boot-up error."""
        long_period = 5760

        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab, period=long_period)
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        # a job should be created.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(1, len(jobs))

        jobs[0].status = Status.JOB_STATUS_DICT["bootup-err"]
        jobs[0].put()
        model_util.UpdateParentSchedule(jobs[0],
                                        Status.JOB_STATUS_DICT["bootup-err"])

        self.PassTime(
            minutes=schedule_worker.BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS + 1)
        self.ResetDevices()

        # new job should be created again.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(2, len(jobs))

        jobs.sort(key=lambda x: x.timestamp, reverse=True)  # latest first
        jobs[0].status = Status.JOB_STATUS_DICT["bootup-err"]
        jobs[0].put()
        model_util.UpdateParentSchedule(jobs[0],
                                        Status.JOB_STATUS_DICT["bootup-err"])

        self.PassTime(
            minutes=schedule_worker.BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS - 1)
        self.ResetDevices()

        # time is not passed enough so there would be no new job.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(2, len(jobs))

        # if latest job is completed successfully, period should be recovered.
        jobs[0].status = Status.JOB_STATUS_DICT["complete"]
        jobs[0].put()
        model_util.UpdateParentSchedule(jobs[0],
                                        Status.JOB_STATUS_DICT["complete"])

        # pass time to (period - 1)
        self.PassTime(minutes=long_period - 1 - (
            schedule_worker.BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS - 1))
        self.ResetDevices()

        # then no job will be created.
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(2, len(jobs))

        # pass time to (period + 1)
        self.PassTime(minutes=2)

        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(3, len(jobs))

    def testSimpleDevicePriorityWithEquipment(self):
        """Asserts a scheduler creates a job with minimum device equipment."""
        equipment_a = "equipment_a"
        equipment_b = "equipment_b"

        device_product = "device_product"
        lab = self.GenerateLabModel()
        lab.put()

        device_a = self.GenerateDeviceModel(
            product=device_product,
            hostname=lab.hostname,
            device_equipment=[equipment_a])
        device_a.put()

        device_b = self.GenerateDeviceModel(
            product=device_product,
            hostname=lab.hostname,
            device_equipment=[equipment_b])
        device_b.put()

        device_c = self.GenerateDeviceModel(
            product=device_product, hostname=lab.hostname)
        device_c.put()

        schedule = self.GenerateScheduleModel(
            device_target="{}-test".format(device_product),
            lab_model=lab,
            required_device_equipment=[equipment_b])
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        # a job should be created and it should be created with equipment_b
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(1, len(jobs))
        self.assertIn(device_b.serial, jobs[0].serial)

    def testDevicePriorityWithEquipment(self):
        """Asserts a scheduler creates a job with minimum device equipment."""
        lab_1 = "lab_1"
        lab_2 = "lab_2"

        host_a = "host_a"
        host_b = "host_b"
        host_c = "host_c"
        host_d = "host_d"
        host_e = "host_e"

        equipment_a = "equipment_a"
        equipment_b = "equipment_b"
        equipment_c = "equipment_c"

        correct_product = "correct"
        wrong_product = "wrong"

        self.GenerateLabModel(lab_name=lab_1, host_name=host_a).put()
        self.GenerateLabModel(lab_name=lab_1, host_name=host_b).put()
        self.GenerateLabModel(lab_name=lab_2, host_name=host_c).put()
        self.GenerateLabModel(lab_name=lab_2, host_name=host_d).put()
        self.GenerateLabModel(lab_name=lab_2, host_name=host_e).put()

        # setting devices through host a to e.
        equipments = [[equipment_a], [equipment_a], [equipment_b],
                      [equipment_a, equipment_b]]
        for equipment in equipments:
            device = self.GenerateDeviceModel(
                product=correct_product, hostname=host_a)
            device.device_equipment = equipment
            device.put()

        equipments = [[], [equipment_a], [equipment_a, equipment_b],
                      [equipment_a, equipment_b]]
        for equipment in equipments:
            device = self.GenerateDeviceModel(
                product=correct_product, hostname=host_b)
            device.device_equipment = equipment
            device.put()

        equipments = [[equipment_a], [equipment_a], [equipment_b],
                      [equipment_b]]
        for equipment in equipments:
            device = self.GenerateDeviceModel(
                product=correct_product, hostname=host_c)
            device.device_equipment = equipment
            device.put()

        equipments = [[equipment_a], [equipment_a, equipment_b, equipment_c],
                      [equipment_a, equipment_b]]
        for equipment in equipments:
            device = self.GenerateDeviceModel(
                product=correct_product, hostname=host_d)
            device.device_equipment = equipment
            device.put()

        products = [correct_product, correct_product, wrong_product]
        for product in products:
            device = self.GenerateDeviceModel(product=product, hostname=host_e)
            device.device_equipment = [equipment_a]
            device.put()

        schedule = self.GenerateScheduleModel(
            device_target="{}-test".format(correct_product), shards=3)
        schedule.required_device_equipment = [equipment_a]
        schedule.device = [
            "{}/{}".format(lab_1, correct_product), "{}/{}".format(
                lab_2, correct_product)
        ]
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        # a job should be created on host_a
        self.scheduler.post()
        jobs = model.JobModel.query().fetch()
        self.assertEqual(1, len(jobs))

        host_a_devices = model.DeviceModel.query(
            model.DeviceModel.hostname == host_a).fetch()
        host_a_devices_serial = [x.serial for x in host_a_devices]

        for job_device in jobs[0].serial:
            self.assertIn(job_device, host_a_devices_serial)

    def testSelectTargetLab(self):
        """Asserts SelectTargetLab() method."""
        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab)
        schedule.put()

        ret_host, ret_device, ret_serials = (
            self.scheduler.SelectTargetLab(schedule))

        self.assertEqual(lab.hostname, ret_host)
        self.assertEqual("{}/{}".format(lab.name, device.product), ret_device)
        self.assertEqual([device.serial], ret_serials)

    def testSimpleJobCreationWithOutdatedBuild(self):
        """Asserts an outdated build is filtered out."""
        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab)
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].timestamp = datetime.datetime.now(
            ) - datetime.timedelta(hours=73)
            build_dict[key].put()

        self.scheduler.post()
        self.assertEqual(0, len(model.JobModel.query().fetch()))

        builds = model.BuildModel().query().fetch()
        for build in builds:
            build.timestamp = datetime.datetime.now()
            build.put()

        self.scheduler.post()
        self.assertEqual(1, len(model.JobModel.query().fetch()))

    def testSimpleJobCreationWithOutdatedSchedule(self):
        """Asserts an outdated schedule is filtered out."""
        lab = self.GenerateLabModel()
        lab.put()

        device = self.GenerateDeviceModel(hostname=lab.hostname)
        device.put()

        schedule = self.GenerateScheduleModel(
            device_model=device, lab_model=lab)
        schedule.timestamp = datetime.datetime.now() - datetime.timedelta(
            hours=73)
        schedule.put()

        build_dict = self.GenerateBuildModel(schedule)
        for key in build_dict:
            build_dict[key].put()

        self.scheduler.post()
        self.assertEqual(0, len(model.JobModel.query().fetch()))

        schedule = model.ScheduleModel().query().fetch()[0]
        schedule.timestamp = datetime.datetime.now()
        schedule.put()

        self.scheduler.post()
        self.assertEqual(1, len(model.JobModel.query().fetch()))


if __name__ == "__main__":
    unittest.main()
