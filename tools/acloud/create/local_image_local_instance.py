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
r"""LocalImageLocalInstance class.

Create class that is responsible for creating a local instance AVD with a
local image. For launching multiple local instances under the same user,
The cuttlefish tool requires 3 variables:
- ANDROID_HOST_OUT: To locate the launch_cvd tool.
- HOME: To specify the temporary folder of launch_cvd.
- CUTTLEFISH_INSTANCE: To specify the instance id.
Acloud user must either set ANDROID_HOST_OUT or run acloud with --local-tool.
Acloud sets the other 2 variables for each local instance.

The adb port and vnc port of local instance will be decided according to
instance id. The rule of adb port will be '6520 + [instance id] - 1' and the vnc
port will be '6444 + [instance id] - 1'.
e.g:
If instance id = 3 the adb port will be 6522 and vnc port will be 6446.

To delete the local instance, we will call stop_cvd with the environment variable
[CUTTLEFISH_CONFIG_FILE] which is pointing to the runtime cuttlefish json.
"""

import logging
import os
import shutil
import subprocess
import threading
import sys

from acloud import errors
from acloud.create import base_avd_create
from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools
from acloud.list import list as list_instance
from acloud.list import instance
from acloud.public import report


logger = logging.getLogger(__name__)

_CMD_LAUNCH_CVD_ARGS = (" -daemon -cpus %s -x_res %s -y_res %s -dpi %s "
                        "-memory_mb %s -run_adb_connector=%s "
                        "-system_image_dir %s -instance_dir %s")
_CMD_LAUNCH_CVD_DISK_ARGS = (" -blank_data_image_mb %s "
                             "-data_policy always_create")
_CONFIRM_RELAUNCH = ("\nCuttlefish AVD[id:%d] is already running. \n"
                     "Enter 'y' to terminate current instance and launch a new "
                     "instance, enter anything else to exit out[y/N]: ")
_LAUNCH_CVD_TIMEOUT_SECS = 120  # default timeout as 120 seconds
_LAUNCH_CVD_TIMEOUT_ERROR = ("Cuttlefish AVD launch timeout, did not complete "
                             "within %d secs.")
_VIRTUAL_DISK_PATHS = "virtual_disk_paths"


