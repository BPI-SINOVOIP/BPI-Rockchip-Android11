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
"""Tests for OtaTools."""

import os
import shutil
import tempfile
import unittest
import mock

from acloud import errors
from acloud.internal.lib import ota_tools

_INPUT_MISC_INFO = """
mkbootimg_args=
lpmake=lpmake
dynamic_partition_list= system vendor
vendor_image=/path/to/vendor.img
super_super_device_size=3229614080
"""

_EXPECTED_MISC_INFO = """
mkbootimg_args=
lpmake=%s
dynamic_partition_list= system vendor
super_super_device_size=3229614080
system_image=/path/to/system.img
vendor_image=/path/to/vendor.img
"""

_INPUT_SYSTEM_QEMU_CONFIG = """
out/target/product/generic_x86_64/vbmeta.img vbmeta 1
out/target/product/generic_x86_64/super.img super 2
"""

_EXPECTED_SYSTEM_QEMU_CONFIG = """
/path/to/vbmeta.img vbmeta 1
/path/to/super.img super 2
"""


def _GetImage(name):
    """Return the image path that appears in the expected output."""
    return "/path/to/" + name + ".img"


class CapturedFile(object):
    """Capture intermediate files created by OtaTools."""

    def __init__(self):
        self.path = None
        self.contents = None

    def Load(self, path):
        """Load file contents to this object."""
        self.path = path
        if not os.path.isfile(path):
            return
        with open(path, "r") as f:
            self.contents = f.read()


