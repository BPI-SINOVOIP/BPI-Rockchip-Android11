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
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src.proto import model
from webapp.src.tasks import removing_outdated_devices
from webapp.src.testing import unittest_base


class RemoveOutdatedDevicesTest(unittest_base.UnitTestBase):
    """Tests for RemoveOutdatedDevices cron class.

    Attributes:
        remove_outdated_device: A mock device_heartbeat.RemoveOutdatedDevices
                                instance.
    """

    def setUp(self):
        """Initializes test"""
        super(RemoveOutdatedDevicesTest, self).setUp()
        # Mocking RemoveOutdatedDevices and essential methods.
        self.remove_outdated_device = (
            removing_outdated_devices.RemoveOutdatedDevices(mock.Mock()))
        self.remove_outdated_device.response = mock.Mock()
        self.remove_outdated_device.response.write = mock.Mock()

    def testRemoveOutdatedDevicesTest(self):
        """Asserts job heartbeat detects unavailable jobs."""
        device_a_serial = "a"
        device_b_serial = "b"
        device_c_serial = "c"
        device_d_serial = "c"

        # create a device A, which is outdated.
        device = self.GenerateDeviceModel(serial=device_a_serial)
        device.timestamp = datetime.datetime.now() - datetime.timedelta(
            hours=100)
        device.put()

        # create a device B, which is offline for a day.
        device = self.GenerateDeviceModel(serial=device_b_serial)
        device.timestamp = datetime.datetime.now() - datetime.timedelta(
            hours=24)
        device.put()

        # create a device C and D, which are alive.
        for serial in [device_c_serial, device_d_serial]:
            device = self.GenerateDeviceModel(serial=serial)
            device.timestamp = datetime.datetime.now()
            device.put()

        # Remove outdated devices.
        self.remove_outdated_device.get()

        devices = model.DeviceModel.query().fetch()

        # device A should not be included.
        self.assertEqual(len(devices), 3)
        self.assertTrue(device_a_serial not in [x.serial for x in devices])

        # change devices' timestamp
        for device in devices:
            device.timestamp = device.timestamp - datetime.timedelta(hours=25)
            device.put()

        # Remove outdated devices.
        self.remove_outdated_device.get()

        devices = model.DeviceModel.query().fetch()

        # Now device B should not be included also.
        self.assertEqual(len(devices), 2)
        self.assertTrue(device_b_serial not in [x.serial for x in devices])

        # change devices' timestamp
        for device in devices:
            device.timestamp = device.timestamp - datetime.timedelta(hours=25)
            device.put()

        # Remove outdated devices.
        self.remove_outdated_device.get()

        devices = model.DeviceModel.query().fetch()

        # Now there should not be no devices.
        self.assertEqual(len(devices), 0)


if __name__ == "__main__":
    unittest.main()
