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

import httplib2
import logging
import socket
import threading
import time

from googleapiclient import errors

from host_controller.command_processor import base_command_processor
from host_controller.command_processor import command_config
from host_controller.console_argument_parser import ConsoleArgumentError
from host_controller.tradefed import remote_operation


class CommandConfigLocal(command_config.CommandConfig):
    """Command processor for config-local command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
        schedule_thread: dict containing threading.Thread instances(s) that
                         update schedule info regularly.
    """

    command = "config_local"
    command_detail = "Uploads configs from local path."

    # @Override
    def UpdateConfig(self, path, update_build, clear_schedule, clear_labinfo):
        """Updates the global configuration data.

        Args:
            path: string, a path where config files are stored.
            update_build: boolean, indicating whether to upload build info.
        """
        if path:
            self.UploadConfig(
                path=path,
                update_build=update_build,
                clear_schedule=clear_schedule,
                clear_labinfo=clear_labinfo)

    # @Override
    def UpdateConfigLoop(self, path, update_build, update_interval,
                         clear_schedule, clear_labinfo):
        """Regularly updates the global configuration.

        Args:
            path: string, a path where config files are stored.
            update_build: boolean, indicating whether to upload build info.
            update_interval: int, number of seconds before repeating
        """
        thread = threading.currentThread()
        while getattr(thread, 'keep_running', True):
            try:
                self.UpdateConfig(path, update_build, clear_schedule,
                                  clear_labinfo)
            except (socket.error, remote_operation.RemoteOperationException,
                    httplib2.HttpLib2Error, errors.HttpError) as e:
                logging.exception(e)
            time.sleep(update_interval)

    # @Override
    def SetUp(self):
        """Initializes the parser for config command."""
        self.schedule_thread = {}
        self.arg_parser.add_argument(
            "--update",
            choices=("single", "start", "stop", "list"),
            default="single",
            help="Update build info")
        self.arg_parser.add_argument(
            "--id",
            default=None,
            help="session ID only required for 'stop' update command")
        self.arg_parser.add_argument(
            "--interval",
            type=int,
            default=60,
            help="Interval (seconds) to repeat build update.")
        self.arg_parser.add_argument(
            "--path",
            required=True,
            help="A path where config files are stored.")
        self.arg_parser.add_argument(
            '--update_build',
            dest='update_build',
            action='store_true',
            help='A boolean value indicating whether to upload build info.')
        self.arg_parser.add_argument(
            "--clear_schedule",
            default=False,
            help="True to clear all schedule data on the scheduler cloud")
        self.arg_parser.add_argument(
            "--clear_labinfo",
            default=False,
            help="True to clear all lab info data on the scheduler cloud")

    # @Override
    def Run(self, arg_line):
        """Updates global config."""
        args = self.arg_parser.ParseLine(arg_line)
        if args.update == "single":
            self.UpdateConfig(args.path, args.update_build,
                              args.clear_schedule, args.clear_labinfo)
        elif args.update == "list":
            logging.info("Running config update sessions:")
            for id in self.schedule_thread:
                logging.info("  ID %d", id)
        elif args.update == "start":
            if args.interval <= 0:
                raise ConsoleArgumentError("update interval must be positive")
            # do not allow user to create new
            # thread if one is currently running
            if args.id is None:
                if not self.schedule_thread:
                    args.id = 1
                else:
                    args.id = max(self.schedule_thread) + 1
            else:
                args.id = int(args.id)
            if args.id in self.schedule_thread and not hasattr(
                    self.schedule_thread[args.id], 'keep_running'):
                logging.warning(
                    'config update already running. '
                    'run config-local --update=stop --id=%s first.', args.id)
                return
            self.schedule_thread[args.id] = threading.Thread(
                target=self.UpdateConfigLoop,
                args=(
                    args.path,
                    args.update_build,
                    args.interval,
                    args.clear_schedule,
                    args.clear_labinfo,
                ))
            self.schedule_thread[args.id].daemon = True
            self.schedule_thread[args.id].start()
        elif args.update == "stop":
            if args.id is None:
                logging.error("--id must be set for stop")
            else:
                self.schedule_thread[int(args.id)].keep_running = False

    def Help(self):
        base_command_processor.BaseCommandProcessor.Help(self)
        logging.info("Sample: config-local --path=/usr/local/home/config/prod "
                     "--update=single --update_build")
