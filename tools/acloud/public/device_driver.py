#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
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

"""Public Device Driver APIs.

This module provides public device driver APIs that can be called
as a Python library.

TODO: The following APIs have not been implemented
  - RebootAVD(ip):
  - RegisterSshPubKey(username, key):
  - UnregisterSshPubKey(username, key):
  - CleanupStaleImages():
  - CleanupStaleDevices():
"""

from __future__ import print_function
import logging
import os

from acloud import errors
from acloud.public import avd
from acloud.public import report
from acloud.public.actions import common_operations
from acloud.internal import constants
from acloud.internal.lib import auth
from acloud.internal.lib import android_build_client
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import gstorage_client
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools


logger = logging.getLogger(__name__)

MAX_BATCH_CLEANUP_COUNT = 100

_SSH_USER = "root"


# pylint: disable=invalid-name
class AndroidVirtualDevicePool(object):
    """A class that manages a pool of devices."""

    def __init__(self, cfg, devices=None):
        self._devices = devices or []
        self._cfg = cfg
        credentials = auth.CreateCredentials(cfg)
        self._build_client = android_build_client.AndroidBuildClient(
            credentials)
        self._storage_client = gstorage_client.StorageClient(credentials)
        self._compute_client = android_compute_client.AndroidComputeClient(
            cfg, credentials)

    @utils.TimeExecute("Creating GCE image")
    def _CreateGceImageWithBuildInfo(self, build_target, build_id):
        """Creates a Gce image using build from Launch Control.

        Clone avd-system.tar.gz of a build to a cache storage bucket
        using launch control api. And then create a Gce image.

        Args:
            build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
            build_id: Build id, a string, e.g. "2263051", "P2804227"

        Returns:
            String, name of the Gce image that has been created.
        """
        logger.info("Creating a new gce image using build: build_id %s, "
                    "build_target %s", build_id, build_target)
        disk_image_id = utils.GenerateUniqueName(
            suffix=self._cfg.disk_image_name)
        self._build_client.CopyTo(
            build_target,
            build_id,
            artifact_name=self._cfg.disk_image_name,
            destination_bucket=self._cfg.storage_bucket_name,
            destination_path=disk_image_id)
        disk_image_url = self._storage_client.GetUrl(
            self._cfg.storage_bucket_name, disk_image_id)
        try:
            image_name = self._compute_client.GenerateImageName(build_target,
                                                                build_id)
            self._compute_client.CreateImage(image_name=image_name,
                                             source_uri=disk_image_url)
        finally:
            self._storage_client.Delete(self._cfg.storage_bucket_name,
                                        disk_image_id)
        return image_name

    @utils.TimeExecute("Creating GCE image")
    def _CreateGceImageWithLocalFile(self, local_disk_image):
        """Create a Gce image with a local image file.

        The local disk image can be either a tar.gz file or a
        raw vmlinux image.
        e.g.  /tmp/avd-system.tar.gz or /tmp/android_system_disk_syslinux.img
        If a raw vmlinux image is provided, it will be archived into a tar.gz file.

        The final tar.gz file will be uploaded to a cache bucket in storage.

        Args:
            local_disk_image: string, path to a local disk image,

        Returns:
            String, name of the Gce image that has been created.

        Raises:
            DriverError: if a file with an unexpected extension is given.
        """
        logger.info("Creating a new gce image from a local file %s",
                    local_disk_image)
        with utils.TempDir() as tempdir:
            if local_disk_image.endswith(self._cfg.disk_raw_image_extension):
                dest_tar_file = os.path.join(tempdir,
                                             self._cfg.disk_image_name)
                utils.MakeTarFile(
                    src_dict={local_disk_image: self._cfg.disk_raw_image_name},
                    dest=dest_tar_file)
                local_disk_image = dest_tar_file
            elif not local_disk_image.endswith(self._cfg.disk_image_extension):
                raise errors.DriverError(
                    "Wrong local_disk_image type, must be a *%s file or *%s file"
                    % (self._cfg.disk_raw_image_extension,
                       self._cfg.disk_image_extension))

            disk_image_id = utils.GenerateUniqueName(
                suffix=self._cfg.disk_image_name)
            self._storage_client.Upload(
                local_src=local_disk_image,
                bucket_name=self._cfg.storage_bucket_name,
                object_name=disk_image_id,
                mime_type=self._cfg.disk_image_mime_type)
        disk_image_url = self._storage_client.GetUrl(
            self._cfg.storage_bucket_name, disk_image_id)
        try:
            image_name = self._compute_client.GenerateImageName()
            self._compute_client.CreateImage(image_name=image_name,
                                             source_uri=disk_image_url)
        finally:
            self._storage_client.Delete(self._cfg.storage_bucket_name,
                                        disk_image_id)
        return image_name

    # pylint: disable=too-many-locals
    def CreateDevices(self,
                      num,
                      build_target=None,
                      build_id=None,
                      gce_image=None,
                      local_disk_image=None,
                      cleanup=True,
                      extra_data_disk_size_gb=None,
                      precreated_data_image=None,
                      avd_spec=None,
                      extra_scopes=None):
        """Creates |num| devices for given build_target and build_id.

        - If gce_image is provided, will use it to create an instance.
        - If local_disk_image is provided, will upload it to a temporary
          caching storage bucket which is defined by user as |storage_bucket_name|
          And then create an gce image with it; and then create an instance.
        - If build_target and build_id are provided, will clone the disk image
          via launch control to the temporary caching storage bucket.
          And then create an gce image with it; and then create an instance.

        Args:
            num: Number of devices to create.
            build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
            build_id: Build id, a string, e.g. "2263051", "P2804227"
            gce_image: string, if given, will use this image
                       instead of creating a new one.
                       implies cleanup=False.
            local_disk_image: string, path to a local disk image, e.g.
                              /tmp/avd-system.tar.gz
            cleanup: boolean, if True clean up compute engine image after creating
                     the instance.
            extra_data_disk_size_gb: Integer, size of extra disk, or None.
            precreated_data_image: A string, the image to use for the extra disk.
            avd_spec: AVDSpec object for pass hw_property.
            extra_scopes: A list of extra scopes given to the new instance.

        Raises:
            errors.DriverError: If no source is specified for image creation.
        """
        if gce_image:
            # GCE image is provided, we can directly move to instance creation.
            logger.info("Using existing gce image %s", gce_image)
            image_name = gce_image
            cleanup = False
        elif local_disk_image:
            image_name = self._CreateGceImageWithLocalFile(local_disk_image)
        elif build_target and build_id:
            image_name = self._CreateGceImageWithBuildInfo(build_target,
                                                           build_id)
        else:
            raise errors.DriverError(
                "Invalid image source, must specify one of the following: gce_image, "
                "local_disk_image, or build_target and build id.")

        # Create GCE instances.
        try:
            for _ in range(num):
                instance = self._compute_client.GenerateInstanceName(
                    build_target, build_id)
                extra_disk_name = None
                if extra_data_disk_size_gb > 0:
                    extra_disk_name = self._compute_client.GetDataDiskName(
                        instance)
                    self._compute_client.CreateDisk(extra_disk_name,
                                                    precreated_data_image,
                                                    extra_data_disk_size_gb)
                self._compute_client.CreateInstance(
                    instance=instance,
                    image_name=image_name,
                    extra_disk_name=extra_disk_name,
                    avd_spec=avd_spec,
                    extra_scopes=extra_scopes)
                ip = self._compute_client.GetInstanceIP(instance)
                self.devices.append(avd.AndroidVirtualDevice(
                    ip=ip, instance_name=instance))
        finally:
            if cleanup:
                self._compute_client.DeleteImage(image_name)

    def DeleteDevices(self):
        """Deletes devices.

        Returns:
            A tuple, (deleted, failed, error_msgs)
            deleted: A list of names of instances that have been deleted.
            faild: A list of names of instances that we fail to delete.
            error_msgs: A list of failure messages.
        """
        instance_names = [device.instance_name for device in self._devices]
        return self._compute_client.DeleteInstances(instance_names,
                                                    self._cfg.zone)

    @utils.TimeExecute("Waiting for AVD to boot")
    def WaitForBoot(self):
        """Waits for all devices to boot up.

        Returns:
            A dictionary that contains all the failures.
            The key is the name of the instance that fails to boot,
            the value is an errors.DeviceBoottError object.
        """
        failures = {}
        for device in self._devices:
            try:
                self._compute_client.WaitForBoot(device.instance_name)
            except errors.DeviceBootError as e:
                failures[device.instance_name] = e
        return failures

    @property
    def devices(self):
        """Returns a list of devices in the pool.

        Returns:
            A list of devices in the pool.
        """
        return self._devices


