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

import os

import webapp2

from webapp.src.handlers import base
from webapp.src.scheduler import device_heartbeat
from webapp.src.scheduler import job_heartbeat
from webapp.src.scheduler import periodic
from webapp.src.tasks import indexing
from webapp.src.tasks import removing_outdated_devices


class RedirectHandler(base.BaseHandler):
    """Redirect handler to redirect to specific appspot version."""
    def get(self, arg):
        if arg:
            return self.redirect("https://{}.appspot.com/".format(arg))


class MainPage(base.BaseHandler):
    """Main web page request handler."""

    def get(self):
        """Generates an HTML page."""
        self.template = "index.html"

        template_values = {}

        self.render(template_values)


config = {}
config['webapp2_extras.sessions'] = {
    'secret_key': os.environ.get('SESSION_SECRET_KEY'),
}

app = webapp2.WSGIApplication(
    [
        ("/tasks/schedule", periodic.PeriodicScheduler),
        ("/tasks/device_heartbeat", device_heartbeat.PeriodicDeviceHeartBeat),
        ("/tasks/job_heartbeat", job_heartbeat.PeriodicJobHeartBeat),
        ("/tasks/remove_outdated_devices",
         removing_outdated_devices.RemoveOutdatedDevices),
        ("/tasks/indexing([/]?.*)", indexing.CreateIndex),
        ("/redirect/(.*)", RedirectHandler),
    ],
    config=config,
    debug=False)
