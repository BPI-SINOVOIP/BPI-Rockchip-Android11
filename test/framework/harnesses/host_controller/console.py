#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import cmd
import ctypes
import datetime
import imp  # Python v2 compatibility
import logging
import multiprocessing
import multiprocessing.pool
import os
import re
import shutil
import signal
import socket
import sys
import tempfile
import threading
import time
import urlparse

from host_controller import common
from host_controller.command_processor import command_adb
from host_controller.command_processor import command_build
from host_controller.command_processor import command_config
from host_controller.command_processor import command_config_local
from host_controller.command_processor import command_copy
from host_controller.command_processor import command_device
from host_controller.command_processor import command_dut
from host_controller.command_processor import command_exit
from host_controller.command_processor import command_fastboot
from host_controller.command_processor import command_fetch
from host_controller.command_processor import command_flash
from host_controller.command_processor import command_gsispl
from host_controller.command_processor import command_info
from host_controller.command_processor import command_lease
from host_controller.command_processor import command_list
from host_controller.command_processor import command_password
from host_controller.command_processor import command_release
from host_controller.command_processor import command_retry
from host_controller.command_processor import command_request
from host_controller.command_processor import command_repack
from host_controller.command_processor import command_sheet
from host_controller.command_processor import command_shell
from host_controller.command_processor import command_sleep
from host_controller.command_processor import command_test
from host_controller.command_processor import command_reproduce
from host_controller.command_processor import command_upload
from host_controller.build import build_info
from host_controller.build import build_provider_ab
from host_controller.build import build_provider_gcs
from host_controller.build import build_provider_local_fs
from host_controller.build import build_provider_pab
from host_controller.utils.ipc import file_lock
from host_controller.utils.ipc import shared_dict
from host_controller.vti_interface import vti_endpoint_client
from vts.runners.host import logger
from vts.utils.python.common import cmd_utils

COMMAND_PROCESSORS = [
    command_adb.CommandAdb,
    command_build.CommandBuild,
    command_config.CommandConfig,
    command_config_local.CommandConfigLocal,
    command_copy.CommandCopy,
    command_device.CommandDevice,
    command_dut.CommandDUT,
    command_exit.CommandExit,
    command_fastboot.CommandFastboot,
    command_fetch.CommandFetch,
    command_flash.CommandFlash,
    command_gsispl.CommandGsispl,
    command_info.CommandInfo,
    command_lease.CommandLease,
    command_list.CommandList,
    command_password.CommandPassword,
    command_release.CommandRelease,
    command_retry.CommandRetry,
    command_request.CommandRequest,
    command_repack.CommandRepack,
    command_sheet.CommandSheet,
    command_shell.CommandShell,
    command_sleep.CommandSleep,
    command_test.CommandTest,
    command_reproduce.CommandReproduce,
    command_upload.CommandUpload,
]


class NonDaemonizedProcess(multiprocessing.Process):
    """Process class which is not daemonized."""

    def _get_daemon(self):
        return False

    def _set_daemon(self, value):
        pass

    daemon = property(_get_daemon, _set_daemon)


class NonDaemonizedPool(multiprocessing.pool.Pool):
    """Pool class which is not daemonized."""

    Process = NonDaemonizedProcess