def AddDeletionResultToReport(report_obj, deleted, failed, error_msgs,
                              resource_name):
    """Adds deletion result to a Report object.

    This function will add the following to report.data.
      "deleted": [
         {"name": "resource_name", "type": "resource_name"},
       ],
      "failed": [
         {"name": "resource_name", "type": "resource_name"},
       ],
    This function will append error_msgs to report.errors.

    Args:
        report_obj: A Report object.
        deleted: A list of names of the resources that have been deleted.
        failed: A list of names of the resources that we fail to delete.
        error_msgs: A list of error message strings to be added to the report.
        resource_name: A string, representing the name of the resource.
    """
    for name in deleted:
        report_obj.AddData(key="deleted",
                           value={"name": name,
                                  "type": resource_name})
    for name in failed:
        report_obj.AddData(key="failed",
                           value={"name": name,
                                  "type": resource_name})
    report_obj.AddErrors(error_msgs)
    if failed or error_msgs:
        report_obj.SetStatus(report.Status.FAIL)


def _FetchSerialLogsFromDevices(compute_client, instance_names, output_file,
                                port):
    """Fetch serial logs from a port for a list of devices to a local file.

    Args:
        compute_client: An object of android_compute_client.AndroidComputeClient
        instance_names: A list of instance names.
        output_file: A path to a file ending with "tar.gz"
        port: The number of serial port to read from, 0 for serial output, 1 for
              logcat.
    """
    with utils.TempDir() as tempdir:
        src_dict = {}
        for instance_name in instance_names:
            serial_log = compute_client.GetSerialPortOutput(
                instance=instance_name, port=port)
            file_name = "%s.log" % instance_name
            file_path = os.path.join(tempdir, file_name)
            src_dict[file_path] = file_name
            with open(file_path, "w") as f:
                f.write(serial_log.encode("utf-8"))
        utils.MakeTarFile(src_dict, output_file)


