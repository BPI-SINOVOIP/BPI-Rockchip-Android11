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
"""A client that manages Goldfish Virtual Device on compute engine.

** GoldfishComputeClient **

GoldfishComputeClient derives from AndroidComputeClient. It manges a google
compute engine project that is setup for running Goldfish Virtual Devices.
It knows how to create a host instance from a Goldfish Stable Host Image, fetch
Android build, an emulator build, and start Android within the host instance.

** Class hierarchy **

  base_cloud_client.BaseCloudApiClient
                ^
                |
       gcompute_client.ComputeClient
                ^
                |
       android_compute_client.AndroidComputeClient
                ^
                |
       goldfish_compute_client.GoldfishComputeClient


TODO: This class should likely be merged with CvdComputeClient
"""

import getpass
import logging

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import gcompute_client


logger = logging.getLogger(__name__)


class GoldfishComputeClient(android_compute_client.AndroidComputeClient):
    """Client that manages Goldfish based Android Virtual Device.

    Attributes:
        acloud_config: An AcloudConfig object.
        oauth2_credentials: An oauth2client.OAuth2Credentials instance.
    """

    # To determine if the boot failed
    BOOT_FAILED_MSG = "VIRTUAL_DEVICE_FAILED"

    # To determine the failure reason
    # If the emulator build is not available
    EMULATOR_FETCH_FAILED_MSG = "EMULATOR_FETCH_FAILED"
    # If the system image build is not available
    ANDROID_FETCH_FAILED_MSG = "ANDROID_FETCH_FAILED"
    # If the emulator could not boot in time
    BOOT_TIMEOUT_MSG = "VIRTUAL_DEVICE_BOOT_FAILED"

    #pylint: disable=signature-differs
    def _GetDiskArgs(self, disk_name, image_name, image_project, disk_size_gb):
        """Helper to generate disk args that is used to create an instance.

        Args:
            disk_name: String, the name of disk.
            image_name: String, the name of the system image.
            image_project: String, the name of the project where the image.
            disk_size_gb: Integer, size of the blank data disk in GB.

        Returns:
            A dictionary representing disk args.
        """
        return [{
            "type": "PERSISTENT",
            "boot": True,
            "mode": "READ_WRITE",
            "autoDelete": True,
            "initializeParams": {
                "diskName":
                disk_name,
                "sourceImage":
                self.GetImage(image_name, image_project)["selfLink"],
                "diskSizeGb":
                disk_size_gb
            },
        }]
    #pylint: disable=signature-differs

    def CheckBootFailure(self, serial_out, instance):
        """Overriding method from the parent class.

        Args:
            serial_out: String
            instance: String

        Raises:
            Raises an errors.DeviceBootError exception if a failure is detected.
        """
        if self.BOOT_FAILED_MSG in serial_out:
            if self.EMULATOR_FETCH_FAILED_MSG in serial_out:
                raise errors.DeviceBootError(
                    "Failed to download emulator build. Re-run with a newer build."
                )
            if self.ANDROID_FETCH_FAILED_MSG in serial_out:
                raise errors.DeviceBootError(
                    "Failed to download system image build. Re-run with a newer build."
                )
            if self.BOOT_TIMEOUT_MSG in serial_out:
                raise errors.DeviceBootError(
                    "Emulator timed out while booting.")

    @staticmethod
    def GetKernelBuildArtifact(target):
        if target == "kernel":
            return "bzImage"
        if target == "kernel_x86_64":
            return "bzImage"
        if target == "kernel_aarch64":
            return "Image.gz"
        raise errors.DeviceBootError(
            "Don't know the artifact name for '%s' target" % target)

    # pylint: disable=too-many-locals,arguments-differ
    # TODO: Refactor CreateInstance to pass in an object instead of all these args.
    def CreateInstance(self,
                       instance,
                       image_name,
                       image_project,
                       build_target,
                       branch,
                       build_id,
                       kernel_branch=None,
                       kernel_build_id=None,
                       kernel_build_target=None,
                       emulator_branch=None,
                       emulator_build_id=None,
                       blank_data_disk_size_gb=None,
                       gpu=None,
                       avd_spec=None,
                       extra_scopes=None,
                       tags=None):
        """Create a goldfish instance given a stable host image and a build id.

        Args:
            instance: String, instance name.
            image_name: String, the name of the system image.
            image_project: String, name of the project where the image belongs.
                           Assume the default project if None.
            build_target: String, target name, e.g. "sdk_phone_x86_64-sdk"
            branch: String, branch name, e.g. "git_pi-dev"
            build_id: String, build id, a string, e.g. "2263051", "P2804227"
            kernel_branch: String, kernel branch name.
            kernel_build_id: String, kernel build id.
            kernel_build_target: kernel target, e.g. "kernel_x86_64"
            emulator_branch: String, emulator branch name, e.g."aosp-emu-master-dev"
            emulator_build_id: String, emulator build id, a string, e.g. "2263051", "P2804227"
            blank_data_disk_size_gb: Integer, size of the blank data disk in GB.
            gpu: String, GPU that should be attached to the instance, or None of no
                 acceleration is needed. e.g. "nvidia-tesla-k80"
            avd_spec: An AVDSpec instance.
            extra_scopes: A list of extra scopes to be passed to the instance.
            tags: A list of tags to associate with the instance. e.g.
                 ["http-server", "https-server"]
        """
        self._CheckMachineSize()

        # Add space for possible data partition.
        boot_disk_size_gb = (
            int(self.GetImage(image_name, image_project)["diskSizeGb"]) +
            blank_data_disk_size_gb)
        disk_args = self._GetDiskArgs(instance, image_name, image_project,
                                      boot_disk_size_gb)

        # Goldfish instances are metadata compatible with cuttlefish devices.
        # See details goto/goldfish-deployment
        metadata = self._metadata.copy()
        metadata["user"] = getpass.getuser()
        metadata[constants.INS_KEY_AVD_TYPE] = constants.TYPE_GF

        # Note that we use the same metadata naming conventions as cuttlefish
        metadata["cvd_01_fetch_android_build_target"] = build_target
        metadata["cvd_01_fetch_android_bid"] = "{branch}/{build_id}".format(
            branch=branch, build_id=build_id)
        if kernel_branch and kernel_build_id and kernel_build_target:
            metadata["cvd_01_fetch_kernel_bid"] = "{branch}/{build_id}".format(
                branch=kernel_branch, build_id=kernel_build_id)
            metadata["cvd_01_fetch_kernel_build_target"] = kernel_build_target
            metadata["cvd_01_fetch_kernel_build_artifact"] = (
                self.GetKernelBuildArtifact(kernel_build_target))
            metadata["cvd_01_use_custom_kernel"] = "true"
        if emulator_branch and emulator_build_id:
            metadata[
                "cvd_01_fetch_emulator_bid"] = "{branch}/{build_id}".format(
                    branch=emulator_branch, build_id=emulator_build_id)
        metadata["cvd_01_launch"] = "1"

        # Update metadata by avd_spec
        # for legacy create_gf cmd, we will keep using resolution.
        # And always use avd_spec for acloud create cmd.
        if avd_spec:
            metadata[constants.INS_KEY_AVD_FLAVOR] = avd_spec.flavor
            metadata["cvd_01_x_res"] = avd_spec.hw_property[constants.HW_X_RES]
            metadata["cvd_01_y_res"] = avd_spec.hw_property[constants.HW_Y_RES]
            metadata["cvd_01_dpi"] = avd_spec.hw_property[constants.HW_ALIAS_DPI]
            metadata[constants.INS_KEY_DISPLAY] = ("%sx%s (%s)" % (
                avd_spec.hw_property[constants.HW_X_RES],
                avd_spec.hw_property[constants.HW_Y_RES],
                avd_spec.hw_property[constants.HW_ALIAS_DPI]))
        else:
            resolution = self._resolution.split("x")
            metadata["cvd_01_x_res"] = resolution[0]
            metadata["cvd_01_y_res"] = resolution[1]
            metadata["cvd_01_dpi"] = resolution[3]

        gcompute_client.ComputeClient.CreateInstance(
            self,
            instance=instance,
            image_name=image_name,
            image_project=image_project,
            disk_args=disk_args,
            metadata=metadata,
            machine_type=self._machine_type,
            network=self._network,
            zone=self._zone,
            gpu=gpu,
            tags=tags,
            extra_scopes=extra_scopes)
