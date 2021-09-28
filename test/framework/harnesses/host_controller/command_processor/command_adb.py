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

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.utils.usb import usb_utils

from vts.utils.python.common import cmd_utils


class CommandAdb(base_command_processor.BaseCommandProcessor):
    """Command processor for adb command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "adb"
    command_detail = "Runs an ADB command."

    # @Override
    def SetUp(self):
        """Initializes the parser for device command."""
        self.arg_parser.add_argument(
            "--serial",
            "-s",
            default=None,
            help="The target device serial to run the command.")
        self.arg_parser.add_argument(
            "--timeout",
            type=float,
            default=common.DEFAULT_DEVICE_TIMEOUT_SECS,
            help="The maximum timeout value of this command in seconds. "
            "Set to 0 to disable the timeout functionality.")
        self.arg_parser.add_argument(
            "command",
            metavar="COMMAND",
            nargs="+",
            help="The command to be executed. If the command contains "
            "arguments starting with \"-\", place the command at end of line "
            "after \"--\".")

    # @Override
    def Run(self, arg_line):
        """Runs an adb command."""
        args = self.arg_parser.ParseLine(arg_line)
        cmd_list = ["adb"]
        if args.serial:
            if "," in args.serial:
                logging.error("Only one serial can be specified")
                return False
            cmd_list.append("-s %s" % args.serial)
        cmd_list.extend(self.ReplaceVars(args.command))
        if args.timeout == 0:
            stdout, stderr, retcode = cmd_utils.ExecuteOneShellCommand(
                " ".join(cmd_list))
        else:
            stdout, stderr, retcode = cmd_utils.ExecuteOneShellCommand(
                " ".join(cmd_list), args.timeout,
                usb_utils.ResetUsbDeviceOfSerial_Callback, args.serial)
        if stdout:
            logging.info(stdout)
        if stderr:
            logging.error(stderr)
            if self.console.job_pool and args.serial:
                self.console.device_status[
                    args.serial] = common._DEVICE_STATUS_DICT["error"]
        if retcode != 0:
            return False