# pylint: disable=too-many-locals
def CreateGCETypeAVD(cfg,
                     build_target=None,
                     build_id=None,
                     num=1,
                     gce_image=None,
                     local_disk_image=None,
                     cleanup=True,
                     serial_log_file=None,
                     autoconnect=False,
                     report_internal_ip=False,
                     avd_spec=None):
    """Creates one or multiple gce android devices.

    Args:
        cfg: An AcloudConfig instance.
        build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
        build_id: Build id, a string, e.g. "2263051", "P2804227"
        num: Number of devices to create.
        gce_image: string, if given, will use this gce image
                   instead of creating a new one.
                   implies cleanup=False.
        local_disk_image: string, path to a local disk image, e.g.
                          /tmp/avd-system.tar.gz
        cleanup: boolean, if True clean up compute engine image and
                 disk image in storage after creating the instance.
        serial_log_file: A path to a file where serial output should
                         be saved to.
        autoconnect: Create ssh tunnel(s) and adb connect after device creation.
        report_internal_ip: Boolean to report the internal ip instead of
                            external ip.
        avd_spec: AVDSpec object for pass hw_property.

    Returns:
        A Report instance.
    """
    r = report.Report(command="create")
    credentials = auth.CreateCredentials(cfg)
    compute_client = android_compute_client.AndroidComputeClient(cfg,
                                                                 credentials)
    try:
        common_operations.CreateSshKeyPairIfNecessary(cfg)
        device_pool = AndroidVirtualDevicePool(cfg)
        device_pool.CreateDevices(
            num,
            build_target,
            build_id,
            gce_image,
            local_disk_image,
            cleanup,
            extra_data_disk_size_gb=cfg.extra_data_disk_size_gb,
            precreated_data_image=cfg.precreated_data_image_map.get(
                cfg.extra_data_disk_size_gb),
            avd_spec=avd_spec,
            extra_scopes=cfg.extra_scopes)
        failures = device_pool.WaitForBoot()
        # Write result to report.
        for device in device_pool.devices:
            ip = (device.ip.internal if report_internal_ip
                  else device.ip.external)
            device_dict = {
                "ip": ip,
                "instance_name": device.instance_name
            }
            if autoconnect:
                forwarded_ports = utils.AutoConnect(
                    ip_addr=ip,
                    rsa_key_file=cfg.ssh_private_key_path,
                    target_vnc_port=constants.GCE_VNC_PORT,
                    target_adb_port=constants.GCE_ADB_PORT,
                    ssh_user=_SSH_USER,
                    client_adb_port=avd_spec.client_adb_port,
                    extra_args_ssh_tunnel=cfg.extra_args_ssh_tunnel)
                device_dict[constants.VNC_PORT] = forwarded_ports.vnc_port
                device_dict[constants.ADB_PORT] = forwarded_ports.adb_port
                if avd_spec.unlock_screen:
                    AdbTools(forwarded_ports.adb_port).AutoUnlockScreen()
            if device.instance_name in failures:
                r.AddData(key="devices_failing_boot", value=device_dict)
                r.AddError(str(failures[device.instance_name]))
            else:
                r.AddData(key="devices", value=device_dict)
        if failures:
            r.SetStatus(report.Status.BOOT_FAIL)
        else:
            r.SetStatus(report.Status.SUCCESS)

        # Dump serial logs.
        if serial_log_file:
            _FetchSerialLogsFromDevices(
                compute_client,
                instance_names=[d.instance_name for d in device_pool.devices],
                port=constants.DEFAULT_SERIAL_PORT,
                output_file=serial_log_file)
    except errors.DriverError as e:
        r.AddError(str(e))
        r.SetStatus(report.Status.FAIL)
    return r


