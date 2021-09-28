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

import imp  # Python v2 compatibility
import logging
import os
import re
import subprocess
import zipfile

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.utils.gcp import gcs_utils

from vti.dashboard.proto import TestSuiteResultMessage_pb2 as SuiteResMsg
from vti.test_serving.proto import TestScheduleConfigMessage_pb2 as SchedCfgMsg


class CommandReproduce(base_command_processor.BaseCommandProcessor):
    """Command processor for reproduce command.

    Attributes:
        campaign_common: campaign module. Dynamically imported since
                         the campaign might need to be separated from the
                         host controller itself.
    """

    command = "reproduce"
    command_detail = ("Reproduce the test environment for a pre-run test and "
                      "execute the tradefed command prompt of the fetched "
                      "test suite. Setup the test env "
                      "(fetching, flashing devices, etc) and retrieve "
                      "formerly run test result to retry on, if the path "
                      "to the report protobuf file is given.")

    # @Override
    def SetUp(self):
        """Initializes the parser for reproduce command."""
        self.campaign_common = None
        self.arg_parser.add_argument(
            "--suite",
            default="vts",
            choices=("vts", "cts", "gts", "sts"),
            help="To specify the type of a test suite to be run.")
        self.arg_parser.add_argument(
            "--report_path",
            required=True,
            help="Google Cloud Storage URL, the path of a report protobuf file."
        )
        self.arg_parser.add_argument(
            "--serial",
            default=None,
            help="The serial numbers for flashing and testing. "
            "Multiple serial numbers are separated by commas.")
        self.arg_parser.add_argument(
            "--automated_retry",
            action="store_true",
            help="Retries automatically until all test cases are passed "
            "or the number or the failed test cases is the same as "
            "the previous one.")

    # @Override
    def Run(self, arg_line):
        """Reproduces the test env of the pre-run test."""
        args = self.arg_parser.ParseLine(arg_line)

        if args.report_path:
            gsutil_path = gcs_utils.GetGsutilPath()
            if not gsutil_path:
                logging.error(
                    "Please check whether gsutil is installed and on your PATH"
                )
                return False

            if (not args.report_path.startswith("gs://")
                    or not gcs_utils.IsGcsFile(gsutil_path, args.report_path)):
                logging.error("%s is not a valid GCS path.", args.report_path)
                return False

            dest_path = os.path.join("".join(self.ReplaceVars(["{tmp_dir}"])),
                                     os.path.basename(args.report_path))
            gcs_utils.Copy(gsutil_path, args.report_path, dest_path)
            report_msg = SuiteResMsg.TestSuiteResultMessage()
            try:
                with open(dest_path, "r") as report_fd:
                    report_msg.ParseFromString(report_fd.read())
            except IOError as e:
                logging.exception(e)
                return False
            serial = []
            if args.serial:
                serial = args.serial.split(",")
            setup_command_list = self.GenerateSetupCommands(report_msg, serial)
            if not setup_command_list:
                suite_fetch_command = self.GenerateTestSuiteFetchCommand(
                    report_msg)
                if suite_fetch_command:
                    setup_command_list.append(suite_fetch_command)
            for command in setup_command_list:
                self.console.onecmd(command)

            if not self.GetResultFromGCS(gsutil_path, report_msg, args.suite):
                return False
        else:
            logging.error("Path to a report protobuf file is required.")
            return False

        if args.suite not in self.console.test_suite_info:
            logging.error("test_suite_info doesn't have '%s': %s", args.suite,
                          self.console.test_suite_info)
            return False

        if args.automated_retry:
            if self.campaign_common is None:
                self.campaign_common = imp.load_source(
                    'campaign_common',
                    os.path.join(os.getcwd(), "host_controller", "campaigns",
                                 "campaign_common.py"))
            retry_command = self.campaign_common.GenerateRetryCommand(
                report_msg.schedule_config.build_target[0].name,
                report_msg.branch, report_msg.suite_name.lower(),
                report_msg.suite_plan, serial)
            self.console.onecmd(retry_command)
        else:
            subprocess.call(self.console.test_suite_info[args.suite])

    def GenerateSetupCommands(self, report_msg, serial):
        """Generates fetch, flash commands using fetch info from report_msg.

        Args:
            report_msg: pb2, contains fetch info of the test suite.
            serial: list of string, serial number(s) of the device(s)
                    to be flashed.

        Returns:
            list of string, console commands to fetch device/gsi images
            and flash the device(s).
        """
        ret = []
        schedule_config = report_msg.schedule_config
        if not schedule_config.manifest_branch:
            logging.error("Report contains no fetch information. "
                          "Aborting pre-test setups on the device(s).")
        elif not serial:
            logging.error("Device serial number(s) not given. "
                          "Aborting pre-test setups on the device(s).")
        else:
            try:
                build_target_msg = schedule_config.build_target[0]
                test_schedule_msg = build_target_msg.test_schedule[0]
            except IndexError as e:
                logging.exception(e)
                return ret
            kwargs = {}
            # common fetch info
            kwargs["shards"] = str(len(serial))
            kwargs["test_name"] = "%s/%s" % (report_msg.suite_name.lower(),
                                             report_msg.suite_plan)
            kwargs["serial"] = serial

            # fetch info for device images
            kwargs["manifest_branch"] = schedule_config.manifest_branch
            kwargs["build_target"] = build_target_msg.name
            kwargs["build_id"] = report_msg.vendor_build_id
            kwargs["pab_account_id"] = schedule_config.pab_account_id
            if kwargs["manifest_branch"].startswith("gs://"):
                kwargs[
                    "build_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_GCS
            else:
                kwargs[
                    "build_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_PAB
            kwargs["require_signed_device_build"] = (
                build_target_msg.require_signed_device_build)
            kwargs["has_bootloader_img"] = build_target_msg.has_bootloader_img
            kwargs["has_radio_img"] = build_target_msg.has_radio_img

            # fetch info for gsi images and gsispl command
            kwargs["gsi_branch"] = test_schedule_msg.gsi_branch
            kwargs["gsi_build_target"] = test_schedule_msg.gsi_build_target
            kwargs["gsi_build_id"] = report_msg.gsi_build_id
            kwargs["gsi_pab_account_id"] = test_schedule_msg.gsi_pab_account_id
            if kwargs["gsi_branch"].startswith("gs://"):
                kwargs["gsi_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_GCS
            else:
                kwargs["gsi_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_PAB
            kwargs["gsi_vendor_version"] = test_schedule_msg.gsi_vendor_version

            # fetch info for test suite
            kwargs["test_build_id"] = report_msg.build_id
            kwargs["test_branch"] = report_msg.branch
            kwargs["test_build_target"] = report_msg.target
            if kwargs["test_build_target"].endswith(".zip"):
                kwargs["test_build_target"] = kwargs["test_build_target"][:-4]
            kwargs[
                "test_pab_account_id"] = test_schedule_msg.test_pab_account_id
            if kwargs["test_branch"].startswith("gs://"):
                kwargs[
                    "test_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_GCS
            else:
                kwargs["gsi_storage_type"] = SchedCfgMsg.BUILD_STORAGE_TYPE_PAB

            self.campaign_common = imp.load_source(
                "campaign_common",
                os.path.join(os.getcwd(), "host_controller", "campaigns",
                             "campaign_common.py"))
            fetch_commands_result, gsi = self.campaign_common.EmitFetchCommands(
                **kwargs)
            ret.extend(fetch_commands_result)
            flash_commands_result = self.campaign_common.EmitFlashCommands(
                gsi, **kwargs)
            ret.extend(flash_commands_result)

        return ret

    def GenerateTestSuiteFetchCommand(self, report_msg):
        """Generates a fetch command line using fetch info from report_msg.

        Args:
            report_msg: pb2, contains fetch info of the test suite.

        Returns:
            string, console command to fetch a test suite artifact.
        """
        ret = "fetch"

        if report_msg.branch.startswith("gs://"):
            ret += " --type=gcs --path=%s/%s --set_suite_as=%s" % (
                report_msg.branch, report_msg.target,
                report_msg.suite_name.lower())
        else:
            ret += (" --type=pab --branch=%s --target=%s --build_id=%s"
                    " --artifact_name=android-%s.zip") % (
                        report_msg.branch, report_msg.target,
                        report_msg.build_id, report_msg.suite_name.lower())
            try:
                build_target_msg = report_msg.schedule_config.build_target[0]
                test_schedule_msg = build_target_msg.test_schedule[0]
            except IndexError as e:
                logging.exception(e)
                test_schedule_msg = SchedCfgMsg.TestScheduleConfigMessage()
            if test_schedule_msg.test_pab_account_id:
                ret += " --account_id=%s" % test_schedule_msg.test_pab_account_id
            else:
                ret += " --account_id=%s" % common._DEFAULT_ACCOUNT_ID_INTERNAL

        return ret

    def GetResultFromGCS(self, gsutil_path, report_msg, suite):
        """Downloads results.zip from GCS and unzip it to the results directory.

        Args:
            gsutil_path: string, path to the gsutil binary.
            report_msg: pb2, contains fetch info of the test suite.
            suite: string, specifies the type of test suite fetched.

        Returns:
            True if successful. False otherwise
        """
        result_base_path = report_msg.result_path
        try:
            tools_path = os.path.dirname(self.console.test_suite_info[suite])
        except KeyError as e:
            logging.exception(e)
            return False
        local_results_path = os.path.join(tools_path,
                                          common._RESULTS_BASE_PATH)

        if not os.path.exists(local_results_path):
            os.mkdir(local_results_path)

        result_path_list = gcs_utils.List(gsutil_path, result_base_path)
        result_zip_url = ""
        for result_path in result_path_list:
            if re.match(".*results_.*\.zip", result_path):
                result_zip_url = result_path
                break

        if (not result_zip_url or not gcs_utils.Copy(
                gsutil_path, result_zip_url, local_results_path)):
            logging.error("Fail to copy from %s.", result_base_path)
            return False

        result_zip_local_path = os.path.join(local_results_path,
                                             os.path.basename(result_zip_url))
        with zipfile.ZipFile(result_zip_local_path, mode="r") as zip_ref:
            zip_ref.extractall(local_results_path)

        return True
