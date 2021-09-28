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

import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src.endpoint import lab_info
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class LabInfoTest(unittest_base.UnitTestBase):
    """A class to test lab_info endpoint API."""

    def setUp(self):
        """Initializes test"""
        super(LabInfoTest, self).setUp()

    def testUpdateErrorDevice(self):
        """Asserts that device update does not create a duplicate."""
        device_serial = self.GetRandomString()
        product = self.GetRandomString()
        device_equipment = [self.GetRandomString()]
        device_info = {
            "serial": device_serial,
            "product": product,
            "device_equipment": device_equipment
        }

        hostname = self.GetRandomString()
        host_info = {
            "hostname": hostname,
            "ip": self.GetRandomString(),
            "script": self.GetRandomString(),
            "device": [device_info],
            "vtslab_version": self.GetRandomString(),
            "host_equipment": [],
        }

        lab_name = self.GetRandomString()
        container = (
            lab_info.LAB_INFO_RESOURCE.combined_message_class(
                name=lab_name,
                owner=self.GetRandomString(),
                admin=[self.GetRandomString()],
                host=[host_info],
            ))

        api = lab_info.LabInfoApi()
        api.set(container)

        devices = model.DeviceModel.query().fetch()
        self.assertEqual(len(devices), 1)
        self.assertEqual(devices[0].product, product)

        # change device product name.
        devices[0].product = "error"
        devices[0].put()

        api.set(container)

        devices = model.DeviceModel.query().fetch()
        # there should not be duplicates.
        self.assertEqual(len(devices), 1)
        # stored device name should be kept.
        self.assertEqual(devices[0].product, "error")


    def testUpdateExistingDevice(self):
        """Asserts that device update does not create a duplicate."""
        device_serial = self.GetRandomString()
        product = self.GetRandomString()
        device_equipment = [self.GetRandomString()]
        device_info = {
            "serial": device_serial,
            "product": product,
            "device_equipment": device_equipment,
        }

        hostname = self.GetRandomString()
        host_info = {
            "hostname": hostname,
            "ip": self.GetRandomString(),
            "script": self.GetRandomString(),
            "device": [device_info],
            "vtslab_version": self.GetRandomString(),
            "host_equipment": [],
        }

        lab_name = self.GetRandomString()
        container = (
            lab_info.LAB_INFO_RESOURCE.combined_message_class(
                name=lab_name,
                owner=self.GetRandomString(),
                admin=[self.GetRandomString()],
                host=[host_info],
            ))

        device = self.GenerateDeviceModel(product="error",
                                          serial=device_serial,
                                          hostname=hostname)
        device.put()

        api = lab_info.LabInfoApi()
        api.set(container)

        devices = model.DeviceModel.query().fetch()
        self.assertEqual(len(devices), 1)

        # stored device name should be kept.
        self.assertEqual(devices[0].product, "error")

        # device equipment should be updated.
        self.assertEqual(set(devices[0].device_equipment),
                         set(device_equipment))


if __name__ == "__main__":
    unittest.main()
