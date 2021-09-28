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
import webapp2

from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.proto import model
from webapp.src.utils import email_util
from webapp.src.utils import logger

DEVICE_RESPONSE_TIMEOUT_SECONDS = 300


class PeriodicDeviceHeartBeat(webapp2.RequestHandler):
    """Main class for /tasks/device_heartbeat.

    Used to find lost devices and change their status properly.

    Attributes:
        logger: Logger class
    """
    logger = logger.Logger()

    def get(self):
        """Generates an HTML page based on the task schedules kept in DB."""
        self.logger.Clear()

        device_query = model.DeviceModel.query(
            model.DeviceModel.status !=
            Status.DEVICE_STATUS_DICT["no-response"])
        devices = device_query.fetch()
        lost_devices = [
            x for x in devices
            if (datetime.datetime.now() - x.timestamp
                ).seconds >= DEVICE_RESPONSE_TIMEOUT_SECONDS
        ]
        devices_to_put = []
        labs_to_alert = {}
        for device in lost_devices:
            self.logger.Println("Device[{}] is not responding.".format(
                device.serial))
            device.status = Status.DEVICE_STATUS_DICT["no-response"]
            devices_to_put.append(device)

            # sending notification
            lab_query = model.LabModel.query(
                model.LabModel.hostname == device.hostname)
            labs = lab_query.fetch()
            if labs:
                lab = labs[0]
                if lab.name not in labs_to_alert:
                    labs_to_alert[lab.name] = {}
                    labs_to_alert[lab.name]["_recipients"] = []
                if device.hostname not in labs_to_alert[lab.name]:
                    labs_to_alert[lab.name][device.hostname] = []
                if lab.owner not in labs_to_alert[lab.name]["_recipients"]:
                    labs_to_alert[lab.name]["_recipients"].append(lab.owner)
                labs_to_alert[lab.name]["_recipients"].extend([
                    x for x in lab.admin
                    if x not in labs_to_alert[lab.name]["_recipients"]
                ])
                labs_to_alert[lab.name][device.hostname].append(device.serial)
            else:
                logging.warning(
                    "Could not find a lab model for hostname {}".format(
                        device.hostname))
                continue

        if devices_to_put:
            ndb.put_multi(devices_to_put)
        if labs_to_alert:
            email_util.send_device_notification(labs_to_alert)

        self.response.write(
            "<pre>\n" + "\n".join(self.logger.Get()) + "\n</pre>")
