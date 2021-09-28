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
r"""GoldfishLocalImageLocalInstance class.

Create class that is responsible for creating a local goldfish instance with
local images.

The emulator binary supports two types of environments, Android build system
and SDK. This class runs the emulator in build environment.
- This class uses the prebuilt emulator in ANDROID_EMULATOR_PREBUILTS.
- If the instance requires mixed images, this class uses the OTA tools in
  ANDROID_HOST_OUT.

To run this program outside of a build environment, the following setup is
required.
- One of the local tool directories is an unzipped SDK emulator repository,
  i.e., sdk-repo-<os>-emulator-<build>.zip.
- If the instance doesn't require mixed images, the local image directory
  should be an unzipped SDK image repository, i.e.,
  sdk-repo-<os>-system-images-<build>.zip.
- If the instance requires mixed images, the local image directory should
  contain both the unzipped update package and the unzipped extra image
  package, i.e., <target>-img-<build>.zip and
  emu-extra-<os>-system-images-<build>.zip.
- If the instance requires mixed images, one of the local tool directories
  should be an unzipped OTA tools package, i.e., otatools.zip.
"""

import logging
import os
import shutil
import subprocess
import sys

from acloud import errors
from acloud.create import base_avd_create
from acloud.internal import constants
from acloud.internal.lib import adb_tools
from acloud.internal.lib import ota_tools
from acloud.internal.lib import utils
from acloud.list import instance
from acloud.public import report


logger = logging.getLogger(__name__)

# Input and output file names
_EMULATOR_BIN_NAME = "emulator"
_SDK_REPO_EMULATOR_DIR_NAME = "emulator"
_SYSTEM_IMAGE_NAME = "system.img"
_SYSTEM_QEMU_IMAGE_NAME = "system-qemu.img"
_NON_MIXED_BACKUP_IMAGE_EXT = ".bak-non-mixed"
_BUILD_PROP_FILE_NAME = "build.prop"
_MISC_INFO_FILE_NAME = "misc_info.txt"
_SYSTEM_QEMU_CONFIG_FILE_NAME = "system-qemu-config.txt"

# Partition names
_SYSTEM_PARTITION_NAME = "system"
_SUPER_PARTITION_NAME = "super"
_VBMETA_PARTITION_NAME = "vbmeta"

# Timeout
_DEFAULT_EMULATOR_TIMEOUT_SECS = 150
_EMULATOR_TIMEOUT_ERROR = "Emulator did not boot within %(timeout)d secs."
_EMU_KILL_TIMEOUT_SECS = 20
_EMU_KILL_TIMEOUT_ERROR = "Emulator did not stop within %(timeout)d secs."

_CONFIRM_RELAUNCH = ("\nGoldfish AVD is already running. \n"
                     "Enter 'y' to terminate current instance and launch a "
                     "new instance, enter anything else to exit out[y/N]: ")

_MISSING_EMULATOR_MSG = ("Emulator binary is not found. Check "
                         "ANDROID_EMULATOR_PREBUILTS in build environment, "
                         "or set --local-tool to an unzipped SDK emulator "
                         "repository.")


def _GetImageForLogicalPartition(partition_name, system_image_path, image_dir):
    """Map a logical partition name to an image path.

    Args:
        partition_name: String. On emulator, the logical partitions include
                        "system", "vendor", and "product".
        system_image_path: String. The path to system image.
        image_dir: String. The directory containing the other images.

    Returns:
        system_image_path if the partition is "system".
        Otherwise, this method returns the path under image_dir.

    Raises
        errors.GetLocalImageError if the image does not exist.
    """
    if partition_name == _SYSTEM_PARTITION_NAME:
        image_path = system_image_path
    else:
        image_path = os.path.join(image_dir, partition_name + ".img")
    if not os.path.isfile(image_path):
        raise errors.GetLocalImageError(
            "Cannot find image for logical partition %s" % partition_name)
    return image_path


