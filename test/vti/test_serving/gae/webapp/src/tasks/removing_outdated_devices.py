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
import webapp2

from google.appengine.ext import ndb

from webapp.src.proto import model

OUTDATED_DEVICE_REMOVE_TIME_IN_HOURS = 48


class RemoveOutdatedDevices(webapp2.RequestHandler):
    """Main class for /tasks/remove_outdated_devices.

    Used to find outdated devices and remove them.
    """

    def get(self):
        device_query = model.DeviceModel.query(
            model.DeviceModel.timestamp < datetime.datetime.now() -
            datetime.timedelta(hours=OUTDATED_DEVICE_REMOVE_TIME_IN_HOURS))
        outdated_devices = device_query.fetch(keys_only=True)
        ndb.delete_multi(outdated_devices)
