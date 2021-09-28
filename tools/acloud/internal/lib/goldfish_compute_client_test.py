#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
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
"""Tests for acloud.internal.lib.goldfish_compute_client."""
import unittest
import mock

from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import goldfish_compute_client


class GoldfishComputeClientTest(driver_test_lib.BaseDriverTest):
    """Test GoldfishComputeClient."""

    SSH_PUBLIC_KEY_PATH = ""
    INSTANCE = "fake-instance"
    IMAGE = "fake-image"
    IMAGE_PROJECT = "fake-iamge-project"
    MACHINE_TYPE = "fake-machine-type"
    NETWORK = "fake-network"
    ZONE = "fake-zone"
    BRANCH = "fake-branch"
    TARGET = "sdk_phone_x86_64-sdk"
    BUILD_ID = "2263051"
    KERNEL_BRANCH = "kernel-p-dev-android-goldfish-4.14-x86-64"
    KERNEL_BUILD_ID = "112233"
    KERNEL_BUILD_TARGET = "kernel_x86_64"
    KERNEL_BUILD_ARTIFACT = "bzImage"
    EMULATOR_BRANCH = "aosp-emu-master-dev"
    EMULATOR_BUILD_ID = "1234567"
    DPI = 160
    X_RES = 720
    Y_RES = 1280
    METADATA = {"metadata_key": "metadata_value"}
    EXTRA_DATA_DISK_SIZE_GB = 4
    BOOT_DISK_SIZE_GB = 10
    GPU = "nvidia-tesla-k80"
    EXTRA_SCOPES = "scope1"
    TAGS = ['http-server']

    def _GetFakeConfig(self):
        """Create a fake configuration object.

        Returns:
            A fake configuration mock object.
        """
        fake_cfg = mock.MagicMock()
        fake_cfg.ssh_public_key_path = self.SSH_PUBLIC_KEY_PATH
        fake_cfg.machine_type = self.MACHINE_TYPE
        fake_cfg.network = self.NETWORK
        fake_cfg.zone = self.ZONE
        fake_cfg.resolution = "{x}x{y}x32x{dpi}".format(
            x=self.X_RES, y=self.Y_RES, dpi=self.DPI)
        fake_cfg.metadata_variable = self.METADATA
        fake_cfg.extra_data_disk_size_gb = self.EXTRA_DATA_DISK_SIZE_GB
        fake_cfg.extra_scopes = self.EXTRA_SCOPES
        return fake_cfg

    def setUp(self):
        """Set up the test."""
        super(GoldfishComputeClientTest, self).setUp()
        self.Patch(goldfish_compute_client.GoldfishComputeClient,
                   "InitResourceHandle")
        self.goldfish_compute_client = (
            goldfish_compute_client.GoldfishComputeClient(
                self._GetFakeConfig(), mock.MagicMock()))
        self.Patch(
            gcompute_client.ComputeClient,
            "CompareMachineSize",
            return_value=1)
        self.Patch(
            gcompute_client.ComputeClient,
            "GetImage",
            return_value={"diskSizeGb": 10})
        self.Patch(gcompute_client.ComputeClient, "CreateInstance")
        self.Patch(
            goldfish_compute_client.GoldfishComputeClient,
            "_GetDiskArgs",
            return_value=[{
                "fake_arg": "fake_value"
            }])

    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstance(self, _mock_user):
        """Test CreateInstance."""

        expected_metadata = {
            "user": "fake_user",
            "avd_type": "goldfish",
            "cvd_01_fetch_android_build_target": self.TARGET,
            "cvd_01_fetch_android_bid":
                "{branch}/{build_id}".format(
                    branch=self.BRANCH, build_id=self.BUILD_ID),
            "cvd_01_fetch_kernel_bid":
                "{branch}/{build_id}".format(
                    branch=self.KERNEL_BRANCH, build_id=self.KERNEL_BUILD_ID),
            "cvd_01_fetch_kernel_build_target": self.KERNEL_BUILD_TARGET,
            "cvd_01_fetch_kernel_build_artifact": self.KERNEL_BUILD_ARTIFACT,
            "cvd_01_use_custom_kernel": "true",
            "cvd_01_fetch_emulator_bid":
                "{branch}/{build_id}".format(
                    branch=self.EMULATOR_BRANCH,
                    build_id=self.EMULATOR_BUILD_ID),
            "cvd_01_launch": "1",
            "cvd_01_dpi": str(self.DPI),
            "cvd_01_x_res": str(self.X_RES),
            "cvd_01_y_res": str(self.Y_RES),
        }
        expected_metadata.update(self.METADATA)
        expected_disk_args = [{"fake_arg": "fake_value"}]

        self.goldfish_compute_client.CreateInstance(
            self.INSTANCE, self.IMAGE, self.IMAGE_PROJECT, self.TARGET,
            self.BRANCH, self.BUILD_ID,
            self.KERNEL_BRANCH,
            self.KERNEL_BUILD_ID,
            self.KERNEL_BUILD_TARGET,
            self.EMULATOR_BRANCH,
            self.EMULATOR_BUILD_ID, self.EXTRA_DATA_DISK_SIZE_GB, self.GPU,
            extra_scopes=self.EXTRA_SCOPES,
            tags=self.TAGS)

        # pylint: disable=no-member
        gcompute_client.ComputeClient.CreateInstance.assert_called_with(
            self.goldfish_compute_client,
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            image_project=self.IMAGE_PROJECT,
            disk_args=expected_disk_args,
            metadata=expected_metadata,
            machine_type=self.MACHINE_TYPE,
            network=self.NETWORK,
            zone=self.ZONE,
            gpu=self.GPU,
            tags=self.TAGS,
            extra_scopes=self.EXTRA_SCOPES)


if __name__ == "__main__":
    unittest.main()
