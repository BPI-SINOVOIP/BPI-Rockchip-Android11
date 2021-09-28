#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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
"""VTS tests to verify boot/recovery image header versions."""

import logging
import os
import shutil
from struct import unpack
import tempfile
import zlib

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import test_runner
from vts.utils.python.android import api
from vts.utils.python.file import target_file_utils

block_dev_path = "/dev/block/platform"  # path to platform block devices
PROPERTY_SLOT_SUFFIX = "ro.boot.slot_suffix"  # indicates current slot suffix for A/B devices
BOOT_HEADER_DTBO_SIZE_OFFSET = 1632  # offset of recovery dtbo size in boot header of version 1.


class VtsFirmwareBootHeaderVerificationTest(base_test.BaseTestClass):
    """Verifies boot/recovery image header.

    Attributes:
        temp_dir: The temporary directory on host.
        slot_suffix: The current slot suffix for A/B devices.
    """

    def setUpClass(self):
        """Initializes the DUT and creates temporary directories."""
        self.dut = self.android_devices[0]
        self.shell = self.dut.shell
        self.adb = self.dut.adb
        self.temp_dir = tempfile.mkdtemp()
        self.launch_api_level = self.dut.getLaunchApiLevel()
        logging.info("Create %s", self.temp_dir)
        self.slot_suffix = self.dut.getProp(PROPERTY_SLOT_SUFFIX)
        if self.slot_suffix is None:
            self.slot_suffix = ""
        logging.info("current slot suffix: %s", self.slot_suffix)

    def setUp(self):
        """Checks if the the preconditions to run the test are met."""
        if "x86" in self.dut.cpu_abi:
            global block_dev_path
            block_dev_path = "/dev/block"
            acpio_idx_string = self.adb.shell(
                "cat /proc/cmdline | "
                "grep -o \"androidboot.acpio_idx=[^ ]*\" |"
                "cut -d \"=\" -f 2 ").replace('\n','')
            asserts.skipIf((len(acpio_idx_string) == 0), "Skipping test for x86 NON-ACPI ABI")

    def get_number_of_pages(self, image_size, page_size):
        """Calculates the number of pages required for the image.

        Args:
            image_size: size of the image.
            page_size : size of page.

        Returns:
            Number of pages required for the image
        """
        return (image_size + page_size - 1) / page_size

    def checkValidRamdisk(self, ramdisk_image):
        """Verifies that the ramdisk extracted from boot.img is a valid gzipped cpio archive.

        Args:
            ramdisk_image: ramdisk extracted from boot.img.
        """
        # Set wbits parameter to zlib.MAX_WBITS|16 to expect a gzip header and
        # trailer.
        unzipped_ramdisk = zlib.decompress(ramdisk_image, zlib.MAX_WBITS|16)
        # The CPIO header magic can be "070701" or "070702" as per kernel
        # documentation: Documentation/early-userspace/buffer-format.txt
        cpio_header_magic = unzipped_ramdisk[0:6]
        asserts.assertTrue(cpio_header_magic == "070701" or cpio_header_magic == "070702",
                           "cpio archive header magic not found in ramdisk")

    def CheckImageHeader(self, boot_image, is_recovery=False):
        """Verifies the boot image format.

        Args:
            boot_image: Path to the boot image.
            is_recovery: Indicates that the image is recovery if true.
        """
        try:
            with open(boot_image, "rb") as image_file:
                image_file.read(8)  # read boot magic
                (kernel_size, _, ramdisk_size, _, _, _, _, page_size,
                 host_image_header_version) = unpack("9I", image_file.read(9 * 4))

                asserts.assertNotEqual(kernel_size, 0, "boot.img/recovery.img must contain kernel")

                if self.launch_api_level > api.PLATFORM_API_LEVEL_P:
                    asserts.assertTrue(
                        host_image_header_version >= 2,
                        "Device must atleast have a boot image of version 2")

                    asserts.assertNotEqual(ramdisk_size, 0, "boot.img must contain ramdisk")

                    # ramdisk comes after the header and kernel pages
                    num_kernel_pages = self.get_number_of_pages(kernel_size, page_size)
                    ramdisk_offset = page_size * (1 + num_kernel_pages)
                    image_file.seek(ramdisk_offset)
                    ramdisk_buf = image_file.read(ramdisk_size)
                    self.checkValidRamdisk(ramdisk_buf)
                else:
                    asserts.assertTrue(
                        host_image_header_version >= 1,
                        "Device must atleast have a boot image of version 1")
                image_file.seek(BOOT_HEADER_DTBO_SIZE_OFFSET)
                recovery_dtbo_size = unpack("I", image_file.read(4))[0]
                image_file.read(8)  # ignore recovery dtbo load address
                if is_recovery:
                    asserts.assertNotEqual(
                        recovery_dtbo_size, 0,
                        "recovery partition for non-A/B devices must contain the recovery DTBO"
                    )
                boot_header_size = unpack("I", image_file.read(4))[0]
                if host_image_header_version > 1:
                    dtb_size = unpack("I", image_file.read(4))[0]
                    asserts.assertNotEqual(dtb_size, 0, "Boot/recovery image must contain DTB")
                    image_file.read(8)  # ignore DTB physical load address
                expected_header_size = image_file.tell()
                asserts.assertEqual(
                    boot_header_size, expected_header_size,
                    "Test failure due to boot header size mismatch. Expected %s Actual %s"
                    % (expected_header_size, boot_header_size))
        except IOError as e:
            logging.exception(e)
            asserts.fail("Unable to open boot image file")

    def testBootImageHeader(self):
        """Validates boot image header."""
        current_boot_partition = "boot" + str(self.slot_suffix)
        boot_path = target_file_utils.FindFiles(
            self.shell, block_dev_path, current_boot_partition, "-type l")
        logging.info("Boot path %s", boot_path)
        if not boot_path:
            asserts.fail("Unable to find path to boot image on device.")
        host_boot_path = os.path.join(self.temp_dir, "boot.img")
        self.adb.pull("%s %s" % (boot_path[0], host_boot_path))
        self.CheckImageHeader(host_boot_path)

    def testRecoveryImageHeader(self):
        """Validates recovery image header."""
        asserts.skipIf(self.slot_suffix,
                       "A/B devices do not have a separate recovery partition")
        recovery_path = target_file_utils.FindFiles(self.shell, block_dev_path,
                                                    "recovery", "-type l")
        logging.info("recovery path %s", recovery_path)
        if not recovery_path:
            asserts.fail("Unable to find path to recovery image on device.")
        host_recovery_path = os.path.join(self.temp_dir, "recovery.img")
        self.adb.pull("%s %s" % (recovery_path[0], host_recovery_path))
        self.CheckImageHeader(host_recovery_path, True)

    def tearDownClass(self):
        """Deletes temporary directories."""
        shutil.rmtree(self.temp_dir)


if __name__ == "__main__":
    test_runner.main()
