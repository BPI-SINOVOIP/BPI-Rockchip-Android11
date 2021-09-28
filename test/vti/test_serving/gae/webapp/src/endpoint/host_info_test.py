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

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import host_info
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class HostInfoTest(unittest_base.UnitTestBase):
    """A class to test host_info endpoint API."""

    def setUp(self):
        """Initializes test"""
        super(HostInfoTest, self).setUp()


    def testUpdateExistingDevice(self):
        """Asserts that device update does not create a duplicate."""
        hostname = self.GetRandomString()
        serial = self.GetRandomString()
        product = self.GetRandomString()
        error_device = {
            "serial": serial,
            "product": "error",
        }
        container = (
            host_info.HOST_INFO_RESOURCE.combined_message_class(
                hostname=hostname,
                devices=[error_device],
            ))

        api = host_info.HostInfoApi()
        api.set(container)

        devices = model.DeviceModel.query().fetch()
        self.assertEqual(len(devices), 1)

        # name "error" is allowed as initial name.
        self.assertEqual(devices[0].product, "error")

        correct_device = {
            "serial": serial,
            "product": product,
        }
        container = (
            host_info.HOST_INFO_RESOURCE.combined_message_class(
                hostname=hostname,
                devices=[correct_device],
            ))
        api.set(container)

        devices = model.DeviceModel.query().fetch()
        self.assertEqual(len(devices), 1)
        # correct product name (which is not "error") should be overwritten.
        self.assertEqual(devices[0].product, product)

        container = (
            host_info.HOST_INFO_RESOURCE.combined_message_class(
                hostname=hostname,
                devices=[error_device],
            ))
        api.set(container)

        devices = model.DeviceModel.query().fetch()
        self.assertEqual(len(devices), 1)
        # "error" should be ignored.
        self.assertEqual(devices[0].product, product)


if __name__ == "__main__":
    unittest.main()
