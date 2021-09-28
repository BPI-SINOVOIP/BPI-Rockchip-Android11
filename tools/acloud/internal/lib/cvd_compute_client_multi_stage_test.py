#!/usr/bin/env python
#
# Copyright 2019 - The Android Open Source Project
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

"""Tests for acloud.internal.lib.cvd_compute_client_multi_stage."""

import glob
import os
import subprocess
import unittest
import mock

from acloud.create import avd_spec
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import cvd_compute_client_multi_stage
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils
from acloud.internal.lib.ssh import Ssh
from acloud.list import list as list_instances

from acloud.internal.lib.cvd_compute_client_multi_stage import _ProcessBuild


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
    GPU = "fake-gpu"

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
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "InitResourceHandle")
        self.Patch(android_build_client.AndroidBuildClient, "InitResourceHandle")
        self.Patch(android_build_client.AndroidBuildClient, "DownloadArtifact")
        self.Patch(list_instances, "GetInstancesFromInstanceNames", return_value=mock.MagicMock())
        self.Patch(list_instances, "ChooseOneRemoteInstance", return_value=mock.MagicMock())
        self.Patch(Ssh, "ScpPushFile")
        self.Patch(Ssh, "WaitForSsh")
        self.Patch(Ssh, "GetBaseCmd")
        self.cvd_compute_client_multi_stage = cvd_compute_client_multi_stage.CvdComputeClient(
            self._GetFakeConfig(), mock.MagicMock(), gpu=self.GPU)
        self.args = mock.MagicMock()
        self.args.local_image = None
        self.args.config_file = ""
        self.args.avd_type = constants.TYPE_CF
        self.args.flavor = "phone"
        self.args.adb_port = None
        self.args.hw_property = "cpu:2,resolution:1080x1920,dpi:240,memory:4g"

    # pylint: disable=protected-access
    @mock.patch.object(utils, "GetBuildEnvironmentVariable", return_value="fake_env")
    @mock.patch.object(glob, "glob", return_value=["fake.img"])
    def testGetLaunchCvdArgs(self, _mock_check_img, _mock_env):
        """test GetLaunchCvdArgs."""
        # test GetLaunchCvdArgs with avd_spec
        fake_avd_spec = avd_spec.AVDSpec(self.args)
        expeted_args = ['-x_res=1080', '-y_res=1920', '-dpi=240', '-cpus=2',
                        '-memory_mb=4096', '--setupwizard_mode=REQUIRED',
                        '-gpu_mode=drm_virgl']
        launch_cvd_args = self.cvd_compute_client_multi_stage._GetLaunchCvdArgs(fake_avd_spec)
        self.assertEqual(launch_cvd_args, expeted_args)

        # test GetLaunchCvdArgs without avd_spec
        expeted_args = ['-x_res=720', '-y_res=1280', '-dpi=160',
                        '--setupwizard_mode=REQUIRED', '-gpu_mode=drm_virgl']
        launch_cvd_args = self.cvd_compute_client_multi_stage._GetLaunchCvdArgs(
            avd_spec=None)
        self.assertEqual(launch_cvd_args, expeted_args)

    # pylint: disable=protected-access
    def testProcessBuild(self):
        """Test creating "cuttlefish build" strings."""
        self.assertEqual(_ProcessBuild(build_id="123", branch="abc", build_target="def"), "123/def")
        self.assertEqual(_ProcessBuild(build_id=None, branch="abc", build_target="def"), "abc/def")
        self.assertEqual(_ProcessBuild(build_id="123", branch=None, build_target="def"), "123/def")
        self.assertEqual(_ProcessBuild(build_id="123", branch="abc", build_target=None), "123")
        self.assertEqual(_ProcessBuild(build_id=None, branch="abc", build_target=None), "abc")
        self.assertEqual(_ProcessBuild(build_id="123", branch=None, build_target=None), "123")
        self.assertEqual(_ProcessBuild(build_id=None, branch=None, build_target=None), None)

    @mock.patch.object(utils, "GetBuildEnvironmentVariable", return_value="fake_env")
    @mock.patch.object(glob, "glob", return_value=["fake.img"])
    @mock.patch.object(gcompute_client.ComputeClient, "CompareMachineSize",
                       return_value=1)
    @mock.patch.object(gcompute_client.ComputeClient, "GetImage",
                       return_value={"diskSizeGb": 10})
    @mock.patch.object(gcompute_client.ComputeClient, "CreateInstance")
    @mock.patch.object(cvd_compute_client_multi_stage.CvdComputeClient, "_GetDiskArgs",
                       return_value=[{"fake_arg": "fake_value"}])
    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstance(self, _get_user, _get_disk_args, mock_create,
                           _get_image, _compare_machine_size, mock_check_img,
                           _mock_env):
        """Test CreateInstance."""
        expected_metadata = dict()
        expected_metadata_local_image = dict()
        expected_metadata.update(self.METADATA)
        expected_metadata_local_image.update(self.METADATA)
        remote_image_metadata = dict(expected_metadata)
        expected_disk_args = [{"fake_arg": "fake_value"}]
        fake_avd_spec = avd_spec.AVDSpec(self.args)
        fake_avd_spec._instance_name_to_reuse = None

        created_subprocess = mock.MagicMock()
        created_subprocess.stdout = mock.MagicMock()
        created_subprocess.stdout.readline = mock.MagicMock(return_value='')
        created_subprocess.poll = mock.MagicMock(return_value=0)
        created_subprocess.returncode = 0
        created_subprocess.communicate = mock.MagicMock(return_value=('', ''))
        self.Patch(subprocess, "Popen", return_value=created_subprocess)
        self.Patch(subprocess, "check_call")
        self.Patch(os, "chmod")
        self.Patch(os, "stat")
        self.Patch(os, "remove")
        self.Patch(os, "rmdir")
        self.cvd_compute_client_multi_stage.CreateInstance(
            self.INSTANCE, self.IMAGE, self.IMAGE_PROJECT, self.TARGET,
            self.BRANCH, self.BUILD_ID, self.KERNEL_BRANCH,
            self.KERNEL_BUILD_ID, self.KERNEL_BUILD_TARGET,
            self.EXTRA_DATA_DISK_SIZE_GB, extra_scopes=self.EXTRA_SCOPES)
        mock_create.assert_called_with(
            self.cvd_compute_client_multi_stage,
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            image_project=self.IMAGE_PROJECT,
            disk_args=expected_disk_args,
            metadata=remote_image_metadata,
            machine_type=self.MACHINE_TYPE,
            network=self.NETWORK,
            zone=self.ZONE,
            extra_scopes=self.EXTRA_SCOPES,
            gpu=self.GPU,
            tags=None)

        mock_check_img.return_value = True
        #test use local image in the remote instance.
        local_image_metadata = dict(expected_metadata_local_image)
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
        self.cvd_compute_client_multi_stage.CreateInstance(
            self.INSTANCE, self.IMAGE, self.IMAGE_PROJECT, self.TARGET, self.BRANCH,
            self.BUILD_ID, self.KERNEL_BRANCH, self.KERNEL_BUILD_ID,
            self.KERNEL_BUILD_TARGET, self.EXTRA_DATA_DISK_SIZE_GB,
            fake_avd_spec, extra_scopes=self.EXTRA_SCOPES)

        mock_create.assert_called_with(
            self.cvd_compute_client_multi_stage,
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            image_project=self.IMAGE_PROJECT,
            disk_args=expected_disk_args,
            metadata=local_image_metadata,
            machine_type=self.MACHINE_TYPE,
            network=self.NETWORK,
            zone=self.ZONE,
            extra_scopes=self.EXTRA_SCOPES,
            gpu=self.GPU,
            tags=None)


if __name__ == "__main__":
    unittest.main()
