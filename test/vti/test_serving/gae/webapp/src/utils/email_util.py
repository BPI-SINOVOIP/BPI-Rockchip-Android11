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
import logging
import re

from google.appengine.api import app_identity
from google.appengine.api import mail

from webapp.src import vtslab_status as Status
from webapp.src.proto import model
from webapp.src.utils import datetime_util

SENDER_ADDRESS = "noreply@{}.appspotmail.com"

SEND_NOTIFICATION_FOOTER = (
    "You are receiving this email because you are "
    "listed as an owner, or an administrator of the "
    "lab {}.\nIf you received this email by mistake, "
    "please send an email to VTS Lab infra development "
    "team. Thank you.")

SEND_DEVICE_NOTIFICATION_TITLE = ("[VTS lab] Devices not responding in lab {} "
                                  "({})")
SEND_DEVICE_NOTIFICATION_HEADER = "Devices in lab {} are not responding."

SEND_JOB_NOTIFICATION_TITLE = ("[VTS lab] Job error has been occurred in "
                               "lab {} ({})")
SEND_JOB_NOTIFICATION_HEADER = ("Jobs in lab {} have been completed "
                                "unexpectedly.")
SEND_SCHEDULE_SUSPENSION_NOTIFICATION_TITLE = (
    "[VTS lab] A job schedule has been {}. ({})")
SEND_SCHEDULE_SUSPENSION_NOTIFICATION_HEADER = ("The below job schedule has "
                                                "been {}.")
SEND_SCHEDULE_SUSPENSION_NOTIFICATION_FOOTER = (
    "You are receiving this email because one or more labs which you are "
    "listed as an owner or an administrator are affected.\nIf you received "
    "this email by mistake, please send an email to VTS Lab infra development "
    "team. Thank you.")


def send_device_notification(devices):
    """Sends notification for not responding devices.

    Args:
        devices: a dict containing lab and host information of no-response
                 devices.
    """
    for lab in devices:
        email_message = mail.EmailMessage()
        email_message.sender = SENDER_ADDRESS.format(
            app_identity.get_application_id())
        try:
            email_message.to = verify_recipient_address(
                devices[lab]["_recipients"])
        except ValueError as e:
            logging.error(e)
            continue
        email_message.subject = SEND_DEVICE_NOTIFICATION_TITLE.format(
            lab,
            datetime_util.GetTimeWithTimezone(
                datetime.datetime.now()).strftime("%Y-%m-%d"))
        message = ""
        message += SEND_DEVICE_NOTIFICATION_HEADER.format(lab)
        message += "\n\n"
        for host in devices[lab]:
            if host == "_recipients" or not devices[lab][host]:
                continue
            message += "hostname\n"
            message += host
            message += "\n\ndevices\n"
            message += "\n".join(devices[lab][host])
            message += "\n\n\n"
        message += "\n\n"
        message += SEND_NOTIFICATION_FOOTER.format(lab)

        try:
            email_message.body = message
            email_message.check_initialized()
            email_message.send()
        except mail.MissingRecipientError as e:
            logging.exception(e)


def send_job_notification(jobs):
    """Sends notification for job error.

    Args:
        jobs: a JobModel entity, or a list of JobModel entities.
    """
    if not jobs:
        return
    if type(jobs) is not list:
        jobs = [jobs]

    # grouping jobs by lab to send to each lab owner and admins at once.
    labs_to_alert = {}
    for job in jobs:
        lab_query = model.LabModel.query(
            model.LabModel.hostname == job.hostname)
        labs = lab_query.fetch()
        if labs:
            lab = labs[0]
            if lab.name not in labs_to_alert:
                labs_to_alert[lab.name] = {}
                labs_to_alert[lab.name]["jobs"] = []
                labs_to_alert[lab.name]["_recipients"] = []
            if lab.owner not in labs_to_alert[lab.name]["_recipients"]:
                labs_to_alert[lab.name]["_recipients"].append(lab.owner)
            labs_to_alert[lab.name]["_recipients"].extend([
                x for x in lab.admin
                if x not in labs_to_alert[lab.name]["_recipients"]
            ])
            labs_to_alert[lab.name]["jobs"].append(job)
        else:
            logging.warning(
                "Could not find a lab model for hostname {}".format(
                    job.hostname))
            continue

    for lab in labs_to_alert:
        email_message = mail.EmailMessage()
        email_message.sender = SENDER_ADDRESS.format(
            app_identity.get_application_id())
        try:
            email_message.to = verify_recipient_address(
                labs_to_alert[lab]["_recipients"])
        except ValueError as e:
            logging.error(e)
            continue
        email_message.subject = SEND_JOB_NOTIFICATION_TITLE.format(
            lab,
            datetime_util.GetTimeWithTimezone(
                datetime.datetime.now()).strftime("%Y-%m-%d"))
        message = ""
        message += SEND_JOB_NOTIFICATION_HEADER.format(lab)
        message += "\n\n"
        message += "http://{}.appspot.com/job".format(
            app_identity.get_application_id())
        message += "\n\n"
        for job in labs_to_alert[lab]["jobs"]:
            message += "hostname: {}\n\n".format(job.hostname)
            message += "device: {}\n".format(job.device.split("/")[1])
            message += "device serial: {}\n".format(", ".join(job.serial))
            message += (
                "device: branch - {}, target - {}, build_id - {}\n").format(
                    job.manifest_branch, job.build_target, job.build_id)
            message += "gsi: branch - {}, target - {}, build_id - {}\n".format(
                job.gsi_branch, job.gsi_build_target, job.gsi_build_id)
            message += "test: branch - {}, target - {}, build_id - {}\n".format(
                job.test_branch, job.test_build_target, job.test_build_id)
            message += "job created: {}\n".format(
                datetime_util.GetTimeWithTimezone(
                    job.timestamp).strftime("%Y-%m-%d %H:%M:%S %Z"))
            message += "job status: {}\n".format([
                key for key, value in Status.JOB_STATUS_DICT.items()
                if value == job.status
            ][0])
            message += "\n\n\n"
        message += "\n\n"
        message += SEND_NOTIFICATION_FOOTER.format(lab)

        try:
            email_message.body = message
            email_message.check_initialized()
            email_message.send()
        except mail.MissingRecipientError as e:
            logging.exception(e)


