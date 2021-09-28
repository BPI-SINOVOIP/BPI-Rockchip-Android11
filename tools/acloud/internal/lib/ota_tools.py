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
"""OtaTools class."""

import logging
import os
import stat
import subprocess
import tempfile

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import utils

logger = logging.getLogger(__name__)

_BIN_DIR_NAME = "bin"
_LPMAKE = "lpmake"
_BUILD_SUPER_IMAGE = "build_super_image"
_AVBTOOL = "avbtool"
_SGDISK = "sgdisk"
_SIMG2IMG = "simg2img"
_MK_COMBINED_IMG = "mk_combined_img"

_BUILD_SUPER_IMAGE_TIMEOUT_SECS = 30
_AVBTOOL_TIMEOUT_SECS = 30
_MK_COMBINED_IMG_TIMEOUT_SECS = 180

_MISSING_OTA_TOOLS_MSG = ("%(tool_name)s is not found. Run `make otatools` "
                          "in build environment, or set --local-tool to an "
                          "extracted otatools.zip.")


def FindOtaTools(search_paths):
    """Find OTA tools in the search paths and in build environment.

    Args:
        search_paths: List of paths, the directories to search for OTA tools.

    Returns:
        The directory containing OTA tools.

    Raises:
        errors.CheckPathError if OTA tools are not found.
    """
    for search_path in search_paths:
        if os.path.isfile(os.path.join(search_path, _BIN_DIR_NAME,
                                       _BUILD_SUPER_IMAGE)):
            return search_path

    host_out_dir = os.environ.get(constants.ENV_ANDROID_HOST_OUT)
    if (host_out_dir and
            os.path.isfile(os.path.join(host_out_dir, _BIN_DIR_NAME,
                                        _BUILD_SUPER_IMAGE))):
        return host_out_dir

    raise errors.CheckPathError(_MISSING_OTA_TOOLS_MSG %
                                {"tool_name": "OTA tool directory"})


