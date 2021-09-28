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
from webapp.src.endpoint import schedule_info
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class ScheduleInfoTest(unittest_base.UnitTestBase):
    """A class to test schedule_info endpoint API."""

    def setUp(self):
        """Initializes test"""
        super(ScheduleInfoTest, self).setUp()

    def testSetWithSimpleMessage(self):
        """Asserts schedule_info/set API receives a simple message."""
        # As of June 8, 2018, these are uploaded from host controller.
        container = (
            schedule_info.SCHEDULE_INFO_RESOURCE.combined_message_class(
                manifest_branch=self.GetRandomString(),
                build_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                build_target=self.GetRandomString(),
                require_signed_device_build=False,
                has_bootloader_img=True,
                has_radio_img=True,
                test_name=self.GetRandomString(),
                period=360,
                priority="high",
                device=[self.GetRandomString()],
                required_host_equipment=[self.GetRandomString()],
                required_device_equipment=[self.GetRandomString()],
                device_pab_account_id=self.GetRandomString(),
                shards=1,
                param=[self.GetRandomString()],
                retry_count=1,
                gsi_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                gsi_branch=self.GetRandomString(),
                gsi_build_target=self.GetRandomString(),
                gsi_pab_account_id=self.GetRandomString(),
                gsi_vendor_version=self.GetRandomString(),
                test_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                test_branch=self.GetRandomString(),
                test_build_target=self.GetRandomString(),
                test_pab_account_id=self.GetRandomString(),
                image_package_repo_base=self.GetRandomString(),
                report_bucket=[self.GetRandomString()],
                report_spreadsheet_id=[self.GetRandomString()],
                report_persistent_url=[self.GetRandomString()],
                report_reference_url=[self.GetRandomString()],
            ))
        api = schedule_info.ScheduleInfoApi()
        response = api.set(container)

        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)

    def testSetWithEmptyRepeatedField(self):
        """Asserts schedule_info/set API receives a message.

        This test sets required_host_equipment to empty and sends to endpoint
        method.
        """
        # As of June 8, 2018, these are uploaded from host controller.
        container = (
            schedule_info.SCHEDULE_INFO_RESOURCE.combined_message_class(
                manifest_branch=self.GetRandomString(),
                build_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                build_target=self.GetRandomString(),
                require_signed_device_build=False,
                has_bootloader_img=True,
                has_radio_img=True,
                test_name=self.GetRandomString(),
                period=360,
                priority="high",
                device=[self.GetRandomString()],
                required_host_equipment=[self.GetRandomString()],
                required_device_equipment=[self.GetRandomString()],
                device_pab_account_id=self.GetRandomString(),
                shards=1,
                param=[self.GetRandomString()],
                retry_count=1,
                gsi_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                gsi_branch=self.GetRandomString(),
                gsi_build_target=self.GetRandomString(),
                gsi_pab_account_id=self.GetRandomString(),
                gsi_vendor_version=self.GetRandomString(),
                test_storage_type=Status.STORAGE_TYPE_DICT["PAB"],
                test_branch=self.GetRandomString(),
                test_build_target=self.GetRandomString(),
                test_pab_account_id=self.GetRandomString(),
                image_package_repo_base=self.GetRandomString(),
                report_bucket=[],
                report_spreadsheet_id=[],
                report_persistent_url=[],
                report_reference_url=[],
            ))
        api = schedule_info.ScheduleInfoApi()
        response = api.set(container)

        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)


if __name__ == "__main__":
    unittest.main()
