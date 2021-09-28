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
import time

from host_controller.command_processor import base_command_processor


class CommandSleep(base_command_processor.BaseCommandProcessor):
    """Command processor for sleep command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "sleep"
    command_detail = "Sleeps for N seconds."

    # @Override
    def SetUp(self):
        """Initializes the parser for device command."""
        self.arg_parser.add_argument(
            "seconds",
            metavar="COMMAND",
            nargs=1,
            help=("an integer indicating the amount of time to sleep "
                  "in seconds"))

    # @Override
    def Run(self, arg_line):
        """Blocks for a specified time interval."""
        args = self.arg_parser.ParseLine(arg_line)
        try:
            time.sleep(int(args.seconds[0]))
        except ValueError as e:
            logging.exception(e)
            return False
