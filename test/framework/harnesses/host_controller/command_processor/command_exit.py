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


class CommandExit(base_command_processor.BaseCommandProcessor):
    """Command processor for exit command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "exit"
    command_detail = ""

    # @Override
    def SetUp(self):
        """Initializes the parser for request command."""
        self.arg_parser.add_argument(
            "--wait_for_jobs",
            default=True,
            help="True to wait for the running jobs to complete before exiting."
        )

    # @Override
    def Run(self, arg_line):
        """Terminates the console.

        Returns:
            True, which stops the cmdloop.
        """
        args = self.arg_parser.ParseLine(arg_line)

        self.console.onecmd("device --update stop")

        if args.wait_for_jobs:
            if (type(args.wait_for_jobs) != str or
                args.wait_for_jobs.lower() == "true"):
                logging.info("waiting for running jobs to complete...")
                self.console.WaitForJobsToExit()

        self.console.StopJobThreadAndProcessPool()
        self.console.__exit__()
        return True
