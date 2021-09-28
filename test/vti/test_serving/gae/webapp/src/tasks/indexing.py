#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License") + "\n</pre>");
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

import logging

from webapp.src import vtslab_status as Status
from webapp.src.proto import model
from webapp.src.scheduler import schedule_worker

import webapp2
from google.appengine.api import taskqueue
from google.appengine.ext import ndb

PAGING_SIZE = 1000
DICT_MODELS = {
    "build": model.BuildModel,
    "device": model.DeviceModel,
    "lab": model.LabModel,
    "job": model.JobModel,
    "schedule": model.ScheduleModel
}


class CreateIndex(webapp2.RequestHandler):
    """Cron class for /tasks/indexing/{model}."""

    def get(self, arg):
        """Creates a task to re-index, with given URL format."""
        index_list = []
        if arg:
            if arg.startswith("/") and arg[1:].lower() in DICT_MODELS.keys():
                index_list.append(arg[1:].lower())
            else:
                self.response.write("<pre>Access Denied. Please visit "
                                    "/tasks/indexing/{model}</pre>")
                return
        else:
            # accessed by /tasks/indexing
            index_list.extend(DICT_MODELS.keys())
        self.response.write(
            "<pre>Re-indexing task{} for {} {} going to be created.</pre>".
            format("s"
                   if len(index_list) > 1 else "", ", ".join(index_list), "are"
                   if len(index_list) > 1 else "is"))

        for model_type in index_list:
            task = taskqueue.add(
                url="/worker/indexing",
                target="worker",
                queue_name="queue-indexing",
                transactional=False,
                params={
                    "model_type": model_type
                })
            self.response.write(
                "<pre>Re-indexing task for {} is created. ETA: {}</pre>".
                format(model_type, task.eta))


class IndexingHandler(webapp2.RequestHandler):
    """Task queue handler class to re-index ndb model."""

    def post(self):
        """Fetch entities and process model specific jobs."""
        reload(model)
        model_type = self.request.get("model_type")

        num_updated = 0
        next_cursor = None
        more = True

        while more:
            query = DICT_MODELS[model_type].query()
            entities, next_cursor, more = query.fetch_page(
                PAGING_SIZE, start_cursor=next_cursor)

            to_put = []
            for entity in entities:
                if model_type == "build":
                    pass
                elif model_type == "device":
                    pass
                elif model_type == "lab":
                    pass
                elif model_type == "job":
                    # uses bits 0-1 to indicate version.
                    test_type = schedule_worker.GetTestVersionType(
                        entity.manifest_branch, entity.gsi_branch)
                    # uses bit 2
                    if entity.require_signed_device_build:
                        test_type |= (
                            Status.TEST_TYPE_DICT[Status.TEST_TYPE_SIGNED])
                    entity.test_type = test_type

                    if not entity.parent_schedule:
                        # finds and links to a parent schedule.
                        parent_schedule_query = model.ScheduleModel.query(
                            model.ScheduleModel.priority == entity.priority,
                            model.ScheduleModel.test_name == entity.test_name,
                            model.ScheduleModel.period == entity.period,
                            model.ScheduleModel.build_storage_type == (
                                entity.build_storage_type),
                            model.ScheduleModel.manifest_branch == (
                                entity.manifest_branch),
                            model.ScheduleModel.build_target == (
                                entity.build_target),
                            model.ScheduleModel.device_pab_account_id == (
                                entity.pab_account_id),
                            model.ScheduleModel.shards == entity.shards,
                            model.ScheduleModel.retry_count == (
                                entity.retry_count),
                            model.ScheduleModel.gsi_storage_type == (
                                entity.gsi_storage_type),
                            model.ScheduleModel.gsi_branch == (
                                entity.gsi_branch),
                            model.ScheduleModel.gsi_build_target == (
                                entity.gsi_build_target),
                            model.ScheduleModel.gsi_pab_account_id == (
                                entity.gsi_pab_account_id),
                            model.ScheduleModel.gsi_vendor_version == (
                                entity.gsi_vendor_version),
                            model.ScheduleModel.test_storage_type == (
                                entity.test_storage_type),
                            model.ScheduleModel.test_branch == (
                                entity.test_branch),
                            model.ScheduleModel.test_build_target == (
                                entity.test_build_target),
                            model.ScheduleModel.test_pab_account_id == (
                                entity.test_pab_account_id))
                        parent_schedules = parent_schedule_query.fetch()
                        if not parent_schedules:
                            logging.error("Parent not found.")
                        else:
                            parent_schedule = parent_schedules[0]
                            parent_schedule.children_jobs.append(entity.key)
                            entity.parent_schedule = parent_schedule.key
                            to_put.append(parent_schedule)

                elif model_type == "schedule":
                    if entity.error_count is None:
                        entity.error_count = 0
                    if entity.suspended is None:
                        entity.suspended = False
                    if entity.build_storage_type is None:
                        entity.build_storage_type = Status.STORAGE_TYPE_DICT[
                            "PAB"]
                    # remove None children jobs.
                    if entity.children_jobs:
                        entity.children_jobs = [
                            x for x in entity.children_jobs if x]
                    else:
                        entity.children_jobs = []

                    for attr in ["has_bootloader_img", "has_radio_img"]:
                        if getattr(entity, attr, None) is None:
                            setattr(entity, attr, True)

                    # set priority_value for old schedules.
                    if entity.priority_value is None:
                        entity.priority_value = Status.GetPriorityValue(
                            entity.priority)
                else:
                    pass
                to_put.append(entity)

            if to_put:
                ndb.put_multi(to_put)
                num_updated += len(to_put)

        logging.info("{} indexing complete with {} updates!".format(
            model_type, num_updated))
