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
import shutil
import subprocess
import tempfile
import threading
import zipfile

from host_controller.command_processor import base_command_processor
from host_controller.utils.parser import xml_utils
from vts.runners.host import utils


class CommandTest(base_command_processor.BaseCommandProcessor):
    """Command processor for test command.

    Attributes:
        _RESULT_ATTRIBUTES: The attributes of <Result> in the XML report.
                            After test execution, the attributes are loaded
                            from report to console's dictionary.
        _result_dir: the path to the temporary result directory.
    """

    command = "test"
    command_detail = "Executes a command on TF."
    _RESULT_TAG = "Result"
    _RESULT_ATTRIBUTES = ["suite_plan"]

    # @Override
    def SetUp(self):
        """Initializes the parser for test command."""
        self._result_dir = None
        self.arg_parser.add_argument(
            "--suite",
            default="vts",
            choices=("vts", "cts", "gts", "sts"),
            help="To specify the type of a test suite to be run.")
        self.arg_parser.add_argument(
            "--serial",
            "-s",
            default=None,
            help="The target device serial to run the command. "
            "A comma-separate list.")
        self.arg_parser.add_argument(
            "--test-exec-mode",
            default="subprocess",
            help="The target exec model.")
        self.arg_parser.add_argument(
            "--keep-result",
            action="store_true",
            help="Keep the path to the result in the console instance.")
        self.arg_parser.add_argument(
            "command",
            metavar="COMMAND",
            nargs="+",
            help="The command to be executed. If the command contains "
            "arguments starting with \"-\", place the command after "
            "\"--\" at end of line. format: plan -m module -t testcase")

    def _ClearResultDir(self):
        """Deletes all files in the result directory."""
        if self._result_dir is None:
            self._result_dir = tempfile.mkdtemp()
            return

        for file_name in os.listdir(self._result_dir):
            shutil.rmtree(os.path.join(self._result_dir, file_name))

    @staticmethod
    def _GenerateTestSuiteCommand(bin_path, command, serials, result_dir=None):
        """Generates a *ts-tradefed command.

        Args:
            bin_path: the path to *ts-tradefed.
            command: a list of strings, the command arguments.
            serials: a list of strings, the serial numbers of the devices.
            result_dir: the path to the temporary directory where the result is
                        saved.

        Returns:
            a list of strings, the *ts-tradefed command.
        """
        cmd = [bin_path, "run", "commandAndExit"]
        cmd.extend(str(c) for c in command)

        for serial in serials:
            cmd.extend(["-s", str(serial)])

        if result_dir:
            cmd.extend(["--log-file-path", result_dir, "--use-log-saver"])

        return cmd

    @staticmethod
    def _ExecuteCommand(cmd):
        """Executes a command and logs output in real time.

        Args:
            cmd: a list of strings, the command to execute.
        """

        def LogOutputStream(log_level, stream):
            try:
                while True:
                    line = stream.readline()
                    if not line:
                        break
                    logging.log(log_level, line.rstrip())
            finally:
                stream.close()

        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        out_thread = threading.Thread(
            target=LogOutputStream, args=(logging.INFO, proc.stdout))
        err_thread = threading.Thread(
            target=LogOutputStream, args=(logging.ERROR, proc.stderr))
        out_thread.daemon = True
        err_thread.daemon = True
        out_thread.start()
        err_thread.start()
        proc.wait()
        logging.info("Return code: %d", proc.returncode)
        proc.stdin.close()
        out_thread.join()
        err_thread.join()

    # @Override
    def Run(self, arg_line):
        """Executes a command using a *TS-TF instance.

        Args:
            arg_line: string, line of command arguments.
        """
        args = self.arg_parser.ParseLine(arg_line)
        if args.serial:
            serials = args.serial.split(",")
        elif self.console.GetSerials():
            serials = self.console.GetSerials()
        else:
            serials = []

        if args.test_exec_mode == "subprocess":
            if args.suite not in self.console.test_suite_info:
                logging.error("test_suite_info doesn't have '%s': %s",
                              args.suite, self.console.test_suite_info)
                return

            if args.keep_result:
                self._ClearResultDir()
                result_dir = self._result_dir
            else:
                result_dir = None

            cmd = self._GenerateTestSuiteCommand(
                self.console.test_suite_info[args.suite], args.command,
                serials, result_dir)

            logging.info("Command: %s", cmd)
            self._ExecuteCommand(cmd)

            if result_dir:
                result_paths = [
                    os.path.join(dir_name, file_name)
                    for dir_name, file_name in utils.iterate_files(result_dir)
                    if file_name.startswith("log-result")
                    and file_name.endswith(".zip")
                ]

                if len(result_paths) != 1:
                    logging.warning("Unexpected number of results: %s",
                                    result_paths)

                self.console.test_result.clear()
                result = {}
                if len(result_paths) > 0:
                    with zipfile.ZipFile(
                            result_paths[0], mode="r") as result_zip:
                        with result_zip.open(
                                "log-result.xml", mode="rU") as result_xml:
                            result = xml_utils.GetAttributes(
                                result_xml, self._RESULT_TAG,
                                self._RESULT_ATTRIBUTES)
                            if not result:
                                logging.warning("Nothing loaded from report.")
                    result["result_zip"] = result_paths[0]

                result_paths_full = [
                    os.path.join(dir_name, file_name)
                    for dir_name, file_name in utils.iterate_files(result_dir)
                    if file_name.endswith(".zip")
                ]
                result["result_full"] = " ".join(result_paths_full)
                result["suite_name"] = args.suite

                logging.debug(result)
                self.console.test_result.update(result)
        else:
            logging.error("unsupported exec mode: %s", args.test_exec_mode)
            return False

    # @Override
    def TearDown(self):
        """Deletes the result directory."""
        if self._result_dir:
            shutil.rmtree(self._result_dir, ignore_errors=True)