class LocalImageLocalInstance(base_avd_create.BaseAVDCreate):
    """Create class for a local image local instance AVD."""

    @utils.TimeExecute(function_description="Total time: ",
                       print_before_call=False, print_status=False)
    def _CreateAVD(self, avd_spec, no_prompts):
        """Create the AVD.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.
            no_prompts: Boolean, True to skip all prompts.

        Raises:
            errors.LaunchCVDFail: Launch AVD failed.

        Returns:
            A Report instance.
        """
        # Running instances on local is not supported on all OS.
        if not utils.IsSupportedPlatform(print_warning=True):
            result_report = report.Report(command="create")
            result_report.SetStatus(report.Status.FAIL)
            return result_report

        self.PrintDisclaimer()
        local_image_path, host_bins_path = self.GetImageArtifactsPath(avd_spec)

        launch_cvd_path = os.path.join(host_bins_path, "bin",
                                       constants.CMD_LAUNCH_CVD)
        cmd = self.PrepareLaunchCVDCmd(launch_cvd_path,
                                       avd_spec.hw_property,
                                       avd_spec.connect_adb,
                                       local_image_path,
                                       avd_spec.local_instance_id)

        result_report = report.Report(command="create")
        instance_name = instance.GetLocalInstanceName(
            avd_spec.local_instance_id)
        try:
            self.CheckLaunchCVD(
                cmd, host_bins_path, avd_spec.local_instance_id, local_image_path,
                no_prompts, avd_spec.boot_timeout_secs or _LAUNCH_CVD_TIMEOUT_SECS)
        except errors.LaunchCVDFail as launch_error:
            result_report.SetStatus(report.Status.BOOT_FAIL)
            result_report.AddDeviceBootFailure(
                instance_name, constants.LOCALHOST, None, None,
                error=str(launch_error))
            return result_report

        active_ins = list_instance.GetActiveCVD(avd_spec.local_instance_id)
        if active_ins:
            result_report.SetStatus(report.Status.SUCCESS)
            result_report.AddDevice(instance_name, constants.LOCALHOST,
                                    active_ins.adb_port, active_ins.vnc_port)
            # Launch vnc client if we're auto-connecting.
            if avd_spec.connect_vnc:
                utils.LaunchVNCFromReport(result_report, avd_spec, no_prompts)
            if avd_spec.unlock_screen:
                AdbTools(active_ins.adb_port).AutoUnlockScreen()
        else:
            err_msg = "cvd_status return non-zero after launch_cvd"
            logger.error(err_msg)
            result_report.SetStatus(report.Status.BOOT_FAIL)
            result_report.AddDeviceBootFailure(
                instance_name, constants.LOCALHOST, None, None, error=err_msg)

        return result_report

    @staticmethod
    def _FindCvdHostBinaries(search_paths):
        """Return the directory that contains CVD host binaries."""
        for search_path in search_paths:
            if os.path.isfile(os.path.join(search_path, "bin",
                                           constants.CMD_LAUNCH_CVD)):
                return search_path

        host_out_dir = os.environ.get(constants.ENV_ANDROID_HOST_OUT)
        if (host_out_dir and
                os.path.isfile(os.path.join(host_out_dir, "bin",
                                            constants.CMD_LAUNCH_CVD))):
            return host_out_dir

        raise errors.GetCvdLocalHostPackageError(
            "CVD host binaries are not found. Please run `make hosttar`, or "
            "set --local-tool to an extracted CVD host package.")

    def GetImageArtifactsPath(self, avd_spec):
        """Get image artifacts path.

        This method will check if launch_cvd is exist and return the tuple path
        (image path and host bins path) where they are located respectively.
        For remote image, RemoteImageLocalInstance will override this method and
        return the artifacts path which is extracted and downloaded from remote.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.

        Returns:
            Tuple of (local image file, host bins package) paths.
        """
        return (avd_spec.local_image_dir,
                self._FindCvdHostBinaries(avd_spec.local_tool_dirs))

    @staticmethod
    def PrepareLaunchCVDCmd(launch_cvd_path, hw_property, connect_adb,
                            system_image_dir, local_instance_id):
        """Prepare launch_cvd command.

        Create the launch_cvd commands with all the required args and add
        in the user groups to it if necessary.

        Args:
            launch_cvd_path: String of launch_cvd path.
            hw_property: dict object of hw property.
            system_image_dir: String of local images path.
            connect_adb: Boolean flag that enables adb_connector.
            local_instance_id: Integer of instance id.

        Returns:
            String, launch_cvd cmd.
        """
        instance_dir = instance.GetLocalInstanceRuntimeDir(local_instance_id)
        launch_cvd_w_args = launch_cvd_path + _CMD_LAUNCH_CVD_ARGS % (
            hw_property["cpu"], hw_property["x_res"], hw_property["y_res"],
            hw_property["dpi"], hw_property["memory"],
            ("true" if connect_adb else "false"), system_image_dir,
            instance_dir)
        if constants.HW_ALIAS_DISK in hw_property:
            launch_cvd_w_args = (launch_cvd_w_args + _CMD_LAUNCH_CVD_DISK_ARGS %
                                 hw_property[constants.HW_ALIAS_DISK])

        launch_cmd = utils.AddUserGroupsToCmd(launch_cvd_w_args,
                                              constants.LIST_CF_USER_GROUPS)
        logger.debug("launch_cvd cmd:\n %s", launch_cmd)
        return launch_cmd

    def CheckLaunchCVD(self, cmd, host_bins_path, local_instance_id,
                       local_image_path, no_prompts=False,
                       timeout_secs=_LAUNCH_CVD_TIMEOUT_SECS):
        """Execute launch_cvd command and wait for boot up completed.

        1. Check if the provided image files are in use by any launch_cvd process.
        2. Check if launch_cvd with the same instance id is running.
        3. Launch local AVD.

        Args:
            cmd: String, launch_cvd command.
            host_bins_path: String of host package directory.
            local_instance_id: Integer of instance id.
            local_image_path: String of local image directory.
            no_prompts: Boolean, True to skip all prompts.
            timeout_secs: Integer, the number of seconds to wait for the AVD to boot up.
        """
        # launch_cvd assumes host bins are in $ANDROID_HOST_OUT, let's overwrite
        # it to wherever we're running launch_cvd since they could be in a
        # different dir (e.g. downloaded image).
        os.environ[constants.ENV_ANDROID_HOST_OUT] = host_bins_path
        # Check if the instance with same id is running.
        existing_ins = list_instance.GetActiveCVD(local_instance_id)
        if existing_ins:
            if no_prompts or utils.GetUserAnswerYes(_CONFIRM_RELAUNCH %
                                                    local_instance_id):
                existing_ins.Delete()
            else:
                sys.exit(constants.EXIT_BY_USER)
        else:
            # Image files can't be shared among instances, so check if any running
            # launch_cvd process is using this path.
            occupied_ins_id = self.IsLocalImageOccupied(local_image_path)
            if occupied_ins_id:
                utils.PrintColorString(
                    "The image path[%s] is already used by current running AVD"
                    "[id:%d]\nPlease choose another path to launch local "
                    "instance." % (local_image_path, occupied_ins_id),
                    utils.TextColors.FAIL)
                sys.exit(constants.EXIT_BY_USER)

        self._LaunchCvd(cmd, local_instance_id, timeout=timeout_secs)

    @staticmethod
    @utils.TimeExecute(function_description="Waiting for AVD(s) to boot up")
    def _LaunchCvd(cmd, local_instance_id, timeout=None):
        """Execute Launch CVD.

        Kick off the launch_cvd command and log the output.

        Args:
            cmd: String, launch_cvd command.
            local_instance_id: Integer of instance id.
            timeout: Integer, the number of seconds to wait for the AVD to boot up.

        Raises:
            errors.LaunchCVDFail when any CalledProcessError.
        """
        # Delete the cvd home/runtime temp if exist. The runtime folder is
        # under the cvd home dir, so we only delete them from home dir.
        cvd_home_dir = instance.GetLocalInstanceHomeDir(local_instance_id)
        cvd_runtime_dir = instance.GetLocalInstanceRuntimeDir(local_instance_id)
        shutil.rmtree(cvd_home_dir, ignore_errors=True)
        os.makedirs(cvd_runtime_dir)

        cvd_env = os.environ.copy()
        cvd_env[constants.ENV_CVD_HOME] = cvd_home_dir
        cvd_env[constants.ENV_CUTTLEFISH_INSTANCE] = str(local_instance_id)
        # Check the result of launch_cvd command.
        # An exit code of 0 is equivalent to VIRTUAL_DEVICE_BOOT_COMPLETED
        process = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT,
                                   env=cvd_env)
        if timeout:
            timer = threading.Timer(timeout, process.kill)
            timer.start()
        process.wait()
        if timeout:
            timer.cancel()
        if process.returncode == 0:
            return
        raise errors.LaunchCVDFail(
            "Can't launch cuttlefish AVD. Return code:%s. \nFor more detail: "
            "%s/launcher.log" % (str(process.returncode), cvd_runtime_dir))

    @staticmethod
    def PrintDisclaimer():
        """Print Disclaimer."""
        utils.PrintColorString(
            "(Disclaimer: Local cuttlefish instance is not a fully supported\n"
            "runtime configuration, fixing breakages is on a best effort SLO.)\n",
            utils.TextColors.WARNING)

    @staticmethod
    def IsLocalImageOccupied(local_image_dir):
        """Check if the given image path is being used by a running CVD process.

        Args:
            local_image_dir: String, path of local image.

        Return:
            Integer of instance id which using the same image path.
        """
        # TODO(149602560): Remove occupied image checking after after cf disk
        # overlay is stable
        for cf_runtime_config_path in instance.GetAllLocalInstanceConfigs():
            ins = instance.LocalInstance(cf_runtime_config_path)
            if ins.CvdStatus():
                for disk_path in ins.virtual_disk_paths:
                    if local_image_dir in disk_path:
                        return ins.instance_id
        return None