class OtaToolsTest(unittest.TestCase):
    """Test OtaToolsTest methods."""

    def setUp(self):
        self._temp_dir = tempfile.mkdtemp()
        os.mkdir(os.path.join(self._temp_dir, "bin"))
        self._ota = ota_tools.OtaTools(self._temp_dir)
        self._captured_files = []

    def tearDown(self):
        shutil.rmtree(self._temp_dir)
        for path in self._captured_files:
            if os.path.isfile(path):
                os.remove(path)

    @staticmethod
    def _CreateFile(path, contents):
        """Create and write to a file."""
        parent_dir = os.path.dirname(path)
        if not os.path.exists(parent_dir):
            os.makedirs(parent_dir)
        with open(path, "w") as f:
            f.write(contents)

    def _CreateBinary(self, name):
        """Create an empty file in bin directory."""
        path = os.path.join(self._temp_dir, "bin", name)
        self._CreateFile(path, "")
        return path

    @staticmethod
    def _MockPopen(return_value):
        """Create a mock Popen object."""
        popen = mock.Mock()
        popen.communicate.return_value = ("stdout", "stderr")
        popen.returncode = return_value
        popen.poll.return_value = return_value
        return popen

    @staticmethod
    def _MockPopenTimeout():
        """Create a mock Popen object that raises timeout error."""
        popen = mock.Mock()
        popen.communicate.side_effect = errors.FunctionTimeoutError(
            "unit test")
        popen.returncode = None
        popen.poll.return_value = None
        return popen

    def testFindOtaTools(self):
        """Test FindOtaTools."""
        # CVD host package contains lpmake but not all tools.
        self._CreateBinary("lpmake")
        with mock.patch.dict("acloud.internal.lib.ota_tools.os.environ",
                             {"ANDROID_HOST_OUT": self._temp_dir}, clear=True):
            with self.assertRaises(errors.CheckPathError):
                ota_tools.FindOtaTools([self._temp_dir])

        # The function identifies OTA tool directory by build_super_image.
        self._CreateBinary("build_super_image")
        with mock.patch.dict("acloud.internal.lib.ota_tools.os.environ",
                             dict(), clear=True):
            self.assertEqual(ota_tools.FindOtaTools([self._temp_dir]),
                             self._temp_dir)

        # ANDROID_HOST_OUT contains OTA tools in build environment.
        with mock.patch.dict("acloud.internal.lib.ota_tools.os.environ",
                             {"ANDROID_HOST_OUT": self._temp_dir}, clear=True):
            self.assertEqual(ota_tools.FindOtaTools([]), self._temp_dir)

    # pylint: disable=broad-except
    def _TestBuildSuperImage(self, mock_popen, mock_popen_object,
                             expected_error=None):
        """Test BuildSuperImage.

        Args:
            mock_popen: Mock class of subprocess.Popen.
            popen_return_value: Mock object of subprocess.Popen.
            expected_error: The error type that BuildSuperImage should raise.
        """
        build_super_image = self._CreateBinary("build_super_image")
        lpmake = self._CreateBinary("lpmake")

        misc_info_path = os.path.join(self._temp_dir, "misc_info.txt")
        self._CreateFile(misc_info_path, _INPUT_MISC_INFO)

        rewritten_misc_info = CapturedFile()

        def _CaptureMiscInfo(cmd, **_kwargs):
            self._captured_files.append(cmd[1])
            rewritten_misc_info.Load(cmd[1])
            return mock_popen_object

        mock_popen.side_effect = _CaptureMiscInfo

        try:
            self._ota.BuildSuperImage("/unit/test", misc_info_path, _GetImage)
            if expected_error:
                self.fail(expected_error.__name__ + " is not raised.")
        except Exception as e:
            if not expected_error or not isinstance(e, expected_error):
                raise

        expected_cmd = (
            build_super_image,
            rewritten_misc_info.path,
            "/unit/test",
        )

        mock_popen.assert_called_once()
        self.assertEqual(mock_popen.call_args[0][0], expected_cmd)
        self.assertEqual(rewritten_misc_info.contents,
                         _EXPECTED_MISC_INFO % lpmake)
        self.assertFalse(os.path.exists(rewritten_misc_info.path))

    @mock.patch("acloud.internal.lib.ota_tools.subprocess.Popen")
    def testBuildSuperImageSuccess(self, mock_popen):
        """Test BuildSuperImage."""
        self._TestBuildSuperImage(mock_popen, self._MockPopen(return_value=0))

    @mock.patch("acloud.internal.lib.ota_tools.subprocess.Popen")
    def testBuildSuperImageTimeout(self, mock_popen):
        """Test BuildSuperImage with command timeout."""
        self._TestBuildSuperImage(mock_popen, self._MockPopenTimeout(),
                                  errors.FunctionTimeoutError)

    @mock.patch("acloud.internal.lib.ota_tools.subprocess.Popen")
    def testMakeDisabledVbmetaImageSuccess(self, mock_popen):
        """Test MakeDisabledVbmetaImage."""
        avbtool = self._CreateBinary("avbtool")

        mock_popen.return_value = self._MockPopen(return_value=0)

        self._ota.MakeDisabledVbmetaImage("/unit/test")

        expected_cmd = (
            avbtool, "make_vbmeta_image",
            "--flag", "2",
            "--padding_size", "4096",
            "--output", "/unit/test",
        )

        mock_popen.assert_called_once()
        self.assertEqual(mock_popen.call_args[0][0], expected_cmd)

    # pylint: disable=broad-except
    def _TestMkCombinedImg(self, mock_popen, mock_popen_object,
                           expected_error=None):
        """Test MkCombinedImg.

        Args:
            mock_popen: Mock class of subprocess.Popen.
            mock_popen_object: Mock object of subprocess.Popen.
            expected_error: The error type that MkCombinedImg should raise.
        """
        mk_combined_img = self._CreateBinary("mk_combined_img")
        sgdisk = self._CreateBinary("sgdisk")
        simg2img = self._CreateBinary("simg2img")

        config_path = os.path.join(self._temp_dir, "misc_info.txt")
        self._CreateFile(config_path, _INPUT_SYSTEM_QEMU_CONFIG)

        rewritten_config = CapturedFile()

        def _CaptureSystemQemuConfig(cmd, **_kwargs):
            self._captured_files.append(cmd[2])
            rewritten_config.Load(cmd[2])
            return mock_popen_object

        mock_popen.side_effect = _CaptureSystemQemuConfig

        try:
            self._ota.MkCombinedImg("/unit/test", config_path, _GetImage)
            if expected_error:
                self.fail(expected_error.__name__ + " is not raised.")
        except Exception as e:
            if not expected_error or not isinstance(e, expected_error):
                raise

        expected_cmd = (
            mk_combined_img,
            "-i", rewritten_config.path,
            "-o", "/unit/test",
        )

        expected_env = {"SGDISK": sgdisk, "SIMG2IMG": simg2img}

        mock_popen.assert_called_once()
        self.assertEqual(mock_popen.call_args[0][0], expected_cmd)
        self.assertEqual(mock_popen.call_args[1].get("env"), expected_env)
        self.assertEqual(rewritten_config.contents,
                         _EXPECTED_SYSTEM_QEMU_CONFIG)
        self.assertFalse(os.path.exists(rewritten_config.path))

    @mock.patch("acloud.internal.lib.ota_tools.subprocess.Popen")
    def testMkCombinedImgSuccess(self, mock_popen):
        """Test MkCombinedImg."""
        return self._TestMkCombinedImg(mock_popen,
                                       self._MockPopen(return_value=0))

    @mock.patch("acloud.internal.lib.ota_tools.subprocess.Popen")
    def testMkCombinedImgFailure(self, mock_popen):
        """Test MkCombinedImg with command failure."""
        return self._TestMkCombinedImg(mock_popen,
                                       self._MockPopen(return_value=1),
                                       errors.SubprocessFail)


if __name__ == "__main__":
    unittest.main()