def send_schedule_suspension_notification(schedule):
    """Sends notification when a schedule is suspended, or resumed.

    Args:
        schedule: a ScheduleModel entity.
    """
    if not schedule:
        return

    if not schedule.device:
        return

    email_message = mail.EmailMessage()
    email_message.sender = SENDER_ADDRESS.format(
        app_identity.get_application_id())

    lab_names = []
    for device in schedule.device:
        if not "/" in device:
            continue
        lab_name = device.split("/")[0]
        lab_names.append(lab_name)

    recipients = []
    for lab_name in lab_names:
        lab_query = model.LabModel.query(model.LabModel.name == lab_name)
        labs = lab_query.fetch()
        if labs:
            lab = labs[0]
            if lab.owner not in recipients:
                recipients.append(lab.owner)
            recipients.extend([x for x in lab.admin if x not in recipients])
        else:
            logging.warning(
                "Could not find a lab model for lab {}".format(lab_name))

    try:
        email_message.to = verify_recipient_address(recipients)
    except ValueError as e:
        logging.error(e)
        return

    status_text = "suspended" if schedule.suspended else "resumed"
    email_message.subject = SEND_SCHEDULE_SUSPENSION_NOTIFICATION_TITLE.format(
        status_text,
        datetime_util.GetTimeWithTimezone(
            datetime.datetime.now()).strftime("%Y-%m-%d"))
    message = ""
    message += SEND_SCHEDULE_SUSPENSION_NOTIFICATION_HEADER.format(status_text)
    message += "\n\n"
    message += "\n\ndevices\n"
    message += "\n".join(schedule.device)
    message += "\n\ndevice branch\n"
    message += schedule.manifest_branch
    message += "\n\ndevice build target\n"
    message += schedule.build_target
    message += "\n\ngsi branch\n"
    message += schedule.gsi_branch
    message += "\n\ngsi build target\n"
    message += schedule.gsi_build_target
    message += "\n\ntest branch\n"
    message += schedule.test_branch
    message += "\n\ntest build target\n"
    message += schedule.test_build_target
    message += "\n\n"
    message += ("Please see the details in the following link: "
                "http://{}.appspot.com/schedule".format(
                    app_identity.get_application_id()))
    message += "\n\n\n\n"
    message += SEND_SCHEDULE_SUSPENSION_NOTIFICATION_FOOTER

    try:
        email_message.body = message
        email_message.check_initialized()
        email_message.send()
    except mail.MissingRecipientError as e:
        logging.exception(e)


def verify_recipient_address(address):
    """Verifies recipients address.

    Args:
        address: a list of strings or a string, recipient(s) address.

    Returns:
        A list of verified addresses if list type argument is given, or
        a string of a verified address if str type argument is given.

    Raises:
        ValueError if type of address is neither list nor str.
    """
    # pattern for 'any@google.com', and 'any name <any@google.com>'
    verify_patterns = [
        re.compile(".*@google\.com$"),
        re.compile(".*<.*@google\.com>$")
    ]
    if not address:
        return None
    if type(address) is list:
        verified_address = [
            x for x in address
            if any(pattern.match(x) for pattern in verify_patterns)
        ]
        return verified_address
    elif type(address) is str:
        return address if any(
            pattern.match(address) for pattern in verify_patterns) else None
    else:
        raise ValueError("Wrong type - {}.".format(type(address)))
