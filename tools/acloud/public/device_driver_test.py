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

"""Tests for acloud.public.device_driver."""

import uuid

import unittest
import mock

from acloud.internal.lib import auth
from acloud.internal.lib import android_build_client
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import gstorage_client
from acloud.internal.lib import ssh
from acloud.public import device_driver


def _CreateCfg():
    """A helper method that creates a mock configuration object."""
    cfg = mock.MagicMock()
    cfg.service_account_name = "fake@service.com"
    cfg.service_account_private_key_path = "/fake/path/to/key"
    cfg.zone = "fake_zone"
    cfg.disk_image_name = "fake_image.tar.gz"
    cfg.disk_image_mime_type = "fake/type"
    cfg.storage_bucket_name = "fake_bucket"
    cfg.extra_data_disk_size_gb = 4
    cfg.precreated_data_image_map = {
        4: "extradisk-image-4gb",
        10: "extradisk-image-10gb"
    }
    cfg.extra_scopes = None
    cfg.ssh_private_key_path = ""
    cfg.ssh_public_key_path = ""

    return cfg


class DeviceDriverTest(driver_test_lib.BaseDriverTest):
    """Test device_driver."""

    def setUp(self):
        """Set up the test."""
        super(DeviceDriverTest, self).setUp()
        self.build_client = mock.MagicMock()
        self.Patch(android_build_client, "AndroidBuildClient",
                   return_value=self.build_client)
        self.storage_client = mock.MagicMock()
        self.Patch(
            gstorage_client, "StorageClient", return_value=self.storage_client)
        self.compute_client = mock.MagicMock()
        self.Patch(
            android_compute_client,
            "AndroidComputeClient",
            return_value=self.compute_client)
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())
        self.fake_avd_spec = mock.MagicMock()
        self.fake_avd_spec.unlock_screen = False
        self.fake_avd_spec.client_adb_port = 1234

    def testCreateGCETypeAVD(self):
        """Test CreateGCETypeAVD."""
        cfg = _CreateCfg()
        fake_gs_url = "fake_gs_url"
        fake_ip = ssh.IP(external="140.1.1.1", internal="10.1.1.1")
        fake_instance = "fake-instance"
        fake_image = "fake-image"
        fake_build_target = "fake_target"
        fake_build_id = "12345"

        # Mock uuid
        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)
        fake_gs_object = fake_uuid.hex + "-" + cfg.disk_image_name
        self.storage_client.GetUrl.return_value = fake_gs_url

        # Mock compute client methods
        disk_name = "extradisk-image-4gb"
        self.compute_client.GetInstanceIP.return_value = fake_ip
        self.compute_client.GenerateImageName.return_value = fake_image
        self.compute_client.GenerateInstanceName.return_value = fake_instance
        self.compute_client.GetDataDiskName.return_value = disk_name

        # Verify
        report = device_driver.CreateGCETypeAVD(
            cfg, fake_build_target, fake_build_id, avd_spec=self.fake_avd_spec)
        self.build_client.CopyTo.assert_called_with(
            fake_build_target, fake_build_id, artifact_name=cfg.disk_image_name,
            destination_bucket=cfg.storage_bucket_name,
            destination_path=fake_gs_object)
        self.compute_client.CreateImage.assert_called_with(
            image_name=fake_image, source_uri=fake_gs_url)
        self.compute_client.CreateInstance.assert_called_with(
            instance=fake_instance,
            image_name=fake_image,
            extra_disk_name=disk_name,
            avd_spec=self.fake_avd_spec,
            extra_scopes=None)
        self.compute_client.DeleteImage.assert_called_with(fake_image)
        self.storage_client.Delete(cfg.storage_bucket_name, fake_gs_object)

        self.assertEqual(
            report.data,
            {
                "devices": [
                    {
                        "instance_name": fake_instance,
                        "ip": fake_ip.external,
                    },
                ],
            }
        )
        self.assertEqual(report.command, "create")
        self.assertEqual(report.status, "SUCCESS")

    # pylint: disable=invalid-name
    def testCreateGCETypeAVDInternalIP(self):
        """Test CreateGCETypeAVD with internal IP."""
        cfg = _CreateCfg()
        fake_ip = ssh.IP(external="140.1.1.1", internal="10.1.1.1")
        fake_instance = "fake-instance"
        fake_build_target = "fake_target"
        fake_build_id = "12345"

        self.compute_client.GetInstanceIP.return_value = fake_ip
        self.compute_client.GenerateInstanceName.return_value = fake_instance

        report = device_driver.CreateGCETypeAVD(
            cfg, fake_build_target, fake_build_id, report_internal_ip=True,
            avd_spec=self.fake_avd_spec)

        self.assertEqual(
            report.data,
            {
                "devices": [
                    {
                        "instance_name": fake_instance,
                        "ip": fake_ip.internal,
                    },
                ],
            }
        )

    def testDeleteAndroidVirtualDevices(self):
        """Test DeleteAndroidVirtualDevices."""
        cfg = _CreateCfg()
        instance_names = ["fake-instance-1", "fake-instance-2"]
        self.compute_client.GetZonesByInstances.return_value = (
            {cfg.zone: instance_names})
        self.compute_client.DeleteInstances.return_value = (instance_names, [],
                                                            [])
        report = device_driver.DeleteAndroidVirtualDevices(cfg, instance_names)
        self.compute_client.DeleteInstances.assert_called_once_with(
            instance_names, cfg.zone)
        self.assertEqual(report.data, {
            "deleted": [
                {
                    "name": instance_names[0],
                    "type": "instance",
                },
                {
                    "name": instance_names[1],
                    "type": "instance",
                },
            ],
        })
        self.assertEqual(report.command, "delete")
        self.assertEqual(report.status, "SUCCESS")


if __name__ == "__main__":
    unittest.main()
