#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
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
import webapp2

from webapp.src.proto import model

from google.appengine.api import taskqueue


class PeriodicScheduler(webapp2.RequestHandler):
    """Main class for /tasks/schedule servlet.

    This class creates a task, which creates schedules, in given period.
    """

    def get(self):
        """Enqueues a scheduling task if scheduler is enabled."""
        schedule_control = model.ScheduleControlModel.query()
        schedule_control_dataset = schedule_control.fetch()
        enabled = True
        if schedule_control_dataset:
            for schedule_control_data_tuple in schedule_control_dataset:
                if (not schedule_control_data_tuple.schedule_name or
                    schedule_control_data_tuple.schedule_name == "global"):
                    enabled = schedule_control_data_tuple.enabled

        if not enabled:
            self.response.write(
                "<pre>\nScheduler not enabled.\n</pre>")
            return

        task = taskqueue.add(
            url="/worker/schedule_handler",
            target="worker",
            queue_name="queue-schedule",
            transactional=False
        )
        self.response.write(
            "<pre>\nScheduling task is enqueued. ETA {}\n</pre>".format(
                task.eta))
