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

import endpoints
import json
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src import vtslab_status as Status
from webapp.src.endpoint import endpoint_base
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class EndpointBaseTest(unittest_base.UnitTestBase):
    """A class to test endpoint_base.EndpointBase class.

    Attributes:
        eb: An EndpointBase class instance.
    """

    def setUp(self):
        """Initializes test"""
        super(EndpointBaseTest, self).setUp()
        self.eb = endpoint_base.EndpointBase()

    def testGetAssignedMessagesAttributes(self):
        attrs = ["hostname", "priority", "test_branch"]
        job_message = model.JobMessage()
        for attr in attrs:
            setattr(job_message, attr, attr)
        result = self.eb.GetAttributes(job_message, assigned_only=True)
        self.assertEqual(set(attrs), set(result))

    def testGetAssignedModelAttributes(self):
        attrs = ["hostname", "priority", "test_branch"]
        job = model.JobModel()
        for attr in attrs:
            setattr(job, attr, attr)
        result = self.eb.GetAttributes(job, assigned_only=True)
        self.assertEqual(set(attrs), set(result))

    def testGetAllMessagesAttributes(self):
        attrs = ["hostname", "priority", "test_branch"]
        full_attrs = [
            "test_type", "hostname", "priority", "test_name",
            "require_signed_device_build", "has_bootloader_img",
            "has_radio_img", "device", "serial", "build_storage_type",
            "manifest_branch", "build_target", "build_id", "pab_account_id",
            "shards", "param", "status", "period", "gsi_storage_type",
            "gsi_branch", "gsi_build_target", "gsi_build_id",
            "gsi_pab_account_id", "gsi_vendor_version", "test_storage_type",
            "test_branch", "test_build_target", "test_build_id",
            "test_pab_account_id", "retry_count", "infra_log_url",
            "image_package_repo_base", "report_bucket",
            "report_spreadsheet_id", "report_persistent_url",
            "report_reference_url"
        ]
        job_message = model.JobMessage()
        for attr in attrs:
            setattr(job_message, attr, attr)
        result = self.eb.GetAttributes(job_message, assigned_only=False)
        self.assertTrue(set(full_attrs) <= set(result))

    def testGetAllModelAttributes(self):
        attrs = ["hostname", "priority", "test_branch"]
        full_attrs = [
            "test_type", "hostname", "priority", "test_name",
            "require_signed_device_build", "has_bootloader_img",
            "has_radio_img", "device", "serial", "build_storage_type",
            "manifest_branch", "build_target", "build_id", "pab_account_id",
            "shards", "param", "status", "period", "gsi_storage_type",
            "gsi_branch", "gsi_build_target", "gsi_build_id",
            "gsi_pab_account_id", "gsi_vendor_version", "test_storage_type",
            "test_branch", "test_build_target", "test_build_id",
            "test_pab_account_id", "timestamp", "heartbeat_stamp",
            "retry_count", "infra_log_url", "parent_schedule",
            "image_package_repo_base", "report_bucket",
            "report_spreadsheet_id", "report_persistent_url",
            "report_reference_url"
        ]
        job = model.JobModel()
        for attr in attrs:
            setattr(job, attr, attr)
        result = self.eb.GetAttributes(job, assigned_only=False)
        self.assertTrue(set(full_attrs) <= set(result))

    def testGetSingleEntity(self):
        """Asserts to get a single entity."""
        device = self.GenerateDeviceModel()
        device.put()

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=0,
                offset=0,
                filter="",
                sort="",
                direction="",
            ))
        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 1)
        self.assertFalse(more)

    def testGetHundredEntities(self):
        """Asserts to get hundred entities."""
        for _ in xrange(100):
            device = self.GenerateDeviceModel()
            device.put()

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=0,
                offset=0,
                filter="",
                sort="",
                direction="",
            ))
        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 100)
        self.assertFalse(more)

    def testGetEntitiesWithPagination(self):
        """Asserts to get entities with pagination."""
        for _ in xrange(100):
            device = self.GenerateDeviceModel()
            device.put()

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=60,
                offset=0,
                filter="",
                sort="",
                direction="",
            ))
        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 60)
        self.assertTrue(more)

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=100,
                offset=60,
                filter="",
                sort="",
                direction="",
            ))
        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 40)
        self.assertFalse(more)

    def testGetWithFilter(self):
        """Asserts to get entities with filter."""
        for _ in xrange(50):
            device = self.GenerateDeviceModel()
            device.put()

        for _ in xrange(50):
            device = self.GenerateDeviceModel(product="product")
            device.put()

        filter = [{
            "key": "product",
            "method": Status.FILTER_METHOD[Status.FILTER_EqualTo],
            "value": "product"
        }]
        filter_string = json.dumps(filter)
        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=0,
                offset=0,
                filter=filter_string,
                sort="",
                direction="",
            ))
        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 50)
        self.assertFalse(more)

    def testGetWithSort(self):
        """Asserts to get entities with sort."""
        for _ in xrange(100):
            device = self.GenerateDeviceModel()
            device.put()

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=0,
                offset=0,
                filter="",
                sort="serial",
                direction="asc",
            ))

        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 100)
        for i in xrange(len(result) - 1):
            self.assertTrue(result[i]["serial"] < result[i + 1]["serial"])

        request_body = (endpoints.ResourceContainer(
            model.GetRequestMessage).combined_message_class(
                size=0,
                offset=0,
                filter="",
                sort="serial",
                direction="desc",
            ))

        result, more = self.eb.Get(
            request=request_body,
            metaclass=model.DeviceModel,
            message=model.DeviceInfoMessage)
        self.assertEqual(len(result), 100)
        for i in xrange(len(result) - 1):
            self.assertTrue(result[i]["serial"] > result[i + 1]["serial"])


if __name__ == "__main__":
    unittest.main()
