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
"""Schedule Info APIs implemented using Google Cloud Endpoints."""

import datetime
import endpoints

from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import endpoint_base
from webapp.src.proto import model
from webapp.src.utils import email_util

SCHEDULE_INFO_RESOURCE = endpoints.ResourceContainer(model.ScheduleInfoMessage)
SCHEDULE_SUSPEND_RESOURCE = endpoints.ResourceContainer(
    model.ScheduleSuspendMessage)


@endpoints.api(name="schedule", version="v1")
class ScheduleInfoApi(endpoint_base.EndpointBase):
    """Endpoint API for schedule_info."""

    @endpoints.method(
        SCHEDULE_INFO_RESOURCE,
        model.DefaultResponse,
        path="clear",
        http_method="POST",
        name="clear")
    def clear(self, request):
        """Clears test schedule info in DB."""
        schedule_query = model.ScheduleModel.query(
            model.ScheduleModel.schedule_type != "green")
        existing_schedules = schedule_query.fetch(keys_only=True)
        if existing_schedules and len(existing_schedules) > 0:
            ndb.delete_multi(existing_schedules)
        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        SCHEDULE_INFO_RESOURCE,
        model.DefaultResponse,
        path="set",
        http_method="POST",
        name="set")
    def set(self, request):
        """Sets the schedule info based on `request`."""
        exist_on_both = self.GetCommonAttributes(request, model.ScheduleModel)
        # check duplicates
        exclusions = [
            "name", "schedule_type", "schedule", "param", "timestamp",
            "children_jobs", "error_count", "suspended"
        ]
        # list of protorpc message fields.
        duplicate_checklist = [x for x in exist_on_both if x not in exclusions]
        empty_list_field = []
        query = model.ScheduleModel.query()
        for attr_name in duplicate_checklist:
            if model.ScheduleModel._properties[attr_name]._repeated:
                value = request.get_assigned_value(attr_name)
                if value:
                    query = query.filter(
                        getattr(model.ScheduleModel, attr_name).IN(
                            request.get_assigned_value(attr_name)))
                else:
                    # empty list cannot be queried.
                    empty_list_field.append(attr_name)
            else:
                query = query.filter(
                    getattr(model.ScheduleModel, attr_name) ==
                    request.get_assigned_value(attr_name))
        duplicated_schedules = query.fetch()

        if empty_list_field:
            duplicated_schedules = [
                schedule for schedule in duplicated_schedules
                if all(
                    [not getattr(schedule, attr) for attr in empty_list_field])
            ]

        if duplicated_schedules:
            schedule = duplicated_schedules[0]
        else:
            schedule = model.ScheduleModel()
            for attr_name in exist_on_both:
                setattr(schedule, attr_name,
                        request.get_assigned_value(attr_name))
            schedule.schedule_type = "test"
            schedule.error_count = 0
            schedule.suspended = False
            schedule.priority_value = Status.GetPriorityValue(schedule.priority)

        schedule.timestamp = datetime.datetime.now()
        schedule.put()

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        endpoint_base.GET_REQUEST_RESOURCE,
        model.ScheduleResponseMessage,
        path="get",
        http_method="POST",
        name="get")
    def get(self, request):
        """Gets the schedules from datastore."""
        return_list, more = self.Get(request=request,
                                     metaclass=model.ScheduleModel,
                                     message=model.ScheduleInfoMessage)

        return model.ScheduleResponseMessage(
            schedules=return_list, has_next=more)

    @endpoints.method(
        endpoint_base.COUNT_REQUEST_RESOURCE,
        model.CountResponseMessage,
        path="count",
        http_method="POST",
        name="count")
    def count(self, request):
        """Gets total number of ScheduleModel entities stored in datastore."""
        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=model.ScheduleModel)

        count = self.Count(metaclass=model.ScheduleModel, filters=filters)

        return model.CountResponseMessage(count=count)

    @endpoints.method(
        SCHEDULE_SUSPEND_RESOURCE,
        model.ScheduleSuspendMessage,
        path="suspend",
        http_method="POST",
        name="suspend")
    def suspend(self, request):
        """Toggles a schedule from suspend to resume, or vice versa."""
        schedules_to_put = []
        schedules_to_return = []
        for schedule in request.schedules:
            schedule_key = ndb.key.Key(urlsafe=schedule.urlsafe_key)
            schedule_entity = schedule_key.get()
            if schedule.suspend:  # to suspend
                schedule_entity.suspended = True
            else:  # to resume
                schedule_entity.error_count = 0
                schedule_entity.suspended = False
            schedules_to_put.append(schedule_entity)
            schedules_to_return.append({"urlsafe_key": schedule.urlsafe_key,
                                        "suspend": schedule_entity.suspended})
            # TODO(jongmok): Minimize a number of emails by merging schedules.
            email_util.send_schedule_suspension_notification(schedule_entity)

        ndb.put_multi(schedules_to_put)
        return model.ScheduleSuspendMessage(schedules=schedules_to_return)


@endpoints.api(name="green_schedule_info", version="v1")
class GreenScheduleInfoApi(endpoint_base.EndpointBase):
    """Endpoint API for green_schedule_info."""

    @endpoints.method(
        SCHEDULE_INFO_RESOURCE,
        model.DefaultResponse,
        path="clear",
        http_method="POST",
        name="clear")
    def clear(self, request):
        """Clears green build schedule info in DB."""
        schedule_query = model.ScheduleModel.query(
            model.ScheduleModel.schedule_type == "green")
        existing_schedules = schedule_query.fetch(keys_only=True)
        if existing_schedules and len(existing_schedules) > 0:
            ndb.delete_multi(existing_schedules)
        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)

    @endpoints.method(
        SCHEDULE_INFO_RESOURCE,
        model.DefaultResponse,
        path="set",
        http_method="POST",
        name="set")
    def set(self, request):
        """Sets the green build schedule info based on `request`."""
        schedule = model.ScheduleModel()
        schedule.name = request.name
        schedule.manifest_branch = request.manifest_branch
        schedule.build_target = request.build_target
        schedule.device_pab_account_id = request.device_pab_account_id
        schedule.test_name = request.test_name
        schedule.schedule = request.schedule
        schedule.priority = request.priority
        schedule.device = request.device
        schedule.shards = request.shards
        schedule.gsi_branch = request.gsi_branch
        schedule.gsi_build_target = request.gsi_build_target
        schedule.gsi_pab_account_id = request.gsi_pab_account_id
        schedule.gsi_vendor_version = request.gsi_vendor_version
        schedule.test_branch = request.test_branch
        schedule.test_build_target = request.test_build_target
        schedule.test_pab_account_id = request.test_pab_account_id
        schedule.timestamp = datetime.datetime.now()
        schedule.schedule_type = "green"
        schedule.put()

        return model.DefaultResponse(
            return_code=model.ReturnCodeMessage.SUCCESS)
