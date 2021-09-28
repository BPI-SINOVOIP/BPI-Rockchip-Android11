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
"""Tests for remote_image_local_instance."""

import unittest
from collections import namedtuple
import os
import mock

from acloud import errors
from acloud.create import create_common
from acloud.create import remote_image_local_instance
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import utils
from acloud.setup import setup_common


# pylint: disable=invalid-name, protected-access
class RemoteImageLocalInstanceTest(driver_test_lib.BaseDriverTest):
    """Test remote_image_local_instance methods."""

    def setUp(self):
        """Initialize remote_image_local_instance."""
        super(RemoteImageLocalInstanceTest, self).setUp()
        self.build_client = mock.MagicMock()
        self.Patch(
            android_build_client,
            "AndroidBuildClient",
            return_value=self.build_client)
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())
        self.RemoteImageLocalInstance = remote_image_local_instance.RemoteImageLocalInstance()
        self._fake_remote_image = {"build_target" : "aosp_cf_x86_phone-userdebug",
                                   "build_id": "1234"}
        self._extract_path = "/tmp/acloud_image_artifacts/1234"

    @mock.patch.object(remote_image_local_instance, "DownloadAndProcessImageFiles")
    def testGetImageArtifactsPath(self, mock_proc):
        """Test get image artifacts path."""
        avd_spec = mock.MagicMock()
        # raise errors.NoCuttlefishCommonInstalled
        self.Patch(setup_common, "PackageInstalled", return_value=False)
        self.assertRaises(errors.NoCuttlefishCommonInstalled,
                          self.RemoteImageLocalInstance.GetImageArtifactsPath,
                          avd_spec)

        # Valid _DownloadAndProcessImageFiles run.
        self.Patch(setup_common, "PackageInstalled", return_value=True)
        self.Patch(remote_image_local_instance,
                   "ConfirmDownloadRemoteImageDir", return_value="/tmp")
        self.Patch(os.path, "exists", return_value=True)
        self.RemoteImageLocalInstance.GetImageArtifactsPath(avd_spec)
        mock_proc.assert_called_once_with(avd_spec)

    @mock.patch.object(create_common, "DownloadRemoteArtifact")
    def testDownloadAndProcessImageFiles(self, mock_download):
        """Test process remote cuttlefish image."""
        avd_spec = mock.MagicMock()
        avd_spec.cfg = mock.MagicMock()
        avd_spec.remote_image = self._fake_remote_image
        avd_spec.image_download_dir = "/tmp"
        self.Patch(os.path, "exists", return_value=False)
        self.Patch(os, "makedirs")
        remote_image_local_instance.DownloadAndProcessImageFiles(avd_spec)
        build_id = "1234"
        build_target = "aosp_cf_x86_phone-userdebug"
        checkfile1 = "aosp_cf_x86_phone-img-1234.zip"
        checkfile2 = "cvd-host_package.tar.gz"

        # To validate DownloadArtifact runs twice.
        self.assertEqual(mock_download.call_count, 2)

        # To validate DownloadArtifact arguments correct.
        mock_download.assert_has_calls([
            mock.call(avd_spec.cfg, build_target, build_id, checkfile1,
                      self._extract_path, decompress=True),
            mock.call(avd_spec.cfg, build_target, build_id, checkfile2,
                      self._extract_path, decompress=True)], any_order=True)

    def testConfirmDownloadRemoteImageDir(self):
        """Test confirm download remote image dir"""
        self.Patch(os.path, "exists", return_value=True)
        self.Patch(os, "makedirs")
        # Default minimum avail space should be more than 10G
        # then return download_dir directly.
        self.Patch(os, "statvfs", return_value=namedtuple(
            "statvfs", "f_bavail, f_bsize")(11, 1073741824))
        download_dir = "/tmp"
        self.assertEqual(
            remote_image_local_instance.ConfirmDownloadRemoteImageDir(
                download_dir), "/tmp")

        # Test when insuficient disk space and input 'q' to exit.
        self.Patch(os, "statvfs", return_value=namedtuple(
            "statvfs", "f_bavail, f_bsize")(9, 1073741824))
        self.Patch(utils, "InteractWithQuestion", return_value="q")
        self.assertRaises(SystemExit,
                          remote_image_local_instance.ConfirmDownloadRemoteImageDir,
                          download_dir)

        # If avail space detect as 9GB, and 2nd input 7GB both less than 10GB
        # 3rd input over 10GB, so return path should be "/tmp3".
        self.Patch(os, "statvfs", side_effect=[
            namedtuple("statvfs", "f_bavail, f_bsize")(9, 1073741824),
            namedtuple("statvfs", "f_bavail, f_bsize")(7, 1073741824),
            namedtuple("statvfs", "f_bavail, f_bsize")(11, 1073741824)])
        self.Patch(utils, "InteractWithQuestion", side_effect=["/tmp2",
                                                               "/tmp3"])
        self.assertEqual(
            remote_image_local_instance.ConfirmDownloadRemoteImageDir(
                download_dir), "/tmp3")

        # Test when path not exist, define --image-download-dir
        # enter anything else to exit out.
        download_dir = "/image_download_dir1"
        self.Patch(os.path, "exists", return_value=False)
        self.Patch(utils, "InteractWithQuestion", return_value="")
        self.assertRaises(SystemExit,
                          remote_image_local_instance.ConfirmDownloadRemoteImageDir,
                          download_dir)

        # Test using --image-dowload-dir and makedirs.
        # enter 'y' to create it.
        self.Patch(utils, "InteractWithQuestion", return_value="y")
        self.Patch(os, "statvfs", return_value=namedtuple(
            "statvfs", "f_bavail, f_bsize")(10, 1073741824))
        self.assertEqual(
            remote_image_local_instance.ConfirmDownloadRemoteImageDir(
                download_dir), "/image_download_dir1")

        # Test when 1st check fails for insufficient disk space, user inputs an
        # alternate dir but it doesn't exist and the user choose to exit.
        self.Patch(os, "statvfs", side_effect=[
            namedtuple("statvfs", "f_bavail, f_bsize")(9, 1073741824),
            namedtuple("statvfs", "f_bavail, f_bsize")(11, 1073741824)])
        self.Patch(os.path, "exists", side_effect=[True, False])
        self.Patch(utils, "InteractWithQuestion",
                   side_effect=["~/nopath", "not_y"])
        self.assertRaises(
            SystemExit,
            remote_image_local_instance.ConfirmDownloadRemoteImageDir,
            download_dir)

        # Test when 1st check fails for insufficient disk space, user inputs an
        # alternate dir but it doesn't exist and they request to create it.
        self.Patch(os, "statvfs", side_effect=[
            namedtuple("statvfs", "f_bavail, f_bsize")(9, 1073741824),
            namedtuple("statvfs", "f_bavail, f_bsize")(10, 1073741824)])
        self.Patch(os.path, "exists", side_effect=[True, False])
        self.Patch(utils, "InteractWithQuestion", side_effect=["~/nopath", "y"])
        self.assertEqual(
            remote_image_local_instance.ConfirmDownloadRemoteImageDir(
                download_dir), os.path.expanduser("~/nopath"))


if __name__ == "__main__":
    unittest.main()
