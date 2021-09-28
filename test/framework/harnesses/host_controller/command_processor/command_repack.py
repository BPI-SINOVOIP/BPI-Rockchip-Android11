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

import logging
import os
import re
import shutil
import tempfile
import zipfile

from host_controller import common
from host_controller.command_processor import base_command_processor

# Name of android-info.txt file which contains prerequisite data for the img.zip
_ANDROID_INFO_TXT_FILENAME = "android-info.txt"


class CommandRepack(base_command_processor.BaseCommandProcessor):
    """Command processor for repack command."""

    command = "repack"
    command_detail = ("Repackage the whole device image files, including GSI "
                      "images if exist.")

    # @Override
    def SetUp(self):
        """Initializes the parser for repack command."""
        self.arg_parser.add_argument(
            "--dest",
            default="gs://vts-release/img_package",
            help="Google Cloud Storage base URL to which the file is uploaded."
        )
        self.arg_parser.add_argument(
            "--additional_files",
            nargs="*",
            default=[],
            help="Additional files that need to be added to the zip file.")

    # @Override
    def Run(self, arg_line):
        """Runs an repack command."""
        args = self.arg_parser.ParseLine(arg_line)
        try:
            device_zipfile_path = self.console.device_image_info[
                common.FULL_ZIPFILE]
        except KeyError as e:
            logging.exception(e)
            logging.error(
                "please execute this command after fetching at least one "
                "device img set.")
            return False

        with zipfile.ZipFile(device_zipfile_path, 'r') as zip_ref:
            zip_ref.extract(_ANDROID_INFO_TXT_FILENAME,
                            common.FULL_ZIPFILE_DIR)
            self.console.tools_info[_ANDROID_INFO_TXT_FILENAME] = os.path.join(
                common.FULL_ZIPFILE_DIR, _ANDROID_INFO_TXT_FILENAME)

        tempdir_base = os.path.join(os.getcwd(), "tmp")
        tmpdir_rezip = tempfile.mkdtemp(dir=tempdir_base)

        dest_url_base, new_zipfile_name = os.path.split(
            self.GetDestURL(args.dest))

        new_zipfile_path = os.path.join(tmpdir_rezip, new_zipfile_name)
        with zipfile.ZipFile(
                new_zipfile_path, "w", allowZip64=True) as zip_ref:
            for img_path in self.console.device_image_info:
                if img_path not in (common.FULL_ZIPFILE,
                                    common.FULL_ZIPFILE_DIR,
                                    common.GSI_ZIPFILE,
                                    common.GSI_ZIPFILE_DIR):
                    logging.info("Adding %s into the zip archive.", img_path)
                    zip_ref.write(
                        self.console.device_image_info[img_path],
                        img_path,
                        compress_type=zipfile.ZIP_DEFLATED)
            if args.additional_files:
                additional_file_list = self.ReplaceVars(args.additional_files)
                for file_path in additional_file_list:
                    file_name = os.path.basename(file_path)
                    logging.info(
                        "Adding additional file %s into the zip archive.",
                        file_name)
                    zip_ref.write(
                        file_path,
                        os.path.join(common._ADDITIONAL_FILES_DIR, file_name),
                        compress_type=zipfile.ZIP_DEFLATED)
            zip_ref.write(
                self.console.tools_info[_ANDROID_INFO_TXT_FILENAME],
                _ANDROID_INFO_TXT_FILENAME,
                compress_type=zipfile.ZIP_DEFLATED)

        logging.info("Repackaged image set: %s", new_zipfile_path)
        logging.info("Uploading %s to %s.", new_zipfile_name, dest_url_base)

        self.console.onecmd("upload --src=%s --dest=%s/%s" %
                            (new_zipfile_path, dest_url_base,
                             new_zipfile_name))

        shutil.rmtree(tmpdir_rezip, ignore_errors=True)

    def GetDestURL(self, dest_base_url):
        """Generates the destination URL to GCS bucket based on dest_base_url.

        Args:
            dest_base_url: string, URL to a GCS bucket.

        Returns:
            A string, device/gsi img sets branch/target info and the final
            .zip file name appended to the dest_base_url.
        """
        device_branch = re.sub(
            "git_", "", self.console.detailed_fetch_info["device"]["branch"])
        device_target = self.console.detailed_fetch_info["device"]["target"]
        device_build_id = self.console.detailed_fetch_info["device"][
            "build_id"]
        new_zipfile_name = ("%s_%s_%s.zip" % (device_branch, device_target,
                                              device_build_id))
        dest_url_base = os.path.join(dest_base_url, device_branch,
                                     device_target)

        if common.GSI_ZIPFILE in self.console.device_image_info:
            gsi_branch = re.sub(
                "git_", "", self.console.detailed_fetch_info["gsi"]["branch"])
            gsi_target = self.console.detailed_fetch_info["gsi"]["target"]
            gsi_build_id = self.console.detailed_fetch_info["gsi"]["build_id"]
            new_zipfile_name = new_zipfile_name[:-4] + ("_%s_%s_%s.zip" % (
                gsi_branch, gsi_target, gsi_build_id))
            dest_url_base = os.path.join(dest_url_base, gsi_branch, gsi_target)

        ret = os.path.join(dest_url_base, new_zipfile_name)
        self.console.repack_dest_path = ret
        return ret
