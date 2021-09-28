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

from host_controller.command_processor import base_command_processor

from vts.utils.python.common import cmd_utils


class CommandShell(base_command_processor.BaseCommandProcessor):
    """Command processor for shell command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "shell"
    command_detail = "Runs a shell command on the host OS."

    # @Override
    def SetUp(self):
        """Initializes the parser for device command."""
        self.arg_parser.add_argument(
            "command",
            metavar="COMMAND",
            nargs="+",
            help="The command to be executed. If the command contains "
            "arguments starting with \"-\", place the command at end of line "
            "after \"--\".")

    # @Override
    def Run(self, arg_line):
        """Runs a shell command."""
        args = self.arg_parser.ParseLine(arg_line)
        cmd_list = self.ReplaceVars(args.command)
        stdout, stderr, retcode = cmd_utils.ExecuteOneShellCommand(
            " ".join(cmd_list))
        if stdout:
            logging.info(stdout)
        if stderr:
            logging.error(stderr)
        if retcode != 0:
            return False
