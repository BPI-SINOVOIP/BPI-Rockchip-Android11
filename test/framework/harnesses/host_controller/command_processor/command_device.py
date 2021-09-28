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

from host_controller import common
from host_controller.command_processor import base_command_processor
from host_controller.console_argument_parser import ConsoleArgumentError
from host_controller.tradefed import remote_operation
from host_controller.utils.usb import usb_utils

from vts.utils.python.common import cmd_utils


class CommandDevice(base_command_processor.BaseCommandProcessor):
    """Command processor for Device command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
        update_thread: threading.Thread that updates device state regularly.
    """

    command = "device"
    command_detail = "Selects device(s) under test."

    def UpdateDevice(self,
                     server_type,
                     host,
                     lease,
                     suppress_lock_warning=True,
                     from_job_pool=False):
        """Updates the device state of all devices on a given host.

        Args:
            server_type: string, the type of a test secheduling server.
            host: HostController object
            lease: boolean, True to lease and execute jobs.
            suppress_lock_warning: bool, True to suppress the warning msg from
                                   file_lock.
            from_job_pool: bool, True if the 'device' command is executed from
                           one of the job pool processes. Checks only
                           the availability of the devices when set.
        """
        if server_type == "vti":
            devices = []

            if from_job_pool:
                devices_dict = {}
                for serial in self.console.GetSerials():
                    device = {}
                    device["serial"] = serial
                    device["status"] = common._DEVICE_STATUS_DICT[
                        "no-response"]
                    device["product"] = "error"
                    devices_dict[serial] = device

            stdout, stderr, returncode = cmd_utils.ExecuteOneShellCommand(
                "adb devices")
            lines_adb = stdout.split("\n")
            stdout, stderr, returncode = cmd_utils.ExecuteOneShellCommand(
                "fastboot devices")
            lines_fastboot = stdout.split("\n")

            for line in lines_adb:
                if (len(line.strip()) and not (line.startswith("* ")
                                               or line.startswith("List "))):
                    device = {}
                    device["serial"] = line.split()[0]
                    serial = device["serial"]

                    if from_job_pool:
                        if (serial in devices_dict
                                and line.split()[1] == "device"):
                            devices_dict[serial][
                                "status"] = common._DEVICE_STATUS_DICT[
                                    "online"]
                            product = (self.console._vti_endpoint_client.
                                       GetJobDeviceProductName())
                            if product:
                                devices_dict[serial]["product"] = product
                        continue

                    if self.console.file_lock.LockDevice(
                            serial, suppress_lock_warning) == False:
                        self.console.device_status[
                            serial] = common._DEVICE_STATUS_DICT["use"]
                        if not suppress_lock_warning:
                            logging.info("Device %s already locked." % serial)
                        continue

                    stdout, _, retcode = cmd_utils.ExecuteOneShellCommand(
                        "adb -s %s reboot bootloader" % device["serial"],
                        common.DEFAULT_DEVICE_TIMEOUT_SECS,
                        usb_utils.ResetUsbDeviceOfSerial_Callback,
                        device["serial"])
                    if retcode == 0:
                        lines_fastboot.append(line)

                    self.console.file_lock.UnlockDevice(serial)

            for line in lines_fastboot:
                if len(line.strip()):
                    device = {}
                    device["serial"] = line.split()[0]
                    serial = device["serial"]

                    if from_job_pool:
                        if serial in devices_dict:
                            devices_dict[serial][
                                "status"] = common._DEVICE_STATUS_DICT[
                                    "fastboot"]
                            product = (self.console._vti_endpoint_client.
                                       GetJobDeviceProductName())
                            if product:
                                devices_dict[serial]["product"] = product
                        continue

                    if self.console.file_lock.LockDevice(
                            serial, suppress_lock_warning) == False:
                        self.console.device_status[
                            serial] = common._DEVICE_STATUS_DICT["use"]
                        if not suppress_lock_warning:
                            logging.info("Device %s already locked." % serial)
                        continue

                    _, stderr, retcode = cmd_utils.ExecuteOneShellCommand(
                        "fastboot -s %s getvar product" % device["serial"],
                        common.DEFAULT_DEVICE_TIMEOUT_SECS,
                        usb_utils.ResetUsbDeviceOfSerial_Callback,
                        device["serial"])
                    if retcode == 0:
                        res = stderr.splitlines()[0].rstrip()
                        if ":" in res:
                            device["product"] = res.split(":")[1].strip()
                        elif "waiting for %s" % serial in res:
                            res = stderr.splitlines()[1].rstrip()
                            device["product"] = res.split(":")[1].strip()
                        else:
                            device["product"] = "error"
                        self.console.device_status[
                            serial] = common._DEVICE_STATUS_DICT["fastboot"]
                    else:
                        device["product"] = "error"
                        self.console.device_status[
                            serial] = common._DEVICE_STATUS_DICT["no-response"]

                    device["status"] = self.console.device_status[serial]
                    devices.append(device)

                    self.console.file_lock.UnlockDevice(serial)

            if from_job_pool:
                devices = devices_dict.values()
                if devices:
                    self.console._vti_endpoint_client.UploadDeviceInfo(
                        host.hostname, devices)
                return

            self.console._vti_endpoint_client.UploadDeviceInfo(
                host.hostname, devices)

            if lease:
                self.console._job_in_queue.put("lease")

            if self.console.vtslab_version:
                self.console._vti_endpoint_client.UploadHostVersion(
                    host.hostname, self.console.vtslab_version)
        elif server_type == "tfc":
            devices = host.ListDevices()
            for device in devices:
                device.Extend(['sim_state', 'sim_operator', 'mac_address'])
            snapshots = self.console._tfc_client.CreateDeviceSnapshot(
                host._cluster_ids[0], host.hostname, devices)
            self.console._tfc_client.SubmitHostEvents([snapshots])
        else:
            logging.error("Error: unknown server_type %s for UpdateDevice",
                          server_type)

    def UpdateDeviceRepeat(self,
                           server_type,
                           host,
                           lease,
                           update_interval,
                           suppress_lock_warning=True,
                           from_job_pool=False):
        """Regularly updates the device state of devices on a given host.

        Args:
            server_type: string, the type of a test secheduling server.
            host: HostController object
            lease: boolean, True to lease and execute jobs.
            update_interval: int, number of seconds before repeating
            suppress_lock_warning: bool, True to suppress the warning msg from
                                   file_lock.
            from_job_pool: bool, True if the 'device' command is executed form
                           one of the job pool processes.
        """
        thread = threading.currentThread()
        while getattr(thread, 'keep_running', True):
            try:
                self.UpdateDevice(server_type, host, lease,
                                  suppress_lock_warning, from_job_pool)
            except (socket.error, remote_operation.RemoteOperationException,
                    httplib2.HttpLib2Error, errors.HttpError) as e:
                logging.exception(e)
            time.sleep(update_interval)

    def RunUSBResetTimer(self, serial, interval):
        """Sets up a timer to run the target function after 'interval' secs.

        Args:
            serial: string, serial number of the device whose USB device file
                    will reset when the timeout happens.
            interval: int, sets up the timer for the target function to be
                      executed after 'interval' seconds, if not canceled.

        Returns:
            threading.Timer, set to reset USB port corresponding the device
            with the given serial number.
        """
        usb_reset_timer = threading.Timer(interval, self.USBResetCallback,
                                          (serial, ))
        usb_reset_timer.daemon = True
        usb_reset_timer.start()

        return usb_reset_timer

    def USBResetCallback(self, serial):
        """Resets USB device file corresponding to the given device serial.

        Args:
            serial: string, serial number of the device whose USB device file
                    will reset.
        """
        device_file_path = usb_utils.GetDevicesUSBFilePath()
        if serial in device_file_path:
            logging.error(
                "Device %s not responding. Resetting device file %s.", serial,
                device_file_path[serial])
            usb_utils.ResetDeviceUsb(device_file_path[serial])

    # @Override
    def SetUp(self):
        """Initializes the parser for device command."""
        self.update_thread = None
        self.arg_parser.add_argument(
            "--set_serial",
            default="",
            help="Serial number for device. Can be a comma-separated list.")
        self.arg_parser.add_argument(
            "--update",
            choices=("single", "start", "stop"),
            default="start",
            help="Update device info on cloud scheduler")
        self.arg_parser.add_argument(
            "--interval",
            type=int,
            default=30,
            help="Interval (seconds) to repeat device update.")
        self.arg_parser.add_argument(
            "--host", type=int, help="The index of the host.")
        self.arg_parser.add_argument(
            "--server_type",
            choices=("vti", "tfc"),
            default="vti",
            help="The type of a cloud-based test scheduler server.")
        self.arg_parser.add_argument(
            "--lease",
            default=False,
            type=bool,
            help="Whether to lease jobs and execute them.")
        self.arg_parser.add_argument(
            "--suppress_lock_warning",
            default=True,
            help="Whether to suppress device lock warning messages.")
        self.arg_parser.add_argument(
            "--from_job_pool",
            action="store_true",
            help="Whether the command is executed from the job pool. "
            "Check only the availability of the devices when set.")

    # @Override
    def Run(self, arg_line):
        """Sets device info such as serial number."""
        args = self.arg_parser.ParseLine(arg_line)
        if args.set_serial:
            self.console.SetSerials(args.set_serial.split(","))
            logging.info("serials: %s", self.console._serials)
        if args.update:
            if args.host is None:
                if len(self.console._hosts) > 1:
                    raise ConsoleArgumentError("More than one host.")
                args.host = 0
            host = self.console._hosts[args.host]

            if args.suppress_lock_warning:
                if (type(args.suppress_lock_warning) != str
                        or args.suppress_lock_warning.lower() == "true"):
                    suppress_lock_warning = True
                else:
                    suppress_lock_warning = False

            if args.update == "single":
                self.UpdateDevice(args.server_type, host, args.lease,
                                  suppress_lock_warning, args.from_job_pool)
            elif args.update == "start":
                if args.interval <= 0:
                    raise ConsoleArgumentError(
                        "update interval must be positive")
                # do not allow user to create new
                # thread if one is currently running
                if self.update_thread is not None and not hasattr(
                        self.update_thread, 'keep_running'):
                    logging.warning('device update already running. '
                                    'run device --update stop first.')
                    return
                self.update_thread = threading.Thread(
                    target=self.UpdateDeviceRepeat,
                    args=(
                        args.server_type,
                        host,
                        args.lease,
                        args.interval,
                        suppress_lock_warning,
                        args.from_job_pool,
                    ))
                self.update_thread.daemon = True
                self.update_thread.start()
            elif args.update == "stop":
                self.update_thread.keep_running = False
                if self.console.GetSerials():
                    self.console.ResetSerials()