def JobMain(vti_address, in_queue, out_queue, device_status, password, hosts):
    """Main() for a child process that executes a leased job.

    Currently, lease jobs must use VTI (not TFC).

    Args:
        vti_client: VtiEndpointClient needed to create Console.
        in_queue: Queue to get new jobs.
        out_queue: Queue to put execution results.
        device_status: SharedDict, contains device status information.
                       shared between processes.
        password: multiprocessing.managers.ValueProxy, a proxy instance of a
                  string(ctypes.c_char_p) represents the password which is
                  to be passed to the prompt when executing certain command
                  as root user.
        hosts: A list of HostController objects. Needed for the device command.
    """

    def SigTermHandler(signum, frame):
        """Signal handler for exiting pool process explicitly.

        Added to resolve orphaned pool process issue.
        """
        sys.exit(0)

    signal.signal(signal.SIGTERM, SigTermHandler)

    vti_client = vti_endpoint_client.VtiEndpointClient(vti_address)
    console = Console(vti_client, None, None, hosts, job_pool=True)
    console.device_status = device_status
    console.password = password
    multiprocessing.util.Finalize(console, console.__exit__, exitpriority=0)

    while True:
        command = in_queue.get()
        if command == "exit":
            break
        elif command == "lease":
            filepath, kwargs = vti_client.LeaseJob(socket.gethostname(), True)
            logging.debug("Job %s -> %s" % (os.getpid(), kwargs))
            if filepath is not None:
                # TODO: redirect console output and add
                # console command to access them.

                console._build_provider[
                    "pab"] = build_provider_pab.BuildProviderPAB()
                console._build_provider[
                    "gcs"] = build_provider_gcs.BuildProviderGCS()

                for serial in kwargs["serial"]:
                    console.ChangeDeviceState(
                        serial, common._DEVICE_STATUS_DICT["use"])
                print_to_console = True
                if not print_to_console:
                    sys.stdout = out
                    sys.stderr = err

                ret, gcs_log_url = console.ProcessConfigurableScript(
                    os.path.join(os.getcwd(), "host_controller", "campaigns",
                                 filepath), **kwargs)
                if ret:
                    job_status = "complete"
                else:
                    job_status = "infra-err"

                vti_client.StopHeartbeat(job_status, gcs_log_url)
                logging.info("Job execution complete. "
                             "Setting job status to {}".format(job_status))

                if not print_to_console:
                    sys.stdout = sys.__stdout__
                    sys.stderr = sys.__stderr__

                for serial in kwargs["serial"]:
                    console.ChangeDeviceState(
                        serial, common._DEVICE_STATUS_DICT["ready"])

                del console._build_provider["pab"]
                del console._build_provider["gcs"]
                console.fetch_info = {}
                console._detailed_fetch_info = {}
        else:
            logging.error("Unknown job command %s", command)

    out_queue.put("exit")


