# Copyright 2019 - The Android Open Source Project
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
"""Tests for remote_instance_cf_device_factory."""

import glob
import os
import shutil
import tempfile
import unittest
import uuid

import mock

from acloud.create import avd_spec
from acloud.internal import constants
from acloud.create import create_common
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import cvd_compute_client_multi_stage
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import ssh
from acloud.internal.lib import utils
from acloud.list import list as list_instances
from acloud.public.actions import remote_instance_cf_device_factory


class RemoteInstanceDeviceFactoryTest(driver_test_lib.BaseDriverTest):
    """Test RemoteInstanceDeviceFactory method."""

    def setUp(self):
        """Set up the test."""
        super(RemoteInstanceDeviceFactoryTest, self).setUp()
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())
        self.Patch(android_build_client.AndroidBuildClient, "InitResourceHandle")
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "InitResourceHandle")
        self.Patch(list_instances, "GetInstancesFromInstanceNames", return_value=mock.MagicMock())
        self.Patch(list_instances, "ChooseOneRemoteInstance", return_value=mock.MagicMock())
        self.Patch(utils, "GetBuildEnvironmentVariable",
                   return_value="test_environ")
        self.Patch(glob, "glob", return_vale=["fake.img"])

    # pylint: disable=protected-access
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_FetchBuild")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_UploadArtifacts")
    def testProcessArtifacts(self, mock_upload, mock_download):
        """test ProcessArtifacts."""
        # Test image source type is local.
        args = mock.MagicMock()
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.local_image = None
        avd_spec_local_img = avd_spec.AVDSpec(args)
        fake_image_name = "/fake/aosp_cf_x86_phone-img-eng.username.zip"
        fake_host_package_name = "/fake/host_package.tar.gz"
        factory_local_img = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec_local_img,
            fake_image_name,
            fake_host_package_name)
        factory_local_img._ProcessArtifacts(constants.IMAGE_SRC_LOCAL)
        self.assertEqual(mock_upload.call_count, 1)

        # Test image source type is remote.
        args.local_image = ""
        args.build_id = "1234"
        args.branch = "fake_branch"
        args.build_target = "fake_target"
        args.system_build_id = "2345"
        args.system_branch = "sys_branch"
        args.system_build_target = "sys_target"
        args.kernel_build_id = "3456"
        args.kernel_branch = "kernel_branch"
        args.kernel_build_target = "kernel_target"
        avd_spec_remote_img = avd_spec.AVDSpec(args)
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "UpdateFetchCvd")
        factory_remote_img = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec_remote_img)
        factory_remote_img._ProcessArtifacts(constants.IMAGE_SRC_REMOTE)
        self.assertEqual(mock_download.call_count, 1)

    # pylint: disable=protected-access
    @mock.patch.dict(os.environ, {constants.ENV_BUILD_TARGET:'fake-target'})
    def testCreateGceInstanceNameMultiStage(self):
        """test create gce instance."""
        # Mock uuid
        args = mock.MagicMock()
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.local_image = None
        args.adb_port = None
        fake_avd_spec = avd_spec.AVDSpec(args)
        fake_avd_spec.cfg.enable_multi_stage = True
        fake_avd_spec._instance_name_to_reuse = None

        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "CreateInstance")
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = "/fake/aosp_cf_x86_phone-img-eng.username.zip"

        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._CreateGceInstance(), "ins-1234-userbuild-aosp-cf-x86-phone")

        # Can't get target name from zip file name.
        fake_image_name = "/fake/aosp_cf_x86_phone.username.zip"
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._CreateGceInstance(), "ins-1234-userbuild-fake-target")

        # No image zip path, it uses local build images.
        fake_image_name = ""
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._CreateGceInstance(), "ins-1234-userbuild-fake-target")

    @mock.patch.dict(os.environ, {constants.ENV_BUILD_TARGET:'fake-target'})
    def testRemoteHostInstanceName(self):
        """Test Remote host instance name."""
        args = mock.MagicMock()
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.remote_host = "1.1.1.1"
        args.local_image = None
        args.adb_port = None
        fake_avd_spec = avd_spec.AVDSpec(args)
        fake_avd_spec.cfg.enable_multi_stage = True
        fake_avd_spec._instance_name_to_reuse = None
        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "CreateInstance")
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "InitRemoteHost")
        fake_host_package_name = "/fake/host_package.tar.gz"

        fake_image_name = "/fake/aosp_cf_x86_phone-img-eng.username.zip"
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._InitRemotehost(), "host-1.1.1.1-userbuild-aosp_cf_x86_phone")

        # No image zip path, it uses local build images.
        fake_image_name = ""
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._InitRemotehost(), "host-1.1.1.1-userbuild-fake-target")

    def testReuseInstanceNameMultiStage(self):
        """Test reuse instance name."""
        args = mock.MagicMock()
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.local_image = None
        args.adb_port = None
        fake_avd_spec = avd_spec.AVDSpec(args)
        fake_avd_spec.cfg.enable_multi_stage = True
        fake_avd_spec._instance_name_to_reuse = "fake-1234-userbuild-fake-target"
        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)
        self.Patch(cvd_compute_client_multi_stage.CvdComputeClient, "CreateInstance")
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = "/fake/aosp_cf_x86_phone-img-eng.username.zip"
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory._CreateGceInstance(), "fake-1234-userbuild-fake-target")

    def testGetBuildInfoDict(self):
        """Test GetBuildInfoDict."""
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = "/fake/aosp_cf_x86_phone-img-eng.username.zip"
        args = mock.MagicMock()
        # Test image source type is local.
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.local_image = "fake_local_image"
        args.adb_port = None
        avd_spec_local_image = avd_spec.AVDSpec(args)
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec_local_image,
            fake_image_name,
            fake_host_package_name)
        self.assertEqual(factory.GetBuildInfoDict(), None)

        # Test image source type is remote.
        args.local_image = ""
        args.build_id = "123"
        args.branch = "fake_branch"
        args.build_target = "fake_target"
        args.system_build_id = "234"
        args.system_branch = "sys_branch"
        args.system_build_target = "sys_target"
        args.kernel_build_id = "345"
        args.kernel_branch = "kernel_branch"
        args.kernel_build_target = "kernel_target"
        avd_spec_remote_image = avd_spec.AVDSpec(args)
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec_remote_image,
            fake_image_name,
            fake_host_package_name)
        expected_build_info = {
            "build_id": "123",
            "build_branch": "fake_branch",
            "build_target": "fake_target",
            "system_build_id": "234",
            "system_build_branch": "sys_branch",
            "system_build_target": "sys_target",
            "kernel_build_id": "345",
            "kernel_build_branch": "kernel_branch",
            "kernel_build_target": "kernel_target"
        }
        self.assertEqual(factory.GetBuildInfoDict(), expected_build_info)

    @mock.patch.object(ssh, "ShellCmdWithRetry")
    @mock.patch.object(ssh.Ssh, "Run")
    def testUploadArtifacts(self, mock_ssh_run, mock_shell):
        """Test UploadArtifacts."""
        fake_host_package = "/fake/host_package.tar.gz"
        fake_image = "/fake/aosp_cf_x86_phone-img-eng.username.zip"
        fake_local_image_dir = "/fake_image"
        fake_ip = ssh.IP(external="1.1.1.1", internal="10.1.1.1")
        args = mock.MagicMock()
        # Test local image extract from image zip case.
        args.config_file = ""
        args.avd_type = constants.TYPE_CF
        args.flavor = "phone"
        args.local_image = "fake_local_image"
        args.adb_port = None
        avd_spec_local_image = avd_spec.AVDSpec(args)
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec_local_image,
            fake_image,
            fake_host_package)
        factory._ssh = ssh.Ssh(ip=fake_ip,
                               user=constants.GCE_USER,
                               ssh_private_key_path="/fake/acloud_rea")
        factory._UploadArtifacts(fake_image, fake_host_package, fake_local_image_dir)
        expected_cmd1 = ("/usr/bin/install_zip.sh . < %s" % fake_image)
        expected_cmd2 = ("tar -x -z -f - < %s" % fake_host_package)
        mock_ssh_run.assert_has_calls([
            mock.call(expected_cmd1),
            mock.call(expected_cmd2)])

        # Test local image get from local folder case.
        fake_image = None
        self.Patch(glob, "glob", return_value=["fake.img"])
        factory._UploadArtifacts(fake_image, fake_host_package, fake_local_image_dir)
        expected_cmd = (
            "tar -cf - --lzop -S -C %s fake.img | "
            "%s -- tar -xf - --lzop -S" %
            (fake_local_image_dir, factory._ssh.GetBaseCmd(constants.SSH_BIN)))
        mock_shell.assert_called_once_with(expected_cmd)

    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_InitRemotehost")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_UploadArtifacts")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_LaunchCvd")
    def testLocalImageRemoteHost(self, mock_launchcvd, mock_upload, mock_init_remote_host):
        """Test local image with remote host."""
        self.Patch(
            cvd_compute_client_multi_stage,
            "CvdComputeClient",
            return_value=mock.MagicMock())
        fake_avd_spec = mock.MagicMock()
        fake_avd_spec.instance_type = constants.INSTANCE_TYPE_HOST
        fake_avd_spec.image_source = constants.IMAGE_SRC_LOCAL
        fake_avd_spec._instance_name_to_reuse = None
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = ""
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        factory.CreateInstance()
        self.assertEqual(mock_init_remote_host.call_count, 1)
        self.assertEqual(mock_upload.call_count, 1)
        self.assertEqual(mock_launchcvd.call_count, 1)

    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_CreateGceInstance")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_UploadArtifacts")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_LaunchCvd")
    def testLocalImageCreateInstance(self, mock_launchcvd, mock_upload, mock_create_gce_instance):
        """Test local image with create instance."""
        self.Patch(
            cvd_compute_client_multi_stage,
            "CvdComputeClient",
            return_value=mock.MagicMock())
        fake_avd_spec = mock.MagicMock()
        fake_avd_spec.instance_type = constants.INSTANCE_TYPE_REMOTE
        fake_avd_spec.image_source = constants.IMAGE_SRC_LOCAL
        fake_avd_spec._instance_name_to_reuse = None
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = ""
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        factory.CreateInstance()
        self.assertEqual(mock_create_gce_instance.call_count, 1)
        self.assertEqual(mock_upload.call_count, 1)
        self.assertEqual(mock_launchcvd.call_count, 1)

    # pylint: disable=no-member
    @mock.patch.object(create_common, "DownloadRemoteArtifact")
    def testDownloadArtifacts(self, mock_download):
        """Test process remote cuttlefish image."""
        extract_path = "/tmp/1111/"
        fake_remote_image = {"build_target" : "aosp_cf_x86_phone-userdebug",
                             "build_id": "1234"}
        self.Patch(
            cvd_compute_client_multi_stage,
            "CvdComputeClient",
            return_value=mock.MagicMock())
        self.Patch(tempfile, "mkdtemp", return_value="/tmp/1111/")
        self.Patch(shutil, "rmtree")
        fake_avd_spec = mock.MagicMock()
        fake_avd_spec.cfg = mock.MagicMock()
        fake_avd_spec.remote_image = fake_remote_image
        fake_avd_spec.image_download_dir = "/tmp"
        self.Patch(os.path, "exists", return_value=False)
        self.Patch(os, "makedirs")
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec)
        factory._DownloadArtifacts(extract_path)
        build_id = "1234"
        build_target = "aosp_cf_x86_phone-userdebug"
        checkfile1 = "aosp_cf_x86_phone-img-1234.zip"
        checkfile2 = "cvd-host_package.tar.gz"

        # To validate DownloadArtifact runs twice.
        self.assertEqual(mock_download.call_count, 2)

        # To validate DownloadArtifact arguments correct.
        mock_download.assert_has_calls([
            mock.call(fake_avd_spec.cfg, build_target, build_id, checkfile1,
                      extract_path, decompress=True),
            mock.call(fake_avd_spec.cfg, build_target, build_id, checkfile2,
                      extract_path)], any_order=True)

    @mock.patch.object(create_common, "DownloadRemoteArtifact")
    @mock.patch.object(remote_instance_cf_device_factory.RemoteInstanceDeviceFactory,
                       "_UploadArtifacts")
    def testProcessRemoteHostArtifacts(self, mock_upload, mock_download):
        """Test process remote host artifacts."""
        self.Patch(
            cvd_compute_client_multi_stage,
            "CvdComputeClient",
            return_value=mock.MagicMock())
        fake_avd_spec = mock.MagicMock()

        # Test process remote host artifacts with local images.
        fake_avd_spec.instance_type = constants.INSTANCE_TYPE_HOST
        fake_avd_spec.image_source = constants.IMAGE_SRC_LOCAL
        fake_avd_spec._instance_name_to_reuse = None
        fake_host_package_name = "/fake/host_package.tar.gz"
        fake_image_name = ""
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec,
            fake_image_name,
            fake_host_package_name)
        factory._ProcessRemoteHostArtifacts()
        self.assertEqual(mock_upload.call_count, 1)

        # Test process remote host artifacts with remote images.
        fake_tmp_folder = "/tmp/1111/"
        mock_upload.call_count = 0
        self.Patch(tempfile, "mkdtemp", return_value=fake_tmp_folder)
        self.Patch(shutil, "rmtree")
        fake_avd_spec.instance_type = constants.INSTANCE_TYPE_HOST
        fake_avd_spec.image_source = constants.IMAGE_SRC_REMOTE
        fake_avd_spec._instance_name_to_reuse = None
        factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            fake_avd_spec)
        factory._ProcessRemoteHostArtifacts()
        self.assertEqual(mock_upload.call_count, 1)
        self.assertEqual(mock_download.call_count, 2)
        shutil.rmtree.assert_called_once_with(fake_tmp_folder)


if __name__ == "__main__":
    unittest.main()
