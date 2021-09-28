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
"""Action to create goldfish device instances.

A Goldfish device is an emulated android device based on the android
emulator.
"""
import logging
import os
import re

from acloud import errors
from acloud.public.actions import base_device_factory
from acloud.public.actions import common_operations
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import goldfish_compute_client
from acloud.internal.lib import utils


logger = logging.getLogger(__name__)

_EMULATOR_INFO_FILENAME = "emulator-info.txt"
_SYSIMAGE_INFO_FILENAME = "android-info.txt"
_VERSION_PATTERN = r"version-.*=(\d+)"


class GoldfishDeviceFactory(base_device_factory.BaseDeviceFactory):
    """A class that can produce a goldfish device.

    Attributes:
        _cfg: An AcloudConfig instance.
        _build_target: String, the build target, e.g. aosp_x86-eng.
        _build_id: String, Build id, e.g. "2263051", "P2804227"
        _emulator_build_target: String, the emulator build target, e.g. aosp_x86-eng.
        _emulator_build_id: String, emulator build id.
        _gpu: String, GPU to attach to the device or None. e.g. "nvidia-tesla-k80"
        _blank_data_disk_size_gb: Integer, extra disk size
        _build_client: An AndroidBuildClient instance
        _branch: String, android branch name, e.g. git_master
        _emulator_branch: String, emulator branch name, e.g. "aosp-emu-master-dev"
    """
    LOG_FILES = ["/home/vsoc-01/emulator.log",
                 "/home/vsoc-01/log/logcat.log",
                 "/home/vsoc-01/log/adb.log",
                 "/var/log/daemon.log"]

    def __init__(self,
                 cfg,
                 build_target,
                 build_id,
                 emulator_build_target,
                 emulator_build_id,
                 kernel_build_id=None,
                 kernel_branch=None,
                 kernel_build_target=None,
                 gpu=None,
                 avd_spec=None,
                 tags=None,
                 branch=None,
                 emulator_branch=None):

        """Initialize.

        Args:
            cfg: An AcloudConfig instance.
            build_target: String, the build target, e.g. aosp_x86-eng.
            build_id: String, Build id, e.g. "2263051", "P2804227"
            emulator_build_target: String, the emulator build target, e.g. aosp_x86-eng.
            emulator_build_id: String, emulator build id.
            gpu: String, GPU to attach to the device or None. e.g. "nvidia-tesla-k80"
            avd_spec: An AVDSpec instance.
            tags: A list of tags to associate with the instance. e.g.
                  ["http-server", "https-server"]
            branch: String, branch of the emulator build target.
            emulator_branch: String, branch of the emulator.
        """

        self.credentials = auth.CreateCredentials(cfg)

        compute_client = goldfish_compute_client.GoldfishComputeClient(
            cfg, self.credentials)
        super(GoldfishDeviceFactory, self).__init__(compute_client)

        # Private creation parameters
        self._cfg = cfg
        self._gpu = gpu
        self._avd_spec = avd_spec
        self._blank_data_disk_size_gb = cfg.extra_data_disk_size_gb
        self._extra_scopes = cfg.extra_scopes
        self._tags = tags

        # Configure clients
        self._build_client = android_build_client.AndroidBuildClient(
            self.credentials)

        # Get build info
        self.build_info = self._build_client.GetBuildInfo(
            build_target, build_id, branch)
        self.emulator_build_info = self._build_client.GetBuildInfo(
            emulator_build_target, emulator_build_id, emulator_branch)
        self.kernel_build_info = self._build_client.GetBuildInfo(
            kernel_build_target or cfg.kernel_build_target, kernel_build_id,
            kernel_branch)

    def GetBuildInfoDict(self):
        """Get build info dictionary.

        Returns:
          A build info dictionary
        """
        build_info_dict = {
            key: val for key, val in utils.GetDictItems(self.build_info) if val}

        build_info_dict.update(
            {"emulator_%s" % key: val
             for key, val in utils.GetDictItems(self.emulator_build_info) if val}
            )

        build_info_dict.update(
            {"kernel_%s" % key: val
             for key, val in utils.GetDictItems(self.kernel_build_info) if val}
            )

        return build_info_dict

    def CreateInstance(self):
        """Creates single configured goldfish device.

        Override method from parent class.

        Returns:
            String, the name of the created instance.
        """
        instance = self._compute_client.GenerateInstanceName(
            build_id=self.build_info.build_id,
            build_target=self.build_info.build_target)

        self._compute_client.CreateInstance(
            instance=instance,
            image_name=self._cfg.stable_goldfish_host_image_name,
            image_project=self._cfg.stable_goldfish_host_image_project,
            build_target=self.build_info.build_target,
            branch=self.build_info.branch,
            build_id=self.build_info.build_id,
            emulator_branch=self.emulator_build_info.branch,
            emulator_build_id=self.emulator_build_info.build_id,
            kernel_branch=self.kernel_build_info.branch,
            kernel_build_id=self.kernel_build_info.build_id,
            kernel_build_target=self.kernel_build_info.build_target,
            gpu=self._gpu,
            blank_data_disk_size_gb=self._blank_data_disk_size_gb,
            avd_spec=self._avd_spec,
            tags=self._tags,
            extra_scopes=self._extra_scopes)

        return instance


