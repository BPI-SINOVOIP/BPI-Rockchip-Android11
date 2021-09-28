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
import itertools
import logging
import re

from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.proto import model
from webapp.src.utils import logger
import webapp2

MAX_LOG_CHARACTERS = 10000  # maximum number of characters per each log
BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS = 60  # retry minutes when boot-up error is occurred

CREATE_JOB_SUCCESS = "success"
CREATE_JOB_FAILED_NO_BUILD = "no_build"
CREATE_JOB_FAILED_NO_DEVICE = "no_device"


def GetTestVersionType(manifest_branch, gsi_branch, test_type=0):
    """Compares manifest branch and gsi branch to get test type.

    This function only completes two LSBs which represent version related
    test type.

    Args:
        manifest_branch: a string, manifest branch name.
        gsi_branch: a string, gsi branch name.
        test_type: an integer, previous test type value.

    Returns:
        An integer, test type value.
    """
    if not test_type:
        value = 0
    else:
        # clear two bits
        value = test_type & ~(1 | 1 << 1)

    if not manifest_branch:
        logging.debug("manifest branch cannot be empty or None.")
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_UNKNOWN]

    if not gsi_branch:
        logging.debug("gsi_branch is empty.")
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_TOT]

    gcs_pattern = "^gs://.*/v([0-9.]*)/.*"
    q_pattern = "(git_)?(aosp-)?q.*"
    p_pattern = "(git_)?(aosp-)?p.*"
    o_mr1_pattern = "(git_)?(aosp-)?o[^-]*-m.*"
    o_pattern = "(git_)?(aosp-)?o.*"
    master_pattern = "(git_)?(aosp-)?master"

    gcs_search = re.search(gcs_pattern, manifest_branch)
    if gcs_search:
        device_version = gcs_search.group(1)
    elif re.match(q_pattern, manifest_branch):
        device_version = "10.0"
    elif re.match(p_pattern, manifest_branch):
        device_version = "9.0"
    elif re.match(o_mr1_pattern, manifest_branch):
        device_version = "8.1"
    elif re.match(o_pattern, manifest_branch):
        device_version = "8.0"
    elif re.match(master_pattern, manifest_branch):
        device_version = "master"
    else:
        logging.debug("Unknown device version.")
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_UNKNOWN]

    gcs_search = re.search(gcs_pattern, gsi_branch)
    if gcs_search:
        gsi_version = gcs_search.group(1)
    elif re.match(q_pattern, gsi_branch):
        gsi_version = "10.0"
    elif re.match(p_pattern, gsi_branch):
        gsi_version = "9.0"
    elif re.match(o_mr1_pattern, gsi_branch):
        gsi_version = "8.1"
    elif re.match(o_pattern, gsi_branch):
        gsi_version = "8.0"
    elif re.match(master_pattern, gsi_branch):
        gsi_version = "master"
    else:
        logging.debug("Unknown gsi version.")
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_UNKNOWN]

    if device_version == gsi_version:
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_TOT]
    else:
        return value | Status.TEST_TYPE_DICT[Status.TEST_TYPE_OTA]


