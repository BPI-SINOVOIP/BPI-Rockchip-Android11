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

import os
import datetime
import logging
import stat
import threading
import zipfile

from host_controller import common
from host_controller.command_processor import base_command_processor

_REPACKAGE_ADDITIONAL_FILE_LIST = [
    "android-vtslab/testcases/DATA/app/WifiUtil/WifiUtil.apk",
    "android-vtslab/testcases/DATA/vtslab-gcs.json",
    "android-vtslab/testcases/DATA/xml/media_profiles_vendor.xml",
    "android-vtslab/testcases/host_controller/build/client_secrets.json",
    "android-vtslab/testcases/host_controller/build/credentials",
]

_REPACKAGE_ADDITIONAL_BIN_LIST = [
    "android-vtslab/bin/adb",
]

# Path to the version.txt file in the fetched vtslab package zip.
_VERSION_INFO_FILE_PATH = "android-vtslab/testcases/version.txt"

# List of strings for supported ak versions.
AK_VERSIONS = ["8.0.0", "8.0.1", "8.1.0", "9", "O", "OMR1", "P", "Q"]

for version in AK_VERSIONS:
    file_path = "android-vtslab/testcases/DATA/ak/.%s.ak" % version
    _REPACKAGE_ADDITIONAL_FILE_LIST.append(file_path)
    file_path += ".pub"
    _REPACKAGE_ADDITIONAL_FILE_LIST.append(file_path)