class OtaTools(object):
    """The class that executes OTA tool commands."""

    def __init__(self, ota_tools_dir):
        self._ota_tools_dir = os.path.abspath(ota_tools_dir)

    def _GetBinary(self, name):
        """Get an executable file from _ota_tools_dir.

        Args:
            name: String, the file name.

        Returns:
            String, the absolute path.

        Raises:
            errors.NoExecuteCmd if the file does not exist.
        """
        path = os.path.join(self._ota_tools_dir, _BIN_DIR_NAME, name)
        if not os.path.isfile(path):
            raise errors.NoExecuteCmd(_MISSING_OTA_TOOLS_MSG %
                                      {"tool_name": name})
        mode = os.stat(path).st_mode
        os.chmod(path, mode | (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH |
                               stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH))
        return path

    @staticmethod
    def _ExecuteCommand(*command, **popen_args):
        """Execute a command and log the output.

        This method waits for the process to terminate. It kills the process
        if it's interrupted due to timeout.

        Args:
            command: Strings, the command.
            popen_kwargs: The arguments to be passed to subprocess.Popen.

        Raises:
            errors.SubprocessFail if the process returns non-zero.
        """
        proc = None
        try:
            logger.info("Execute %s", command)
            popen_args["stdin"] = subprocess.PIPE
            popen_args["stdout"] = subprocess.PIPE
            popen_args["stderr"] = subprocess.PIPE
            proc = subprocess.Popen(command, **popen_args)
            stdout, stderr = proc.communicate()
            logger.info("%s stdout: %s", command[0], stdout)
            logger.info("%s stderr: %s", command[0], stderr)

            if proc.returncode != 0:
                raise errors.SubprocessFail("%s returned %d." %
                                            (command[0], proc.returncode))
        finally:
            if proc and proc.poll() is None:
                logger.info("Kill %s", command[0])
                proc.kill()

    @staticmethod
    def _RewriteMiscInfo(output_file, input_file, lpmake_path, get_image):
        """Rewrite lpmake and image paths in misc_info.txt.

        Misc info consists of multiple lines of <key>=<value>.
        Sample input_file:
        lpmake=lpmake
        dynamic_partition_list= system system_ext product vendor

        Sample output_file:
        lpmake=/path/to/lpmake
        dynamic_partition_list= system system_ext product vendor
        system_image=/path/to/system.img
        system_ext_image=/path/to/system_ext.img
        product_image=/path/to/product.img
        vendor_image=/path/to/vendor.img

        This method replaces lpmake with the specified path, and sets
        *_image for every partition in dynamic_partition_list.

        Args:
            output_file: The output file object.
            input_file: The input file object.
            lpmake_path: The path to lpmake binary.
            get_image: A function that takes the partition name as the
                       parameter and returns the image path.
        """
        partition_names = ()
        for line in input_file:
            split_line = line.strip().split("=", 1)
            if len(split_line) < 2:
                split_line = (split_line[0], "")
            if split_line[0] == "dynamic_partition_list":
                partition_names = split_line[1].split()
            elif split_line[0] == "lpmake":
                output_file.write("lpmake=%s\n" % lpmake_path)
                continue
            elif split_line[0].endswith("_image"):
                continue
            output_file.write(line)

        if not partition_names:
            logger.w("No dynamic partition list in misc info.")

        for partition_name in partition_names:
            output_file.write("%s_image=%s\n" %
                              (partition_name, get_image(partition_name)))

    @utils.TimeExecute(function_description="Build super image")
    @utils.TimeoutException(_BUILD_SUPER_IMAGE_TIMEOUT_SECS)
    def BuildSuperImage(self, output_path, misc_info_path, get_image):
        """Use build_super_image to create a super image.

        Args:
            output_path: The path to the output super image.
            misc_info_path: The path to the misc info that provides parameters
                            to create the super image.
            get_image: A function that takes the partition name as the
                       parameter and returns the image path.
        """
        build_super_image = self._GetBinary(_BUILD_SUPER_IMAGE)
        lpmake = self._GetBinary(_LPMAKE)

        new_misc_info_path = None
        try:
            with open(misc_info_path, "r") as misc_info:
                with tempfile.NamedTemporaryFile(
                    prefix="misc_info_", suffix=".txt",
                    delete=False) as new_misc_info:
                    new_misc_info_path = new_misc_info.name
                    self._RewriteMiscInfo(new_misc_info, misc_info, lpmake,
                                          get_image)

            self._ExecuteCommand(build_super_image, new_misc_info_path,
                                 output_path)
        finally:
            if new_misc_info_path:
                os.remove(new_misc_info_path)

    @utils.TimeExecute(function_description="Make disabled vbmeta image.")
    @utils.TimeoutException(_AVBTOOL_TIMEOUT_SECS)
    def MakeDisabledVbmetaImage(self, output_path):
        """Use avbtool to create a vbmeta image with verification disabled.

        Args:
            output_path: The path to the output vbmeta image.
        """
        avbtool = self._GetBinary(_AVBTOOL)
        self._ExecuteCommand(avbtool, "make_vbmeta_image",
                             "--flag", "2",
                             "--padding_size", "4096",
                             "--output", output_path)

    @staticmethod
    def _RewriteSystemQemuConfig(output_file, input_file, get_image):
        """Rewrite image paths in system-qemu-config.txt.

        Sample input_file:
        out/target/product/generic_x86_64/vbmeta.img vbmeta 1
        out/target/product/generic_x86_64/super.img super 2

        Sample output_file:
        /path/to/vbmeta.img vbmeta 1
        /path/to/super.img super 2

        This method replaces the first entry of each line with the path
        returned by get_image.

        Args:
            output_file: The output file object.
            input_file: The input file object.
            get_image: A function that takes the partition name as the
                       parameter and returns the image path.
        """
        for line in input_file:
            split_line = line.split()
            if len(split_line) == 3:
                output_file.write("%s %s %s\n" % (get_image(split_line[1]),
                                                  split_line[1],
                                                  split_line[2]))
            else:
                output_file.write(line)

    @utils.TimeExecute(function_description="Make combined image")
    @utils.TimeoutException(_MK_COMBINED_IMG_TIMEOUT_SECS)
    def MkCombinedImg(self, output_path, system_qemu_config_path, get_image):
        """Use mk_combined_img to create a disk image.

        Args:
            output_path: The path to the output disk image.
            system_qemu_config: The path to the config that provides the
                                parition information on the disk.
            get_image: A function that takes the partition name as the
                       parameter and returns the image path.
        """
        mk_combined_img = self._GetBinary(_MK_COMBINED_IMG)
        sgdisk = self._GetBinary(_SGDISK)
        simg2img = self._GetBinary(_SIMG2IMG)

        new_config_path = None
        try:
            with open(system_qemu_config_path, "r") as config:
                with tempfile.NamedTemporaryFile(
                    prefix="system-qemu-config_", suffix=".txt",
                    delete=False) as new_config:
                    new_config_path = new_config.name
                    self._RewriteSystemQemuConfig(new_config, config,
                                                  get_image)

            mk_combined_img_env = {"SGDISK": sgdisk, "SIMG2IMG": simg2img}
            self._ExecuteCommand(mk_combined_img,
                                 "-i", new_config_path,
                                 "-o", output_path,
                                 env=mk_combined_img_env)
        finally:
            if new_config_path:
                os.remove(new_config_path)
