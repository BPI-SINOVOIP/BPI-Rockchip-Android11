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

"""Tests for create_cuttlefish_action.

Tests for acloud.public.actions.create_cuttlefish_action.
"""

import uuid
import unittest
import mock

from acloud.internal.lib import android_build_client
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import auth
from acloud.internal.lib import cvd_compute_client
from acloud.internal.lib import cvd_compute_client_multi_stage
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import ssh
from acloud.public.actions import create_cuttlefish_action


class CreateCuttlefishActionTest(driver_test_lib.BaseDriverTest):
    """Test create_cuttlefish_action."""

    IP = ssh.IP(external="127.0.0.1", internal="10.0.0.1")
    INSTANCE = "fake-instance"
    IMAGE = "fake-image"
    BUILD_TARGET = "fake-build-target"
    BUILD_ID = "12345"
    KERNEL_BRANCH = "fake-kernel-branch"
    KERNEL_BUILD_ID = "54321"
    KERNEL_BUILD_TARGET = "kernel"
    BRANCH = "fake-branch"
    SYSTEM_BRANCH = "fake-system-branch"
    SYSTEM_BUILD_ID = "23456"
    SYSTEM_BUILD_TARGET = "fake-system-build-target"
    STABLE_HOST_IMAGE_NAME = "fake-stable-host-image-name"
    STABLE_HOST_IMAGE_PROJECT = "fake-stable-host-image-project"
    EXTRA_DATA_DISK_GB = 4
    EXTRA_SCOPES = ["scope1", "scope2"]

    def setUp(self):
        """Set up the test."""
        super(CreateCuttlefishActionTest, self).setUp()
        self.build_client = mock.MagicMock()
        self.Patch(
            android_build_client,
            "AndroidBuildClient",
            return_value=self.build_client)
        self.compute_client = mock.MagicMock()
        self.Patch(
            cvd_compute_client,
            "CvdComputeClient",
            return_value=self.compute_client)
        self.Patch(
            cvd_compute_client_multi_stage,
            "CvdComputeClient",
            return_value=self.compute_client)
        self.Patch(
            android_compute_client,
            "AndroidComputeClient",
            return_value=self.compute_client)
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())

    def _CreateCfg(self):
        """A helper method that creates a mock configuration object."""
        cfg = mock.MagicMock()
        cfg.service_account_name = "fake@service.com"
        cfg.service_account_private_key_path = "/fake/path/to/key"
        cfg.zone = "fake_zone"
        cfg.disk_image_name = "fake_image.tar.gz"
        cfg.disk_image_mime_type = "fake/type"
        cfg.ssh_private_key_path = ""
        cfg.ssh_public_key_path = ""
        cfg.stable_host_image_name = self.STABLE_HOST_IMAGE_NAME
        cfg.stable_host_image_project = self.STABLE_HOST_IMAGE_PROJECT
        cfg.extra_data_disk_size_gb = self.EXTRA_DATA_DISK_GB
        cfg.kernel_build_target = self.KERNEL_BUILD_TARGET
        cfg.extra_scopes = self.EXTRA_SCOPES
        cfg.enable_multi_stage = False
        return cfg

    def testCreateDevices(self):
        """Test CreateDevices."""
        cfg = self._CreateCfg()

        # Mock uuid
        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)

        # Mock compute client methods
        self.compute_client.GetInstanceIP.return_value = self.IP
        self.compute_client.GenerateImageName.return_value = self.IMAGE
        self.compute_client.GenerateInstanceName.return_value = self.INSTANCE

        # Mock build client method
        self.build_client.GetBuildInfo.side_effect = [
            android_build_client.BuildInfo(
                self.BRANCH, self.BUILD_ID, self.BUILD_TARGET, None),
            android_build_client.BuildInfo(
                self.KERNEL_BRANCH, self.KERNEL_BUILD_ID,
                self.KERNEL_BUILD_TARGET, None),
            android_build_client.BuildInfo(
                self.SYSTEM_BRANCH, self.SYSTEM_BUILD_ID,
                self.SYSTEM_BUILD_TARGET, None)]

        # Call CreateDevices
        report = create_cuttlefish_action.CreateDevices(
            cfg, self.BUILD_TARGET, self.BUILD_ID, branch=self.BRANCH,
            kernel_build_id=self.KERNEL_BUILD_ID,
            system_build_target=self.SYSTEM_BUILD_TARGET,
            system_branch=self.SYSTEM_BRANCH,
            system_build_id=self.SYSTEM_BUILD_ID)

        # Verify
        self.compute_client.CreateInstance.assert_called_with(
            instance=self.INSTANCE,
            image_name=self.STABLE_HOST_IMAGE_NAME,
            image_project=self.STABLE_HOST_IMAGE_PROJECT,
            build_target=self.BUILD_TARGET,
            branch=self.BRANCH,
            build_id=self.BUILD_ID,
            kernel_branch=self.KERNEL_BRANCH,
            kernel_build_id=self.KERNEL_BUILD_ID,
            kernel_build_target=self.KERNEL_BUILD_TARGET,
            system_branch=self.SYSTEM_BRANCH,
            system_build_id=self.SYSTEM_BUILD_ID,
            system_build_target=self.SYSTEM_BUILD_TARGET,
            blank_data_disk_size_gb=self.EXTRA_DATA_DISK_GB,
            extra_scopes=self.EXTRA_SCOPES)

        self.assertEqual(report.data, {
            "devices": [
                {
                    "branch": self.BRANCH,
                    "build_id": self.BUILD_ID,
                    "build_target": self.BUILD_TARGET,
                    "kernel_branch": self.KERNEL_BRANCH,
                    "kernel_build_id": self.KERNEL_BUILD_ID,
                    "kernel_build_target": self.KERNEL_BUILD_TARGET,
                    "system_branch": self.SYSTEM_BRANCH,
                    "system_build_id": self.SYSTEM_BUILD_ID,
                    "system_build_target": self.SYSTEM_BUILD_TARGET,
                    "instance_name": self.INSTANCE,
                    "ip": self.IP.external,
                },
            ],
        })
        self.assertEqual(report.command, "create_cf")
        self.assertEqual(report.status, "SUCCESS")


if __name__ == "__main__":
    unittest.main()
