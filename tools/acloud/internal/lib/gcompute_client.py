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
"""A client that manages Google Compute Engine.

** ComputeClient **

ComputeClient is a wrapper around Google Compute Engine APIs.
It provides a set of methods for managing a google compute engine project,
such as creating images, creating instances, etc.

Design philosophy: We tried to make ComputeClient as stateless as possible,
and it only keeps states about authentication. ComputeClient should be very
generic, and only knows how to talk to Compute Engine APIs.
"""
# pylint: disable=too-many-lines
import copy
import functools
import getpass
import logging
import os
import re

import six

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import base_cloud_client
from acloud.internal.lib import utils
from acloud.internal.lib.ssh import IP


logger = logging.getLogger(__name__)

_MAX_RETRIES_ON_FINGERPRINT_CONFLICT = 10
_METADATA_KEY = "key"
_METADATA_KEY_VALUE = "value"
_SSH_KEYS_NAME = "sshKeys"
_ITEMS = "items"
_METADATA = "metadata"
_ZONE_RE = re.compile(r"^zones/(?P<zone>.+)")

BASE_DISK_ARGS = {
    "type": "PERSISTENT",
    "boot": True,
    "mode": "READ_WRITE",
    "autoDelete": True,
    "initializeParams": {},
}


class OperationScope(object):
    """Represents operation scope enum."""
    ZONE = "zone"
    REGION = "region"
    GLOBAL = "global"


class PersistentDiskType(object):
    """Represents different persistent disk types.

    pd-standard for regular hard disk.
    pd-ssd for solid state disk.
    """
    STANDARD = "pd-standard"
    SSD = "pd-ssd"


class ImageStatus(object):
    """Represents the status of an image."""
    PENDING = "PENDING"
    READY = "READY"
    FAILED = "FAILED"


def _IsFingerPrintError(exc):
    """Determine if the exception is a HTTP error with code 412.

    Args:
        exc: Exception instance.

    Returns:
        Boolean. True if the exception is a "Precondition Failed" error.
    """
    return isinstance(exc, errors.HttpError) and exc.code == 412


