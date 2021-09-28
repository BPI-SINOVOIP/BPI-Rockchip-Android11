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

import itertools
import logging
import os
import zipfile

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.utils.gcp import gcs_utils
from host_controller.utils.parser import xml_utils

from vts.utils.python.common import cmd_utils

# The command list for cleaning up each devices listed for the retry command.
_DEVICE_CLEANUP_COMMAND_LIST = [
    "adb -s {serial} reboot bootloader",
    "fastboot -s {serial} erase metadata -- -w",
    "fastboot -s {serial} reboot",
    "adb -s {serial} wait-for-device",
    "dut --operation=wifi_on --serial={serial} --ap=" +
    common._DEFAULT_WIFI_AP,
]


class CommandRetry(base_command_processor.BaseCommandProcessor):
    """Command processor for retry command."""

    command = "retry"
    command_detail = "Retry last run test plan for certain times."

    def IsResultZipFile(self, zip_ref):
        """Determines whether the given zip_ref is the right result archive.

        Need to check the number of contents of the zip file since
        the "log-result_<>.zip" file only contains "test_result.xml",
        but cannot parsed by vts-tf properly when trying to do the retry.

        Args:
            zip_ref: ZipFile, reference to the downloaded results_<>.zip file

        Returns:
            True if the downloaded zip file is usable from vts-fs,
            False otherwise.
        """
        if len(zip_ref.namelist()) > 1:
            for name in zip_ref.namelist():
                if common._TEST_RESULT_XML in name:
                    return True
        return False

    def GetResultFromGCS(self, gcs_result_path, local_results_dir):
        """Downloads a vts-tf result zip archive from GCS.

        And unzip the file to "android-vts/results/" path so
        the vts-tf will parse the result correctly.

        Args:
            gcs_result_path: string, path to GCS file.
            local_results_dir: string, abs path to the result directory of
                               currently running vts-tf.
        Returns:
            A string which is the name of unzipped result directory.
            None if the download has failed or the downloaded zip file
            is not a correct result archive.
        """
        gsutil_path = gcs_utils.GetGsutilPath()
        if not gsutil_path:
            logging.error("Please check gsutil is installed and on your PATH")
            return None

        if (not gcs_result_path.startswith("gs://")
                or not gcs_utils.IsGcsFile(gsutil_path, gcs_result_path)):
            logging.error("%s is not correct GCS url.", gcs_result_path)
            return None
        if not gcs_result_path.endswith(".zip"):
            logging.error("%s is not a correct result archive file.",
                          gcs_result_path)
            return None

        if not os.path.exists(local_results_dir):
            os.mkdir(local_results_dir)
        if not gcs_utils.Copy(gsutil_path, gcs_result_path, local_results_dir):
            logging.error("Fail to copy from %s.", gcs_result_path)
            return None
        result_zip = os.path.join(local_results_dir,
                                  gcs_result_path.split("/")[-1])
        with zipfile.ZipFile(result_zip, mode="r") as zip_ref:
            if self.IsResultZipFile(zip_ref):
                unzipped_result_dir = zip_ref.namelist()[0].rstrip("/")
                zip_ref.extractall(local_results_dir)
                return unzipped_result_dir
            else:
                logging.error("Not a correct vts-tf result archive file.")
                return None

    # @Override
    def SetUp(self):
        """Initializes the parser for retry command."""
        self.arg_parser.add_argument(
            "--suite",
            default="vts",
            choices=("vts", "cts", "gts", "sts"),
            help="To specify the type of a test suite to be run.")
        self.arg_parser.add_argument(
            "--count",
            type=int,
            default=common.DEFAULT_RETRY_COUNT,
            help="Retry count. Default retry count is %s." %
            common.DEFAULT_RETRY_COUNT)
        self.arg_parser.add_argument(
            "--force-count",
            type=int,
            default=3,
            help="Forced retry count. Retry certain test plan for the given "
            "times whether all testcases has passed or not.")
        self.arg_parser.add_argument(
            "--result-from-gcs",
            help="Google Cloud Storage URL from which the result is downloaded. "
            "Will retry based on the fetched result data")
        self.arg_parser.add_argument(
            "--serial",
            action="append",
            default=[],
            help="Serial number for device. Can pass this flag multiple times."
        )
        self.arg_parser.add_argument(
            "--shards", type=int, help="Test plan's shard count.")
        self.arg_parser.add_argument(
            "--shard-count",
            type=int,
            help=
            "Test plan's shard count. Same as the \"--shards\" flag but the "
            "value will be passed to the tradefed with \"--shard-count\" flag."
        )
        self.arg_parser.add_argument(
            "--cleanup_devices",
            default=False,
            type=bool,
            help="True to erase metadata and userdata (equivalent to "
            "factory reset) between retries.")
        self.arg_parser.add_argument(
            "--retry_plan", help="The name of a retry plan to use.")

    # @Override
    def Run(self, arg_line):
        """Retry last run plan for certain times."""
        args = self.arg_parser.ParseLine(arg_line)
        retry_count = args.count
        force_retry_count = args.force_count

        if args.suite not in self.console.test_suite_info:
            logging.error("test_suite_info doesn't have '%s': %s", args.suite,
                          self.console.test_suite_info)
            return False

        tools_path = os.path.dirname(self.console.test_suite_info[args.suite])
        results_path = os.path.join(tools_path, common._RESULTS_BASE_PATH)

        unzipped_result_dir = ""
        unzipped_result_session_id = -1
        if args.result_from_gcs:
            unzipped_result_dir = self.GetResultFromGCS(
                args.result_from_gcs, results_path)
            if not unzipped_result_dir:
                return False

        former_results = [
            result for result in os.listdir(results_path)
            if os.path.isdir(os.path.join(results_path, result))
            and not os.path.islink(os.path.join(results_path, result))
        ]
        former_result_count = len(former_results)
        if former_result_count < 1:
            logging.error(
                "No test plan has been run yet, former results count is %d",
                former_result_count)
            return False

        if unzipped_result_dir:
            former_results.sort()
            unzipped_result_session_id = former_results.index(
                unzipped_result_dir)

        for result_index in range(retry_count):
            if unzipped_result_session_id >= 0:
                session_id = unzipped_result_session_id
                unzipped_result_session_id = -1
                latest_result_xml_path = os.path.join(
                    results_path, unzipped_result_dir, common._TEST_RESULT_XML)
            else:
                session_id = former_result_count - 1 + result_index
                latest_result_xml_path = os.path.join(results_path, "latest",
                                                      common._TEST_RESULT_XML)
                if not os.path.exists(latest_result_xml_path):
                    latest_result_xml_path = os.path.join(
                        results_path, former_results[-1],
                        common._TEST_RESULT_XML)

            result_attrs = xml_utils.GetAttributes(
                latest_result_xml_path, common._RESULT_TAG,
                [common._SUITE_PLAN_ATTR_KEY])

            summary_attrs = xml_utils.GetAttributes(
                latest_result_xml_path, common._SUMMARY_TAG, [
                    common._FAILED_ATTR_KEY, common._MODULES_TOTAL_ATTR_KEY,
                    common._MODULES_DONE_ATTR_KEY
                ])

            result_fail_count = int(summary_attrs[common._FAILED_ATTR_KEY])
            result_skip_count = int(
                summary_attrs[common._MODULES_TOTAL_ATTR_KEY]) - int(
                    summary_attrs[common._MODULES_DONE_ATTR_KEY])

            if (result_index >= force_retry_count and result_skip_count == 0
                    and result_fail_count == 0):
                logging.info("All modules have run and passed. "
                             "Skipping remaining %d retry runs.",
                             (retry_count - result_index))
                break

            shard_flag_literal = ""
            if args.shards:
                shard_flag_literal = "--shards"
                shard_num = args.shards
            if args.shard_count:
                shard_flag_literal = "--shard-count"
                shard_num = args.shard_count

            if args.retry_plan:
                retry_plan = args.retry_plan
            else:
                retry_plan = result_attrs[common._SUITE_PLAN_ATTR_KEY]
            if shard_flag_literal:
                retry_test_command = (
                    "test --suite=%s --keep-result -- %s --retry %d %s %d" %
                    (args.suite, retry_plan, session_id, shard_flag_literal,
                     shard_num))
            else:
                retry_test_command = (
                    "test --suite=%s --keep-result -- %s --retry %d" %
                    (args.suite, retry_plan, session_id))
            if args.serial:
                for serial in args.serial:
                    retry_test_command += " --serial %s" % serial

            if args.cleanup_devices:
                for (command, serial) in itertools.product(
                        _DEVICE_CLEANUP_COMMAND_LIST, args.serial):
                    if not self.console.onecmd(command.format(serial=serial)):
                        logging.error(
                            "Factory reset failed on the devices %s. "
                            "Skipping retry run(s)", serial)
                        self.console.device_status[
                            serial] = common._DEVICE_STATUS_DICT["use"]
                        return

            self.console.onecmd(retry_test_command)

            for result in os.listdir(results_path):
                new_result = os.path.join(results_path, result)
                if (os.path.isdir(new_result)
                        and not os.path.islink(new_result)
                        and result not in former_results):
                    former_results.append(result)
                    break

            summary_after_retry = xml_utils.GetAttributes(
                os.path.join(results_path, former_results[-1],
                             common._TEST_RESULT_XML), common._SUMMARY_TAG,
                [
                    common._FAILED_ATTR_KEY, common._MODULES_TOTAL_ATTR_KEY,
                    common._MODULES_DONE_ATTR_KEY
                ])
            fail_count_after_retry = int(
                summary_after_retry[common._FAILED_ATTR_KEY])
            skip_count_after_retry = int(
                summary_after_retry[common._MODULES_TOTAL_ATTR_KEY]) - int(
                    summary_after_retry[common._MODULES_DONE_ATTR_KEY])
            if (result_index >= force_retry_count
                    and fail_count_after_retry == result_fail_count
                    and skip_count_after_retry == result_skip_count):
                logging.warning(
                    "Same result as the former test run from the retry run. "
                    "Skipping remaining %d retry runs.",
                    (retry_count - result_index))
                break
