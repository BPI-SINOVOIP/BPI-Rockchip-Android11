#!/usr/bin/env python3
#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging
import os
import posixpath as target_path_module
import sys
import unittest

from vts.testcases.vndk import utils
from vts.testcases.vndk.golden import vndk_data
from vts.utils.python.vndk import vndk_utils


class VtsVndkFilesTest(unittest.TestCase):
    """A test for VNDK files and directories.

    Attributes:
        _dut: The AndroidDevice under test.
        _vndk_version: The VNDK version of the device.
    """
    # Some LL-NDK libraries may load the implementations with the same names
    # from /vendor/lib. Since a vendor may install an implementation of an
    # LL-NDK library with the same name, testNoLlndkInVendor doesn't raise
    # errors on these LL-NDK libraries.
    _LL_NDK_COLLIDING_NAMES = ("libEGL.so", "libGLESv1_CM.so", "libGLESv2.so",
                               "libGLESv3.so")
    _TARGET_ODM_LIB = "/odm/{LIB}"
    _TARGET_VENDOR_LIB = "/vendor/{LIB}"

    def setUp(self):
        """Initializes attributes."""
        serial_number = os.environ.get("ANDROID_SERIAL")
        self.assertTrue(serial_number, "$ANDROID_SERIAL is empty.")
        self._dut = utils.AndroidDevice(serial_number)
        self.assertTrue(self._dut.IsRoot(), "This test requires adb root.")
        self._vndk_version = self._dut.GetVndkVersion()

    def _ListFiles(self, dir_path):
        """Lists all files in a directory except subdirectories.

        Args:
            dir_path: A string, path to the directory on device.

        Returns:
            A list of strings, the file paths in the directory.
        """
        if not self._dut.Exists(dir_path):
            logging.info("%s not found", dir_path)
            return []
        return self._dut.FindFiles(dir_path, "*", "!", "-type", "d")

    def _Fail(self, unexpected_paths):
        """Logs error and fails current test.

        Args:
            unexpected_paths: A list of strings, the paths to be shown in the
                              log message.
        """
        logging.error("Unexpected files:\n%s", "\n".join(unexpected_paths))
        assert_lines = unexpected_paths[:20]
        if len(unexpected_paths) > 20:
            assert_lines.append("...")
        assert_lines.append(
            "Total number of errors: %d" % len(unexpected_paths))
        self.fail("\n".join(assert_lines))

    def _TestVndkDirectory(self, vndk_dir, vndk_list_names):
        """Verifies that the VNDK directory doesn't contain extra files.

        Args:
            vndk_dir: The path to the VNDK directory on device.
            vndk_list_names: Strings, the categories of the VNDK libraries
                             that can be in the directory.
        """
        vndk_lists = vndk_data.LoadVndkLibraryListsFromResources(
            self._vndk_version, *vndk_list_names)
        self.assertTrue(vndk_lists, "Cannot load VNDK library lists.")
        vndk_set = set().union(*vndk_lists)
        logging.debug("vndk set: %s", vndk_set)
        unexpected = [x for x in self._ListFiles(vndk_dir) if
                      target_path_module.basename(x) not in vndk_set]
        if unexpected:
            self._Fail(unexpected)

    def _TestNotInVndkDirecotory(self, vndk_dir, vndk_list_names, except_libs):
        """Verifies that VNDK directory doesn't contain specific files.

        Args:
            vndk_dir, The path to the VNDK directory on device.
            vndk_list_names: A list of strings, the categories of the VNDK
                             libraries that should not be in the directory.
            except_libs: A set of strings, the file names of the libraries that
                         are exceptions to this test.
        """
        vndk_lists = vndk_data.LoadVndkLibraryListsFromResources(
            self._vndk_version, *vndk_list_names)
        self.assertTrue(vndk_lists, "Cannot load VNDK library lists.")
        vndk_set = set().union(*vndk_lists)
        vndk_set.difference_update(except_libs)
        logging.debug("vndk set: %s", vndk_set)
        unexpected = [x for x in self._ListFiles(vndk_dir) if
                      target_path_module.basename(x) in vndk_set]
        if unexpected:
            self._Fail(unexpected)

    def _TestVndkCoreDirectory(self, bitness):
        """Verifies that VNDK directory doesn't contain extra files."""
        if not vndk_utils.IsVndkRuntimeEnforced(self._dut):
            logging.info("Skip the test as VNDK runtime is not enforced on "
                         "the device.")
            return
        self._TestVndkDirectory(
            vndk_utils.GetVndkDirectory(bitness, self._vndk_version),
            (vndk_data.VNDK, vndk_data.VNDK_PRIVATE, vndk_data.VNDK_SP,
             vndk_data.VNDK_SP_PRIVATE,))

    def testVndkCoreDirectory32(self):
        """Runs _TestVndkCoreDirectory for 32-bit libraries."""
        self._TestVndkCoreDirectory(32)

    def testVndkCoreDirectory64(self):
        """Runs _TestVndkCoreDirectory for 64-bit libraries."""
        if self._dut.GetCpuAbiList(64):
            self._TestVndkCoreDirectory(64)
        else:
            logging.info("Skip the test as the device doesn't support 64-bit "
                         "ABI.")

    def _TestNoLlndkInVendor(self, bitness):
        """Verifies that vendor partition has no LL-NDK libraries."""
        self._TestNotInVndkDirecotory(
            vndk_utils.FormatVndkPath(self._TARGET_VENDOR_LIB, bitness),
            (vndk_data.LL_NDK,),
            self._LL_NDK_COLLIDING_NAMES)

    def testNoLlndkInVendor32(self):
        """Runs _TestNoLlndkInVendor for 32-bit libraries."""
        self._TestNoLlndkInVendor(32)

    def testNoLlndkInVendor64(self):
        """Runs _TestNoLlndkInVendor for 64-bit libraries."""
        if self._dut.GetCpuAbiList(64):
            self._TestNoLlndkInVendor(64)
        else:
            logging.info("Skip the test as the device doesn't support 64-bit "
                         "ABI.")

    def _TestNoLlndkInOdm(self, bitness):
        """Verifies that odm partition has no LL-NDK libraries."""
        self._TestNotInVndkDirecotory(
            vndk_utils.FormatVndkPath(self._TARGET_ODM_LIB, bitness),
            (vndk_data.LL_NDK,),
            self._LL_NDK_COLLIDING_NAMES)

    def testNoLlndkInOdm32(self):
        """Runs _TestNoLlndkInOdm for 32-bit libraries."""
        self._TestNoLlndkInOdm(32)

    def testNoLlndkInOdm64(self):
        """Runs _TestNoLlndkInOdm for 64-bit libraries."""
        if self._dut.GetCpuAbiList(64):
            self._TestNoLlndkInOdm(64)
        else:
            logging.info("Skip the test as the device doesn't support 64-bit "
                         "ABI.")


if __name__ == "__main__":
    # The logs are written to stdout so that TradeFed test runner can parse the
    # results from stderr.
    logging.basicConfig(stream=sys.stdout)
    # Setting verbosity is required to generate output that the TradeFed test
    # runner can parse.
    unittest.main(verbosity=3)
