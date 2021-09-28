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

from webapp.src import vtslab_status as Status
from webapp.src.utils import email_util


def UpdateParentSchedule(job, status):
    """Updates a parent schedule of the given job with status.

    Args:
        job: a JobModel entity.
        status: an integer, job status value.
    """
    if status not in [
        Status.JOB_STATUS_DICT["complete"],
        Status.JOB_STATUS_DICT["infra-err"],
        Status.JOB_STATUS_DICT["expired"],
        Status.JOB_STATUS_DICT["bootup-err"]
    ]:
        return

    if job.parent_schedule:
        schedule = job.parent_schedule.get()
        if schedule:
            previous_suspended = schedule.suspended
            if schedule.error_count is None:
                schedule.error_count = 0
            if status == Status.JOB_STATUS_DICT["complete"]:
                schedule.error_count = 0
                schedule.suspended = False
            elif status in [
                Status.JOB_STATUS_DICT["infra-err"],
                Status.JOB_STATUS_DICT["expired"],
                Status.JOB_STATUS_DICT["bootup-err"]
            ]:
                schedule.error_count += 1
                if schedule.error_count >= Status.NUM_ERRORS_FOR_SUSPENSION:
                    schedule.suspended = True
            schedule.put()
            if previous_suspended != schedule.suspended:
                email_util.send_schedule_suspension_notification(schedule)