class ScheduleHandler(webapp2.RequestHandler):
    """Background worker class for /worker/schedule_handler.

    This class pull tasks from 'queue-schedule' queue and processes in
    background service 'worker'.

    Attributes:
        logger: Logger class
    """
    logger = logger.Logger()

    def ReserveDevices(self, target_device_serials):
        """Reserves devices.

        Args:
            target_device_serials: a list of strings, containing target device
                                   serial numbers.
        """
        device_query = model.DeviceModel.query(
            model.DeviceModel.serial.IN(target_device_serials))
        devices = device_query.fetch()
        devices_to_put = []
        for device in devices:
            device.scheduling_status = Status.DEVICE_SCHEDULING_STATUS_DICT[
                "reserved"]
            devices_to_put.append(device)
        if devices_to_put:
            ndb.put_multi(devices_to_put)

    def FindBuildId(self, artifact_type, manifest_branch, target,
                    signed=False):
        """Finds a designated build ID.

        Args:
            artifact_type: a string, build artifact type.
            manifest_branch: a string, build manifest branch.
            target: a string which build target and type are joined by '-'.
            signed: a boolean to get a signed build.

        Return:
            string, build ID found.
        """
        build_id = ""
        if "-" in target:
            build_target, build_type = target.split("-")
        else:
            build_target = target
            build_type = ""
        if not artifact_type or not manifest_branch or not build_target:
            self.logger.Println("The argument format is invalid.")
            return build_id
        build_query = model.BuildModel.query(
            model.BuildModel.artifact_type == artifact_type,
            model.BuildModel.manifest_branch == manifest_branch,
            model.BuildModel.build_target == build_target,
            model.BuildModel.build_type == build_type)
        builds = build_query.fetch()

        if builds:
            builds = [
                build for build in builds
                if (build.timestamp >
                    datetime.datetime.now() - datetime.timedelta(hours=72))
            ]

        if builds:
            self.logger.Println("-- Found build ID")
            builds.sort(key=lambda x: x.build_id, reverse=True)
            for build in builds:
                if not signed or build.signed:
                    build_id = build.build_id
                    break
        return build_id

    def post(self):
        self.logger.Clear()
        manual_job = False
        schedule_key = self.request.get("schedule_key")
        if schedule_key:
            key = ndb.key.Key(urlsafe=schedule_key)
            manual_job = True
            schedules = [key.get()]
        else:
            schedule_query = model.ScheduleModel.query(
                model.ScheduleModel.suspended != True)
            schedules = schedule_query.fetch()

        if schedules:
            # filter out the schedules which are not updated within 72 hours.
            schedules = [
                schedule for schedule in schedules
                if (schedule.timestamp >
                    datetime.datetime.now() - datetime.timedelta(hours=72))
            ]
            schedules = self.FilterWithPeriod(schedules)

        if schedules:
            schedules.sort(key=lambda x: self.GetProductName(x))
            group_by_product = [
                list(g)
                for _, g in itertools.groupby(schedules,
                                              lambda x: self.GetProductName(x))
            ]
            for group in group_by_product:
                group.sort(key=lambda x: x.priority_value if (
                    x.priority_value) else Status.GetPriorityValue(x.priority))
                create_result = {
                    CREATE_JOB_SUCCESS: [],
                    CREATE_JOB_FAILED_NO_BUILD: [],
                    CREATE_JOB_FAILED_NO_DEVICE: []
                }
                for schedule in group:
                    self.logger.Println("")
                    self.logger.Println("Schedule: %s (branch: %s)" %
                                        (schedule.test_name,
                                         schedule.manifest_branch))
                    self.logger.Println(
                        "Build Target: %s" % schedule.build_target)
                    self.logger.Println("Device: %s" % schedule.device)
                    self.logger.Indent()
                    result, lab = self.CreateJob(schedule, manual_job)
                    if result == CREATE_JOB_SUCCESS:
                        create_result[result].append(lab)
                    else:
                        create_result[result].append(schedule)
                    self.logger.Unindent()
                # if any schedule in group created a job, increase priority of
                # the schedules which couldn't create due to out of devices.
                schedules_to_put = []
                for lab in create_result[CREATE_JOB_SUCCESS]:
                    for schedule in create_result[CREATE_JOB_FAILED_NO_DEVICE]:
                        if any([lab in target for target in schedule.device
                                ]) and schedule not in schedules_to_put:
                            if schedule.priority_value is None:
                                schedule.priority_value = (
                                    Status.GetPriorityValue(schedule.priority))
                            if schedule.priority_value > 0:
                                schedule.priority_value -= 1
                                schedules_to_put.append(schedule)
                if schedules_to_put:
                    ndb.put_multi(schedules_to_put)

        self.logger.Println("Scheduling completed.")

        lines = self.logger.Get()
        lines = [line.strip() for line in lines]
        outputs = []
        chars = 0
        for line in lines:
            chars += len(line)
            if chars > MAX_LOG_CHARACTERS:
                logging.info("\n".join(outputs))
                outputs = []
                chars = len(line)
            outputs.append(line)
        logging.info("\n".join(outputs))

    def CreateJob(self, schedule, manual_job=False):
        """Creates a job for given schedule.

        Args:
            schedule: model.ScheduleModel instance.
            manual_job: True if a job is created by a user, False otherwise.

        Returns:
            a string of job creation result message.
            a string of lab name if job is created, otherwise empty string.
        """
        target_host, target_device, target_device_serials = (
            self.SelectTargetLab(schedule))
        if not target_host:
            return CREATE_JOB_FAILED_NO_DEVICE, ""

        self.logger.Println("- Target host: %s" % target_host)
        self.logger.Println("- Target device: %s" % target_device)
        self.logger.Println("- Target serials: %s" % target_device_serials)

        # create job and add.
        new_job = model.JobModel()
        new_job.hostname = target_host
        new_job.priority = schedule.priority
        new_job.test_name = schedule.test_name
        new_job.require_signed_device_build = (
            schedule.require_signed_device_build)
        new_job.device = target_device
        new_job.period = schedule.period
        new_job.serial.extend(target_device_serials)
        new_job.build_storage_type = schedule.build_storage_type
        new_job.manifest_branch = schedule.manifest_branch
        new_job.build_target = schedule.build_target
        new_job.pab_account_id = schedule.device_pab_account_id
        new_job.shards = schedule.shards
        new_job.param = schedule.param
        new_job.retry_count = schedule.retry_count
        new_job.gsi_storage_type = schedule.gsi_storage_type
        new_job.gsi_branch = schedule.gsi_branch
        new_job.gsi_build_target = schedule.gsi_build_target
        new_job.gsi_pab_account_id = schedule.gsi_pab_account_id
        new_job.gsi_vendor_version = schedule.gsi_vendor_version
        new_job.test_storage_type = schedule.test_storage_type
        new_job.test_branch = schedule.test_branch
        new_job.test_build_target = schedule.test_build_target
        new_job.test_pab_account_id = schedule.test_pab_account_id
        new_job.parent_schedule = schedule.key
        new_job.image_package_repo_base = schedule.image_package_repo_base
        new_job.required_host_equipment = schedule.required_host_equipment
        new_job.required_device_equipment = schedule.required_device_equipment
        new_job.has_bootloader_img = schedule.has_bootloader_img
        new_job.has_radio_img = schedule.has_radio_img
        new_job.report_bucket = schedule.report_bucket
        new_job.report_spreadsheet_id = schedule.report_spreadsheet_id
        new_job.report_persistent_url = schedule.report_persistent_url
        new_job.report_reference_url = schedule.report_reference_url

        # uses bit 0-1 to indicate version.
        test_type = GetTestVersionType(schedule.manifest_branch,
                                       schedule.gsi_branch)
        # uses bit 2
        if schedule.require_signed_device_build:
            test_type |= Status.TEST_TYPE_DICT[Status.TEST_TYPE_SIGNED]

        if manual_job:
            test_type |= Status.TEST_TYPE_DICT[Status.TEST_TYPE_MANUAL]

        new_job.test_type = test_type

        new_job.build_id = ""
        new_job.gsi_build_id = ""
        new_job.test_build_id = ""
        for artifact_type in ["device", "gsi", "test"]:
            if artifact_type == "device":
                storage_type_text = "build_storage_type"
                manifest_branch_text = "manifest_branch"
                build_target_text = "build_target"
                build_id_text = "build_id"
                signed = new_job.require_signed_device_build
            else:
                storage_type_text = artifact_type + "_storage_type"
                manifest_branch_text = artifact_type + "_branch"
                build_target_text = artifact_type + "_build_target"
                build_id_text = artifact_type + "_build_id"
                signed = False

            manifest_branch = getattr(new_job, manifest_branch_text)
            build_target = getattr(new_job, build_target_text)
            storage_type = getattr(new_job, storage_type_text)
            if storage_type == Status.STORAGE_TYPE_DICT["PAB"]:
                build_id = self.FindBuildId(
                    artifact_type=artifact_type,
                    manifest_branch=manifest_branch,
                    target=build_target,
                    signed=signed)
            elif storage_type == Status.STORAGE_TYPE_DICT["GCS"]:
                # temp value to distinguish from empty values.
                build_id = "gcs"
            else:
                build_id = ""
                self.logger.Println(
                    "Unexpected storage type (%s)." % storage_type)
            setattr(new_job, build_id_text, build_id)

        if ((not new_job.manifest_branch or new_job.build_id)
                and (not new_job.gsi_branch or new_job.gsi_build_id)
                and (not new_job.test_branch or new_job.test_build_id)):
            new_job.build_id = new_job.build_id.replace("gcs", "")
            new_job.gsi_build_id = (new_job.gsi_build_id.replace("gcs", ""))
            new_job.test_build_id = (new_job.test_build_id.replace("gcs", ""))
            self.ReserveDevices(target_device_serials)
            new_job.status = Status.JOB_STATUS_DICT["ready"]
            new_job.timestamp = datetime.datetime.now()
            new_job_key = new_job.put()
            schedule.children_jobs.append(new_job_key)
            schedule.priority_value = Status.GetPriorityValue(
                schedule.priority)
            schedule.put()
            self.logger.Println("A new job has been created.")
            labs = model.LabModel.query(
                model.LabModel.hostname == target_host).fetch()
            return CREATE_JOB_SUCCESS, labs[0].name
        else:
            self.logger.Println("Cannot find builds to create a job.")
            self.logger.Println("- Device branch / build - {} / {}".format(
                new_job.manifest_branch, new_job.build_id))
            self.logger.Println("- GSI branch / build - {} / {}".format(
                new_job.gsi_branch, new_job.gsi_build_id))
            self.logger.Println("- Test branch / build - {} / {}".format(
                new_job.test_branch, new_job.test_build_id))
            return CREATE_JOB_FAILED_NO_BUILD, ""

    def FilterWithPeriod(self, schedules):
        """Filters schedules with period.

        This method filters schedules if any children jobs are created within
        period time.

        Args:
            schedules: a list of model.ScheduleModel instances.

        Returns:
            a list of model.ScheduleModel instances which need to create a new
            job.
        """
        ret_list = []
        if not schedules:
            return ret_list

        if type(schedules) is not list:
            schedules = [schedules]

        for schedule in schedules:
            if not schedule.children_jobs:
                ret_list.append(schedule)
                continue

            latest_job_key = schedule.children_jobs[-1]
            latest_job = latest_job_key.get()

            if datetime.datetime.now() - latest_job.timestamp > (
                    datetime.timedelta(
                        minutes=self.GetCorrectedPeriod(schedule))):
                ret_list.append(schedule)

        return ret_list

    def SelectTargetLab(self, schedule):
        """Find target host and devices to schedule a new job.

        Args:
            schedule: a proto containing the information of a schedule.

        Returns:
            a string which represents hostname,
            a string containing target lab and product with '/' separator,
            a list of selected devices serial (see whether devices will be
            selected later when the job is picked up.)
        """

        available_devices = []
        for target_device in schedule.device:
            if "/" not in target_device:
                self.logger.Println(
                    "Device malformed - {}".format(target_device))
                continue

            target_lab, target_product_type = target_device.split("/")
            self.logger.Println("- Lab %s" % target_lab)
            self.logger.Indent()
            host_query = model.LabModel.query(
                model.LabModel.name == target_lab)
            target_hosts = host_query.fetch()

            if target_hosts:
                for host in target_hosts:
                    if not (set(schedule.required_host_equipment) <= set(
                            host.host_equipment)):
                        continue
                    self.logger.Println("- Host: %s" % host.hostname)
                    self.logger.Indent()
                    device_query = model.DeviceModel.query(
                        model.DeviceModel.hostname == host.hostname,
                        model.DeviceModel.scheduling_status ==
                        Status.DEVICE_SCHEDULING_STATUS_DICT["free"],
                        model.DeviceModel.status.IN([
                            Status.DEVICE_STATUS_DICT["fastboot"],
                            Status.DEVICE_STATUS_DICT["online"],
                            Status.DEVICE_STATUS_DICT["ready"]
                        ]))
                    host_devices = device_query.fetch()
                    host_devices = [
                        x for x in host_devices
                        if x.product.lower() == target_product_type.lower() and
                        (set(schedule.required_device_equipment) <= set(
                            x.device_equipment))
                    ]
                    if len(host_devices) < schedule.shards:
                        self.logger.Println(
                            "A host {} does not have enough devices. "
                            "# of devices = {}, shards = {}".format(
                                host.hostname, len(host_devices),
                                schedule.shards))
                        self.logger.Unindent()
                        continue
                    host_devices.sort(
                        key=lambda x: (len(x.device_equipment)
                                       if x.device_equipment else 0))
                    available_devices.append((host_devices, target_device))
                    self.logger.Unindent()

            self.logger.Unindent()

        if not available_devices:
            self.logger.Println("No hosts have enough devices for schedule!")
            return None, None, []

        available_devices.sort(key=lambda x: (
            sum([len(y.device_equipment) for y in x[0][:schedule.shards]])))
        selected_host_devices = available_devices[0]
        return selected_host_devices[0][0].hostname, selected_host_devices[
            1], [x.serial for x in selected_host_devices[0][:schedule.shards]]

    def GetProductName(self, schedule):
        """Gets a product name from schedule instance.

        Args:
            schedule: a schedule instance.

        Returns:
            a string, product name in lowercase.
        """
        if not schedule or not schedule.device:
            return ""

        if "/" not in schedule.device[0]:
            return ""

        return schedule.device[0].split("/")[1].lower()

    def GetCorrectedPeriod(self, schedule):
        """Corrects and returns period value based on latest children jobs.

        Args:
            schedule: a model.ScheduleModel instance containing schedule
                      information.

        Returns:
            an integer, corrected schedule period.
        """
        if not schedule.error_count or not schedule.children_jobs or (
                schedule.period <= BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS):
            return schedule.period

        latest_job = schedule.children_jobs[-1].get()

        if latest_job.status == Status.JOB_STATUS_DICT["bootup-err"]:
            return BOOTUP_ERROR_RETRY_INTERVAL_IN_MINS
        else:
            return schedule.period