class CommandRelease(base_command_processor.BaseCommandProcessor):
    """Command processor for update command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
        _timers: dict, instances of scheduled threading.Timer.
                 Uses timestamp("%H:%M") string as a key.
        _vtslab_package_version: string, version information of the fetched
                                 vtslab package.
                                 (<git commit timestamp>:<git commit hash value>)
    """

    command = "release"
    command_detail = "Release HC. Used for fetching HC package from PAB and uploading to GCS."

    # @Override
    def SetUp(self):
        """Initializes the parser for update command."""
        self._timers = {}
        self._vtslab_package_version = ""
        self.arg_parser.add_argument(
            "--schedule-for",
            default="17:00",
            help="Schedule to update HC package at the given time every day. "
            "Example: --schedule-for=%%H:%%M")
        self.arg_parser.add_argument(
            "--account_id",
            default=common._DEFAULT_ACCOUNT_ID,
            help="Partner Android Build account_id to use.")
        self.arg_parser.add_argument(
            "--branch", help="Branch to grab the artifact from.")
        self.arg_parser.add_argument(
            "--target",
            help="a comma-separate list of build target product(s).")
        self.arg_parser.add_argument(
            "--dest",
            help="Google Cloud Storage URL to which the file is uploaded.")
        self.arg_parser.add_argument(
            "--cancel", help="Cancel all scheduled release if given.")
        self.arg_parser.add_argument(
            "--print-all", help="Print all scheduled timers.")
        self.arg_parser.add_argument(
            "--additional_files_bucket",
            default="gs://vtslab-release",
            help="GCS bucket URL from where to fetch the additional files "
            "required for HC to run properly.")

    # @Override
    def Run(self, arg_line):
        """Schedule a host_constroller package release at a certain time."""
        args = self.arg_parser.ParseLine(arg_line)

        if args.print_all:
            logging.info(self._timers)
            return

        if not args.cancel:
            if args.schedule_for == "now":
                self.ReleaseCallback(args.schedule_for, args.account_id,
                                     args.branch, args.target, args.dest,
                                     args.additional_files_bucket)
                return

            elif len(args.schedule_for.split(":")) != 2:
                logging.error("The format of --schedule-for flag is %H:%M")
                return False

            if (int(args.schedule_for.split(":")[0]) not in range(24)
                    or int(args.schedule_for.split(":")[-1]) not in range(60)):
                logging.error("The value of --schedule-for flag must be in "
                              "\"00:00\"..\"23:59\" inclusive")
                return False

            if not args.schedule_for in self._timers:
                delta_time = datetime.datetime.now().replace(
                    hour=int(args.schedule_for.split(":")[0]),
                    minute=int(args.schedule_for.split(":")[-1]),
                    second=0,
                    microsecond=0) - datetime.datetime.now()

                if delta_time <= datetime.timedelta(0):
                    delta_time += datetime.timedelta(days=1)

                self._timers[args.schedule_for] = threading.Timer(
                    delta_time.total_seconds(), self.ReleaseCallback,
                    (args.schedule_for, args.account_id, args.branch,
                     args.target, args.dest, args.additional_files_bucket))
                self._timers[args.schedule_for].daemon = True
                self._timers[args.schedule_for].start()
                logging.info("Release job scheduled for {}".format(
                    datetime.datetime.now() + delta_time))
        else:
            self.CancelAllEvents()

    def FetchVtslab(self, account_id, branch, target, bucket):
        """Fetchs android-vtslab.zip and return the fetched file path.

        Args:
            account_id: string, Partner Android Build account_id to use.
            branch: string, branch to grab the artifact from.
            targets: string, a comma-separate list of build target product(s).
            bucket: string, GCS bucket URL from where to fetch the additional
                    files.

        Returns:
            path to the fetched android-vtslab.zip file. None if the fetching
            has failed.
        """
        self.console.build_provider["pab"].Authenticate()
        fetched_path = self.console.build_provider[
            "pab"].FetchLatestBuiltHCPackage(account_id, branch, target)

        with zipfile.ZipFile(fetched_path, mode="a") as vtslab_package:
            if _VERSION_INFO_FILE_PATH in vtslab_package.namelist():
                self._vtslab_package_version = vtslab_package.open(
                    _VERSION_INFO_FILE_PATH).readline().strip()
            else:
                self._vtslab_package_version = ""

            for path in _REPACKAGE_ADDITIONAL_FILE_LIST:
                additional_file = os.path.join(bucket, path)
                self.console.build_provider["gcs"].Fetch(additional_file)
                try:
                    logging.info("Adding file %s into %s" %
                                 (os.path.basename(path),
                                  os.path.basename(fetched_path)))
                    additional_file_path = self.console.build_provider[
                        "gcs"].GetAdditionalFile(os.path.basename(path))
                    vtslab_package.write(additional_file_path, path)
                except KeyError as e:
                    logging.exception(e)

            for bin in _REPACKAGE_ADDITIONAL_BIN_LIST:
                additional_bin = os.path.join(bucket, bin)
                self.console.build_provider["gcs"].Fetch(additional_bin)
                try:
                    logging.info("Adding executable %s into %s" %
                                 (os.path.basename(bin),
                                  os.path.basename(fetched_path)))
                    additional_bin_path = self.console.build_provider[
                        "gcs"].GetAdditionalFile(os.path.basename(bin))
                    bin_mode = os.stat(additional_bin_path).st_mode
                    os.chmod(
                        additional_bin_path,
                        bin_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
                    vtslab_package.write(additional_bin_path, bin)
                except KeyError as e:
                    logging.exception(e)

        return fetched_path

    def UploadVtslab(self, package_file_path, dest_path):
        """upload repackaged vtslab package to GCS.

        Args:
            package_file_path: string, path to the vtslab package file.
            dest_path: string, URL to GCS.
        """
        if dest_path and dest_path.endswith("/"):
            split_list = os.path.basename(package_file_path).split(".")
            if self._vtslab_package_version:
                try:
                    timestamp, hash = self._vtslab_package_version.split(":")
                    split_list[0] += "-%s-%s" % (timestamp, hash)
                except ValueError as e:
                    logging.exception(e)
                    split_list[0] += "-{timestamp_date}"
            else:
                split_list[0] += "-{timestamp_date}"
            dest_path += ".".join(split_list)

        upload_command = "upload --src %s --dest %s" % (package_file_path,
                                                        dest_path)
        self.console.onecmd(upload_command)

    def ReleaseCallback(self, schedule_for, account_id, branch, target, dest,
                        bucket):
        """Target function for the scheduled Timer.

        Args:
            schedule_for: string, scheduled time for this Timer.
                          Format: "%H:%M" (from "00:00" to  "23:59" inclusive)
            account_id: string, Partner Android Build account_id to use.
            branch: string, branch to grab the artifact from.
            targets: string, a comma-separate list of build target product(s).
            dest: string, URL to GCS.
            bucket: string, GCS bucket URL from where to fetch the additional
                    files.
        """
        fetched_path = self.FetchVtslab(account_id, branch, target, bucket)
        if fetched_path:
            self.UploadVtslab(fetched_path, dest)

        if schedule_for != "now":
            delta_time = datetime.datetime.now().replace(
                hour=int(schedule_for.split(":")[0]),
                minute=int(schedule_for.split(":")[-1]),
                second=0,
                microsecond=0) - datetime.datetime.now() + datetime.timedelta(
                    days=1)
            self._timers[schedule_for] = threading.Timer(
                delta_time.total_seconds(), self.ReleaseCallback,
                (schedule_for, account_id, branch, target, dest, bucket))
            self._timers[schedule_for].daemon = True
            self._timers[schedule_for].start()
            logging.info("Release job scheduled for {}".format(
                datetime.datetime.now() + delta_time))

    def CancelAllEvents(self):
        """Cancel all scheduled Timer."""
        for scheduled_time in self._timers:
            self._timers[scheduled_time].cancel()
        self._timers = {}
