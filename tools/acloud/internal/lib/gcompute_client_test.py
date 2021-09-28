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
"""Tests for acloud.internal.lib.gcompute_client."""
# pylint: disable=too-many-lines

import copy
import os

import unittest
import mock
import six

# pylint: disable=import-error
import apiclient.http

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils


GS_IMAGE_SOURCE_URI = "https://storage.googleapis.com/fake-bucket/fake.tar.gz"
GS_IMAGE_SOURCE_DISK = (
    "https://www.googleapis.com/compute/v1/projects/fake-project/zones/"
    "us-east1-d/disks/fake-disk")
PROJECT = "fake-project"


# pylint: disable=protected-access, too-many-public-methods
class ComputeClientTest(driver_test_lib.BaseDriverTest):
    """Test ComputeClient."""

    PROJECT_OTHER = "fake-project-other"
    INSTANCE = "fake-instance"
    IMAGE = "fake-image"
    IMAGE_URL = "http://fake-image-url"
    IMAGE_OTHER = "fake-image-other"
    MACHINE_TYPE = "fake-machine-type"
    MACHINE_TYPE_URL = "http://fake-machine-type-url"
    METADATA = ("metadata_key", "metadata_value")
    ACCELERATOR_URL = "http://speedy-gpu"
    NETWORK = "fake-network"
    NETWORK_URL = "http://fake-network-url"
    SUBNETWORK_URL = "http://fake-subnetwork-url"
    ZONE = "fake-zone"
    REGION = "fake-region"
    OPERATION_NAME = "fake-op"
    IMAGE_FINGERPRINT = "L_NWHuz7wTY="
    GPU = "fancy-graphics"
    SSHKEY = (
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDBkTOTRze9v2VOqkkf7RG"
        "jSkg6Z2kb9Q9UHsDGatvend3fmjIw1Tugg0O7nnjlPkskmlgyd4a/j99WOeLL"
        "CPk6xPyoVjrPUVBU/pAk09ORTC4Zqk6YjlW7LOfzvqmXhmIZfYu6Q4Yt50pZzhl"
        "lllfu26nYjY7Tg12D019nJi/kqPX5+NKgt0LGXTu8T1r2Gav/q4V7QRWQrB8Eiu"
        "pxXR7I2YhynqovkEt/OXG4qWgvLEXGsWtSQs0CtCzqEVxz0Y9ECr7er4VdjSQxV"
        "AaeLAsQsK9ROae8hMBFZ3//8zLVapBwpuffCu+fUoql9qeV9xagZcc9zj8XOUOW"
        "ApiihqNL1111 test@test1.org")
    EXTRA_SCOPES = ["scope1"]

    def setUp(self):
        """Set up test."""
        super(ComputeClientTest, self).setUp()
        self.Patch(gcompute_client.ComputeClient, "InitResourceHandle")
        fake_cfg = mock.MagicMock()
        fake_cfg.project = PROJECT
        fake_cfg.extra_scopes = self.EXTRA_SCOPES
        self.compute_client = gcompute_client.ComputeClient(
            fake_cfg, mock.MagicMock())
        self.compute_client._service = mock.MagicMock()

        self._disk_args = copy.deepcopy(gcompute_client.BASE_DISK_ARGS)
        self._disk_args["initializeParams"] = {"diskName": self.INSTANCE,
                                               "sourceImage": self.IMAGE_URL}

    # pylint: disable=invalid-name
    def _SetupMocksForGetOperationStatus(self, mock_result, operation_scope):
        """A helper class for setting up mocks for testGetOperationStatus*.

        Args:
            mock_result: The result to return by _GetOperationStatus.
            operation_scope: A value of OperationScope.

        Returns:
            A mock for Resource object.
        """
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        if operation_scope == gcompute_client.OperationScope.GLOBAL:
            self.compute_client._service.globalOperations = mock.MagicMock(
                return_value=resource_mock)
        elif operation_scope == gcompute_client.OperationScope.ZONE:
            self.compute_client._service.zoneOperations = mock.MagicMock(
                return_value=resource_mock)
        elif operation_scope == gcompute_client.OperationScope.REGION:
            self.compute_client._service.regionOperations = mock.MagicMock(
                return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value=mock_result)
        return resource_mock

    def testGetOperationStatusGlobal(self):
        """Test _GetOperationStatus for global."""
        resource_mock = self._SetupMocksForGetOperationStatus(
            {"status": "GOOD"}, gcompute_client.OperationScope.GLOBAL)
        status = self.compute_client._GetOperationStatus(
            {"name": self.OPERATION_NAME},
            gcompute_client.OperationScope.GLOBAL)
        self.assertEqual(status, "GOOD")
        resource_mock.get.assert_called_with(
            project=PROJECT, operation=self.OPERATION_NAME)

    def testGetOperationStatusZone(self):
        """Test _GetOperationStatus for zone."""
        resource_mock = self._SetupMocksForGetOperationStatus(
            {"status": "GOOD"}, gcompute_client.OperationScope.ZONE)
        status = self.compute_client._GetOperationStatus(
            {"name": self.OPERATION_NAME}, gcompute_client.OperationScope.ZONE,
            self.ZONE)
        self.assertEqual(status, "GOOD")
        resource_mock.get.assert_called_with(
            project=PROJECT,
            operation=self.OPERATION_NAME,
            zone=self.ZONE)

    def testGetOperationStatusRegion(self):
        """Test _GetOperationStatus for region."""
        resource_mock = self._SetupMocksForGetOperationStatus(
            {"status": "GOOD"}, gcompute_client.OperationScope.REGION)
        self.compute_client._GetOperationStatus(
            {"name": self.OPERATION_NAME},
            gcompute_client.OperationScope.REGION, self.REGION)
        resource_mock.get.assert_called_with(
            project=PROJECT, operation=self.OPERATION_NAME, region=self.REGION)

    def testGetOperationStatusError(self):
        """Test _GetOperationStatus failed."""
        self._SetupMocksForGetOperationStatus(
            {"error": {"errors": ["error1", "error2"]}},
            gcompute_client.OperationScope.GLOBAL)
        six.assertRaisesRegex(self,
                              errors.DriverError,
                              "Get operation state failed.*error1.*error2",
                              self.compute_client._GetOperationStatus,
                              {"name": self.OPERATION_NAME},
                              gcompute_client.OperationScope.GLOBAL)

    @mock.patch.object(errors, "GceOperationTimeoutError")
    @mock.patch.object(utils, "PollAndWait")
    def testWaitOnOperation(self, mock_poll, mock_gce_operation_timeout_error):
        """Test WaitOnOperation."""
        mock_error = mock.MagicMock()
        mock_gce_operation_timeout_error.return_value = mock_error
        self.compute_client.WaitOnOperation(
            operation={"name": self.OPERATION_NAME},
            operation_scope=gcompute_client.OperationScope.REGION,
            scope_name=self.REGION)
        mock_poll.assert_called_with(
            func=self.compute_client._GetOperationStatus,
            expected_return="DONE",
            timeout_exception=mock_error,
            timeout_secs=self.compute_client.OPERATION_TIMEOUT_SECS,
            sleep_interval_secs=self.compute_client.OPERATION_POLL_INTERVAL_SECS,
            operation={"name": self.OPERATION_NAME},
            operation_scope=gcompute_client.OperationScope.REGION,
            scope_name=self.REGION)

    def testGetImage(self):
        """Test GetImage."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value={"name": self.IMAGE})
        result = self.compute_client.GetImage(self.IMAGE)
        self.assertEqual(result, {"name": self.IMAGE})
        resource_mock.get.assert_called_with(project=PROJECT, image=self.IMAGE)

    def testGetImageOther(self):
        """Test GetImage with other project."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value={"name": self.IMAGE_OTHER})
        result = self.compute_client.GetImage(
            image_name=self.IMAGE_OTHER,
            image_project=self.PROJECT_OTHER)
        self.assertEqual(result, {"name": self.IMAGE_OTHER})
        resource_mock.get.assert_called_with(
            project=self.PROJECT_OTHER, image=self.IMAGE_OTHER)

    def testCreateImageWithSourceURI(self):
        """Test CreateImage with src uri."""
        source_uri = GS_IMAGE_SOURCE_URI
        source_disk = None
        labels = None
        expected_body = {"name": self.IMAGE,
                         "rawDisk": {"source": GS_IMAGE_SOURCE_URI}}
        mock_check = self.Patch(gcompute_client.ComputeClient,
                                "CheckImageExists",
                                return_value=False)
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateImage(
            image_name=self.IMAGE, source_uri=source_uri,
            source_disk=source_disk, labels=labels)
        resource_mock.insert.assert_called_with(
            project=PROJECT, body=expected_body)
        mock_wait.assert_called_with(
            operation=mock.ANY,
            operation_scope=gcompute_client.OperationScope.GLOBAL)
        mock_check.assert_called_with(self.IMAGE)

    def testCreateImageWithSourceDisk(self):
        """Test CreateImage with src disk."""
        source_uri = None
        source_disk = GS_IMAGE_SOURCE_DISK
        labels = None
        expected_body = {"name": self.IMAGE,
                         "sourceDisk": GS_IMAGE_SOURCE_DISK}
        mock_check = self.Patch(gcompute_client.ComputeClient,
                                "CheckImageExists",
                                return_value=False)
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateImage(
            image_name=self.IMAGE, source_uri=source_uri,
            source_disk=source_disk, labels=labels)
        resource_mock.insert.assert_called_with(
            project=PROJECT, body=expected_body)
        mock_wait.assert_called_with(
            operation=mock.ANY,
            operation_scope=gcompute_client.OperationScope.GLOBAL)
        mock_check.assert_called_with(self.IMAGE)

    def testCreateImageWithSourceDiskAndLabel(self):
        """Test CreateImage with src disk and label."""
        source_uri = None
        source_disk = GS_IMAGE_SOURCE_DISK
        labels = {"label1": "xxx"}
        expected_body = {"name": self.IMAGE,
                         "sourceDisk": GS_IMAGE_SOURCE_DISK,
                         "labels": {"label1": "xxx"}}
        mock_check = self.Patch(gcompute_client.ComputeClient,
                                "CheckImageExists",
                                return_value=False)
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateImage(
            image_name=self.IMAGE, source_uri=source_uri,
            source_disk=source_disk, labels=labels)
        resource_mock.insert.assert_called_with(
            project=PROJECT, body=expected_body)
        mock_wait.assert_called_with(
            operation=mock.ANY,
            operation_scope=gcompute_client.OperationScope.GLOBAL)
        mock_check.assert_called_with(self.IMAGE)

    @mock.patch.object(gcompute_client.ComputeClient, "GetImage")
    def testSetImageLabel(self, mock_get_image):
        """Test SetImageLabel."""
        with mock.patch.object(self.compute_client._service, "images",
                               return_value=mock.MagicMock(
                                   setLabels=mock.MagicMock())) as _:
            image = {"name": self.IMAGE,
                     "sourceDisk": GS_IMAGE_SOURCE_DISK,
                     "labelFingerprint": self.IMAGE_FINGERPRINT,
                     "labels": {"a": "aaa", "b": "bbb"}}
            mock_get_image.return_value = image
            new_labels = {"a": "xxx", "c": "ccc"}
            # Test
            self.compute_client.SetImageLabels(
                self.IMAGE, new_labels)
            # Check result
            expected_labels = {"a": "xxx", "b": "bbb", "c": "ccc"}
            self.compute_client._service.images().setLabels.assert_called_with(
                project=PROJECT,
                resource=self.IMAGE,
                body={
                    "labels": expected_labels,
                    "labelFingerprint": self.IMAGE_FINGERPRINT
                })

    def testCreateImageRaiseDriverErrorWithValidInput(self):
        """Test CreateImage with valid input."""
        source_uri = GS_IMAGE_SOURCE_URI
        source_disk = GS_IMAGE_SOURCE_DISK
        self.Patch(gcompute_client.ComputeClient, "CheckImageExists", return_value=False)
        self.assertRaises(errors.DriverError, self.compute_client.CreateImage,
                          image_name=self.IMAGE, source_uri=source_uri,
                          source_disk=source_disk)

    def testCreateImageRaiseDriverErrorWithInvalidInput(self):
        """Test CreateImage with valid input."""
        source_uri = None
        source_disk = None
        self.Patch(gcompute_client.ComputeClient, "CheckImageExists", return_value=False)
        self.assertRaises(errors.DriverError, self.compute_client.CreateImage,
                          image_name=self.IMAGE, source_uri=source_uri,
                          source_disk=source_disk)

    @mock.patch.object(gcompute_client.ComputeClient, "DeleteImage")
    @mock.patch.object(gcompute_client.ComputeClient, "CheckImageExists",
                       side_effect=[False, True])
    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation",
                       side_effect=errors.DriverError("Expected fake error"))
    def testCreateImageFail(self, mock_wait, mock_check, mock_delete):
        """Test CreateImage fails."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()

        expected_body = {
            "name": self.IMAGE,
            "rawDisk": {
                "source": GS_IMAGE_SOURCE_URI,
            },
        }
        six.assertRaisesRegex(
            self,
            errors.DriverError,
            "Expected fake error",
            self.compute_client.CreateImage,
            image_name=self.IMAGE,
            source_uri=GS_IMAGE_SOURCE_URI)
        resource_mock.insert.assert_called_with(
            project=PROJECT, body=expected_body)
        mock_wait.assert_called_with(
            operation=mock.ANY,
            operation_scope=gcompute_client.OperationScope.GLOBAL)
        mock_check.assert_called_with(self.IMAGE)
        mock_delete.assert_called_with(self.IMAGE)

    def testCheckImageExistsTrue(self):
        """Test CheckImageExists return True."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value={"name": self.IMAGE})
        self.assertTrue(self.compute_client.CheckImageExists(self.IMAGE))

    def testCheckImageExistsFalse(self):
        """Test CheckImageExists return False."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(
            side_effect=errors.ResourceNotFoundError(404, "no image"))
        self.assertFalse(self.compute_client.CheckImageExists(self.IMAGE))

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDeleteImage(self, mock_wait):
        """Test DeleteImage."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.delete = mock.MagicMock()
        self.compute_client.DeleteImage(self.IMAGE)
        resource_mock.delete.assert_called_with(
            project=PROJECT, image=self.IMAGE)
        self.assertTrue(mock_wait.called)

    def _SetupBatchHttpRequestMock(self):
        """Setup BatchHttpRequest mock."""
        requests = {}

        def _Add(request, callback, request_id):
            requests[request_id] = (request, callback)

        def _Execute():
            for rid in requests:
                _, callback = requests[rid]
                callback(
                    request_id=rid, response=mock.MagicMock(), exception=None)
        mock_batch = mock.MagicMock()
        mock_batch.add = _Add
        mock_batch.execute = _Execute
        self.Patch(apiclient.http, "BatchHttpRequest", return_value=mock_batch)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDeleteImages(self, mock_wait):
        """Test DeleteImages."""
        self._SetupBatchHttpRequestMock()
        fake_images = ["fake_image_1", "fake_image_2"]
        mock_api = mock.MagicMock()
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.delete = mock.MagicMock(return_value=mock_api)
        # Call the API.
        deleted, failed, error_msgs = self.compute_client.DeleteImages(
            fake_images)
        # Verify
        calls = [
            mock.call(project=PROJECT, image="fake_image_1"),
            mock.call(project=PROJECT, image="fake_image_2")
        ]
        resource_mock.delete.assert_has_calls(calls, any_order=True)
        self.assertEqual(mock_wait.call_count, 2)
        self.assertEqual(error_msgs, [])
        self.assertEqual(failed, [])
        self.assertEqual(set(deleted), set(fake_images))

    def testListImages(self):
        """Test ListImages."""
        fake_token = "fake_next_page_token"
        image_1 = "image_1"
        image_2 = "image_2"
        response_1 = {"items": [image_1], "nextPageToken": fake_token}
        response_2 = {"items": [image_2]}
        self.Patch(
            gcompute_client.ComputeClient,
            "Execute",
            side_effect=[response_1, response_2])
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.list = mock.MagicMock()
        images = self.compute_client.ListImages()
        calls = [
            mock.call(project=PROJECT, filter=None, pageToken=None),
            mock.call(project=PROJECT, filter=None, pageToken=fake_token)
        ]
        resource_mock.list.assert_has_calls(calls)
        self.assertEqual(images, [image_1, image_2])

    def testListImagesFromExternalProject(self):
        """Test ListImages which accepts different project."""
        image = "image_1"
        response = {"items": [image]}
        self.Patch(gcompute_client.ComputeClient, "Execute", side_effect=[response])
        resource_mock = mock.MagicMock()
        self.compute_client._service.images = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.list = mock.MagicMock()
        images = self.compute_client.ListImages(
            image_project="fake-project-2")
        calls = [
            mock.call(project="fake-project-2", filter=None, pageToken=None)]
        resource_mock.list.assert_has_calls(calls)
        self.assertEqual(images, [image])

    def testGetInstance(self):
        """Test GetInstance."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value={"name": self.INSTANCE})
        result = self.compute_client.GetInstance(self.INSTANCE, self.ZONE)
        self.assertEqual(result, {"name": self.INSTANCE})
        resource_mock.get.assert_called_with(
            project=PROJECT, zone=self.ZONE, instance=self.INSTANCE)

    def testListInstances(self):
        """Test ListInstances."""
        instance_1 = "instance_1"
        instance_2 = "instance_2"
        response = {"items": {'zones/fake_zone': {"instances": [instance_1, instance_2]}}}
        self.Patch(
            gcompute_client.ComputeClient,
            "Execute",
            side_effect=[response])
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.aggregatedList = mock.MagicMock()
        instances = self.compute_client.ListInstances()
        calls = [
            mock.call(
                project=PROJECT,
                filter=None,
                pageToken=None),
        ]
        resource_mock.aggregatedList.assert_has_calls(calls)
        self.assertEqual(instances, [instance_1, instance_2])

    def testGetZoneByInstance(self):
        """Test GetZoneByInstance."""
        instance_1 = "instance_1"
        response = {"items": {'zones/fake_zone': {"instances": [instance_1]}}}
        self.Patch(
            gcompute_client.ComputeClient,
            "Execute",
            side_effect=[response])
        expected_zone = "fake_zone"
        self.assertEqual(self.compute_client.GetZoneByInstance(instance_1),
                         expected_zone)

        # Test unable to find 'zone' from instance name.
        response = {"items": {'zones/fake_zone': {"warning": "No instances."}}}
        self.Patch(
            gcompute_client.ComputeClient,
            "Execute",
            side_effect=[response])
        with self.assertRaises(errors.GetGceZoneError):
            self.compute_client.GetZoneByInstance(instance_1)

    def testGetZonesByInstances(self):
        """Test GetZonesByInstances."""
        instances = ["instance_1", "instance_2"]
        # Test instances in the same zone.
        self.Patch(
            gcompute_client.ComputeClient,
            "GetZoneByInstance",
            side_effect=["zone_1", "zone_1"])
        expected_result = {"zone_1": ["instance_1", "instance_2"]}
        self.assertEqual(self.compute_client.GetZonesByInstances(instances),
                         expected_result)

        # Test instances in different zones.
        self.Patch(
            gcompute_client.ComputeClient,
            "GetZoneByInstance",
            side_effect=["zone_1", "zone_2"])
        expected_result = {"zone_1": ["instance_1"],
                           "zone_2": ["instance_2"]}
        self.assertEqual(self.compute_client.GetZonesByInstances(instances),
                         expected_result)

    @mock.patch.object(gcompute_client.ComputeClient, "GetImage")
    @mock.patch.object(gcompute_client.ComputeClient, "GetNetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetSubnetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetMachineType")
    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstance(self, _get_user, mock_wait, mock_get_mach_type,
                           mock_get_subnetwork_url, mock_get_network_url,
                           mock_get_image):
        """Test CreateInstance."""
        mock_get_mach_type.return_value = {"selfLink": self.MACHINE_TYPE_URL}
        mock_get_network_url.return_value = self.NETWORK_URL
        mock_get_subnetwork_url.return_value = self.SUBNETWORK_URL
        mock_get_image.return_value = {"selfLink": self.IMAGE_URL}
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.Patch(
            self.compute_client,
            "_GetExtraDiskArgs",
            return_value=[{"fake_extra_arg": "fake_extra_value"}])
        extra_disk_name = "gce-x86-userdebug-2345-abcd-data"
        expected_disk_args = [self._disk_args]
        expected_disk_args.extend([{"fake_extra_arg": "fake_extra_value"}])
        expected_scope = []
        expected_scope.extend(self.compute_client.DEFAULT_INSTANCE_SCOPE)
        expected_scope.extend(self.EXTRA_SCOPES)

        expected_body = {
            "machineType": self.MACHINE_TYPE_URL,
            "name": self.INSTANCE,
            "networkInterfaces": [
                {
                    "network": self.NETWORK_URL,
                    "subnetwork": self.SUBNETWORK_URL,
                    "accessConfigs": [
                        {"name": "External NAT",
                         "type": "ONE_TO_ONE_NAT"}
                    ],
                }
            ],
            "disks": expected_disk_args,
            "serviceAccounts": [
                {"email": "default",
                 "scopes": expected_scope}
            ],
            "metadata": {
                "items": [{"key": self.METADATA[0],
                           "value": self.METADATA[1]}],
            },
            "labels":{constants.LABEL_CREATE_BY: "fake_user"},
        }

        self.compute_client.CreateInstance(
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            machine_type=self.MACHINE_TYPE,
            metadata={self.METADATA[0]: self.METADATA[1]},
            network=self.NETWORK,
            zone=self.ZONE,
            extra_disk_name=extra_disk_name,
            extra_scopes=self.EXTRA_SCOPES)

        resource_mock.insert.assert_called_with(
            project=PROJECT, zone=self.ZONE, body=expected_body)
        mock_wait.assert_called_with(
            mock.ANY,
            operation_scope=gcompute_client.OperationScope.ZONE,
            scope_name=self.ZONE)


    @mock.patch.object(gcompute_client.ComputeClient, "GetImage")
    @mock.patch.object(gcompute_client.ComputeClient, "GetNetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetSubnetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetMachineType")
    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstanceWithTags(self,
                                   _get_user,
                                   mock_wait,
                                   mock_get_mach_type,
                                   mock_get_subnetwork_url,
                                   mock_get_network_url,
                                   mock_get_image):
        """Test CreateInstance."""
        mock_get_mach_type.return_value = {"selfLink": self.MACHINE_TYPE_URL}
        mock_get_network_url.return_value = self.NETWORK_URL
        mock_get_subnetwork_url.return_value = self.SUBNETWORK_URL
        mock_get_image.return_value = {"selfLink": self.IMAGE_URL}
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.Patch(
            self.compute_client,
            "_GetExtraDiskArgs",
            return_value=[{"fake_extra_arg": "fake_extra_value"}])
        extra_disk_name = "gce-x86-userdebug-2345-abcd-data"
        expected_disk_args = [self._disk_args]
        expected_disk_args.extend([{"fake_extra_arg": "fake_extra_value"}])
        expected_scope = []
        expected_scope.extend(self.compute_client.DEFAULT_INSTANCE_SCOPE)
        expected_scope.extend(self.EXTRA_SCOPES)

        expected_body = {
            "machineType": self.MACHINE_TYPE_URL,
            "name": self.INSTANCE,
            "networkInterfaces": [
                {
                    "network": self.NETWORK_URL,
                    "subnetwork": self.SUBNETWORK_URL,
                    "accessConfigs": [
                        {"name": "External NAT",
                         "type": "ONE_TO_ONE_NAT"}
                    ],
                }
            ],
            'tags': {'items': ['https-server']},
            "disks": expected_disk_args,
            "serviceAccounts": [
                {"email": "default",
                 "scopes": expected_scope}
            ],
            "metadata": {
                "items": [{"key": self.METADATA[0],
                           "value": self.METADATA[1]}],
            },
            "labels":{'created_by': "fake_user"},
        }

        self.compute_client.CreateInstance(
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            machine_type=self.MACHINE_TYPE,
            metadata={self.METADATA[0]: self.METADATA[1]},
            network=self.NETWORK,
            zone=self.ZONE,
            extra_disk_name=extra_disk_name,
            tags=["https-server"],
            extra_scopes=self.EXTRA_SCOPES)

        resource_mock.insert.assert_called_with(
            project=PROJECT, zone=self.ZONE, body=expected_body)
        mock_wait.assert_called_with(
            mock.ANY,
            operation_scope=gcompute_client.OperationScope.ZONE,
            scope_name=self.ZONE)

    @mock.patch.object(gcompute_client.ComputeClient, "GetAcceleratorUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetImage")
    @mock.patch.object(gcompute_client.ComputeClient, "GetNetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetSubnetworkUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "GetMachineType")
    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    @mock.patch("getpass.getuser", return_value="fake_user")
    def testCreateInstanceWithGpu(self, _get_user, mock_wait, mock_get_mach,
                                  mock_get_subnetwork, mock_get_network,
                                  mock_get_image, mock_get_accel):
        """Test CreateInstance with a GPU parameter not set to None."""
        mock_get_mach.return_value = {"selfLink": self.MACHINE_TYPE_URL}
        mock_get_network.return_value = self.NETWORK_URL
        mock_get_subnetwork.return_value = self.SUBNETWORK_URL
        mock_get_accel.return_value = self.ACCELERATOR_URL
        mock_get_image.return_value = {"selfLink": self.IMAGE_URL}

        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()

        expected_body = {
            "machineType":
                self.MACHINE_TYPE_URL,
            "name":
                self.INSTANCE,
            "networkInterfaces": [{
                "network": self.NETWORK_URL,
                "subnetwork": self.SUBNETWORK_URL,
                "accessConfigs": [{
                    "name": "External NAT",
                    "type": "ONE_TO_ONE_NAT"
                }],
            }],
            "disks": [self._disk_args],
            "serviceAccounts": [{
                "email": "default",
                "scopes": self.compute_client.DEFAULT_INSTANCE_SCOPE
            }],
            "scheduling": {
                "onHostMaintenance": "terminate"
            },
            "guestAccelerators": [{
                "acceleratorCount": 1,
                "acceleratorType": "http://speedy-gpu"
            }],
            "metadata": {
                "items": [{
                    "key": self.METADATA[0],
                    "value": self.METADATA[1]
                }],
            },
            "labels":{'created_by': "fake_user"},
        }

        self.compute_client.CreateInstance(
            instance=self.INSTANCE,
            image_name=self.IMAGE,
            machine_type=self.MACHINE_TYPE,
            metadata={self.METADATA[0]: self.METADATA[1]},
            network=self.NETWORK,
            zone=self.ZONE,
            gpu=self.GPU,
            extra_scopes=None)

        resource_mock.insert.assert_called_with(
            project=PROJECT, zone=self.ZONE, body=expected_body)
        mock_wait.assert_called_with(
            mock.ANY, operation_scope=gcompute_client.OperationScope.ZONE,
            scope_name=self.ZONE)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDeleteInstance(self, mock_wait):
        """Test DeleteInstance."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.delete = mock.MagicMock()
        self.compute_client.DeleteInstance(
            instance=self.INSTANCE, zone=self.ZONE)
        resource_mock.delete.assert_called_with(
            project=PROJECT, zone=self.ZONE, instance=self.INSTANCE)
        mock_wait.assert_called_with(
            mock.ANY,
            operation_scope=gcompute_client.OperationScope.ZONE,
            scope_name=self.ZONE)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDeleteInstances(self, mock_wait):
        """Test DeleteInstances."""
        self._SetupBatchHttpRequestMock()
        fake_instances = ["fake_instance_1", "fake_instance_2"]
        mock_api = mock.MagicMock()
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.delete = mock.MagicMock(return_value=mock_api)
        deleted, failed, error_msgs = self.compute_client.DeleteInstances(
            fake_instances, self.ZONE)
        calls = [
            mock.call(
                project=PROJECT,
                instance="fake_instance_1",
                zone=self.ZONE),
            mock.call(
                project=PROJECT,
                instance="fake_instance_2",
                zone=self.ZONE)
        ]
        resource_mock.delete.assert_has_calls(calls, any_order=True)
        self.assertEqual(mock_wait.call_count, 2)
        self.assertEqual(error_msgs, [])
        self.assertEqual(failed, [])
        self.assertEqual(set(deleted), set(fake_instances))

    def testCreateDiskWithProject(self):
        """Test CreateDisk with images using a set project."""
        source_project = "fake-image-project"
        expected_project_to_use = "fake-image-project"
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.disks = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateDisk(
            "fake_disk", "fake_image", 10, self.ZONE, source_project=source_project)
        resource_mock.insert.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            sourceImage="projects/%s/global/images/fake_image" %
            expected_project_to_use,
            body={
                "name":
                    "fake_disk",
                "sizeGb":
                    10,
                "type":
                    "projects/%s/zones/%s/diskTypes/pd-standard" % (PROJECT,
                                                                    self.ZONE)
            })
        self.assertTrue(mock_wait.called)

    def testCreateDiskWithNoSourceProject(self):
        """Test CreateDisk with images with no set project."""
        source_project = None
        expected_project_to_use = PROJECT
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.disks = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateDisk(
            "fake_disk", "fake_image", 10, self.ZONE, source_project=source_project)
        resource_mock.insert.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            sourceImage="projects/%s/global/images/fake_image" %
            expected_project_to_use,
            body={
                "name":
                    "fake_disk",
                "sizeGb":
                    10,
                "type":
                    "projects/%s/zones/%s/diskTypes/pd-standard" % (PROJECT,
                                                                    self.ZONE)
            })
        self.assertTrue(mock_wait.called)

    def testCreateDiskWithTypeStandard(self):
        """Test CreateDisk with images using standard."""
        disk_type = gcompute_client.PersistentDiskType.STANDARD
        expected_disk_type_string = "pd-standard"
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.disks = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateDisk(
            "fake_disk",
            "fake_image",
            10,
            self.ZONE,
            source_project="fake-project",
            disk_type=disk_type)
        resource_mock.insert.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            sourceImage="projects/%s/global/images/fake_image" % "fake-project",
            body={
                "name":
                    "fake_disk",
                "sizeGb":
                    10,
                "type":
                    "projects/%s/zones/%s/diskTypes/%s" %
                    (PROJECT, self.ZONE, expected_disk_type_string)
            })
        self.assertTrue(mock_wait.called)

    def testCreateDiskWithTypeSSD(self):
        """Test CreateDisk with images using standard."""
        disk_type = gcompute_client.PersistentDiskType.SSD
        expected_disk_type_string = "pd-ssd"
        mock_wait = self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client._service.disks = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.insert = mock.MagicMock()
        self.compute_client.CreateDisk(
            "fake_disk",
            "fake_image",
            10,
            self.ZONE,
            source_project="fake-project",
            disk_type=disk_type)
        resource_mock.insert.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            sourceImage="projects/%s/global/images/fake_image" % "fake-project",
            body={
                "name":
                    "fake_disk",
                "sizeGb":
                    10,
                "type":
                    "projects/%s/zones/%s/diskTypes/%s" %
                    (PROJECT, self.ZONE, expected_disk_type_string)
            })
        self.assertTrue(mock_wait.called)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testAttachDisk(self, mock_wait):
        """Test AttachDisk."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.attachDisk = mock.MagicMock()
        self.compute_client.AttachDisk(
            "fake_instance_1", self.ZONE, deviceName="fake_disk",
            source="fake-selfLink")
        resource_mock.attachDisk.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            instance="fake_instance_1",
            body={
                "deviceName": "fake_disk",
                "source": "fake-selfLink"
            })
        self.assertTrue(mock_wait.called)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDetachDisk(self, mock_wait):
        """Test DetachDisk."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.detachDisk = mock.MagicMock()
        self.compute_client.DetachDisk("fake_instance_1", self.ZONE, "fake_disk")
        resource_mock.detachDisk.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            instance="fake_instance_1",
            deviceName="fake_disk")
        self.assertTrue(mock_wait.called)

    @mock.patch.object(gcompute_client.ComputeClient, "GetAcceleratorUrl")
    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testAttachAccelerator(self, mock_wait, mock_get_accel):
        """Test AttachAccelerator."""
        mock_get_accel.return_value = self.ACCELERATOR_URL
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.attachAccelerator = mock.MagicMock()
        self.compute_client.AttachAccelerator("fake_instance_1", self.ZONE, 1,
                                              "nvidia-tesla-k80")
        resource_mock.setMachineResources.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            instance="fake_instance_1",
            body={
                "guestAccelerators": [{
                    "acceleratorType": self.ACCELERATOR_URL,
                    "acceleratorCount": 1
                }]
            })
        self.assertTrue(mock_wait.called)

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testBatchExecuteOnInstances(self, mock_wait):
        """Test BatchExecuteOnInstances."""
        self._SetupBatchHttpRequestMock()
        action = mock.MagicMock(return_value=mock.MagicMock())
        fake_instances = ["fake_instance_1", "fake_instance_2"]
        done, failed, error_msgs = self.compute_client._BatchExecuteOnInstances(
            fake_instances, self.ZONE, action)
        calls = [mock.call(instance="fake_instance_1"),
                 mock.call(instance="fake_instance_2")]
        action.assert_has_calls(calls, any_order=True)
        self.assertEqual(mock_wait.call_count, 2)
        self.assertEqual(set(done), set(fake_instances))
        self.assertEqual(error_msgs, [])
        self.assertEqual(failed, [])

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testResetInstance(self, mock_wait):
        """Test ResetInstance."""
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.reset = mock.MagicMock()
        self.compute_client.ResetInstance(
            instance=self.INSTANCE, zone=self.ZONE)
        resource_mock.reset.assert_called_with(
            project=PROJECT, zone=self.ZONE, instance=self.INSTANCE)
        mock_wait.assert_called_with(
            mock.ANY,
            operation_scope=gcompute_client.OperationScope.ZONE,
            scope_name=self.ZONE)

    def _CompareMachineSizeTestHelper(self,
                                      machine_info_1,
                                      machine_info_2,
                                      expected_result=None,
                                      expected_error_type=None):
        """Helper class for testing CompareMachineSize.

        Args:
            machine_info_1: A dictionary representing the first machine size.
            machine_info_2: A dictionary representing the second machine size.
            expected_result: An integer, 0, 1 or -1, or None if not set.
            expected_error_type: An exception type, if set will check for exception.
        """
        mock_get_mach_type = self.Patch(
            gcompute_client.ComputeClient,
            "GetMachineType",
            side_effect=[machine_info_1, machine_info_2])
        if expected_error_type:
            self.assertRaises(expected_error_type,
                              self.compute_client.CompareMachineSize, "name1",
                              "name2", self.ZONE)
        else:
            result = self.compute_client.CompareMachineSize("name1", "name2",
                                                            self.ZONE)
            self.assertEqual(result, expected_result)

        mock_get_mach_type.assert_has_calls(
            [mock.call("name1", self.ZONE), mock.call("name2", self.ZONE)])

    def testCompareMachineSizeSmall(self):
        """Test CompareMachineSize where the first one is smaller."""
        machine_info_1 = {"guestCpus": 10, "memoryMb": 100}
        machine_info_2 = {"guestCpus": 10, "memoryMb": 200}
        self._CompareMachineSizeTestHelper(machine_info_1, machine_info_2, -1)

    def testCompareMachineSizeSmallSmallerOnSecond(self):
        """Test CompareMachineSize where the first one is smaller."""
        machine_info_1 = {"guestCpus": 11, "memoryMb": 100}
        machine_info_2 = {"guestCpus": 10, "memoryMb": 200}
        self._CompareMachineSizeTestHelper(machine_info_1, machine_info_2, -1)

    def testCompareMachineSizeLarge(self):
        """Test CompareMachineSize where the first one is larger."""
        machine_info_1 = {"guestCpus": 11, "memoryMb": 200}
        machine_info_2 = {"guestCpus": 10, "memoryMb": 100}
        self._CompareMachineSizeTestHelper(machine_info_1, machine_info_2, 1)

    def testCompareMachineSizeLargeWithEqualElement(self):
        """Test CompareMachineSize where the first one is larger."""
        machine_info_1 = {"guestCpus": 10, "memoryMb": 200}
        machine_info_2 = {"guestCpus": 10, "memoryMb": 100}
        self._CompareMachineSizeTestHelper(machine_info_1, machine_info_2, 1)

    def testCompareMachineSizeEqual(self):
        """Test CompareMachineSize where two machine sizes are equal."""
        machine_info = {"guestCpus": 10, "memoryMb": 100}
        self._CompareMachineSizeTestHelper(machine_info, machine_info, 0)

    def testCompareMachineSizeBadMetric(self):
        """Test CompareMachineSize with bad metric."""
        machine_info = {"unknown_metric": 10, "memoryMb": 100}
        self._CompareMachineSizeTestHelper(
            machine_info, machine_info, expected_error_type=errors.DriverError)

    def testGetMachineType(self):
        """Test GetMachineType."""
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.machineTypes = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.get = mock.MagicMock(return_value=mock_api)
        mock_api.execute = mock.MagicMock(
            return_value={"name": self.MACHINE_TYPE})
        result = self.compute_client.GetMachineType(self.MACHINE_TYPE,
                                                    self.ZONE)
        self.assertEqual(result, {"name": self.MACHINE_TYPE})
        resource_mock.get.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            machineType=self.MACHINE_TYPE)

    def _GetSerialPortOutputTestHelper(self, response):
        """Helper function for testing GetSerialPortOutput.

        Args:
            response: A dictionary representing a fake response.
        """
        resource_mock = mock.MagicMock()
        mock_api = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.getSerialPortOutput = mock.MagicMock(
            return_value=mock_api)
        mock_api.execute = mock.MagicMock(return_value=response)

        if "contents" in response:
            result = self.compute_client.GetSerialPortOutput(
                instance=self.INSTANCE, zone=self.ZONE)
            self.assertEqual(result, "fake contents")
        else:
            six.assertRaisesRegex(
                self,
                errors.DriverError,
                "Malformed response.*",
                self.compute_client.GetSerialPortOutput,
                instance=self.INSTANCE,
                zone=self.ZONE)
        resource_mock.getSerialPortOutput.assert_called_with(
            project=PROJECT,
            zone=self.ZONE,
            instance=self.INSTANCE,
            port=1)

    def testGetSerialPortOutput(self):
        """Test GetSerialPortOutput."""
        response = {"contents": "fake contents"}
        self._GetSerialPortOutputTestHelper(response)

    def testGetSerialPortOutputFail(self):
        """Test GetSerialPortOutputFail."""
        response = {"malformed": "fake contents"}
        self._GetSerialPortOutputTestHelper(response)

    def testGetInstanceNamesByIPs(self):
        """Test GetInstanceNamesByIPs."""
        good_instance = {
            "name": "instance_1",
            "networkInterfaces": [
                {
                    "accessConfigs": [
                        {"natIP": "172.22.22.22"},
                    ],
                },
            ],
        }
        bad_instance = {"name": "instance_2"}
        self.Patch(
            gcompute_client.ComputeClient,
            "ListInstances",
            return_value=[good_instance, bad_instance])
        ip_name_map = self.compute_client.GetInstanceNamesByIPs(
            ips=["172.22.22.22", "172.22.22.23"])
        self.assertEqual(ip_name_map, {"172.22.22.22": "instance_1",
                                       "172.22.22.23": None})

    def testRsaNotInMetadata(self):
        """Test rsa not in metadata."""
        fake_user = "fake_user"
        fake_ssh_key = "fake_ssh"
        metadata = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": "%s:%s" % (fake_user, self.SSHKEY)
                }
            ]
        }
        # Test rsa doesn't exist in metadata.
        new_entry = "%s:%s" % (fake_user, fake_ssh_key)
        self.assertEqual(True, gcompute_client.RsaNotInMetadata(metadata, new_entry))

        # Test rsa exists in metadata.
        exist_entry = "%s:%s" %(fake_user, self.SSHKEY)
        self.assertEqual(False, gcompute_client.RsaNotInMetadata(metadata, exist_entry))

    def testGetSshKeyFromMetadata(self):
        """Test get ssh key from metadata."""
        fake_user = "fake_user"
        metadata_key_exist_value_is_empty = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": ""
                }
            ]
        }
        metadata_key_exist = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": "%s:%s" % (fake_user, self.SSHKEY)
                }
            ]
        }
        metadata_key_not_exist = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                }
            ]
        }
        expected_key_exist_value_is_empty = {
            "key": "sshKeys",
            "value": ""
        }
        expected_key_exist = {
            "key": "sshKeys",
            "value": "%s:%s" % (fake_user, self.SSHKEY)
        }
        self.assertEqual(expected_key_exist_value_is_empty,
                         gcompute_client.GetSshKeyFromMetadata(metadata_key_exist_value_is_empty))
        self.assertEqual(expected_key_exist,
                         gcompute_client.GetSshKeyFromMetadata(metadata_key_exist))
        self.assertEqual(None,
                         gcompute_client.GetSshKeyFromMetadata(metadata_key_not_exist))


    def testGetRsaKeyPathExistsFalse(self):
        """Test the rsa key path not exists."""
        fake_ssh_rsa_path = "/path/to/test_rsa.pub"
        self.Patch(os.path, "exists", return_value=False)
        six.assertRaisesRegex(self,
                              errors.DriverError,
                              "RSA file %s does not exist." % fake_ssh_rsa_path,
                              gcompute_client.GetRsaKey,
                              ssh_rsa_path=fake_ssh_rsa_path)

    def testGetRsaKey(self):
        """Test get the rsa key."""
        fake_ssh_rsa_path = "/path/to/test_rsa.pub"
        self.Patch(os.path, "exists", return_value=True)
        m = mock.mock_open(read_data=self.SSHKEY)
        with mock.patch.object(six.moves.builtins, "open", m):
            result = gcompute_client.GetRsaKey(fake_ssh_rsa_path)
            self.assertEqual(self.SSHKEY, result)

    def testUpdateRsaInMetadata(self):
        """Test update rsa in metadata."""
        fake_ssh_key = "fake_ssh"
        fake_metadata_sshkeys_not_exist = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "not_sshKeys",
                    "value": ""
                }
            ]
        }
        new_entry = "new_user:%s" % fake_ssh_key
        expected = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "not_sshKeys",
                    "value": ""
                },
                {
                    "key": "sshKeys",
                    "value": new_entry
                }
            ]
        }
        self.Patch(os.path, "exists", return_value=True)
        self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        resource_mock = mock.MagicMock()
        self.compute_client.SetInstanceMetadata = mock.MagicMock(
            return_value=resource_mock)
        # Test the key item not exists in the metadata.
        self.compute_client.UpdateRsaInMetadata(
            "fake_zone",
            "fake_instance",
            fake_metadata_sshkeys_not_exist,
            new_entry)
        self.compute_client.SetInstanceMetadata.assert_called_with(
            "fake_zone",
            "fake_instance",
            expected)

        # Test the key item exists in the metadata.
        fake_metadata_ssh_keys_exists = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": "old_user:%s" % self.SSHKEY
                }
            ]
        }
        expected_ssh_keys_exists = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": "old_user:%s\n%s" % (self.SSHKEY, new_entry)
                }
            ]
        }

        self.compute_client.UpdateRsaInMetadata(
            "fake_zone",
            "fake_instance",
            fake_metadata_ssh_keys_exists,
            new_entry)
        self.compute_client.SetInstanceMetadata.assert_called_with(
            "fake_zone",
            "fake_instance",
            expected_ssh_keys_exists)

    def testAddSshRsaToInstance(self):
        """Test add ssh rsa key to instance."""
        fake_user = "fake_user"
        instance_metadata_key_not_exist = {
            "metadata": {
                "kind": "compute#metadata",
                "fingerprint": "a-23icsyx4E=",
                "items": [
                    {
                        "key": "sshKeys",
                        "value": ""
                    }
                ]
            }
        }
        instance_metadata_key_exist = {
            "metadata": {
                "kind": "compute#metadata",
                "fingerprint": "a-23icsyx4E=",
                "items": [
                    {
                        "key": "sshKeys",
                        "value": "%s:%s" % (fake_user, self.SSHKEY)
                    }
                ]
            }
        }
        expected = {
            "kind": "compute#metadata",
            "fingerprint": "a-23icsyx4E=",
            "items": [
                {
                    "key": "sshKeys",
                    "value": "%s:%s" % (fake_user, self.SSHKEY)
                }
            ]
        }

        self.Patch(os.path, "exists", return_value=True)
        m = mock.mock_open(read_data=self.SSHKEY)
        self.Patch(gcompute_client.ComputeClient, "WaitOnOperation")
        self.Patch(gcompute_client.ComputeClient, "GetZoneByInstance",
                   return_value="fake_zone")
        resource_mock = mock.MagicMock()
        self.compute_client._service.instances = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.setMetadata = mock.MagicMock()

        # Test the key not exists in the metadata.
        self.Patch(
            gcompute_client.ComputeClient, "GetInstance",
            return_value=instance_metadata_key_not_exist)
        with mock.patch.object(six.moves.builtins, "open", m):
            self.compute_client.AddSshRsaInstanceMetadata(
                fake_user,
                "/path/to/test_rsa.pub",
                "fake_instance")
            resource_mock.setMetadata.assert_called_with(
                project=PROJECT,
                zone="fake_zone",
                instance="fake_instance",
                body=expected)

        # Test the key already exists in the metadata.
        resource_mock.setMetadata.call_count = 0
        self.Patch(
            gcompute_client.ComputeClient, "GetInstance",
            return_value=instance_metadata_key_exist)
        with mock.patch.object(six.moves.builtins, "open", m):
            self.compute_client.AddSshRsaInstanceMetadata(
                fake_user,
                "/path/to/test_rsa.pub",
                "fake_instance")
            resource_mock.setMetadata.assert_not_called()

    @mock.patch.object(gcompute_client.ComputeClient, "WaitOnOperation")
    def testDeleteDisks(self, mock_wait):
        """Test DeleteDisks."""
        self._SetupBatchHttpRequestMock()
        fake_disks = ["fake_disk_1", "fake_disk_2"]
        mock_api = mock.MagicMock()
        resource_mock = mock.MagicMock()
        self.compute_client._service.disks = mock.MagicMock(
            return_value=resource_mock)
        resource_mock.delete = mock.MagicMock(return_value=mock_api)
        # Call the API.
        deleted, failed, error_msgs = self.compute_client.DeleteDisks(
            fake_disks, zone=self.ZONE)
        # Verify
        calls = [
            mock.call(project=PROJECT, disk="fake_disk_1", zone=self.ZONE),
            mock.call(project=PROJECT, disk="fake_disk_2", zone=self.ZONE)
        ]
        resource_mock.delete.assert_has_calls(calls, any_order=True)
        self.assertEqual(mock_wait.call_count, 2)
        self.assertEqual(error_msgs, [])
        self.assertEqual(failed, [])
        self.assertEqual(set(deleted), set(fake_disks))

    def testRetryOnFingerPrintError(self):
        """Test RetryOnFingerPrintError."""
        @utils.RetryOnException(gcompute_client._IsFingerPrintError, 10)
        def Raise412(sentinel):
            """Raise 412 HTTP exception."""
            if not sentinel.hitFingerPrintConflict.called:
                sentinel.hitFingerPrintConflict()
                raise errors.HttpError(412, "resource labels have changed")
            return "Passed"

        sentinel = mock.MagicMock()
        result = Raise412(sentinel)
        self.assertEqual(1, sentinel.hitFingerPrintConflict.call_count)
        self.assertEqual("Passed", result)

    def testCheckAccess(self):
        """Test CheckAccess."""
        # Checking non-403 should raise error
        error = errors.HttpError(503, "fake retriable error.")
        self.Patch(
            gcompute_client.ComputeClient, "Execute",
            side_effect=error)

        with self.assertRaises(errors.HttpError):
            self.compute_client.CheckAccess()

        # Checking 403 should return False
        error = errors.HttpError(403, "fake retriable error.")
        self.Patch(
            gcompute_client.ComputeClient, "Execute",
            side_effect=error)
        self.assertFalse(self.compute_client.CheckAccess())


if __name__ == "__main__":
    unittest.main()
