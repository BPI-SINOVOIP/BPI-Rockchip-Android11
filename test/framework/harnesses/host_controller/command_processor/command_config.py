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
import itertools
import logging
import os
import socket
import threading
import time

from googleapiclient import errors
from google.protobuf import text_format

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.console_argument_parser import ConsoleArgumentError
from host_controller.tradefed import remote_operation

from vti.test_serving.proto import TestLabConfigMessage_pb2 as LabCfgMsg
from vti.test_serving.proto import TestScheduleConfigMessage_pb2 as SchedCfgMsg


class CommandConfig(base_command_processor.BaseCommandProcessor):
    """Command processor for config command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
        schedule_thread: dict containing threading.Thread instances(s) that
                         update schedule info regularly.
    """

    command = "config"
    command_detail = "Specifies a global config type to monitor."

    def UpdateConfig(self, account_id, branch, targets, config_type, method,
                     update_build, clear_schedule, clear_labinfo):
        """Updates the global configuration data.

        Args:
            account_id: string, Partner Android Build account_id to use.
            branch: string, branch to grab the artifact from.
            targets: string, a comma-separate list of build target product(s).
            config_type: string, config type (`prod` or `test').
            method: string, HTTP method for fetching.
            update_build: boolean, indicating whether to upload build info.
            clear_schedule: bool, True to clear all schedule data exist on the
                            scheduler
            clear_labinfo: bool, True to clear all lab data exist on the
                           scheduler
        """
        for target in targets.split(","):
            fetch_path = self.FetchConfig(
                account_id=account_id,
                branch=branch,
                target=target,
                config_type=config_type,
                method=method)
            if fetch_path:
                self.UploadConfig(
                    path=fetch_path,
                    update_build=update_build,
                    clear_schedule=clear_schedule,
                    clear_labinfo=clear_labinfo)

    def FetchConfig(self, account_id, branch, target, config_type, method):
        """Fetches config files from the PAB build provider.

        Args:
            account_id: string, Partner Android Build account_id to use.
            branch: string, branch to grab the artifact from.
            target: string, build target.
            config_type: string, config type (`prod` or `test').
            method: string, HTTP method for fetching.

        Returns:
            string, a path to the temp directory where config files are stored.
        """
        path = ""
        self.console._build_provider["pab"].Authenticate()
        try:
            listed_builds = self.console._build_provider["pab"].GetBuildList(
                account_id=account_id,
                branch=branch,
                target=target,
                page_token="",
                max_results=1,
                method="GET")
        except ValueError as e:
            logging.exception(e)
            return path

        if listed_builds and len(listed_builds) > 0:
            listed_build = listed_builds[0]
            if listed_build["successful"]:
                device_images, test_suites, artifacts, configs = (
                    self.console._build_provider["pab"].GetArtifact(
                        account_id=account_id,
                        branch=branch,
                        target=target,
                        artifact_name=(
                            "vti-global-config-%s.zip" % config_type),
                        build_id=listed_build["build_id"],
                        method=method))
                path = os.path.dirname(configs[config_type])

        return path

    def UploadConfig(self, path, update_build, clear_schedule, clear_labinfo):
        """Uploads configs to VTI server.

        Args:
            path: string, a path where config files are stored.
            update_build: boolean, indicating whether to upload build info.
            clear_schedule: bool, True to clear all schedule data exist on the
                            scheduler
            clear_labinfo: bool, True to clear all lab data exist on the
                           scheduler
        """
        schedules_pbs = []
        lab_pbs = []
        for root, dirs, files in os.walk(path):
            for config_file in files:
                full_path = os.path.join(root, config_file)
                try:
                    if config_file.endswith(".schedule_config"):
                        with open(full_path, "r") as fd:
                            context = fd.read()
                            sched_cfg_msg = SchedCfgMsg.ScheduleConfigMessage()
                            text_format.Merge(context, sched_cfg_msg)
                            schedules_pbs.append(sched_cfg_msg)
                            logging.info(sched_cfg_msg.manifest_branch)
                    elif config_file.endswith(".lab_config"):
                        with open(full_path, "r") as fd:
                            context = fd.read()
                            lab_cfg_msg = LabCfgMsg.LabConfigMessage()
                            text_format.Merge(context, lab_cfg_msg)
                            lab_pbs.append(lab_cfg_msg)
                except text_format.ParseError as e:
                    logging.error("ERROR: Config parsing error %s", e)
        if update_build:
            commands = self.GetBuildCommands(schedules_pbs)
            if commands:
                for command in commands:
                    ret = self.console.onecmd(command)
                    if ret == False:
                        break
        self.console._vti_endpoint_client.UploadScheduleInfo(
            schedules_pbs, clear_schedule)
        self.console._vti_endpoint_client.UploadLabInfo(lab_pbs, clear_labinfo)

    def UpdateConfigLoop(self, account_id, branch, target, config_type, method,
                         update_build, update_interval, clear_schedule,
                         clear_labinfo):
        """Regularly updates the global configuration.

        Args:
            account_id: string, Partner Android Build account_id to use.
            branch: string, branch to grab the artifact from.
            targets: string, a comma-separate list of build target product(s).
            config_type: string, config type (`prod` or `test').
            method: string, HTTP method for fetching.
            update_build: boolean, indicating whether to upload build info.
            update_interval: int, number of seconds before repeating
            clear_schedule: bool, True to clear all schedule data exist on the
                            scheduler
            clear_labinfo: bool, True to clear all lab data exist on the
                           scheduler
        """
        thread = threading.currentThread()
        while getattr(thread, 'keep_running', True):
            try:
                self.UpdateConfig(account_id, branch, target, config_type,
                                  method, update_build, clear_schedule,
                                  clear_labinfo)
            except (socket.error, remote_operation.RemoteOperationException,
                    httplib2.HttpLib2Error, errors.HttpError) as e:
                logging.exception(e)
            time.sleep(update_interval)

    def GetBuildCommands(self, schedule_pbs):
        """Generates a list of build commands with given schedules.

        Args:
            schedule_pbs: a list of TestScheduleConfig protobuf messages.

        Returns:
            a list of build command strings
        """
        attrs = {}
        attrs["device"] = [
            "build_storage_type", "manifest_branch", "pab_account_id",
            "require_signed_device_build", "name"
        ]
        attrs["gsi"] = [
            "gsi_storage_type", "gsi_branch", "gsi_pab_account_id",
            "gsi_build_target"
        ]
        attrs["test"] = [
            "test_storage_type", "test_branch", "test_pab_account_id",
            "test_build_target"
        ]

        class BuildInfo(object):
            """A build information class."""

            def __init__(self, _build_type):
                if _build_type in attrs:
                    for attribute in attrs[_build_type]:
                        setattr(self, attribute, "")

            def __eq__(self, compare):
                return self.__dict__ == compare.__dict__

        build_commands = []
        if not schedule_pbs:
            return build_commands

        # parses the given protobuf and stores as BuildInfo object.
        builds = {"device": [], "gsi": [], "test": []}
        for pb in schedule_pbs:
            for build_target in pb.build_target:
                build_type = "device"
                device = BuildInfo(build_type)
                for attr in attrs[build_type]:
                    if hasattr(pb, attr):
                        setattr(device, attr, getattr(pb, attr, None))
                    elif hasattr(build_target, attr):
                        setattr(device, attr, getattr(build_target, attr,
                                                      None))
                if not [x for x in builds[build_type] if x == device]:
                    builds[build_type].append(device)
                for test_schedule in build_target.test_schedule:
                    build_type = "gsi"
                    gsi = BuildInfo(build_type)
                    for attr in attrs[build_type]:
                        if hasattr(test_schedule, attr):
                            setattr(gsi, attr,
                                    getattr(test_schedule, attr, None))
                    if not [x for x in builds[build_type] if x == gsi]:
                        builds[build_type].append(gsi)

                    build_type = "test"
                    test = BuildInfo(build_type)
                    for attr in attrs[build_type]:
                        if hasattr(test_schedule, attr):
                            setattr(test, attr,
                                    getattr(test_schedule, attr, None))
                    if not [x for x in builds[build_type] if x == test]:
                        builds[build_type].append(test)

        # groups by artifact, branch, and account id, and builds a command.
        for artifact in attrs:
            load_attrs = attrs[artifact]
            if artifact == "device":
                storage_type_text = "build_storage_type"
            else:
                storage_type_text = "" + artifact + "_storage_type"
            pab_builds = [
                x for x in builds[artifact]
                if getattr(x, storage_type_text) ==
                SchedCfgMsg.BUILD_STORAGE_TYPE_PAB
            ]
            pab_builds.sort(key=lambda x: tuple([getattr(x, attribute)
                                                 for attribute in load_attrs]))
            groups = [list(g) for k, g in itertools.groupby(
                pab_builds, lambda x: tuple([getattr(x, attribute)
                                             for attribute
                                             in load_attrs[1:-1]]))]
            for group in groups:
                command = ("build --artifact-type={} --method=GET "
                           "--noauth_local_webserver=True --update=single".
                           format(artifact))
                if artifact == "device":
                    if group[0].manifest_branch:
                        command += " --branch={}".format(
                            group[0].manifest_branch)
                    else:
                        logging.debug(
                            "Device manifest branch is a mandatory field.")
                        continue
                    if group[0].pab_account_id:
                        command += " --account_id={}".format(
                            group[0].pab_account_id)
                    if group[0].require_signed_device_build:
                        command += " --verify-signed-build=True"
                    targets = ",".join([x.name for x in group if x.name])
                    if targets:
                        command += " --target={}".format(targets)
                        build_commands.append(command)
                else:
                    if getattr(group[0], "" + artifact + "_branch"):
                        command += " --branch={}".format(
                            getattr(group[0], "" + artifact + "_branch"))
                    else:
                        logging.debug(
                            "{} branch is a mandatory field.".format(artifact))
                        continue
                    if getattr(group[0], "" + artifact + "_pab_account_id"):
                        command += " --account_id={}".format(
                            getattr(group[0],
                                    "" + artifact + "_pab_account_id"))
                    targets = ",".join([
                        getattr(x, "" + artifact + "_build_target")
                        for x in group
                        if getattr(x, "" + artifact + "_build_target")
                    ])
                    if targets:
                        command += " --target={}".format(targets)
                        build_commands.append(command)

        return build_commands

    # @Override
    def SetUp(self):
        """Initializes the parser for config command."""
        self.schedule_thread = {}
        self.arg_parser.add_argument(
            "--update",
            choices=("single", "start", "stop", "list"),
            default="start",
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
            "--config-type",
            choices=("prod", "test"),
            default="prod",
            help="Whether it's for prod")
        self.arg_parser.add_argument(
            "--branch",
            required=True,
            help="Branch to grab the artifact from.")
        self.arg_parser.add_argument(
            "--target",
            required=True,
            help="a comma-separate list of build target product(s).")
        self.arg_parser.add_argument(
            "--account_id",
            default=common._DEFAULT_ACCOUNT_ID,
            help="Partner Android Build account_id to use.")
        self.arg_parser.add_argument(
            '--method',
            default='GET',
            choices=('GET', 'POST'),
            help='Method for fetching')
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
            self.UpdateConfig(args.account_id, args.branch, args.target,
                              args.config_type, args.method, args.update_build,
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
                logging.warning('config update already running. '
                                'run config --update=stop --id=%s first.',
                                args.id)
                return
            self.schedule_thread[args.id] = threading.Thread(
                target=self.UpdateConfigLoop,
                args=(
                    args.account_id,
                    args.branch,
                    args.target,
                    args.config_type,
                    args.method,
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
        logging.info("Sample: config --branch=<branch name> "
                     "--target=<build target> "
                     "--account_id=<account id> --config-type=[prod|test] "
                     "--update=single")