def _GetImageForPhysicalPartition(partition_name, super_image_path,
                                  vbmeta_image_path, image_dir):
    """Map a physical partition name to an image path.

    Args:
        partition_name: String. On emulator, the physical partitions include
                        "super" and "vbmeta".
        super_image_path: String. The path to super image.
        vbmeta_image_path: String. The path to vbmeta image.
        image_dir: String. The directory containing the other images.

    Returns:
        super_image_path if the partition is "super".
        vbmeta_image_path if the partition is "vbmeta".
        Otherwise, this method returns the path under image_dir.

    Raises:
        errors.GetLocalImageError if the image does not exist.
    """
    if partition_name == _SUPER_PARTITION_NAME:
        image_path = super_image_path
    elif partition_name == _VBMETA_PARTITION_NAME:
        image_path = vbmeta_image_path
    else:
        image_path = os.path.join(image_dir, partition_name + ".img")
    if not os.path.isfile(image_path):
        raise errors.GetLocalImageError(
            "Unexpected physical partition: %s" % partition_name)
    return image_path


class GoldfishLocalImageLocalInstance(base_avd_create.BaseAVDCreate):
    """Create class for a local image local instance emulator."""

    def _CreateAVD(self, avd_spec, no_prompts):
        """Create the AVD.

        Args:
            avd_spec: AVDSpec object that provides the local image directory.
            no_prompts: Boolean, True to skip all prompts.

        Returns:
            A Report instance.

        Raises:
            errors.GetSdkRepoPackageError if emulator binary is not found.
            errors.GetLocalImageError if the local image directory does not
            contain required files.
            errors.CreateError if an instance exists and cannot be deleted.
            errors.CheckPathError if OTA tools are not found.
        """
        if not utils.IsSupportedPlatform(print_warning=True):
            result_report = report.Report(command="create")
            result_report.SetStatus(report.Status.FAIL)
            return result_report

        emulator_path = self._FindEmulatorBinary(avd_spec.local_tool_dirs)
        emulator_path = os.path.abspath(emulator_path)

        image_dir = os.path.abspath(avd_spec.local_image_dir)

        if not (os.path.isfile(os.path.join(image_dir, _SYSTEM_IMAGE_NAME)) or
                os.path.isfile(os.path.join(image_dir,
                                            _SYSTEM_QEMU_IMAGE_NAME))):
            raise errors.GetLocalImageError("No system image in %s." %
                                            image_dir)

        # TODO(b/141898893): In Android build environment, emulator gets build
        # information from $ANDROID_PRODUCT_OUT/system/build.prop.
        # If image_dir is an extacted SDK repository, the file is at
        # image_dir/build.prop. Acloud copies it to
        # image_dir/system/build.prop.
        self._CopyBuildProp(image_dir)

        instance_id = avd_spec.local_instance_id
        inst = instance.LocalGoldfishInstance(instance_id,
                                              avd_flavor=avd_spec.flavor)
        adb = adb_tools.AdbTools(adb_port=inst.adb_port,
                                 device_serial=inst.device_serial)

        self._CheckRunningEmulator(adb, no_prompts)

        instance_dir = inst.instance_dir
        shutil.rmtree(instance_dir, ignore_errors=True)
        os.makedirs(instance_dir)

        extra_args = self._ConvertAvdSpecToArgs(avd_spec, instance_dir)

        logger.info("Instance directory: %s", instance_dir)
        proc = self._StartEmulatorProcess(emulator_path, instance_dir,
                                          image_dir, inst.console_port,
                                          inst.adb_port, extra_args)

        boot_timeout_secs = (avd_spec.boot_timeout_secs or
                             _DEFAULT_EMULATOR_TIMEOUT_SECS)
        result_report = report.Report(command="create")
        try:
            self._WaitForEmulatorToStart(adb, proc, boot_timeout_secs)
        except (errors.DeviceBootTimeoutError, errors.SubprocessFail) as e:
            result_report.SetStatus(report.Status.BOOT_FAIL)
            result_report.AddDeviceBootFailure(inst.name, inst.ip,
                                               inst.adb_port, vnc_port=None,
                                               error=str(e))
        else:
            result_report.SetStatus(report.Status.SUCCESS)
            result_report.AddDevice(inst.name, inst.ip, inst.adb_port,
                                    vnc_port=None)

        if proc.poll() is None:
            inst.WriteCreationTimestamp()

        return result_report

    @staticmethod
    def _MixImages(output_dir, image_dir, system_image_dir, ota):
        """Mix emulator images and a system image into a disk image.

        Args:
            output_dir: The path to the output directory.
            image_dir: The input directory that provides images except
                       system.img.
            system_image_dir: The input directory that provides system.img.
            ota: An instance of ota_tools.OtaTools.

        Returns:
            The path to the mixed disk image in output_dir.
        """
        # Create the super image.
        mixed_super_image_path = os.path.join(output_dir, "mixed_super.img")
        system_image_path = os.path.join(system_image_dir, _SYSTEM_IMAGE_NAME)
        ota.BuildSuperImage(mixed_super_image_path,
                            os.path.join(image_dir, _MISC_INFO_FILE_NAME),
                            lambda partition: _GetImageForLogicalPartition(
                                partition, system_image_path, image_dir))

        # Create the vbmeta image.
        disabled_vbmeta_image_path = os.path.join(output_dir,
                                                  "disabled_vbmeta.img")
        ota.MakeDisabledVbmetaImage(disabled_vbmeta_image_path)

        # Create the disk image.
        combined_image = os.path.join(output_dir, "combined.img")
        ota.MkCombinedImg(combined_image,
                          os.path.join(image_dir,
                                       _SYSTEM_QEMU_CONFIG_FILE_NAME),
                          lambda partition: _GetImageForPhysicalPartition(
                              partition, mixed_super_image_path,
                              disabled_vbmeta_image_path, image_dir))
        return combined_image

    @staticmethod
    def _FindEmulatorBinary(search_paths):
        """Return the path to the emulator binary."""
        # Find in unzipped sdk-repo-*.zip.
        for search_path in search_paths:
            path = os.path.join(search_path, _EMULATOR_BIN_NAME)
            if os.path.isfile(path):
                return path

            path = os.path.join(search_path, _SDK_REPO_EMULATOR_DIR_NAME,
                                _EMULATOR_BIN_NAME)
            if os.path.isfile(path):
                return path

        # Find in build environment.
        prebuilt_emulator_dir = os.environ.get(
            constants.ENV_ANDROID_EMULATOR_PREBUILTS)
        if prebuilt_emulator_dir:
            path = os.path.join(prebuilt_emulator_dir, _EMULATOR_BIN_NAME)
            if os.path.isfile(path):
                return path

        raise errors.GetSdkRepoPackageError(_MISSING_EMULATOR_MSG)

    @staticmethod
    def _IsEmulatorRunning(adb):
        """Check existence of an emulator by sending an empty command.

        Args:
            adb: adb_tools.AdbTools initialized with the emulator's serial.

        Returns:
            Boolean, whether the emulator is running.
        """
        return adb.EmuCommand() == 0

    def _CheckRunningEmulator(self, adb, no_prompts):
        """Attempt to delete a running emulator.

        Args:
            adb: adb_tools.AdbTools initialized with the emulator's serial.
            no_prompts: Boolean, True to skip all prompts.

        Raises:
            errors.CreateError if the emulator isn't deleted.
        """
        if not self._IsEmulatorRunning(adb):
            return
        logger.info("Goldfish AVD is already running.")
        if no_prompts or utils.GetUserAnswerYes(_CONFIRM_RELAUNCH):
            if adb.EmuCommand("kill") != 0:
                raise errors.CreateError("Cannot kill emulator.")
            self._WaitForEmulatorToStop(adb)
        else:
            sys.exit(constants.EXIT_BY_USER)

    @staticmethod
    def _CopyBuildProp(image_dir):
        """Copy build.prop to system/build.prop if it doesn't exist.

        Args:
            image_dir: The directory to find build.prop in.

        Raises:
            errors.GetLocalImageError if build.prop does not exist.
        """
        build_prop_path = os.path.join(image_dir, "system",
                                       _BUILD_PROP_FILE_NAME)
        if os.path.exists(build_prop_path):
            return
        build_prop_src_path = os.path.join(image_dir, _BUILD_PROP_FILE_NAME)
        if not os.path.isfile(build_prop_src_path):
            raise errors.GetLocalImageError("No %s in %s." %
                                            _BUILD_PROP_FILE_NAME, image_dir)
        build_prop_dir = os.path.dirname(build_prop_path)
        logger.info("Copy %s to %s", _BUILD_PROP_FILE_NAME, build_prop_path)
        if not os.path.exists(build_prop_dir):
            os.makedirs(build_prop_dir)
        shutil.copyfile(build_prop_src_path, build_prop_path)

    @staticmethod
    def _ReplaceSystemQemuImg(new_image, image_dir):
        """Replace system-qemu.img in the directory.

        Args:
            new_image: The path to the new image.
            image_dir: The directory containing system-qemu.img.
        """
        system_qemu_img = os.path.join(image_dir, _SYSTEM_QEMU_IMAGE_NAME)
        if os.path.exists(system_qemu_img):
            system_qemu_img_bak = system_qemu_img + _NON_MIXED_BACKUP_IMAGE_EXT
            if not os.path.exists(system_qemu_img_bak):
                # If system-qemu.img.bak-non-mixed does not exist, the
                # system-qemu.img was not created by acloud and should be
                # preserved. The user can restore it by renaming the backup to
                # system-qemu.img.
                logger.info("Rename %s to %s%s.",
                            system_qemu_img, _SYSTEM_QEMU_IMAGE_NAME,
                            _NON_MIXED_BACKUP_IMAGE_EXT)
                os.rename(system_qemu_img, system_qemu_img_bak)
            else:
                # The existing system-qemu.img.bak-non-mixed was renamed by
                # the previous invocation on acloud. The existing
                # system-qemu.img is a mixed image. Acloud removes the mixed
                # image because it is large and not reused.
                os.remove(system_qemu_img)
        try:
            logger.info("Link %s to %s.", system_qemu_img, new_image)
            os.link(new_image, system_qemu_img)
        except OSError:
            logger.info("Fail to link. Copy %s to %s",
                        system_qemu_img, new_image)
            shutil.copyfile(new_image, system_qemu_img)

    def _ConvertAvdSpecToArgs(self, avd_spec, instance_dir):
        """Convert AVD spec to emulator arguments.

        Args:
            avd_spec: AVDSpec object.
            instance_dir: The instance directory for mixed images.

        Returns:
            List of strings, the arguments for emulator command.
        """
        args = []

        if avd_spec.gpu:
            args.extend(("-gpu", avd_spec.gpu))

        if not avd_spec.autoconnect:
            args.append("-no-window")

        if avd_spec.local_system_image_dir:
            mixed_image_dir = os.path.join(instance_dir, "mixed_images")
            os.mkdir(mixed_image_dir)

            image_dir = os.path.abspath(avd_spec.local_image_dir)

            ota_tools_dir = ota_tools.FindOtaTools(avd_spec.local_tool_dirs)
            ota_tools_dir = os.path.abspath(ota_tools_dir)

            mixed_image = self._MixImages(
                mixed_image_dir, image_dir,
                os.path.abspath(avd_spec.local_system_image_dir),
                ota_tools.OtaTools(ota_tools_dir))

            # TODO(b/142228085): Use -system instead of modifying image_dir.
            self._ReplaceSystemQemuImg(mixed_image, image_dir)

            # Unlock the device so that the disabled vbmeta takes effect.
            args.extend(("-qemu", "-append",
                         "androidboot.verifiedbootstate=orange"))

        return args

    @staticmethod
    def _StartEmulatorProcess(emulator_path, working_dir, image_dir,
                              console_port, adb_port, extra_args):
        """Start an emulator process.

        Args:
            emulator_path: The path to emulator binary.
            working_dir: The working directory for the emulator process.
                         The emulator command creates files in the directory.
            image_dir: The directory containing the required images.
                       e.g., composite system.img or system-qemu.img.
            console_port: The console port of the emulator.
            adb_port: The ADB port of the emulator.
            extra_args: List of strings, the extra arguments.

        Returns:
            A Popen object, the emulator process.
        """
        emulator_env = os.environ.copy()
        emulator_env[constants.ENV_ANDROID_PRODUCT_OUT] = image_dir
        # Set ANDROID_TMP for emulator to create AVD info files in.
        emulator_env[constants.ENV_ANDROID_TMP] = working_dir
        # Set ANDROID_BUILD_TOP so that the emulator considers itself to be in
        # build environment.
        if constants.ENV_ANDROID_BUILD_TOP not in emulator_env:
            emulator_env[constants.ENV_ANDROID_BUILD_TOP] = image_dir

        logcat_path = os.path.join(working_dir, "logcat.txt")
        stdouterr_path = os.path.join(working_dir, "stdouterr.txt")
        # The command doesn't create -stdouterr-file automatically.
        with open(stdouterr_path, "w") as _:
            pass

        emulator_cmd = [
            os.path.abspath(emulator_path),
            "-verbose", "-show-kernel", "-read-only",
            "-ports", str(console_port) + "," + str(adb_port),
            "-logcat-output", logcat_path,
            "-stdouterr-file", stdouterr_path
        ]
        emulator_cmd.extend(extra_args)
        logger.debug("Execute %s", emulator_cmd)

        with open(os.devnull, "rb+") as devnull:
            return subprocess.Popen(
                emulator_cmd, shell=False, cwd=working_dir, env=emulator_env,
                stdin=devnull, stdout=devnull, stderr=devnull)

    def _WaitForEmulatorToStop(self, adb):
        """Wait for an emulator to be unavailable on the console port.

        Args:
            adb: adb_tools.AdbTools initialized with the emulator's serial.

        Raises:
            errors.CreateError if the emulator does not stop within timeout.
        """
        create_error = errors.CreateError(_EMU_KILL_TIMEOUT_ERROR %
                                          {"timeout": _EMU_KILL_TIMEOUT_SECS})
        utils.PollAndWait(func=lambda: self._IsEmulatorRunning(adb),
                          expected_return=False,
                          timeout_exception=create_error,
                          timeout_secs=_EMU_KILL_TIMEOUT_SECS,
                          sleep_interval_secs=1)

    def _WaitForEmulatorToStart(self, adb, proc, timeout):
        """Wait for an emulator to be available on the console port.

        Args:
            adb: adb_tools.AdbTools initialized with the emulator's serial.
            proc: Popen object, the running emulator process.
            timeout: Integer, timeout in seconds.

        Raises:
            errors.DeviceBootTimeoutError if the emulator does not boot within
            timeout.
            errors.SubprocessFail if the process terminates.
        """
        timeout_error = errors.DeviceBootTimeoutError(_EMULATOR_TIMEOUT_ERROR %
                                                      {"timeout": timeout})
        utils.PollAndWait(func=lambda: (proc.poll() is None and
                                        self._IsEmulatorRunning(adb)),
                          expected_return=True,
                          timeout_exception=timeout_error,
                          timeout_secs=timeout,
                          sleep_interval_secs=5)
        if proc.poll() is not None:
            raise errors.SubprocessFail("Emulator process returned %d." %
                                        proc.returncode)
