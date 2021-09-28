#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Common operations between managing GCE and Cuttlefish devices.

This module provides the common operations between managing GCE (device_driver)
and Cuttlefish (create_cuttlefish_action) devices. Should not be called
directly.
"""

import logging
import os

from acloud import errors
from acloud.public import avd
from acloud.public import report
from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools


logger = logging.getLogger(__name__)


def CreateSshKeyPairIfNecessary(cfg):
    """Create ssh key pair if necessary.

    Args:
        cfg: An Acloudconfig instance.

    Raises:
        error.DriverError: If it falls into an unexpected condition.
    """
    if not cfg.ssh_public_key_path:
        logger.warning(
            "ssh_public_key_path is not specified in acloud config. "
            "Project-wide public key will "
            "be used when creating AVD instances. "
            "Please ensure you have the correct private half of "
            "a project-wide public key if you want to ssh into the "
            "instances after creation.")
    elif cfg.ssh_public_key_path and not cfg.ssh_private_key_path:
        logger.warning(
            "Only ssh_public_key_path is specified in acloud config, "
            "but ssh_private_key_path is missing. "
            "Please ensure you have the correct private half "
            "if you want to ssh into the instances after creation.")
    elif cfg.ssh_public_key_path and cfg.ssh_private_key_path:
        utils.CreateSshKeyPairIfNotExist(cfg.ssh_private_key_path,
                                         cfg.ssh_public_key_path)
    else:
        # Should never reach here.
        raise errors.DriverError(
            "Unexpected error in CreateSshKeyPairIfNecessary")


class DevicePool(object):
    """A class that manages a pool of virtual devices.

    Attributes:
        devices: A list of devices in the pool.
    """

    def __init__(self, device_factory, devices=None):
        """Constructs a new DevicePool.

        Args:
            device_factory: A device factory capable of producing a goldfish or
                cuttlefish device. The device factory must expose an attribute with
                the credentials that can be used to retrieve information from the
                constructed device.
            devices: List of devices managed by this pool.
        """
        self._devices = devices or []
        self._device_factory = device_factory
        self._compute_client = device_factory.GetComputeClient()

    def CreateDevices(self, num):
        """Creates |num| devices for given build_target and build_id.

        Args:
            num: Number of devices to create.
        """
        # Create host instances for cuttlefish/goldfish device.
        # Currently one instance supports only 1 device.
        for _ in range(num):
            instance = self._device_factory.CreateInstance()
            ip = self._compute_client.GetInstanceIP(instance)
            time_info = self._compute_client.execution_time if hasattr(
                self._compute_client, "execution_time") else {}
            self.devices.append(
                avd.AndroidVirtualDevice(ip=ip, instance_name=instance,
                                         time_info=time_info))

    @utils.TimeExecute(function_description="Waiting for AVD(s) to boot up",
                       result_evaluator=utils.BootEvaluator)
    def WaitForBoot(self, boot_timeout_secs):
        """Waits for all devices to boot up.

        Args:
            boot_timeout_secs: Integer, the maximum time in seconds used to
                               wait for the AVD to boot.

        Returns:
            A dictionary that contains all the failures.
            The key is the name of the instance that fails to boot,
            and the value is an errors.DeviceBootError object.
        """
        failures = {}
        for device in self._devices:
            try:
                self._compute_client.WaitForBoot(device.instance_name, boot_timeout_secs)
            except errors.DeviceBootError as e:
                failures[device.instance_name] = e
        return failures

    def CollectSerialPortLogs(self, output_file,
                              port=constants.DEFAULT_SERIAL_PORT):
        """Tar the instance serial logs into specified output_file.

        Args:
            output_file: String, the output tar file path
            port: The serial port number to be collected
        """
        # For emulator, the serial log is the virtual host serial log.
        # For GCE AVD device, the serial log is the AVD device serial log.
        with utils.TempDir() as tempdir:
            src_dict = {}
            for device in self._devices:
                logger.info("Store instance %s serial port %s output to %s",
                            device.instance_name, port, output_file)
                serial_log = self._compute_client.GetSerialPortOutput(
                    instance=device.instance_name, port=port)
                file_name = "%s_serial_%s.log" % (device.instance_name, port)
                file_path = os.path.join(tempdir, file_name)
                src_dict[file_path] = file_name
                with open(file_path, "w") as f:
                    f.write(serial_log.encode("utf-8"))
            utils.MakeTarFile(src_dict, output_file)

    def SetDeviceBuildInfo(self):
        """Add devices build info."""
        for device in self._devices:
            device.build_info = self._device_factory.GetBuildInfoDict()

    @property
    def devices(self):
        """Returns a list of devices in the pool.

        Returns:
            A list of devices in the pool.
        """
        return self._devices

# pylint: disable=too-many-locals,unused-argument,too-many-branches
def CreateDevices(command, cfg, device_factory, num, avd_type,
                  report_internal_ip=False, autoconnect=False,
                  serial_log_file=None, client_adb_port=None,
                  boot_timeout_secs=None, unlock_screen=False,
                  wait_for_boot=True):
    """Create a set of devices using the given factory.

    Main jobs in create devices.
        1. Create GCE instance: Launch instance in GCP(Google Cloud Platform).
        2. Starting up AVD: Wait device boot up.

    Args:
        command: The name of the command, used for reporting.
        cfg: An AcloudConfig instance.
        device_factory: A factory capable of producing a single device.
        num: The number of devices to create.
        avd_type: String, the AVD type(cuttlefish, goldfish...).
        report_internal_ip: Boolean to report the internal ip instead of
                            external ip.
        serial_log_file: String, the file path to tar the serial logs.
        autoconnect: Boolean, whether to auto connect to device.
        client_adb_port: Integer, Specify port for adb forwarding.
        boot_timeout_secs: Integer, boot timeout secs.
        unlock_screen: Boolean, whether to unlock screen after invoke vnc client.
        wait_for_boot: Boolean, True to check serial log include boot up
                       message.

    Raises:
        errors: Create instance fail.

    Returns:
        A Report instance.
    """
    reporter = report.Report(command=command)
    try:
        CreateSshKeyPairIfNecessary(cfg)
        device_pool = DevicePool(device_factory)
        device_pool.CreateDevices(num)
        device_pool.SetDeviceBuildInfo()
        if wait_for_boot:
            failures = device_pool.WaitForBoot(boot_timeout_secs)
        else:
            failures = device_factory.GetFailures()

        if failures:
            reporter.SetStatus(report.Status.BOOT_FAIL)
        else:
            reporter.SetStatus(report.Status.SUCCESS)

        # Collect logs
        if serial_log_file:
            device_pool.CollectSerialPortLogs(
                serial_log_file, port=constants.DEFAULT_SERIAL_PORT)

        # Write result to report.
        for device in device_pool.devices:
            ip = (device.ip.internal if report_internal_ip
                  else device.ip.external)
            device_dict = {
                "ip": ip,
                "instance_name": device.instance_name
            }
            if device.build_info:
                device_dict.update(device.build_info)
            if device.time_info:
                device_dict.update(device.time_info)
            if autoconnect:
                forwarded_ports = utils.AutoConnect(
                    ip_addr=ip,
                    rsa_key_file=cfg.ssh_private_key_path,
                    target_vnc_port=utils.AVD_PORT_DICT[avd_type].vnc_port,
                    target_adb_port=utils.AVD_PORT_DICT[avd_type].adb_port,
                    ssh_user=constants.GCE_USER,
                    client_adb_port=client_adb_port,
                    extra_args_ssh_tunnel=cfg.extra_args_ssh_tunnel)
                device_dict[constants.VNC_PORT] = forwarded_ports.vnc_port
                device_dict[constants.ADB_PORT] = forwarded_ports.adb_port
                if unlock_screen:
                    AdbTools(forwarded_ports.adb_port).AutoUnlockScreen()
            if device.instance_name in failures:
                reporter.AddData(key="devices_failing_boot", value=device_dict)
                reporter.AddError(str(failures[device.instance_name]))
            else:
                reporter.AddData(key="devices", value=device_dict)
    except errors.DriverError as e:
        reporter.AddError(str(e))
        reporter.SetStatus(report.Status.FAIL)
    return reporter