class Console(cmd.Cmd):
    """The console for host controllers.

    Attributes:
        command_processors: dict of string:BaseCommandProcessor,
                            map between command string and command processors.
        device_image_info: dict containing info about device image files.
        prompt: The prompt string at the beginning of each command line.
        test_result: dict containing info about the last test result.
        test_suite_info: dict containing info about test suite package files.
        tools_info: dict containing info about custom tool files.
        scheduler_thread: dict containing threading.Thread instances(s) that
                          update configs regularly.
        _build_provider_pab: The BuildProviderPAB used to download artifacts.
        _vti_address: string, VTI service URI.
        _vti_client: VtiEndpoewrClient, used to upload data to a test
                     scheduling infrastructure.
        _tfc_client: The TfcClient that the host controllers connect to.
        _hosts: A list of HostController objects.
        _in_file: The input file object.
        _out_file: The output file object.
        _serials: A list of string where each string is a device serial.
        _device_status: SharedDict, shared with process pool.
                        contains status data on each devices.
        _job_pool: bool, True if Console is created from job pool process
                   context.
        _password: multiprocessing.managers.ValueProxy, a proxy instance of a
                   string(ctypes.c_char_p) represents the password which is
                   to be passed to the prompt when executing certain command
                   as root user.
        _manager: SyncManager. an instance of a manager for shared objects and
                  values between processes.
        _vtslab_version: string, contains version information of vtslab package.
                         (<git commit timestamp>:<git commit hash value>)
        _detailed_fetch_info: A nested dict, holds the branch and target value
                              of the device, gsi, or test suite artifact.
        _file_lock: FileLock, an instance used for synchronizing the devices'
                    use when the automated self-update happens.
    """

    def __init__(self,
                 vti_endpoint_client,
                 tfc,
                 pab,
                 host_controllers,
                 vti_address=None,
                 in_file=sys.stdin,
                 out_file=sys.stdout,
                 job_pool=False,
                 password=None):
        """Initializes the attributes and the parsers."""
        # cmd.Cmd is old-style class.
        cmd.Cmd.__init__(self, stdin=in_file, stdout=out_file)
        self._build_provider = {}
        self._job_pool = job_pool
        if not self._job_pool:
            self._build_provider["pab"] = pab
            self._build_provider["gcs"] = build_provider_gcs.BuildProviderGCS()
            self._build_provider[
                "local_fs"] = build_provider_local_fs.BuildProviderLocalFS()
            self._build_provider["ab"] = build_provider_ab.BuildProviderAB()
            self._manager = multiprocessing.Manager()
            self._device_status = shared_dict.SharedDict(self._manager)
            self._password = self._manager.Value(ctypes.c_char_p, password)
            try:
                with open(common._VTSLAB_VERSION_TXT, "r") as file:
                    self._vtslab_version = file.readline().strip()
                    file.close()
                    logging.info("VTSLAB version: %s" % self._vtslab_version)
            except IOError as e:
                logging.exception(e)
                logging.error("Version info missing in vtslab package. "
                              "Setting version as %s",
                              common._VTSLAB_VERSION_DEFAULT_VALUE)
                self._vtslab_version = common._VTSLAB_VERSION_DEFAULT_VALUE
            self._logfile_upload_path = ""

        self._vti_endpoint_client = vti_endpoint_client
        self._vti_address = vti_address
        self._tfc_client = tfc
        self._hosts = host_controllers
        self._in_file = in_file
        self._out_file = out_file
        self.prompt = "> "
        self.command_processors = {}
        self.device_image_info = build_info.BuildInfo()
        self.test_result = {}
        self.test_suite_info = build_info.BuildInfo()
        self.tools_info = build_info.BuildInfo()
        self.fetch_info = {}
        self._detailed_fetch_info = {}
        self.test_results = {}
        self._file_lock = file_lock.FileLock()
        self.repack_dest_path = ""

        if common._ANDROID_SERIAL in os.environ:
            self._serials = [os.environ[common._ANDROID_SERIAL]]
        else:
            self._serials = []

        self.InitCommandModuleParsers()
        self.SetUpCommandProcessors()

        tempdir_base = os.path.join(os.getcwd(), "tmp")
        if not os.path.exists(tempdir_base):
            os.mkdir(tempdir_base)
        self._tmpdir_default = tempfile.mkdtemp(dir=tempdir_base)
        self._tmp_logdir = tempfile.mkdtemp(dir=tempdir_base)
        if not self._job_pool:
            self._logfile_path = logger.setupTestLogger(
                self._tmp_logdir, create_symlink=False)

    def __exit__(self):
        """Finalizes the build provider attributes explicitly when exited."""
        for bp in self._build_provider:
            self._build_provider[bp].__del__()
        if os.path.exists(self._tmp_logdir):
            shutil.rmtree(self._tmp_logdir)

    @property
    def job_pool(self):
        """getter for self._job_pool"""
        return self._job_pool

    @property
    def device_status(self):
        """getter for self._device_status"""
        return self._device_status

    @device_status.setter
    def device_status(self, device_status):
        """setter for self._device_status"""
        self._device_status = device_status

    @property
    def build_provider(self):
        """getter for self._build_provider"""
        return self._build_provider

    @property
    def tmpdir_default(self):
        """getter for self._password"""
        return self._tmpdir_default

    @tmpdir_default.setter
    def tmpdir_default(self, tmpdir):
        """getter for self._password"""
        self._tmpdir_default = tmpdir

    @property
    def password(self):
        """getter for self._password"""
        return self._password

    @password.setter
    def password(self, password):
        """getter for self._password"""
        self._password = password

    @property
    def logfile_path(self):
        """getter for self._logfile_path"""
        return self._logfile_path

    @property
    def tmp_logdir(self):
        """getter for self._tmp_logdir"""
        return self._tmp_logdir

    @property
    def vti_endpoint_client(self):
        """getter for self._vti_endpoint_client"""
        return self._vti_endpoint_client

    @property
    def vtslab_version(self):
        """getter for self._vtslab_version"""
        return self._vtslab_version

    @property
    def detailed_fetch_info(self):
        return self._detailed_fetch_info

    def UpdateFetchInfo(self, artifact_type):
        if artifact_type in common._ARTIFACT_TYPE_LIST:
            self._detailed_fetch_info[artifact_type] = {}
            self._detailed_fetch_info[artifact_type].update(self.fetch_info)
        else:
            logging.error("Unrecognized artifact type: %s", artifact_type)

    @property
    def file_lock(self):
        """getter for self._file_lock"""
        return self._file_lock

    def ChangeDeviceState(self, serial, state):
        """Changes a device's state and (un)locks the file lock if necessary.

        Args:
            serial: string, serial number of a device.
            state: int, devices' status value pre-defined in
                   common._DEVICE_STATUS_DICT.
        Returns:
            True if the state change and locking/unlocking are successful.
            False otherwise.
        """
        if state == common._DEVICE_STATUS_DICT["use"]:
            ret = self._file_lock.LockDevice(serial)
            if ret == False:
                return False

        current_status = self.device_status[serial]
        self.device_status[serial] = state

        if (current_status in (common._DEVICE_STATUS_DICT["use"],
                               common._DEVICE_STATUS_DICT["error"])
                and current_status != state):
            self._file_lock.UnlockDevice(serial)

    def InitCommandModuleParsers(self):
        """Init all console command modules"""
        for name in dir(self):
            if name.startswith('_Init') and name.endswith('Parser'):
                attr_func = getattr(self, name)
                if hasattr(attr_func, '__call__'):
                    attr_func()

    def SetUpCommandProcessors(self):
        """Sets up all command processors."""
        for command_processor in COMMAND_PROCESSORS:
            cp = command_processor()
            cp._SetUp(self)
            do_text = "do_%s" % cp.command
            help_text = "help_%s" % cp.command
            setattr(self, do_text, cp._Run)
            setattr(self, help_text, cp._Help)
            self.command_processors[cp.command] = cp

    def TearDown(self):
        """Removes all command processors."""
        for command_processor in self.command_processors.itervalues():
            command_processor._TearDown()
        self.command_processors.clear()
        self.__exit__()

    def FormatString(self, format_string):
        """Replaces variables with the values in the console's dictionaries.

        Args:
            format_string: The string containing variables enclosed in {}.

        Returns:
            The formatted string.

        Raises:
            KeyError if a variable is not found in the dictionaries or the
            value is empty.
        """

        def ReplaceVariable(match):
            """Replacement functioon for re.sub().

            replaces string encased in braces with values in the console's dict.

            Args:
                match: regex, used for extracting the variable name.

            Returns:
                string value corresponding to the input variable name.
            """
            name = match.group(1)
            if name in ("build_id", "branch", "target", "account_id"):
                value = self.fetch_info[name]
            elif name in ("result_full", "result_zip", "suite_plan",
                          "suite_name"):
                value = self.test_result[name]
            elif "timestamp" in name:
                current_datetime = datetime.datetime.now()
                value_date = current_datetime.strftime("%Y%m%d")
                value_time = current_datetime.strftime("%H%M%S")
                if "_date" in name:
                    value = value_date
                elif "_time" in name:
                    value = value_time
                elif "_year" in name:
                    value = value_date[0:4]
                elif "_month" in name:
                    value = value_date[4:6]
                elif "_day" in name:
                    value = value_date[6:8]
                else:
                    value = "%s-%s" % (value_date, value_time)
            elif name in ("hc_log", "hc_log_file", "hc_log_upload_path"):
                # hc_log: full abs path to the current process's infra log.
                # hc_log_file: infra log file name, with no path information.
                # hc_log_upload_path: path of the infra log file in GCS.
                value = self._logfile_path
                if name == "hc_log_file":
                    value = os.path.basename(value)
                elif name == "hc_log_upload_path":
                    value = self._logfile_upload_path
            elif name in ("repack_path"):
                value = self.repack_dest_path
                self.repack_dest_path = ""
            elif name in ("hostname"):
                value = socket.gethostname()
            elif "." in name and name.split(".")[0] in self.command_processors:
                command, arg = name.split(".")
                try:
                    value = self.command_processors[command].arg_buffer[arg]
                except KeyError as e:
                    logging.exception(e)
                    value = ""
                if value is None:
                    value = ""
            else:
                value = None

            if value is None:
                raise KeyError(name)

            return value

        return re.sub("{([^}]+)}", ReplaceVariable, format_string)

    def ProcessScript(self, script_file_path):
        """Processes a .py script file.

        A script file implements a function which emits a list of console
        commands to execute. That function emits an empty list or None if
        no more command needs to be processed.

        Args:
            script_file_path: string, the path of a script file (.py file).

        Returns:
            True if successful; False otherwise
        """
        if not script_file_path.endswith(".py"):
            logging.error("Script file is not .py file: %s" % script_file_path)
            return False

        script_module = imp.load_source('script_module', script_file_path)

        commands = script_module.EmitConsoleCommands()
        if commands:
            for command in commands:
                ret = self.onecmd(command)
                if ret == False:
                    return False
        return True

    def ProcessConfigurableScript(self, script_file_path, **kwargs):
        """Processes a .py script file.

        A script file implements a function which emits a list of console
        commands to execute. That function emits an empty list or None if
        no more command needs to be processed.

        Args:
            script_file_path: string, the path of a script file (.py file).
            kwargs: extra args for the interface function defined in
                    the script file.

        Returns:
            True if successful; False otherwise
            String which represents URL to the upload infra log file.
        """
        if script_file_path and not script_file_path.endswith(".py"):
            script_file_path += ".py"

        if not script_file_path.endswith(".py"):
            logging.error("Script file is not .py file: %s", script_file_path)
            return False

        ret = True

        self._logfile_path, file_handler = logger.addLogFile(self._tmp_logdir)
        src = self.FormatString("{hc_log}")
        dest = self.FormatString(
            "gs://vts-report/infra_log/{hostname}/%s_{timestamp}/{hc_log_file}"
            % kwargs["build_target"])
        self._logfile_upload_path = dest

        script_module = imp.load_source('script_module', script_file_path)

        commands = script_module.EmitConsoleCommands(**kwargs)
        logging.info("Command list: %s", commands)
        if commands:
            logging.info("Console commands: %s", commands)
            for command in commands:
                ret = self.onecmd(command)
                if ret == False:
                    break
        else:
            ret = False

        file_handler.flush()
        infra_log_upload_command = "upload"
        infra_log_upload_command += " --src=%s" % src
        infra_log_upload_command += " --dest=%s" % dest
        for serial in kwargs["serial"]:
            if self.device_status[serial] == common._DEVICE_STATUS_DICT[
                    "error"]:
                self.vti_endpoint_client.SetJobStatusFromLeasedTo("bootup-err")
                break
        if not self.vti_endpoint_client.CheckBootUpStatus():
            infra_log_upload_command += (" --report_path=gs://vts-report/"
                                         "suite_result/{timestamp_year}/"
                                         "{timestamp_month}/{timestamp_day}")
            suite_name, plan_name = kwargs["test_name"].split("/")
            infra_log_upload_command += (
                " --result_from_suite=%s" % suite_name)
            infra_log_upload_command += (" --result_from_plan=%s" % plan_name)
        self.onecmd(infra_log_upload_command)
        if self.GetSerials():
            self.onecmd("device --update=stop")
        logging.getLogger().removeHandler(file_handler)
        os.remove(self._logfile_path)
        return (ret != False), dest

    def _Print(self, string):
        """Prints a string and a new line character.

        Args:
            string: The string to be printed.
        """
        self._out_file.write(string + "\n")

    def _PrintObjects(self, objects, attr_names):
        """Shows objects as a table.

        Args:
            object: The objects to be shown, one object in a row.
            attr_names: The attributes to be shown, one attribute in a column.
        """
        width = [len(name) for name in attr_names]
        rows = [attr_names]
        for dev_info in objects:
            attrs = [
                _ToPrintString(getattr(dev_info, name, ""))
                for name in attr_names
            ]
            rows.append(attrs)
            for index, attr in enumerate(attrs):
                width[index] = max(width[index], len(attr))

        for row in rows:
            self._Print("  ".join(
                attr.ljust(width[index]) for index, attr in enumerate(row)))

    def DownloadTestResources(self, request_id):
        """Download all of the test resources for a TFC request id.

        Args:
            request_id: int, TFC request id
        """
        resources = self._tfc_client.TestResourceList(request_id)
        for resource in resources:
            self.DownloadTestResource(resource['url'])

    def DownloadTestResource(self, url):
        """Download a test resource with build provider, given a url.

        Args:
            url: a resource locator (not necessarily HTTP[s])
                with the scheme specifying the build provider.
        """
        parsed = urlparse.urlparse(url)
        path = (parsed.netloc + parsed.path).split('/')
        if parsed.scheme == "pab":
            if len(path) != 5:
                logging.error("Invalid pab resource locator: %s", url)
                return
            account_id, branch, target, build_id, artifact_name = path
            cmd = ("fetch"
                   " --type=pab"
                   " --account_id=%s"
                   " --branch=%s"
                   " --target=%s"
                   " --build_id=%s"
                   " --artifact_name=%s") % (account_id, branch, target,
                                             build_id, artifact_name)
            self.onecmd(cmd)
        elif parsed.scheme == "ab":
            if len(path) != 4:
                logging.error("Invalid ab resource locator: %s", url)
                return
            branch, target, build_id, artifact_name = path
            cmd = ("fetch"
                   "--type=ab"
                   " --branch=%s"
                   " --target=%s"
                   " --build_id=%s"
                   " --artifact_name=%s") % (branch, target, build_id,
                                             artifact_name)
            self.onecmd(cmd)
        elif parsed.scheme == gcs:
            cmd = "fetch --type=gcs --path=%s" % url
            self.onecmd(cmd)
        else:
            logging.error("Invalid URL: %s", url)

    def SetSerials(self, serials):
        """Sets the default serial numbers for flashing and testing.

        Args:
            serials: A list of strings, the serial numbers.
        """
        self._serials = serials

    def FlashImgPackage(self, package_path_gcs):
        """Fetches a repackaged image set from GCS and flashes to the device(s).

        Args:
            package_path_gcs: GCS URL to the packaged img zip file. May contain
                              the GSI imgs.
        """
        self.onecmd("fetch --type=gcs --path=%s --full_device_images=True" %
                    package_path_gcs)
        if common.FULL_ZIPFILE not in self.device_image_info:
            logging.error("Failed to fetch the given file: %s",
                          package_path_gcs)
            return False

        if not self._serials:
            logging.error("Please specify the serial number(s) of target "
                          "device(s) for flashing.")
            return False

        campaign_common = imp.load_source(
            'campaign_common',
            os.path.join(os.getcwd(), "host_controller", "campaigns",
                         "campaign_common.py"))
        flash_command_list = []

        for serial in self._serials:
            flash_commands = []
            cmd_utils.ExecuteOneShellCommand(
                "adb -s %s reboot bootloader" % serial)
            _, stderr, retcode = cmd_utils.ExecuteOneShellCommand(
                "fastboot -s %s getvar product" % serial)
            if retcode == 0:
                res = stderr.splitlines()[0].rstrip()
                if ":" in res:
                    product = res.split(":")[1].strip()
                elif "waiting for %s" % serial in res:
                    res = stderr.splitlines()[1].rstrip()
                    product = res.split(":")[1].strip()
                else:
                    product = "error"
            else:
                product = "error"
            logging.info("Device %s product type: %s", serial, product)
            if product in campaign_common.FLASH_COMMAND_EMITTER:
                flash_commands.append(
                    campaign_common.FLASH_COMMAND_EMITTER[product](
                        serial, repacked_imageset=True))
            elif product != "error":
                flash_commands.append(
                    "flash --current --serial %s --skip-vbmeta=True" % serial)
            else:
                logging.error(
                    "Device %s does not exist. Omitting the flashing "
                    "to the device.", serial)
                continue
            flash_command_list.append(flash_commands)

        ret = self.onecmd(flash_command_list)
        if ret == False:
            logging.error("Flash failed on device %s.", self._serials)
        else:
            logging.info("Flash succeeded on device %s.", self._serials)

        return ret

    def GetSerials(self):
        """Returns the serial numbers saved in the console.

        Returns:
            A list of strings, the serial numbers.
        """
        return self._serials

    def ResetSerials(self):
        """Clears all the serial numbers set to this console obj."""
        self._serials = []

    def JobThread(self):
        """Job thread which monitors and uploads results."""
        thread = threading.currentThread()
        while getattr(thread, "keep_running", True):
            time.sleep(1)

        if self._job_pool:
            self._job_pool.close()
            self._job_pool.terminate()
            self._job_pool.join()

    def StartJobThreadAndProcessPool(self):
        """Starts a background thread to control leased jobs."""
        self._job_in_queue = multiprocessing.Queue()
        self._job_out_queue = multiprocessing.Queue()
        self._job_pool = NonDaemonizedPool(
            common._MAX_LEASED_JOBS, JobMain,
            (self._vti_address, self._job_in_queue, self._job_out_queue,
             self._device_status, self._password, self._hosts))

        self._job_thread = threading.Thread(target=self.JobThread)
        self._job_thread.daemon = True
        self._job_thread.start()

    def StopJobThreadAndProcessPool(self):
        """Terminates the thread and processes that runs the leased job."""
        if hasattr(self, "_job_thread"):
            self._job_thread.keep_running = False
            self._job_thread.join()

    def WaitForJobsToExit(self):
        """Wait for the running jobs to complete before exiting HC."""
        if self._job_pool:
            pool_process_count = common._MAX_LEASED_JOBS
            for _ in range(common._MAX_LEASED_JOBS):
                self._job_in_queue.put("exit")

            while True:
                response = self._job_out_queue.get()
                if response == "exit":
                    pool_process_count -= 1
                if pool_process_count <= 0:
                    break

    # @Override
    def onecmd(self, line, depth=1, ret_out_queue=None):
        """Executes command(s) and prints any exception.

        Parallel execution only for 2nd-level list element.

        Args:
            line: a list of string or string which keeps the command to run.
        """
        if not line:
            return

        if type(line) == list:
            if depth == 1:  # 1 to use multi-threading
                jobs = []
                ret_queue = multiprocessing.Queue()
                for sub_command in line:
                    p = multiprocessing.Process(
                        target=self.onecmd,
                        args=(
                            sub_command,
                            depth + 1,
                            ret_queue,
                        ))
                    jobs.append(p)
                    p.start()
                for job in jobs:
                    job.join()

                ret_cmd_list = True
                while not ret_queue.empty():
                    ret_from_subprocess = ret_queue.get()
                    ret_cmd_list = ret_cmd_list and ret_from_subprocess
                if ret_cmd_list == False:
                    return False
            else:
                for sub_command in line:
                    ret_cmd_list = self.onecmd(sub_command, depth + 1)
                    if ret_cmd_list == False and ret_out_queue:
                        ret_out_queue.put(False)
                        return False
            return

        logging.info("Command: %s", line)
        try:
            ret_cmd = cmd.Cmd.onecmd(self, line)
            if ret_cmd == False and ret_out_queue:
                ret_out_queue.put(ret_cmd)
            return ret_cmd
        except Exception as e:
            self._Print("%s: %s" % (type(e).__name__, e))
            if ret_out_queue:
                ret_out_queue.put(False)
            return False

    # @Override
    def emptyline(self):
        """Ignores empty lines."""
        pass

    # @Override
    def default(self, line):
        """Handles unrecognized commands.

        Returns:
            True if receives EOF; otherwise delegates to default handler.
        """
        if line == "EOF":
            return self.do_exit(line)
        return cmd.Cmd.default(self, line)


def _ToPrintString(obj):
    """Converts an object to printable string on console.

    Args:
        obj: The object to be printed.
    """
    if isinstance(obj, (list, tuple, set)):
        return ",".join(str(x) for x in obj)
    return str(obj)
