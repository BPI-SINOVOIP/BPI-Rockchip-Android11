#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import datetime
import logging
import os
import shutil
import tempfile
import zipfile

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.utils.gsi import img_utils

from vts.utils.python.common import cmd_utils


class CommandGsispl(base_command_processor.BaseCommandProcessor):
    """Command processor for gsispl command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "gsispl"
    command_detail = "Changes security patch level on a selected GSI file."

    # @Override
    def SetUp(self):
        """Initializes the parser for device command."""
        self.arg_parser.add_argument(
            "--gsi",
            help="Path to GSI image to change security patch level. "
            "If path is not given, the most recently fetched system.img "
            "kept in device_image_info dictionary is used and then "
            "device_image_info will be updated with the new GSI file.")
        self.arg_parser.add_argument(
            "--version", help="New version ID. It should be YYYY-mm-dd format")
        self.arg_parser.add_argument(
            "--version_from_path",
            help="Path to vendor provided image file to retrieve SPL version. "
            "If just a file name is given, the most recently fetched .img "
            "file will be used.")
        self.arg_parser.add_argument(
            "--vendor_version",
            help="The version of vendor.img that will be used (e.g., 8.1.0).")

    # @Override
    def Run(self, arg_line):
        """Changes security patch level on a selected GSI file."""
        args = self.arg_parser.ParseLine(arg_line)
        if args.gsi:
            if os.path.isfile(args.gsi):
                gsi_path = args.gsi
            else:
                logging.error("Cannot find system image in given path")
                return
        elif "system.img" in self.console.device_image_info:
            gsi_path = self.console.device_image_info["system.img"]
        else:
            logging.error("Cannot find system image.")
            return False

        if args.version:
            try:
                version_date = datetime.datetime.strptime(
                    args.version, "%Y-%m-%d")
                version = "{:04d}-{:02d}-{:02d}".format(
                    version_date.year, version_date.month, version_date.day)
            except ValueError as e:
                logging.error("version ID should be YYYY-mm-dd format.")
                return
        elif args.version_from_path:
            dest_path = None
            if os.path.isabs(args.version_from_path) and os.path.exists(
                    args.version_from_path):
                img_path = args.version_from_path
            elif args.version_from_path in self.console.device_image_info:
                img_path = self.console.device_image_info[
                    args.version_from_path]
            elif (args.version_from_path == "boot.img"
                  and "full-zipfile" in self.console.device_image_info):
                tempdir_base = os.path.join(os.getcwd(), "tmp")
                if not os.path.exists(tempdir_base):
                    os.mkdir(tempdir_base)
                dest_path = tempfile.mkdtemp(dir=tempdir_base)

                with zipfile.ZipFile(
                        self.console.device_image_info["full-zipfile"],
                        'r') as zip_ref:
                    zip_ref.extractall(dest_path)
                    img_path = os.path.join(dest_path, "boot.img")
                    if not os.path.exists(img_path):
                        logging.error("No %s file in device img .zip.",
                                      args.version_from_path)
                        shutil.rmtree(dest_path)
                        return
            else:
                logging.error("Cannot find %s file.", args.version_from_path)
                return False

            version_dict = img_utils.GetSPLVersionFromBootImg(img_path)
            if dest_path:
                shutil.rmtree(dest_path)
            if "year" in version_dict and "month" in version_dict:
                version = "{:04d}-{:02d}-{:02d}".format(
                    version_dict["year"], version_dict["month"],
                    common._SPL_DEFAULT_DAY)
            else:
                logging.error("Failed to fetch SPL version from %s file.",
                              img_path)
                return False
        else:
            logging.error("version ID or path of .img file must be given.")
            return False

        output_path = os.path.join(
            os.path.dirname(os.path.abspath(gsi_path)),
            "system-{}.img".format(version))
        command = "{} {} {} {}".format(
            os.path.join(os.getcwd(), "..", "bin",
                         "change_security_patch_ver.sh"), gsi_path,
            output_path, version)
        if args.vendor_version:
            command = command + " -v " + args.vendor_version
        if self.console.password.value:
            command = "echo {} | sudo -S {}".format(
                self.console.password.value, command)
        stdout, stderr, err_code = cmd_utils.ExecuteOneShellCommand(command)
        if err_code is 0:
            if not args.gsi:
                logging.info(
                    "system.img path is updated to : {}".format(output_path))
                self.console.device_image_info["system.img"] = output_path
        else:
            logging.error("gsispl error: {} {}".format(stdout, stderr))
            return False
