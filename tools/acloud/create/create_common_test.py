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
"""Tests for create_common."""

import os
import unittest

import mock

from acloud import errors
from acloud.create import create_common
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import utils


class FakeZipFile(object):
    """Fake implementation of ZipFile()"""

    # pylint: disable=invalid-name,unused-argument,no-self-use
    def write(self, filename, arcname=None, compress_type=None):
        """Fake write method."""
        return

    # pylint: disable=invalid-name,no-self-use
    def close(self):
        """Fake close method."""
        return


# pylint: disable=invalid-name,protected-access
class CreateCommonTest(driver_test_lib.BaseDriverTest):
    """Test create_common functions."""

    # pylint: disable=protected-access
    def testProcessHWPropertyWithInvalidArgs(self):
        """Test ParseHWPropertyArgs with invalid args."""
        # Checking wrong property value.
        args_str = "cpu:3,disk:"
        with self.assertRaises(errors.MalformedDictStringError):
            create_common.ParseHWPropertyArgs(args_str)

        # Checking wrong property format.
        args_str = "cpu:3,disk"
        with self.assertRaises(errors.MalformedDictStringError):
            create_common.ParseHWPropertyArgs(args_str)

    def testParseHWPropertyStr(self):
        """Test ParseHWPropertyArgs."""
        expected_dict = {"cpu": "2", "resolution": "1080x1920", "dpi": "240",
                         "memory": "4g", "disk": "4g"}
        args_str = "cpu:2,resolution:1080x1920,dpi:240,memory:4g,disk:4g"
        result_dict = create_common.ParseHWPropertyArgs(args_str)
        self.assertTrue(expected_dict == result_dict)

    def testGetCvdHostPackage(self):
        """test GetCvdHostPackage."""
        # Can't find the cvd host package
        with mock.patch("os.path.exists") as exists:
            exists.return_value = False
            self.assertRaises(
                errors.GetCvdLocalHostPackageError,
                create_common.GetCvdHostPackage)

        self.Patch(os.environ, "get", return_value="/fake_dir2")
        self.Patch(utils, "GetDistDir", return_value="/fake_dir1")
        # First path is host out dir, 2nd path is dist dir.
        self.Patch(os.path, "exists",
                   side_effect=[False, True])

        # Find cvd host in dist dir.
        self.assertEqual(
            create_common.GetCvdHostPackage(),
            "/fake_dir1/cvd-host_package.tar.gz")

        # Find cvd host in host out dir.
        self.Patch(os.environ, "get", return_value="/fake_dir2")
        self.Patch(utils, "GetDistDir", return_value=None)
        with mock.patch("os.path.exists") as exists:
            exists.return_value = True
            self.assertEqual(
                create_common.GetCvdHostPackage(),
                "/fake_dir2/cvd-host_package.tar.gz")

    @mock.patch.object(utils, "Decompress")
    def testDownloadRemoteArtifact(self, mock_decompress):
        """Test Download cuttlefish package."""
        mock_build_client = mock.MagicMock()
        self.Patch(
            android_build_client,
            "AndroidBuildClient",
            return_value=mock_build_client)
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())
        avd_spec = mock.MagicMock()
        avd_spec.cfg = mock.MagicMock()
        avd_spec.remote_image = {"build_target" : "aosp_cf_x86_phone-userdebug",
                                 "build_id": "1234"}
        build_id = "1234"
        build_target = "aosp_cf_x86_phone-userdebug"
        checkfile1 = "aosp_cf_x86_phone-img-1234.zip"
        checkfile2 = "cvd-host_package.tar.gz"
        extract_path = "/tmp/1234"

        create_common.DownloadRemoteArtifact(
            avd_spec.cfg,
            avd_spec.remote_image["build_target"],
            avd_spec.remote_image["build_id"],
            checkfile1,
            extract_path,
            decompress=True)

        self.assertEqual(mock_build_client.DownloadArtifact.call_count, 1)
        mock_build_client.DownloadArtifact.assert_called_once_with(
            build_target,
            build_id,
            checkfile1,
            "%s/%s" % (extract_path, checkfile1))
        self.assertEqual(mock_decompress.call_count, 1)

        mock_decompress.call_count = 0
        mock_build_client.DownloadArtifact.call_count = 0
        create_common.DownloadRemoteArtifact(
            avd_spec.cfg,
            avd_spec.remote_image["build_target"],
            avd_spec.remote_image["build_id"],
            checkfile2,
            extract_path)

        self.assertEqual(mock_build_client.DownloadArtifact.call_count, 1)
        mock_build_client.DownloadArtifact.assert_called_once_with(
            build_target,
            build_id,
            checkfile2,
            "%s/%s" % (extract_path, checkfile2))
        self.assertEqual(mock_decompress.call_count, 0)


if __name__ == "__main__":
    unittest.main()
