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
"""VTS tests to verify ACPIO partition overlay application."""

import logging
import os
import shutil
import struct
import subprocess
import tempfile
import zlib

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner
from vts.utils.python.android import api
from vts.utils.python.file import target_file_utils
from vts.utils.python.os import path_utils
from hashlib import sha1

BLOCK_DEV_PATH = "/dev/block"  # path to block devices
PROPERTY_SLOT_SUFFIX = "ro.boot.slot_suffix"  # indicates current slot suffix for A/B devices
SSDT_PATH = "/sys/firmware/acpi/tables"
acpio_idx_string = ""

class VtsFirmwareAcpioVerification(base_test.BaseTestClass):
    """Validates ACPI overlay application.

    Attributes:
        temp_dir: The temporary directory on host.
        device_path: The temporary directory on device.
    """

    def setUpClass(self):
        """Initializes the DUT and creates temporary directories."""
        self.dut = self.android_devices[0]
        self.shell = self.dut.shell
        self.adb = self.dut.adb
        self.temp_dir = tempfile.mkdtemp()
        logging.debug("Create %s", self.temp_dir)

    def setUp(self):
        """Checks if the the preconditions to run the test are met."""
        global acpio_idx_string
        asserts.skipIf("x86" not in self.dut.cpu_abi, "Skipping test for NON-x86 ABI")
        acpio_idx_string = self.adb.shell(
            "cat /proc/cmdline | "
            "grep -o \"androidboot.acpio_idx=[^ ]*\" |"
            "cut -d \"=\" -f 2 ")
        acpio_idx_string = acpio_idx_string.replace('\n','')
        asserts.assertTrue(acpio_idx_string, "Skipping test for x86 NON-ACPI ABI")

    def getSha1(self, filename):
        """Check the file hash."""
        sha1Obj = sha1()
        with open(filename, 'rb') as f:
            sha1Obj.update(f.read())
        return sha1Obj.hexdigest()

    def testAcpioPartition(self):
        """Validates ACPIO partition using mkdtboimg.py."""
        temp_SSDT_dump = "SSDT.dump"
        temp_SSDT_dump_hashes = []
        current_SSDT = "SSDT"
        current_SSDT_hashes = []

        slot_suffix = str(self.dut.getProp(PROPERTY_SLOT_SUFFIX))
        current_acpio_partition = "acpio" + slot_suffix
        acpio_path = target_file_utils.FindFiles(
            self.shell, BLOCK_DEV_PATH, current_acpio_partition, "-type l")
        if not acpio_path:
            asserts.fail("Unable to find path to acpio image on device.")
        logging.debug("ACPIO path %s", acpio_path)
        host_acpio_image = os.path.join(self.temp_dir, "acpio")
        self.adb.pull("%s %s" % (acpio_path[0], host_acpio_image))
        mkdtimg_bin_path = os.path.join("host", "bin", "mkdtboimg.py")
        unpacked_acpio_file = os.path.join(self.temp_dir, temp_SSDT_dump)
        acpio_dump_cmd = [
            "%s" % mkdtimg_bin_path, "dump",
            "%s" % host_acpio_image, "-b",
            "%s" % unpacked_acpio_file
        ]
        try:
            subprocess.check_call(acpio_dump_cmd)
        except Exception as e:
            logging.exception(e)
            asserts.fail("Invalid ACPIO Image")
        asserts.assertTrue(acpio_idx_string, "Kernel command line missing androidboot.acpio_idx")
        acpio_idx_list = acpio_idx_string.split(",")
        for idx in acpio_idx_list:
            temp_SSDT_dump_file = "SSDT.dump." + idx.rstrip()
            temp_SSDT_dump_file_hash=self.getSha1(os.path.join(self.temp_dir, temp_SSDT_dump_file))
            temp_SSDT_dump_hashes.append(temp_SSDT_dump_file_hash)

        SSDT_path = target_file_utils.FindFiles(self.shell, SSDT_PATH, "SSDT*")
        for current_SSDT_file in SSDT_path:
            host_SSDT_file = os.path.join(self.temp_dir, current_SSDT)
            self.adb.pull("%s %s" % (current_SSDT_file, host_SSDT_file))
            SSDT_file_hash=self.getSha1(host_SSDT_file)
            current_SSDT_hashes.append(SSDT_file_hash)
        asserts.assertTrue(current_SSDT_hashes, "No get current SSDT hash.")
        asserts.assertTrue(set(temp_SSDT_dump_hashes) & set(current_SSDT_hashes), "Hash is not the same.")

if __name__ == "__main__":
    test_runner.main()