# pylint: disable=too-many-public-methods
class ComputeClient(base_cloud_client.BaseCloudApiClient):
    """Client that manages GCE."""

    # API settings, used by BaseCloudApiClient.
    API_NAME = "compute"
    API_VERSION = "v1"
    SCOPE = " ".join([
        "https://www.googleapis.com/auth/compute",
        "https://www.googleapis.com/auth/devstorage.read_write"
    ])
    # Default settings for gce operations
    DEFAULT_INSTANCE_SCOPE = [
        "https://www.googleapis.com/auth/androidbuild.internal",
        "https://www.googleapis.com/auth/devstorage.read_only",
        "https://www.googleapis.com/auth/logging.write"
    ]
    OPERATION_TIMEOUT_SECS = 30 * 60  # 30 mins
    OPERATION_POLL_INTERVAL_SECS = 20
    MACHINE_SIZE_METRICS = ["guestCpus", "memoryMb"]
    ACCESS_DENIED_CODE = 403

    def __init__(self, acloud_config, oauth2_credentials):
        """Initialize.

        Args:
            acloud_config: An AcloudConfig object.
            oauth2_credentials: An oauth2client.OAuth2Credentials instance.
        """
        super(ComputeClient, self).__init__(oauth2_credentials)
        self._project = acloud_config.project

    def _GetOperationStatus(self, operation, operation_scope, scope_name=None):
        """Get status of an operation.

        Args:
            operation: An Operation resource in the format of json.
            operation_scope: A value from OperationScope, "zone", "region",
                             or "global".
            scope_name: If operation_scope is "zone" or "region", this should be
                        the name of the zone or region, e.g. "us-central1-f".

        Returns:
            Status of the operation, one of "DONE", "PENDING", "RUNNING".

        Raises:
            errors.DriverError: if the operation fails.
        """
        operation_name = operation["name"]
        if operation_scope == OperationScope.GLOBAL:
            api = self.service.globalOperations().get(
                project=self._project, operation=operation_name)
            result = self.Execute(api)
        elif operation_scope == OperationScope.ZONE:
            api = self.service.zoneOperations().get(
                project=self._project,
                operation=operation_name,
                zone=scope_name)
            result = self.Execute(api)
        elif operation_scope == OperationScope.REGION:
            api = self.service.regionOperations().get(
                project=self._project,
                operation=operation_name,
                region=scope_name)
            result = self.Execute(api)

        if result.get("error"):
            errors_list = result["error"]["errors"]
            raise errors.DriverError(
                "Get operation state failed, errors: %s" % str(errors_list))
        return result["status"]

    def WaitOnOperation(self, operation, operation_scope, scope_name=None):
        """Wait for an operation to finish.

        Args:
            operation: An Operation resource in the format of json.
            operation_scope: A value from OperationScope, "zone", "region",
                             or "global".
            scope_name: If operation_scope is "zone" or "region", this should be
                        the name of the zone or region, e.g. "us-central1-f".
        """
        timeout_exception = errors.GceOperationTimeoutError(
            "Operation hits timeout, did not complete within %d secs." %
            self.OPERATION_TIMEOUT_SECS)
        utils.PollAndWait(
            func=self._GetOperationStatus,
            expected_return="DONE",
            timeout_exception=timeout_exception,
            timeout_secs=self.OPERATION_TIMEOUT_SECS,
            sleep_interval_secs=self.OPERATION_POLL_INTERVAL_SECS,
            operation=operation,
            operation_scope=operation_scope,
            scope_name=scope_name)

    def GetProject(self):
        """Get project information.

        Returns:
          A project resource in json.
        """
        api = self.service.projects().get(project=self._project)
        return self.Execute(api)

    def GetDisk(self, disk_name, zone):
        """Get disk information.

        Args:
          disk_name: A string.
          zone: String, name of zone.

        Returns:
          An disk resource in json.
          https://cloud.google.com/compute/docs/reference/latest/disks#resource
        """
        api = self.service.disks().get(
            project=self._project, zone=zone, disk=disk_name)
        return self.Execute(api)

    def CheckDiskExists(self, disk_name, zone):
        """Check if disk exists.

        Args:
          disk_name: A string
          zone: String, name of zone.

        Returns:
          True if disk exists, otherwise False.
        """
        try:
            self.GetDisk(disk_name, zone)
            exists = True
        except errors.ResourceNotFoundError:
            exists = False
        logger.debug("CheckDiskExists: disk_name: %s, result: %s", disk_name,
                     exists)
        return exists

    def CreateDisk(self,
                   disk_name,
                   source_image,
                   size_gb,
                   zone,
                   source_project=None,
                   disk_type=PersistentDiskType.STANDARD):
        """Create a gce disk.

        Args:
            disk_name: String
            source_image: String, name of the image.
            size_gb: Integer, size in gb.
            zone: String, name of the zone, e.g. us-central1-b.
            source_project: String, required if the image is located in a different
                            project.
            disk_type: String, a value from PersistentDiskType, STANDARD
                       for regular hard disk or SSD for solid state disk.
        """
        source_project = source_project or self._project
        source_image = "projects/%s/global/images/%s" % (
            source_project, source_image) if source_image else None
        logger.info("Creating disk %s, size_gb: %d, source_image: %s",
                    disk_name, size_gb, str(source_image))
        body = {
            "name": disk_name,
            "sizeGb": size_gb,
            "type": "projects/%s/zones/%s/diskTypes/%s" % (self._project, zone,
                                                           disk_type),
        }
        api = self.service.disks().insert(
            project=self._project,
            sourceImage=source_image,
            zone=zone,
            body=body)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.DriverError:
            logger.error("Creating disk failed, cleaning up: %s", disk_name)
            if self.CheckDiskExists(disk_name, zone):
                self.DeleteDisk(disk_name, zone)
            raise
        logger.info("Disk %s has been created.", disk_name)

    def DeleteDisk(self, disk_name, zone):
        """Delete a gce disk.

        Args:
            disk_name: A string, name of disk.
            zone: A string, name of zone.
        """
        logger.info("Deleting disk %s", disk_name)
        api = self.service.disks().delete(
            project=self._project, zone=zone, disk=disk_name)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation=operation,
            operation_scope=OperationScope.ZONE,
            scope_name=zone)
        logger.info("Deleted disk %s", disk_name)

    def DeleteDisks(self, disk_names, zone):
        """Delete multiple disks.

        Args:
            disk_names: A list of disk names.
            zone: A string, name of zone.

        Returns:
            A tuple, (deleted, failed, error_msgs)
            deleted: A list of names of disks that have been deleted.
            failed: A list of names of disks that we fail to delete.
            error_msgs: A list of failure messages.
        """
        if not disk_names:
            logger.warning("Nothing to delete. Arg disk_names is not provided.")
            return [], [], []
        # Batch send deletion requests.
        logger.info("Deleting disks: %s", disk_names)
        delete_requests = {}
        for disk_name in set(disk_names):
            request = self.service.disks().delete(
                project=self._project, disk=disk_name, zone=zone)
            delete_requests[disk_name] = request
        return self._BatchExecuteAndWait(
            delete_requests, OperationScope.ZONE, scope_name=zone)

    def ListDisks(self, zone, disk_filter=None):
        """List disks.

        Args:
            zone: A string, representing zone name. e.g. "us-central1-f"
            disk_filter: A string representing a filter in format of
                             FIELD_NAME COMPARISON_STRING LITERAL_STRING
                             e.g. "name ne example-instance"
                             e.g. "name eq "example-instance-[0-9]+""

        Returns:
            A list of disks.
        """
        return self.ListWithMultiPages(
            api_resource=self.service.disks().list,
            project=self._project,
            zone=zone,
            filter=disk_filter)

    def CreateImage(self,
                    image_name,
                    source_uri=None,
                    source_disk=None,
                    labels=None):
        """Create a Gce image.

        Args:
            image_name: String, name of image
            source_uri: Full Google Cloud Storage URL where the disk image is
                        stored.  e.g. "https://storage.googleapis.com/my-bucket/
                        avd-system-2243663.tar.gz"
            source_disk: String, this should be the disk's selfLink value
                         (including zone and project), rather than the disk_name
                         e.g. https://www.googleapis.com/compute/v1/projects/
                              google.com:android-builds-project/zones/
                              us-east1-d/disks/<disk_name>
            labels: Dict, will be added to the image's labels.

        Raises:
            errors.DriverError: For malformed request or response.
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        if self.CheckImageExists(image_name):
            return
        if (source_uri and source_disk) or (not source_uri
                                            and not source_disk):
            raise errors.DriverError(
                "Creating image %s requires either source_uri %s or "
                "source_disk %s but not both" % (image_name, source_uri,
                                                 source_disk))
        elif source_uri:
            logger.info("Creating image %s, source_uri %s", image_name,
                        source_uri)
            body = {
                "name": image_name,
                "rawDisk": {
                    "source": source_uri,
                },
            }
        else:
            logger.info("Creating image %s, source_disk %s", image_name,
                        source_disk)
            body = {
                "name": image_name,
                "sourceDisk": source_disk,
            }
        if labels is not None:
            body["labels"] = labels
        api = self.service.images().insert(project=self._project, body=body)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation, operation_scope=OperationScope.GLOBAL)
        except errors.DriverError:
            logger.error("Creating image failed, cleaning up: %s", image_name)
            if self.CheckImageExists(image_name):
                self.DeleteImage(image_name)
            raise
        logger.info("Image %s has been created.", image_name)

    @utils.RetryOnException(_IsFingerPrintError,
                            _MAX_RETRIES_ON_FINGERPRINT_CONFLICT)
    def SetImageLabels(self, image_name, new_labels):
        """Update image's labels. Retry for finger print conflict.

        Note: Decorator RetryOnException will retry the call for FingerPrint
          conflict (HTTP error code 412). The fingerprint is used to detect
          conflicts of GCE resource updates. The fingerprint is initially generated
          by Compute Engine and changes after every request to modify or update
          resources (e.g. GCE "image" resource has "fingerPrint" for "labels"
          updates).

        Args:
            image_name: A string, the image name.
            new_labels: Dict, will be added to the image's labels.

        Returns:
            A GlobalOperation resouce.
            https://cloud.google.com/compute/docs/reference/latest/globalOperations
        """
        image = self.GetImage(image_name)
        labels = image.get("labels", {})
        labels.update(new_labels)
        body = {
            "labels": labels,
            "labelFingerprint": image["labelFingerprint"]
        }
        api = self.service.images().setLabels(
            project=self._project, resource=image_name, body=body)
        return self.Execute(api)

    def CheckImageExists(self, image_name):
        """Check if image exists.

        Args:
            image_name: A string

        Returns:
            True if image exists, otherwise False.
        """
        try:
            self.GetImage(image_name)
            exists = True
        except errors.ResourceNotFoundError:
            exists = False
        logger.debug("CheckImageExists: image_name: %s, result: %s",
                     image_name, exists)
        return exists

    def GetImage(self, image_name, image_project=None):
        """Get image information.

        Args:
            image_name: A string
            image_project: A string

        Returns:
            An image resource in json.
            https://cloud.google.com/compute/docs/reference/latest/images#resource
        """
        api = self.service.images().get(
            project=image_project or self._project, image=image_name)
        return self.Execute(api)

    def DeleteImage(self, image_name):
        """Delete an image.

        Args:
            image_name: A string
        """
        logger.info("Deleting image %s", image_name)
        api = self.service.images().delete(
            project=self._project, image=image_name)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation=operation, operation_scope=OperationScope.GLOBAL)
        logger.info("Deleted image %s", image_name)

    def DeleteImages(self, image_names):
        """Delete multiple images.

        Args:
            image_names: A list of image names.

        Returns:
            A tuple, (deleted, failed, error_msgs)
            deleted: A list of names of images that have been deleted.
            failed: A list of names of images that we fail to delete.
            error_msgs: A list of failure messages.
        """
        if not image_names:
            return [], [], []
        # Batch send deletion requests.
        logger.info("Deleting images: %s", image_names)
        delete_requests = {}
        for image_name in set(image_names):
            request = self.service.images().delete(
                project=self._project, image=image_name)
            delete_requests[image_name] = request
        return self._BatchExecuteAndWait(delete_requests,
                                         OperationScope.GLOBAL)

    def ListImages(self, image_filter=None, image_project=None):
        """List images.

        Args:
            image_filter: A string representing a filter in format of
                          FIELD_NAME COMPARISON_STRING LITERAL_STRING
                          e.g. "name ne example-image"
                          e.g. "name eq "example-image-[0-9]+""
            image_project: String. If not provided, will list images from the default
                           project. Otherwise, will list images from the given
                           project, which can be any arbitrary project where the
                           account has read access
                           (i.e. has the role "roles/compute.imageUser")

        Read more about image sharing across project:
        https://cloud.google.com/compute/docs/images/sharing-images-across-projects

        Returns:
            A list of images.
        """
        return self.ListWithMultiPages(
            api_resource=self.service.images().list,
            project=image_project or self._project,
            filter=image_filter)

    def GetInstance(self, instance, zone):
        """Get information about an instance.

        Args:
            instance: A string, representing instance name.
            zone: A string, representing zone name. e.g. "us-central1-f"

        Returns:
            An instance resource in json.
            https://cloud.google.com/compute/docs/reference/latest/instances#resource
        """
        api = self.service.instances().get(
            project=self._project, zone=zone, instance=instance)
        return self.Execute(api)

    def AttachAccelerator(self, instance, zone, accelerator_count,
                          accelerator_type):
        """Attach a GPU accelerator to the instance.

        Note: In order for this to succeed the following must hold:
        - The machine schedule must be set to "terminate" i.e:
          SetScheduling(self, instance, zone, on_host_maintenance="terminate")
          must have been called.
        - The machine is not starting or running. i.e.
          StopInstance(self, instance) must have been called.

        Args:
            instance: A string, representing instance name.
            zone: String, name of zone.
            accelerator_count: The number accelerators to be attached to the instance.
             a value of 0 will detach all accelerators.
            accelerator_type: The type of accelerator to attach. e.g.
              "nvidia-tesla-k80"
        """
        body = {
            "guestAccelerators": [{
                "acceleratorType":
                self.GetAcceleratorUrl(accelerator_type, zone),
                "acceleratorCount":
                accelerator_count
            }]
        }
        api = self.service.instances().setMachineResources(
            project=self._project, zone=zone, instance=instance, body=body)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Attach instance failed: %s", instance)
            raise
        logger.info("%d x %s have been attached to instance %s.",
                    accelerator_count, accelerator_type, instance)

    def AttachDisk(self, instance, zone, **kwargs):
        """Attach the external disk to the instance.

        Args:
            instance: A string, representing instance name.
            zone: String, name of zone.
            **kwargs: The attachDisk request body. See "https://cloud.google.com/
              compute/docs/reference/latest/instances/attachDisk" for detail.
              {
                "kind": "compute#attachedDisk",
                "type": string,
                "mode": string,
                "source": string,
                "deviceName": string,
                "index": integer,
                "boot": boolean,
                "initializeParams": {
                  "diskName": string,
                  "sourceImage": string,
                  "diskSizeGb": long,
                  "diskType": string,
                  "sourceImageEncryptionKey": {
                    "rawKey": string,
                    "sha256": string
                  }
                },
                "autoDelete": boolean,
                "licenses": [
                  string
                ],
                "interface": string,
                "diskEncryptionKey": {
                  "rawKey": string,
                  "sha256": string
                }
              }

        Returns:
            An disk resource in json.
            https://cloud.google.com/compute/docs/reference/latest/disks#resource


        Raises:
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        api = self.service.instances().attachDisk(
            project=self._project, zone=zone, instance=instance, body=kwargs)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Attach instance failed: %s", instance)
            raise
        logger.info("Disk has been attached to instance %s.", instance)

    def DetachDisk(self, instance, zone, disk_name):
        """Attach the external disk to the instance.

        Args:
            instance: A string, representing instance name.
            zone: String, name of zone.
            disk_name: A string, the name of the detach disk.

        Returns:
            A ZoneOperation resource.
            See https://cloud.google.com/compute/docs/reference/latest/zoneOperations

        Raises:
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        api = self.service.instances().detachDisk(
            project=self._project,
            zone=zone,
            instance=instance,
            deviceName=disk_name)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Detach instance failed: %s", instance)
            raise
        logger.info("Disk has been detached to instance %s.", instance)

    def StartInstance(self, instance, zone):
        """Start |instance| in |zone|.

        Args:
            instance: A string, representing instance name.
            zone: A string, representing zone name. e.g. "us-central1-f"

        Raises:
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        api = self.service.instances().start(
            project=self._project, zone=zone, instance=instance)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Start instance failed: %s", instance)
            raise
        logger.info("Instance %s has been started.", instance)

    def StartInstances(self, instances, zone):
        """Start |instances| in |zone|.

        Args:
            instances: A list of strings, representing instance names's list.
            zone: A string, representing zone name. e.g. "us-central1-f"

        Returns:
            A tuple, (done, failed, error_msgs)
            done: A list of string, representing the names of instances that
              have been executed.
            failed: A list of string, representing the names of instances that
              we failed to execute.
            error_msgs: A list of string, representing the failure messages.
        """
        action = functools.partial(
            self.service.instances().start, project=self._project, zone=zone)
        return self._BatchExecuteOnInstances(instances, zone, action)

    def StopInstance(self, instance, zone):
        """Stop |instance| in |zone|.

        Args:
            instance: A string, representing instance name.
            zone: A string, representing zone name. e.g. "us-central1-f"

        Raises:
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        api = self.service.instances().stop(
            project=self._project, zone=zone, instance=instance)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Stop instance failed: %s", instance)
            raise
        logger.info("Instance %s has been terminated.", instance)

    def StopInstances(self, instances, zone):
        """Stop |instances| in |zone|.

        Args:
            instances: A list of strings, representing instance names's list.
            zone: A string, representing zone name. e.g. "us-central1-f"

        Returns:
            A tuple, (done, failed, error_msgs)
            done: A list of string, representing the names of instances that
                  have been executed.
            failed: A list of string, representing the names of instances that
                    we failed to execute.
            error_msgs: A list of string, representing the failure messages.
        """
        action = functools.partial(
            self.service.instances().stop, project=self._project, zone=zone)
        return self._BatchExecuteOnInstances(instances, zone, action)

    def SetScheduling(self,
                      instance,
                      zone,
                      automatic_restart=True,
                      on_host_maintenance="MIGRATE"):
        """Update scheduling config |automatic_restart| and |on_host_maintenance|.

        Args:
            instance: A string, representing instance name.
            zone: A string, representing zone name. e.g. "us-central1-f".
            automatic_restart: Boolean, determine whether the instance will
                               automatically restart if it crashes or not,
                               default to True.
            on_host_maintenance: enum["MIGRATE", "TERMINATE"]
                                 The instance's maintenance behavior, which
                                 determines whether the instance is live
                                 "MIGRATE" or "TERMINATE" when there is
                                 a maintenance event.

        Raises:
            errors.GceOperationTimeoutError: Operation takes too long to finish.
        """
        body = {
            "automaticRestart": automatic_restart,
            "onHostMaintenance": on_host_maintenance
        }
        api = self.service.instances().setScheduling(
            project=self._project, zone=zone, instance=instance, body=body)
        operation = self.Execute(api)
        try:
            self.WaitOnOperation(
                operation=operation,
                operation_scope=OperationScope.ZONE,
                scope_name=zone)
        except errors.GceOperationTimeoutError:
            logger.error("Set instance scheduling failed: %s", instance)
            raise
        logger.info(
            "Instance scheduling changed:\n"
            "    automaticRestart: %s\n"
            "    onHostMaintenance: %s\n",
            str(automatic_restart).lower(), on_host_maintenance)

    def ListInstances(self, instance_filter=None):
        """List instances cross all zones.

        Gcompute response instance. For example:
        {
            'items':
            {
                'zones/europe-west3-b':
                {
                    'warning':
                    {
                        'message': "There are no results for scope
                        'zones/europe-west3-b' on this page.",
                        'code': 'NO_RESULTS_ON_PAGE',
                        'data': [{'value': u'zones/europe-west3-b',
                                  'key': u'scope'}]
                    }
                },
                'zones/asia-east1-b':
                {
                    'instances': [
                    {
                        'name': 'ins-bc212dc8-userbuild-aosp-cf-x86-64-phone'
                        'status': 'RUNNING',
                        'cpuPlatform': 'Intel Broadwell',
                        'startRestricted': False,
                        'labels': {u'created_by': u'herbertxue'},
                        'name': 'ins-bc212dc8-userbuild-aosp-cf-x86-64-phone',
                        ...
                    }]
                }
            }
        }

        Args:
            instance_filter: A string representing a filter in format of
                             FIELD_NAME COMPARISON_STRING LITERAL_STRING
                             e.g. "name ne example-instance"
                             e.g. "name eq "example-instance-[0-9]+""

        Returns:
            A list of instances.
        """
        # aggregatedList will only return 500 results max, so if there are more,
        # we need to send in the next page token to get the next 500 (and so on
        # and so forth.
        get_more_instances = True
        page_token = None
        instances_list = []
        while get_more_instances:
            api = self.service.instances().aggregatedList(
                project=self._project,
                filter=instance_filter,
                pageToken=page_token)
            response = self.Execute(api)
            page_token = response.get("nextPageToken")
            get_more_instances = page_token is not None
            for instances_data in response["items"].values():
                if "instances" in instances_data:
                    for instance in instances_data.get("instances"):
                        instances_list.append(instance)

        return instances_list

    def SetSchedulingInstances(self,
                               instances,
                               zone,
                               automatic_restart=True,
                               on_host_maintenance="MIGRATE"):
        """Update scheduling config |automatic_restart| and |on_host_maintenance|.

        See //cloud/cluster/api/mixer_instances.proto Scheduling for config option.

        Args:
            instances: A list of string, representing instance names.
            zone: A string, representing zone name. e.g. "us-central1-f".
            automatic_restart: Boolean, determine whether the instance will
                               automatically restart if it crashes or not,
                               default to True.
            on_host_maintenance: enum["MIGRATE", "TERMINATE"]
                                 The instance's maintenance behavior, which
                                 determines whether the instance is live
                                 migrated or terminated when there is
                                 a maintenance event.

        Returns:
            A tuple, (done, failed, error_msgs)
            done: A list of string, representing the names of instances that
                  have been executed.
            failed: A list of string, representing the names of instances that
                    we failed to execute.
            error_msgs: A list of string, representing the failure messages.
        """
        body = {
            "automaticRestart": automatic_restart,
            "OnHostMaintenance": on_host_maintenance
        }
        action = functools.partial(
            self.service.instances().setScheduling,
            project=self._project,
            zone=zone,
            body=body)
        return self._BatchExecuteOnInstances(instances, zone, action)

    def _BatchExecuteOnInstances(self, instances, zone, action):
        """Batch processing operations requiring computing time.

        Args:
            instances: A list of instance names.
            zone: A string, e.g. "us-central1-f".
            action: partial func, all kwargs for this gcloud action has been
                    defined in the caller function (e.g. See "StartInstances")
                    except 'instance' which will be defined by iterating the
                    |instances|.

        Returns:
            A tuple, (done, failed, error_msgs)
            done: A list of string, representing the names of instances that
                  have been executed.
            failed: A list of string, representing the names of instances that
                    we failed to execute.
            error_msgs: A list of string, representing the failure messages.
        """
        if not instances:
            return [], [], []
        # Batch send requests.
        logger.info("Batch executing instances: %s", instances)
        requests = {}
        for instance_name in set(instances):
            requests[instance_name] = action(instance=instance_name)
        return self._BatchExecuteAndWait(
            requests, operation_scope=OperationScope.ZONE, scope_name=zone)

    def _BatchExecuteAndWait(self, requests, operation_scope, scope_name=None):
        """Batch processing requests and wait on the operation.

        Args:
            requests: A dictionary. The key is a string representing the resource
                      name. For example, an instance name, or an image name.
            operation_scope: A value from OperationScope, "zone", "region",
                             or "global".
            scope_name: If operation_scope is "zone" or "region", this should be
                        the name of the zone or region, e.g. "us-central1-f".
        Returns:
            A tuple, (done, failed, error_msgs)
            done: A list of string, representing the resource names that have
                  been executed.
            failed: A list of string, representing resource names that
                    we failed to execute.
            error_msgs: A list of string, representing the failure messages.
        """
        results = self.BatchExecute(requests)
        # Initialize return values
        failed = []
        error_msgs = []
        for resource_name, (_, error) in results.iteritems():
            if error is not None:
                failed.append(resource_name)
                error_msgs.append(str(error))
        done = []
        # Wait for the executing operations to finish.
        logger.info("Waiting for executing operations")
        for resource_name in six.iterkeys(requests):
            operation, _ = results[resource_name]
            if operation:
                try:
                    self.WaitOnOperation(operation, operation_scope,
                                         scope_name)
                    done.append(resource_name)
                except errors.DriverError as exc:
                    failed.append(resource_name)
                    error_msgs.append(str(exc))
        return done, failed, error_msgs

    def ListZones(self):
        """List all zone instances in the project.

        Returns:
            Gcompute response instance. For example:
            {
              "id": "projects/google.com%3Aandroid-build-staging/zones",
              "kind": "compute#zoneList",
              "selfLink": "https://www.googleapis.com/compute/v1/projects/"
                  "google.com:android-build-staging/zones"
              "items": [
                {
                  'creationTimestamp': '2014-07-15T10:44:08.663-07:00',
                  'description': 'asia-east1-c',
                  'id': '2222',
                  'kind': 'compute#zone',
                  'name': 'asia-east1-c',
                  'region': 'https://www.googleapis.com/compute/v1/projects/'
                      'google.com:android-build-staging/regions/asia-east1',
                  'selfLink': 'https://www.googleapis.com/compute/v1/projects/'
                      'google.com:android-build-staging/zones/asia-east1-c',
                  'status': 'UP'
                }, {
                  'creationTimestamp': '2014-05-30T18:35:16.575-07:00',
                  'description': 'asia-east1-b',
                  'id': '2221',
                  'kind': 'compute#zone',
                  'name': 'asia-east1-b',
                  'region': 'https://www.googleapis.com/compute/v1/projects/'
                      'google.com:android-build-staging/regions/asia-east1',
                  'selfLink': 'https://www.googleapis.com/compute/v1/projects'
                      '/google.com:android-build-staging/zones/asia-east1-b',
                  'status': 'UP'
                }]
            }
            See cloud cluster's api/mixer_zones.proto
        """
        api = self.service.zones().list(project=self._project)
        return self.Execute(api)

    def ListRegions(self):
        """List all the regions for a project.

        Returns:
            A dictionary containing all the zones and additional data. See this link
            for the detailed response:
            https://cloud.google.com/compute/docs/reference/latest/regions/list.
            Example:
            {
              'items': [{
                  'name':
                      'us-central1',
                  'quotas': [{
                      'usage': 2.0,
                      'limit': 24.0,
                      'metric': 'CPUS'
                  }, {
                      'usage': 1.0,
                      'limit': 23.0,
                      'metric': 'IN_USE_ADDRESSES'
                  }, {
                      'usage': 209.0,
                      'limit': 10240.0,
                      'metric': 'DISKS_TOTAL_GB'
                  }, {
                      'usage': 1000.0,
                      'limit': 20000.0,
                      'metric': 'INSTANCES'
                  }]
              },..]
            }
        """
        api = self.service.regions().list(project=self._project)
        return self.Execute(api)

    def _GetNetworkArgs(self, network, zone):
        """Helper to generate network args that is used to create an instance.

        Args:
            network: A string, e.g. "default".
            zone: String, representing zone name, e.g. "us-central1-f"

        Returns:
            A dictionary representing network args.
        """
        network_args = {
            "network": self.GetNetworkUrl(network),
            "accessConfigs": [{
                "name": "External NAT",
                "type": "ONE_TO_ONE_NAT"
            }]
        }
        # default network can be blank or set to default, we don't need to
        # specify the subnetwork for that.
        if network and network != "default":
            network_args["subnetwork"] = self.GetSubnetworkUrl(network, zone)
        return network_args

    def _GetDiskArgs(self,
                     disk_name,
                     image_name,
                     image_project=None,
                     disk_size_gb=None):
        """Helper to generate disk args that is used to create an instance.

        Args:
            disk_name: A string
            image_name: A string
            image_project: A string
            disk_size_gb: An integer

        Returns:
            List holding dict of disk args.
        """
        args = copy.deepcopy(BASE_DISK_ARGS)
        args["initializeParams"] = {
            "diskName": disk_name,
            "sourceImage": self.GetImage(image_name,
                                         image_project)["selfLink"],
        }
        # TODO: Remove this check once it's validated that we can either pass in
        # a None diskSizeGb or we find an appropriate default val.
        if disk_size_gb:
            args["diskSizeGb"] = disk_size_gb
        return [args]

    def _GetExtraDiskArgs(self, extra_disk_name, zone):
        """Get extra disk arg for given disk.

        Args:
            extra_disk_name: String, name of the disk.
            zone: String, representing zone name, e.g. "us-central1-f"

        Returns:
            A dictionary of disk args.
        """
        return [{
            "type": "PERSISTENT",
            "mode": "READ_WRITE",
            "source": "projects/%s/zones/%s/disks/%s" % (self._project, zone,
                                                         extra_disk_name),
            "autoDelete": True,
            "boot": False,
            "interface": "SCSI",
            "deviceName": extra_disk_name,
        }]

    # pylint: disable=too-many-locals
    def CreateInstance(self,
                       instance,
                       image_name,
                       machine_type,
                       metadata,
                       network,
                       zone,
                       disk_args=None,
                       image_project=None,
                       gpu=None,
                       extra_disk_name=None,
                       extra_scopes=None,
                       tags=None):
        """Create a gce instance with a gce image.

        Args:
            instance: String, instance name.
            image_name: String, source image used to create this disk.
            machine_type: String, representing machine_type,
                          e.g. "n1-standard-1"
            metadata: Dict, maps a metadata name to its value.
            network: String, representing network name, e.g. "default"
            zone: String, representing zone name, e.g. "us-central1-f"
            disk_args: A list of extra disk args (strings), see _GetDiskArgs
                       for example, if None, will create a disk using the given
                       image.
            image_project: String, name of the project where the image
                           belongs. Assume the default project if None.
            gpu: String, type of gpu to attach. e.g. "nvidia-tesla-k80", if
                 None no gpus will be attached. For more details see:
                 https://cloud.google.com/compute/docs/gpus/add-gpus
            extra_disk_name: String,the name of the extra disk to attach.
            extra_scopes: A list of extra scopes to be provided to the instance.
            tags: A list of tags to associate with the instance. e.g.
                  ["http-server", "https-server"]
        """
        disk_args = (disk_args
                     or self._GetDiskArgs(instance, image_name, image_project))
        if extra_disk_name:
            disk_args.extend(self._GetExtraDiskArgs(extra_disk_name, zone))

        scopes = []
        scopes.extend(self.DEFAULT_INSTANCE_SCOPE)
        if extra_scopes:
            scopes.extend(extra_scopes)

        # Add labels for giving the instances ability to be filter for
        # acloud list/delete cmds.
        body = {
            "machineType": self.GetMachineType(machine_type, zone)["selfLink"],
            "name": instance,
            "networkInterfaces": [self._GetNetworkArgs(network, zone)],
            "disks": disk_args,
            "labels": {constants.LABEL_CREATE_BY: getpass.getuser()},
            "serviceAccounts": [{
                "email": "default",
                "scopes": scopes,
            }],
        }

        if tags:
            body["tags"] = {"items": tags}
        if gpu:
            body["guestAccelerators"] = [{
                "acceleratorType": self.GetAcceleratorUrl(gpu, zone),
                "acceleratorCount": 1
            }]
            # Instances with GPUs cannot live migrate because they are assigned
            # to specific hardware devices.
            body["scheduling"] = {"onHostMaintenance": "terminate"}
        if metadata:
            metadata_list = [{
                _METADATA_KEY: key,
                _METADATA_KEY_VALUE: val
            } for key, val in metadata.iteritems()]
            body[_METADATA] = {_ITEMS: metadata_list}
        logger.info("Creating instance: project %s, zone %s, body:%s",
                    self._project, zone, body)
        api = self.service.instances().insert(
            project=self._project, zone=zone, body=body)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation, operation_scope=OperationScope.ZONE, scope_name=zone)
        logger.info("Instance %s has been created.", instance)

    def DeleteInstance(self, instance, zone):
        """Delete a gce instance.

        Args:
            instance: A string, instance name.
            zone: A string, e.g. "us-central1-f"
        """
        logger.info("Deleting instance: %s", instance)
        api = self.service.instances().delete(
            project=self._project, zone=zone, instance=instance)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation, operation_scope=OperationScope.ZONE, scope_name=zone)
        logger.info("Deleted instance: %s", instance)

    def DeleteInstances(self, instances, zone):
        """Delete multiple instances.

        Args:
            instances: A list of instance names.
            zone: A string, e.g. "us-central1-f".

        Returns:
            A tuple, (deleted, failed, error_msgs)
            deleted: A list of names of instances that have been deleted.
            failed: A list of names of instances that we fail to delete.
            error_msgs: A list of failure messages.
        """
        action = functools.partial(
            self.service.instances().delete, project=self._project, zone=zone)
        return self._BatchExecuteOnInstances(instances, zone, action)

    def ResetInstance(self, instance, zone):
        """Reset the gce instance.

        Args:
            instance: A string, instance name.
            zone: A string, e.g. "us-central1-f".
        """
        logger.info("Resetting instance: %s", instance)
        api = self.service.instances().reset(
            project=self._project, zone=zone, instance=instance)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation, operation_scope=OperationScope.ZONE, scope_name=zone)
        logger.info("Instance has been reset: %s", instance)

    def GetMachineType(self, machine_type, zone):
        """Get URL for a given machine typle.

        Args:
            machine_type: A string, name of the machine type.
            zone: A string, e.g. "us-central1-f"

        Returns:
            A machine type resource in json.
            https://cloud.google.com/compute/docs/reference/latest/
            machineTypes#resource
        """
        api = self.service.machineTypes().get(
            project=self._project, zone=zone, machineType=machine_type)
        return self.Execute(api)

    def GetAcceleratorUrl(self, accelerator_type, zone):
        """Get URL for a given type of accelator.

        Args:
            accelerator_type: A string, representing the accelerator, e.g
              "nvidia-tesla-k80"
            zone: A string representing a zone, e.g. "us-west1-b"

        Returns:
            A URL that points to the accelerator resource, e.g.
            https://www.googleapis.com/compute/v1/projects/<project id>/zones/
            us-west1-b/acceleratorTypes/nvidia-tesla-k80
        """
        api = self.service.acceleratorTypes().get(
            project=self._project, zone=zone, acceleratorType=accelerator_type)
        result = self.Execute(api)
        return result["selfLink"]

    def GetNetworkUrl(self, network):
        """Get URL for a given network.

        Args:
            network: A string, representing network name, e.g "default"

        Returns:
            A URL that points to the network resource, e.g.
            https://www.googleapis.com/compute/v1/projects/<project id>/
            global/networks/default
        """
        api = self.service.networks().get(
            project=self._project, network=network)
        result = self.Execute(api)
        return result["selfLink"]

    def GetSubnetworkUrl(self, network, zone):
        """Get URL for a given network and zone.

        Return the subnetwork for the network in the specified region that the
        specified zone resides in. If there is no subnetwork for the specified
        zone, raise an exception.

        Args:
            network: A string, representing network name, e.g "default"
            zone: String, representing zone name, e.g. "us-central1-f"

        Returns:
            A URL that points to the network resource, e.g.
            https://www.googleapis.com/compute/v1/projects/<project id>/
            global/networks/default

        Raises:
            errors.NoSubnetwork: When no subnetwork exists for the zone
            specified.
        """
        api = self.service.networks().get(
            project=self._project, network=network)
        result = self.Execute(api)
        region = zone.rsplit("-", 1)[0]
        for subnetwork in result.get("subnetworks", []):
            if region in subnetwork:
                return subnetwork
        raise errors.NoSubnetwork("No subnetwork for network %s in region %s" %
                                  (network, region))

    def CompareMachineSize(self, machine_type_1, machine_type_2, zone):
        """Compare the size of two machine types.

        Args:
            machine_type_1: A string representing a machine type, e.g. n1-standard-1
            machine_type_2: A string representing a machine type, e.g. n1-standard-1
            zone: A string representing a zone, e.g. "us-central1-f"

        Returns:
            -1 if any metric of machine size of the first type is smaller than
                the second type.
            0 if all metrics of machine size are equal.
            1 if at least one metric of machine size of the first type is
                greater than the second type and all metrics of first type are
                greater or equal to the second type.

        Raises:
            errors.DriverError: For malformed response.
        """
        machine_info_1 = self.GetMachineType(machine_type_1, zone)
        machine_info_2 = self.GetMachineType(machine_type_2, zone)
        result = 0
        for metric in self.MACHINE_SIZE_METRICS:
            if metric not in machine_info_1 or metric not in machine_info_2:
                raise errors.DriverError(
                    "Malformed machine size record: Can't find '%s' in %s or %s"
                    % (metric, machine_info_1, machine_info_2))
            cmp_result = machine_info_1[metric] - machine_info_2[metric]
            if cmp_result < 0:
                return -1
            if cmp_result > 0:
                result = 1
        return result

    def GetSerialPortOutput(self, instance, zone, port=1):
        """Get serial port output.

        Args:
            instance: string, instance name.
            zone: string, zone name.
            port: int, which COM port to read from, 1-4, default to 1.

        Returns:
            String, contents of the output.

        Raises:
            errors.DriverError: For malformed response.
        """
        api = self.service.instances().getSerialPortOutput(
            project=self._project, zone=zone, instance=instance, port=port)
        result = self.Execute(api)
        if "contents" not in result:
            raise errors.DriverError(
                "Malformed response for GetSerialPortOutput: %s" % result)
        return result["contents"]

    def GetInstanceNamesByIPs(self, ips):
        """Get Instance names by IPs.

        This function will go through all instances, which
        could be slow if there are too many instances.  However, currently
        GCE doesn't support search for instance by IP.

        Args:
            ips: A set of IPs.

        Returns:
            A dictionary where key is IP and value is instance name or None
            if instance is not found for the given IP.
        """
        ip_name_map = dict.fromkeys(ips)
        for instance in self.ListInstances():
            try:
                ip = instance["networkInterfaces"][0]["accessConfigs"][0][
                    "natIP"]
                if ip in ips:
                    ip_name_map[ip] = instance["name"]
            except (IndexError, KeyError) as e:
                logger.error("Could not get instance names by ips: %s", str(e))
        return ip_name_map

    def GetInstanceIP(self, instance, zone):
        """Get Instance IP given instance name.

        Args:
            instance: String, representing instance name.
            zone: String, name of the zone.

        Returns:
            ssh.IP object, that stores internal and external ip of the instance.
        """
        instance = self.GetInstance(instance, zone)
        internal_ip = instance["networkInterfaces"][0]["networkIP"]
        external_ip = instance["networkInterfaces"][0]["accessConfigs"][0]["natIP"]
        return IP(internal=internal_ip, external=external_ip)

    @utils.TimeExecute(function_description="Updating instance metadata: ")
    def SetInstanceMetadata(self, zone, instance, body):
        """Set instance metadata.

        Args:
            zone: String, name of zone.
            instance: String, representing instance name.
            body: Dict, Metadata body.
                  metdata is in the following format.
                  {
                    "kind": "compute#metadata",
                    "fingerprint": "a-23icsyx4E=",
                    "items": [
                      {
                        "key": "sshKeys",
                        "value": "key"
                      }, ...
                    ]
                  }
        """
        api = self.service.instances().setMetadata(
            project=self._project, zone=zone, instance=instance, body=body)
        operation = self.Execute(api)
        self.WaitOnOperation(
            operation, operation_scope=OperationScope.ZONE, scope_name=zone)

    def AddSshRsaInstanceMetadata(self, user, ssh_rsa_path, instance):
        """Add the public rsa key to the instance's metadata.

        Confirm that the instance has this public key in the instance's
        metadata, if not we will add this public key.

        Args:
            user: String, name of the user which the key belongs to.
            ssh_rsa_path: String, The absolute path to public rsa key.
            instance: String, representing instance name.
        """
        ssh_rsa_path = os.path.expanduser(ssh_rsa_path)
        rsa = GetRsaKey(ssh_rsa_path)
        entry = "%s:%s" % (user, rsa)
        logger.debug("New RSA entry: %s", entry)

        zone = self.GetZoneByInstance(instance)
        gce_instance = self.GetInstance(instance, zone)
        metadata = gce_instance.get(_METADATA)
        if RsaNotInMetadata(metadata, entry):
            self.UpdateRsaInMetadata(zone, instance, metadata, entry)

    def GetZoneByInstance(self, instance):
        """Get the zone from instance name.

        Gcompute response instance. For example:
        {
            'items':
            {
                'zones/europe-west3-b':
                {
                    'warning':
                    {
                        'message': "There are no results for scope
                        'zones/europe-west3-b' on this page.",
                        'code': 'NO_RESULTS_ON_PAGE',
                        'data': [{'value': u'zones/europe-west3-b',
                                  'key': u'scope'}]
                    }
                },
                'zones/asia-east1-b':
                {
                    'instances': [
                    {
                        'name': 'ins-bc212dc8-userbuild-aosp-cf-x86-64-phone'
                        'status': 'RUNNING',
                        'cpuPlatform': 'Intel Broadwell',
                        'startRestricted': False,
                        'labels': {u'created_by': u'herbertxue'},
                        'name': 'ins-bc212dc8-userbuild-aosp-cf-x86-64-phone',
                        ...
                    }]
                }
            }
        }

        Args:
            instance: String, representing instance name.

        Raises:
            errors.GetGceZoneError: Can't get zone from instance name.

        Returns:
            String of zone name.
        """
        api = self.service.instances().aggregatedList(
            project=self._project,
            filter="name=%s" % instance)
        response = self.Execute(api)
        for zone, instance_data in response["items"].items():
            if "instances" in instance_data:
                zone_match = _ZONE_RE.match(zone)
                if zone_match:
                    return zone_match.group("zone")
        raise errors.GetGceZoneError("Can't get zone from the instance name %s"
                                     % instance)

    def GetZonesByInstances(self, instances):
        """Get the zone from instance name.

        Args:
            instances: List of strings, representing instance names.

        Returns:
            A dictionary that contains the name of all instances in the zone.
            The key is the name of the zone, and the value is a list contains
            the name of the instances.
        """
        zone_instances = {}
        for instance in instances:
            zone = self.GetZoneByInstance(instance)
            if zone in zone_instances:
                zone_instances[zone].append(instance)
            else:
                zone_instances[zone] = [instance]
        return zone_instances

    def CheckAccess(self):
        """Check if the user has read access to the cloud project.

        Returns:
            True if the user has at least read access to the project.
            False otherwise.

        Raises:
            errors.HttpError if other unexpected error happens when
            accessing the project.
        """
        api = self.service.zones().list(project=self._project)
        retry_http_codes = copy.copy(self.RETRY_HTTP_CODES)
        retry_http_codes.remove(self.ACCESS_DENIED_CODE)
        try:
            self.Execute(api, retry_http_codes=retry_http_codes)
        except errors.HttpError as e:
            if e.code == self.ACCESS_DENIED_CODE:
                return False
            raise
        return True

    def UpdateRsaInMetadata(self, zone, instance, metadata, entry):
        """Update ssh public key to sshKeys's value in this metadata.

        Args:
            zone: String, name of zone.
            instance: String, representing instance name.
            metadata: Dict, maps a metadata name to its value.
            entry: String, ssh public key.
        """
        ssh_key_item = GetSshKeyFromMetadata(metadata)
        if ssh_key_item:
            # The ssh key exists in the metadata so update the reference to it
            # in the metadata. There may not be an actual ssh key value so
            # that's why we filter for None to avoid an empty line in front.
            ssh_key_item[_METADATA_KEY_VALUE] = "\n".join(
                list(filter(None, [ssh_key_item[_METADATA_KEY_VALUE], entry])))
        else:
            # Since there is no ssh key item in the metadata, we need to add it in.
            ssh_key_item = {_METADATA_KEY: _SSH_KEYS_NAME,
                            _METADATA_KEY_VALUE: entry}
            metadata[_ITEMS].append(ssh_key_item)
        utils.PrintColorString(
            "Ssh public key doesn't exist in the instance(%s), adding it."
            % instance, utils.TextColors.WARNING)
        self.SetInstanceMetadata(zone, instance, metadata)


def RsaNotInMetadata(metadata, entry):
    """Check ssh public key exist in sshKeys's value.

    Args:
        metadata: Dict, maps a metadata name to its value.
        entry: String, ssh public key.

    Returns:
        Boolean. True if ssh public key doesn't exist in metadata.
    """
    for item in metadata.setdefault(_ITEMS, []):
        if item[_METADATA_KEY] == _SSH_KEYS_NAME:
            if entry in item[_METADATA_KEY_VALUE]:
                return False
    return True


def GetSshKeyFromMetadata(metadata):
    """Get ssh key item from metadata.

    Args:
        metadata: Dict, maps a metadata name to its value.

    Returns:
        Dict of ssk_key_item in metadata, None if can't find the ssh key item
        in metadata.
    """
    for item in metadata.setdefault(_ITEMS, []):
        if item.get(_METADATA_KEY, '') == _SSH_KEYS_NAME:
            return item
    return None


def GetRsaKey(ssh_rsa_path):
    """Get rsa key from rsa path.

    Args:
        ssh_rsa_path: String, The absolute path to public rsa key.

    Returns:
        String, rsa key.

    Raises:
        errors.DriverError: RSA file does not exist.
    """
    ssh_rsa_path = os.path.expanduser(ssh_rsa_path)
    if not os.path.exists(ssh_rsa_path):
        raise errors.DriverError(
            "RSA file %s does not exist." % ssh_rsa_path)

    with open(ssh_rsa_path) as f:
        rsa = f.read()
        # The space must be removed here for string processing,
        # if it is not string, it doesn't have a strip function.
        rsa = rsa.strip() if rsa else rsa
        utils.VerifyRsaPubKey(rsa)
    return rsa
