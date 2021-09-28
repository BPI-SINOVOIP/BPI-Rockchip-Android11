#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for android_compute_client."""
import unittest
import mock

from acloud import errors
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gcompute_client


class AndroidComputeClientTest(driver_test_lib.BaseDriverTest):
    """Test AndroidComputeClient."""

    PROJECT = "fake-project"
    SERVICE_ACCOUNT_NAME = "fake@fake.com"
    PRIVATE_KEY_PATH = "/fake/key/path"
    SSH_PUBLIC_KEY_PATH = ""
    IMAGE = "fake-image"
    GS_IMAGE_SOURCE_URI = "https://storage.googleapis.com/fake-bucket/fake.tar.gz"
    MACHINE_TYPE = "fake-machine-type"
    MIN_MACHINE_SIZE = "fake-min-machine-size"
    METADATA = ("metadata_key", "metadata_value")
    NETWORK = "fake-network"
    ZONE = "fake-zone"
    ORIENTATION = "portrait"
    DEVICE_RESOLUTION = "1200x1200x1200x1200"
    TARGET = "aosp_cf_x86_phone-userdebug"
    BUILD_ID = "2263051"
    INSTANCE = "fake-instance"
    BOOT_COMPLETED_MSG = "VIRTUAL_DEVICE_BOOT_COMPLETED"
    BOOT_STARTED_MSG = "VIRTUAL_DEVICE_BOOT_STARTED"
    DPI = 160
    X_RES = 720
    Y_RES = 1280
    EXTRA_SCOPES = None

    def _GetFakeConfig(self):
        """Create a fake configuration object.

        Returns:
          A fake configuration mock object.
        """
        fake_cfg = mock.MagicMock()
        fake_cfg.project = self.PROJECT
        fake_cfg.service_account_name = self.SERVICE_ACCOUNT_NAME
        fake_cfg.service_account_private_key_path = self.PRIVATE_KEY_PATH
        fake_cfg.ssh_public_key_path = self.SSH_PUBLIC_KEY_PATH
        fake_cfg.zone = self.ZONE
        fake_cfg.machine_type = self.MACHINE_TYPE
        fake_cfg.min_machine_size = self.MIN_MACHINE_SIZE
        fake_cfg.network = self.NETWORK
        fake_cfg.orientation = self.ORIENTATION
        fake_cfg.resolution = self.DEVICE_RESOLUTION
        fake_cfg.metadata_variable = {self.METADATA[0]: self.METADATA[1]}
        fake_cfg.extra_scopes = self.EXTRA_SCOPES
        return fake_cfg

    def setUp(self):
        """Set up the test."""
        super(AndroidComputeClientTest, self).setUp()
        self.Patch(android_compute_client.AndroidComputeClient,
                   "InitResourceHandle")
        self.android_compute_client = android_compute_client.AndroidComputeClient(
            self._GetFakeConfig(), mock.MagicMock())

   # pylint: disable=no-member
    def testCreateImage(self):
        """Test CreateImage."""
        self.Patch(gcompute_client.ComputeClient, "CreateImage")
        self.Patch(
            gcompute_client.ComputeClient,
            "CheckImageExists",
            return_value=False)
        image_name = "image-gce-x86-userdebug-2345-abcd"
        self.android_compute_client.CreateImage(image_name,
                                                self.GS_IMAGE_SOURCE_URI)
        super(android_compute_client.AndroidComputeClient,
              self.android_compute_client).CreateImage.assert_called_with(
                  image_name, self.GS_IMAGE_SOURCE_URI)

    def testCreateInstance(self):
        """Test CreateInstance."""
        image_project = None
        gpu = None
        self.Patch(
            gcompute_client.ComputeClient,
            "CompareMachineSize",
            return_value=1)
        self.Patch(gcompute_client.ComputeClient, "CreateInstance")
        self.Patch(
            gcompute_client.ComputeClient,
            "_GetDiskArgs",
            return_value=[{
                "fake_arg": "fake_value"
            }])
        instance_name = "gce-x86-userdebug-2345-abcd"
        extra_disk_name = "gce-x86-userdebug-2345-abcd-data"
        expected_metadata = {
            self.METADATA[0]: self.METADATA[1],
            "cfg_sta_display_resolution": self.DEVICE_RESOLUTION,
            "t_force_orientation": self.ORIENTATION,
            "avd_type": "gce",
            "display": "720x1280 (160)"
        }

        expected_disk_args = [{
            "fake_arg": "fake_value"
        }]

        fake_avd_spec = mock.MagicMock()
        fake_avd_spec.avd_type = "gce"
        fake_avd_spec.hw_property = {
            "x_res" : str(self.X_RES),
            "y_res" : str(self.Y_RES),
            "dpi" : str(self.DPI)}

        self.android_compute_client.CreateInstance(instance_name, self.IMAGE,
                                                   extra_disk_name=extra_disk_name,
                                                   avd_spec=fake_avd_spec)
        super(android_compute_client.AndroidComputeClient,
              self.android_compute_client).CreateInstance.assert_called_with(
                  instance_name, self.IMAGE, self.MACHINE_TYPE,
                  expected_metadata, self.NETWORK, self.ZONE,
                  expected_disk_args, image_project, gpu, extra_disk_name,
                  extra_scopes=self.EXTRA_SCOPES, tags=None)

    # pylint: disable=invalid-name
    def testCheckMachineSizeMeetsRequirement(self):
        """Test CheckMachineSize when machine size meets requirement."""
        self.Patch(
            gcompute_client.ComputeClient,
            "CompareMachineSize",
            return_value=1)
        # pylint: disable=protected-access
        self.android_compute_client._CheckMachineSize()
        self.android_compute_client.CompareMachineSize.assert_called_with(
            self.MACHINE_TYPE, self.MIN_MACHINE_SIZE, self.ZONE)

    def testCheckMachineSizeDoesNotMeetRequirement(self):
        """Test CheckMachineSize when machine size does not meet requirement."""
        self.Patch(
            gcompute_client.ComputeClient,
            "CompareMachineSize",
            return_value=-1)
        self.assertRaisesRegexp(
            errors.DriverError,
            ".*does not meet the minimum required machine size.*",
            # pylint: disable=protected-access
            self.android_compute_client._CheckMachineSize)
        self.android_compute_client.CompareMachineSize.assert_called_with(
            self.MACHINE_TYPE, self.MIN_MACHINE_SIZE, self.ZONE)

    def testCheckBoot(self):
        """Test CheckBoot."""
        self.Patch(
            gcompute_client.ComputeClient,
            "GetSerialPortOutput",
            return_value="")
        self.assertFalse(self.android_compute_client.CheckBoot(self.INSTANCE))
        self.Patch(
            gcompute_client.ComputeClient,
            "GetSerialPortOutput",
            return_value=self.BOOT_COMPLETED_MSG)
        self.assertTrue(self.android_compute_client.CheckBoot(self.INSTANCE))
        self.Patch(
            gcompute_client.ComputeClient,
            "GetSerialPortOutput",
            return_value=self.BOOT_STARTED_MSG)
        self.assertTrue(self.android_compute_client.CheckBoot(self.INSTANCE))


if __name__ == "__main__":
    unittest.main()