def ParseBuildInfo(filename):
    """Parse build id based on a substring.

    This will parse a file which contains build information to be used. For an
    emulator build, the file will contain the information about the
    corresponding stable system image build id. In emulator-info.txt, the file
    will contains the information about the corresponding stable emulator
    build id, for example "require version-emulator=5292001". In
    android-info.txt, the file will contains the information about a stable
    system image build id, for example
    "version-sysimage-git_pi-dev-sdk_gphone_x86_64-userdebug=4833817"

    Args:
        filename: Name of file to parse.

    Returns:
        Build id parsed from the file based on pattern
        Returns None if pattern not found in file
    """
    with open(filename) as build_info_file:
        for line in build_info_file:
            match = re.search(_VERSION_PATTERN, line)
            if match:
                return match.group(1)
    return None


def _FetchBuildIdFromFile(cfg, build_target, build_id, filename):
    """Parse and fetch build id from a file based on a pattern.

    Verify if one of the system image or emulator binary build id is missing.
    If found missing, then update according to the resource file.

    Args:
        cfg: An AcloudConfig instance.
        build_target: Target name.
        build_id: Build id, a string, e.g. "2263051", "P2804227"
        filename: Name of file containing the build info.

    Returns:
        A build id or None
    """
    build_client = android_build_client.AndroidBuildClient(
        auth.CreateCredentials(cfg))

    with utils.TempDir() as tempdir:
        temp_filename = os.path.join(tempdir, filename)
        build_client.DownloadArtifact(build_target,
                                      build_id,
                                      filename,
                                      temp_filename)

        return ParseBuildInfo(temp_filename)