def DeleteAndroidVirtualDevices(cfg, instance_names, default_report=None):
    """Deletes android devices.

    Args:
        cfg: An AcloudConfig instance.
        instance_names: A list of names of the instances to delete.
        default_report: A initialized Report instance.

    Returns:
        A Report instance.
    """
    # delete, failed, error_msgs are used to record result.
    deleted = []
    failed = []
    error_msgs = []

    r = default_report if default_report else report.Report(command="delete")
    credentials = auth.CreateCredentials(cfg)
    compute_client = android_compute_client.AndroidComputeClient(cfg,
                                                                 credentials)
    zone_instances = compute_client.GetZonesByInstances(instance_names)

    try:
        for zone, instances in zone_instances.items():
            deleted_ins, failed_ins, error_ins = compute_client.DeleteInstances(
                instances, zone)
            deleted.extend(deleted_ins)
            failed.extend(failed_ins)
            error_msgs.extend(error_ins)
        AddDeletionResultToReport(
            r, deleted,
            failed, error_msgs,
            resource_name="instance")
        if r.status == report.Status.UNKNOWN:
            r.SetStatus(report.Status.SUCCESS)
    except errors.DriverError as e:
        r.AddError(str(e))
        r.SetStatus(report.Status.FAIL)
    return r


def CheckAccess(cfg):
    """Check if user has access.

    Args:
         cfg: An AcloudConfig instance.
    """
    credentials = auth.CreateCredentials(cfg)
    compute_client = android_compute_client.AndroidComputeClient(
        cfg, credentials)
    logger.info("Checking if user has access to project %s", cfg.project)
    if not compute_client.CheckAccess():
        logger.error("User does not have access to project %s", cfg.project)
        # Print here so that command line user can see it.
        print("Looks like you do not have access to %s. " % cfg.project)
        if cfg.project in cfg.no_project_access_msg_map:
            print(cfg.no_project_access_msg_map[cfg.project])
