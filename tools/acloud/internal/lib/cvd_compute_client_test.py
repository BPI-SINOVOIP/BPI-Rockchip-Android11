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

"""Tests for acloud.internal.lib.cvd_compute_client."""

import glob
import unittest
import mock

from acloud.create import avd_spec
from acloud.internal import constants
from acloud.internal.lib import cvd_compute_client
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils
from acloud.list import list as list_instances


class CvdComputeClientTest(driver_test_lib.BaseDriverTest):
    """Test CvdComputeClient."""

    SSH_PUBLIC_KEY_PATH = ""
    INSTANCE = "fake-instance"
    IMAGE = "fake-image"
    IMAGE_PROJECT = "fake-iamge-project"
    MACHINE_TYPE = "fake-machine-type"
    NETWORK = "fake-network"
    ZONE = "fake-zone"
    BRANCH = "fake-branch"
    TARGET = "aosp_cf_x86_phone-userdebug"
    BUILD_ID = "2263051"
    KERNEL_BRANCH = "fake-kernel-branch"
    KERNEL_BUILD_ID = "1234567"
    KERNEL_BUILD_TARGET = "kernel"
    DPI = 160
    X_RES = 720
    Y_RES = 1280
    METADATA = {"metadata_key": "metadata_value"}
    EXTRA_DATA_DISK_SIZE_GB = 4
    BOOT_DISK_SIZE_GB = 10
    LAUNCH_ARGS = "--setupwizard_mode=REQUIRED"
    EXTRA_SCOPES = ["scope1"]

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
        fake_cfg.launch_args = self.LAUNCH_ARGS
        fake_cfg.extra_scopes = self.EXTRA_SCOPES
        return fake_cfg

    def setUp(self):
        """Set up the test."""
        super(CvdComputeClientTest, self).setUp()
        self.Patch(cvd_compute_client.CvdComputeClient, "InitResourceHandle")
        self.Patch(list_instances, "ChooseOneRemoteInstance", return_value=mock.MagicMock())
        self.Patch(list_instances, "GetInstancesFromInstanceNames", return_value=mock.MagicMock())
        self.cvd_compute_client = cvd_compute_client.CvdComputeClient(
            self._GetFakeConfig(), mock.MagicMock())

    @mock.patch.object(utils, "GetBuildEnvironmentVariable", return_value="fake_env")
    @mock.patch.object(glob, "glob", return_value=["fake.img"])
    @mock.patch.object(gcompute_client.ComputeClient, "CompareMachineSize",
                       return_value=1)
    @mock.patch.object(gcompute_client.ComputeClient, "GetImage",
                       return_value={"diskSizeGb": 10})
    @mock.patch.object(gcompute_client.ComputeClient, "CreateInstance")
    @mock.patch.object(cvd_compute_client.CvdComputeClient, "_GetDiskArgs",
                       return_value=[{"fake_arg": "fake_value"}])
    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstance(self, _get_user, _get_disk_args, mock_create,
                           _get_image, _compare_machine_size, mock_check_img,
                           _mock_env):
        """Test CreateInstance."""
        expected_metadata = {
            "cvd_01_dpi": str(self.DPI),
            "cvd_01_fetch_android_build_target": self.TARGET,
            "cvd_01_fetch_android_bid": "{branch}/{build_id}".format(
                branch=self.BRANCH, build_id=self.BUILD_ID),
            "cvd_01_fetch_kernel_bid": "{branch}/{build_id}".format(
                branch=self.KERNEL_BRANCH, build_id=self.KERNEL_BUILD_ID),
            "cvd_01_fetch_kernel_build_target": self.KERNEL_BUILD_TARGET,
            "cvd_01_x_res": str(self.X_RES),
            "cvd_01_y_res": str(self.Y_RES),
            "user": "fake_user",
            "cvd_01_data_policy":
                self.cvd_compute_client.DATA_POLICY_CREATE_IF_MISSING,
            "cvd_01_blank_data_disk_size": str(self.EXTRA_DATA_DISK_SIZE_GB * 1024),
        }
        expected_metadata_local_image = {
            "cvd_01_dpi": str(self.DPI),
            "cvd_01_x_res": str(self.X_RES),
            "cvd_01_y_res": str(self.Y_RES),
            "user": "fake_user",
            "cvd_01_data_policy":
                self.cvd_compute_client.DATA_POLICY_CREATE_IF_MISSING,
            "cvd_01_blank_data_disk_size": str(self.EXTRA_DATA_DISK_SIZE_GB * 1024),
        }
        expected_metadata.update(self.METADATA)
        expected_metadata_local_image.update(self.METADATA)
        remote_image_metadata = dict(expected_metadata)
        remote_image_metadata["cvd_01_launch"] = self.LAUNCH_ARGS
        expected_disk_args = [{"fake_arg": "fake_value"}]

        self.cvd_compute_client.CreateInstance(
            self.INSTANCE, self.IMAGE, self.IMAGE_PROJECT, self.TARGET,
            self.BRANCH, self.BUILD_ID, self.KERNEL_BRANCH,
            self.KERNEL_BUILD_ID, self.KERNEL_BUILD_TARGET,
            self.EXTRA_DATA_DISK_SIZE_GB, extra_scopes=self.EXTRA_SCOPES)
        mock_create.assert_called_with(
            self.cvd_compute_client,
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            image_project=self.IMAGE_PROJECT,
            disk_args=expected_disk_args,
            metadata=remote_image_metadata,
            machine_type=self.MACHINE_TYPE,
            network=self.NETWORK,
            zone=self.ZONE,
            extra_scopes=self.EXTRA_SCOPES)

        #test use local image in the remote instance.
        local_image_metadata = dict(expected_metadata_local_image)
        args = mock.MagicMock()
        mock_check_img.return_value = True
        args.local_image = None
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.adb_port = None
        fake_avd_spec = avd_spec.AVDSpec(args)
        fake_avd_spec.hw_property[constants.HW_X_RES] = str(self.X_RES)
        fake_avd_spec.hw_property[constants.HW_Y_RES] = str(self.Y_RES)
        fake_avd_spec.hw_property[constants.HW_ALIAS_DPI] = str(self.DPI)
        fake_avd_spec.hw_property[constants.HW_ALIAS_DISK] = str(
            self.EXTRA_DATA_DISK_SIZE_GB * 1024)
        local_image_metadata["avd_type"] = constants.TYPE_CF
        local_image_metadata["flavor"] = "phone"
        local_image_metadata[constants.INS_KEY_DISPLAY] = ("%sx%s (%s)" % (
            fake_avd_spec.hw_property[constants.HW_X_RES],
            fake_avd_spec.hw_property[constants.HW_Y_RES],
            fake_avd_spec.hw_property[constants.HW_ALIAS_DPI]))
        self.cvd_compute_client.CreateInstance(
            self.INSTANCE, self.IMAGE, self.IMAGE_PROJECT, self.TARGET, self.BRANCH,
            self.BUILD_ID, self.KERNEL_BRANCH, self.KERNEL_BUILD_ID,
            self.KERNEL_BUILD_TARGET, self.EXTRA_DATA_DISK_SIZE_GB,
            fake_avd_spec, extra_scopes=self.EXTRA_SCOPES)

        mock_create.assert_called_with(
            self.cvd_compute_client,
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            image_project=self.IMAGE_PROJECT,
            disk_args=expected_disk_args,
            metadata=local_image_metadata,
            machine_type=self.MACHINE_TYPE,
            network=self.NETWORK,
            zone=self.ZONE,
            extra_scopes=self.EXTRA_SCOPES)

    # pylint: disable=protected-access
    def testGetLaunchCvdArgs(self):
        """Test GetLaunchCvdArgs"""
        fake_avd_spec = mock.MagicMock()
        fake_avd_spec.hw_property = {}
        fake_avd_spec.hw_property[constants.HW_ALIAS_CPUS] = "2"
        fake_avd_spec.hw_property[constants.HW_ALIAS_MEMORY] = "4096"

        # Test get launch_args exist from config
        self.assertEqual(self.cvd_compute_client._GetLaunchCvdArgs(fake_avd_spec),
                         self.LAUNCH_ARGS)

        # Test get launch_args from cpu and memory
        expected_args = "-cpus=2 -memory_mb=4096"
        self.cvd_compute_client._launch_args = None
        self.assertEqual(self.cvd_compute_client._GetLaunchCvdArgs(fake_avd_spec),
                         expected_args)

        # Test to set launch_args as "1" for no customized args
        expected_args = "1"
        fake_avd_spec.hw_property = {}
        self.assertEqual(self.cvd_compute_client._GetLaunchCvdArgs(fake_avd_spec),
                         expected_args)

        self.cvd_compute_client._launch_args = self.LAUNCH_ARGS


if __name__ == "__main__":
    unittest.main()
