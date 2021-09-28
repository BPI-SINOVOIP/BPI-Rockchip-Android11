# Copyright 2017 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Job Queue Info APIs implemented using Google Cloud Endpoints."""

import datetime
import endpoints
import logging
import re

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import endpoint_base
from webapp.src.proto import model
from webapp.src.utils import email_util
from webapp.src.utils import model_util

from google.appengine.ext import ndb

JOB_QUEUE_RESOURCE = endpoints.ResourceContainer(model.JobMessage)
GCS_URL_PREFIX = "gs://"
HTTP_HTTPS_REGEX = "^https?://"
STORAGE_API_URL = "https://storage.cloud.google.com/"


@endpoints.api(name='job', version='v1')
class JobQueueApi(endpoint_base.EndpointBase):
    """Endpoint API for job_queue."""

    @endpoints.method(
        JOB_QUEUE_RESOURCE,
        model.JobLeaseResponse,
        path='lease',
        http_method='POST',
        name='lease')
    def lease(self, request):
        """Gets the job(s) based on the condition specified in `request`."""
        job_query = model.JobModel.query(
            model.JobModel.hostname == request.hostname,
            model.JobModel.status == Status.JOB_STATUS_DICT["ready"])
        existing_jobs = job_query.fetch()

        priority_sorted_jobs = sorted(
            existing_jobs,
            key=lambda x: (Status.GetPriorityValue(x.priority), x.timestamp))

        if priority_sorted_jobs:
            job = priority_sorted_jobs[0]
            job.status = Status.JOB_STATUS_DICT["leased"]
            job.put()

            job_message = model.JobMessage()
            common_attributes = self.GetCommonAttributes(job, model.JobMessage)
            for attr in common_attributes:
                setattr(job_message, attr, getattr(job, attr))

            device_query = model.DeviceModel.query(
                model.DeviceModel.serial.IN(job.serial))
            devices = device_query.fetch()
            devices_to_put = []
            for device in devices:
                device.scheduling_status = Status.DEVICE_SCHEDULING_STATUS_DICT[
                    "use"]
                devices_to_put.append(device)
            if devices_to_put:
                ndb.put_multi(devices_to_put)

            return model.JobLeaseResponse(
                return_code=model.ReturnCodeMessage.SUCCESS,
                jobs=[job_message])
        else:
            return model.JobLeaseResponse(
                return_code=model.ReturnCodeMessage.FAIL, jobs=[])

    @endpoints.method(
        JOB_QUEUE_RESOURCE,
        model.JobLeaseResponse,
        path='heartbeat',
        http_method='POST',
        name='heartbeat')
    def heartbeat(self, request):
        """Processes the heartbeat signal from HC which leased queued job(s)."""
        # minify jobs by query and confirm with serial from fetched jobs
        job_query = model.JobModel.query(
            model.JobModel.hostname == request.hostname,
            model.JobModel.manifest_branch == request.manifest_branch,
            model.JobModel.build_target == request.build_target,
            model.JobModel.test_name == request.test_name,
            model.JobModel.status == Status.JOB_STATUS_DICT["leased"])
        existing_jobs = job_query.fetch()
        same_jobs = [
            x for x in existing_jobs if set(x.serial) == set(request.serial)
        ]

        if len(same_jobs) > 1:
            logging.warning("[heartbeat] more than one job is found!")
            logging.warning(
                "[heartbeat] <hostname>{} <manifest_branch>{} "
                "<build_target>{} <test_name>{} <serials>{}".format(
                    request.hostname, request.manifest_branch,
                    request.build_target, request.test_name, request.serial))

        if same_jobs:
            job = same_jobs[0]
            job_message = model.JobMessage()
            common_attributes = self.GetCommonAttributes(job, model.JobMessage)
            for attr in common_attributes:
                setattr(job_message, attr, getattr(job, attr))

            device_query = model.DeviceModel.query(
                model.DeviceModel.serial.IN(job.serial))
            devices = device_query.fetch()
            logging.debug("[heartbeat] heartbeat job: hostname={}, "
                          "test_name={}, job creation time={}".format(
                              job.hostname, job.test_name, job.timestamp))
            logging.debug("[heartbeat] request status: {}".format(
                request.status))
            logging.debug("[heartbeat]  - devices = {}".format(
                ", ".join([device.serial for device in devices])))
            devices_to_put = []
            if request.status == Status.JOB_STATUS_DICT["complete"]:
                job.status = request.status
                for device in devices:
                    device.scheduling_status = (
                        Status.DEVICE_SCHEDULING_STATUS_DICT["free"])
                    devices_to_put.append(device)
            elif (request.status in [
                    Status.JOB_STATUS_DICT["infra-err"],
                    Status.JOB_STATUS_DICT["bootup-err"]
            ]):
                job.status = request.status
                email_util.send_job_notification(job)
                for device in devices:
                    device.scheduling_status = (
                        Status.DEVICE_SCHEDULING_STATUS_DICT["free"])
                    device.status = Status.DEVICE_STATUS_DICT["unknown"]
                    devices_to_put.append(device)
            elif request.status == Status.JOB_STATUS_DICT["leased"]:
                job.status = request.status
                for device in devices:
                    device.timestamp = datetime.datetime.now()
                    devices_to_put.append(device)
            else:
                logging.error(
                    "[heartbeat] Unexpected job status is received. - {}".
                    format(request.serial))
            if devices_to_put:
                ndb.put_multi(devices_to_put)

            if request.infra_log_url:
                if request.infra_log_url.startswith(GCS_URL_PREFIX):
                    url = "{}{}".format(
                        STORAGE_API_URL,
                        request.infra_log_url[len(GCS_URL_PREFIX):])
                    job.infra_log_url = url
                elif re.match(HTTP_HTTPS_REGEX, request.infra_log_url):
                    job.infra_log_url = request.infra_log_url
                else:
                    logging.debug("[heartbeat] Wrong infra_log_url address.")

            job.heartbeat_stamp = datetime.datetime.now()
            job.put()
            model_util.UpdateParentSchedule(job, request.status)
            return model.JobLeaseResponse(
                return_code=model.ReturnCodeMessage.SUCCESS,
                jobs=[job_message])

        return model.JobLeaseResponse(
            return_code=model.ReturnCodeMessage.FAIL, jobs=[])

    @endpoints.method(
        endpoint_base.GET_REQUEST_RESOURCE,
        model.JobResponseMessage,
        path="get",
        http_method="POST",
        name="get")
    def get(self, request):
        """Gets the jobs from datastore."""
        return_list, more = self.Get(request=request,
                                     metaclass=model.JobModel,
                                     message=model.JobMessage)

        return model.JobResponseMessage(jobs=return_list, has_next=more)

    @endpoints.method(
        endpoint_base.COUNT_REQUEST_RESOURCE,
        model.CountResponseMessage,
        path="count",
        http_method="POST",
        name="count")
    def count(self, request):
        """Gets total number of JobModel entities stored in datastore."""
        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=model.JobModel)
        count = self.Count(metaclass=model.JobModel, filters=filters)

        return model.CountResponseMessage(count=count)