#pylint: disable=too-many-locals
def CreateDevices(avd_spec=None,
                  cfg=None,
                  build_target=None,
                  build_id=None,
                  emulator_build_id=None,
                  emulator_branch=None,
                  kernel_build_id=None,
                  kernel_branch=None,
                  kernel_build_target=None,
                  gpu=None,
                  num=1,
                  serial_log_file=None,
                  autoconnect=False,
                  branch=None,
                  tags=None,
                  report_internal_ip=False):
    """Create one or multiple Goldfish devices.

    Args:
        avd_spec: An AVDSpec instance.
        cfg: An AcloudConfig instance.
        build_target: String, the build target, e.g. aosp_x86-eng.
        build_id: String, Build id, e.g. "2263051", "P2804227"
        branch: String, Branch name for system image.
        emulator_build_id: String, emulator build id.
        emulator_branch: String, Emulator branch name.
        gpu: String, GPU to attach to the device or None. e.g. "nvidia-k80"
        kernel_build_id: Kernel build id, a string.
        kernel_branch: Kernel branch name, a string.
        kernel_build_target: Kernel build artifact, a string.
        num: Integer, Number of devices to create.
        serial_log_file: String, A path to a file where serial output should
                        be saved to.
        autoconnect: Boolean, Create ssh tunnel(s) and adb connect after device
                     creation.
        branch: String, Branch name for system image.
        tags: A list of tags to associate with the instance. e.g.
              ["http-server", "https-server"]
        report_internal_ip: Boolean to report the internal ip instead of
                            external ip.

    Returns:
        A Report instance.
    """
    client_adb_port = None
    boot_timeout_secs = None
    if avd_spec:
        cfg = avd_spec.cfg
        build_target = avd_spec.remote_image[constants.BUILD_TARGET]
        build_id = avd_spec.remote_image[constants.BUILD_ID]
        branch = avd_spec.remote_image[constants.BUILD_BRANCH]
        num = avd_spec.num
        emulator_build_id = avd_spec.emulator_build_id
        gpu = avd_spec.gpu
        serial_log_file = avd_spec.serial_log_file
        autoconnect = avd_spec.autoconnect
        report_internal_ip = avd_spec.report_internal_ip
        client_adb_port = avd_spec.client_adb_port
        boot_timeout_secs = avd_spec.boot_timeout_secs

    # If emulator_build_id and emulator_branch is None, retrieve emulator
    # build id from platform build emulator-info.txt artifact
    # Example: require version-emulator=5292001
    if not emulator_build_id and not emulator_branch:
        logger.info("emulator_build_id not provided. "
                    "Try to get %s from build %s/%s.", _EMULATOR_INFO_FILENAME,
                    build_id, build_target)
        emulator_build_id = _FetchBuildIdFromFile(cfg,
                                                  build_target,
                                                  build_id,
                                                  _EMULATOR_INFO_FILENAME)

    if not emulator_build_id:
        raise errors.CommandArgError("Emulator build id not found "
                                     "in %s" % _EMULATOR_INFO_FILENAME)

    # If build_id and branch is None, retrieve build_id from
    # emulator build android-info.txt artifact
    # Example: version-sysimage-git_pi-dev-sdk_gphone_x86_64-userdebug=4833817
    if not build_id and not branch:
        build_id = _FetchBuildIdFromFile(cfg,
                                         cfg.emulator_build_target,
                                         emulator_build_id,
                                         _SYSIMAGE_INFO_FILENAME)

    if not build_id:
        raise errors.CommandArgError("Emulator system image build id not found "
                                     "in %s" % _SYSIMAGE_INFO_FILENAME)
    logger.info(
        "Creating a goldfish device in project %s, build_target: %s, "
        "build_id: %s, emulator_bid: %s, kernel_build_id: %s, "
        "kernel_branch: %s, kernel_build_target: %s, GPU: %s, num: %s, "
        "serial_log_file: %s, "
        "autoconnect: %s", cfg.project, build_target, build_id,
        emulator_build_id, kernel_build_id, kernel_branch, kernel_build_target,
        gpu, num, serial_log_file, autoconnect)

    device_factory = GoldfishDeviceFactory(
        cfg, build_target, build_id,
        cfg.emulator_build_target,
        emulator_build_id, gpu=gpu,
        avd_spec=avd_spec, tags=tags,
        branch=branch,
        emulator_branch=emulator_branch,
        kernel_build_id=kernel_build_id,
        kernel_branch=kernel_branch,
        kernel_build_target=kernel_build_target)

    return common_operations.CreateDevices("create_gf", cfg, device_factory,
                                           num, constants.TYPE_GF,
                                           report_internal_ip, autoconnect,
                                           serial_log_file, client_adb_port,
                                           boot_timeout_secs)
