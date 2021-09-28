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

import os
import time
import unittest

from checkpoint_utils import  ADB
from vts.testcases.vndk import utils

class VtsKernelCheckpointTest(unittest.TestCase):

    _CHECKPOINTTESTFILE = "/data/local/tmp/checkpointtest"
    _ORIGINALVALUE = "original value"
    _MODIFIEDVALUE = "modified value"

    def setUp(self):
        serial_number = os.environ.get("ANDROID_SERIAL")
        self.assertTrue(serial_number, "$ANDROID_SERIAL is empty.")
        self.dut = utils.AndroidDevice(serial_number)
        self.adb = ADB(serial_number)
        self.isCheckpoint_ = self.isCheckpoint()

    def getFstab(self):
        # Make sure device is ready for adb.
        self.adb.Execute(["wait-for-device"], timeout=900)
        self.adb.Execute(["root"])

        for prop in ["hardware", "hardware.platform"]:
            out, err, return_code = self.dut.Execute("getprop ro.boot." + prop)
            extension = out
            if not extension:
                continue

            for filename in ["/odm/etc/fstab.", "/vendor/etc/fstab.", "/fstab."]:
                out, err, return_code = self.dut.Execute("cat " + filename + extension)
                if return_code != 0:
                    continue

                return out

        return ""

    def isCheckpoint(self):
        fstab = self.getFstab().splitlines()
        for line in fstab:
            parts = line.split()
            if len(parts) != 5: # fstab has five parts for each entry:
                                # [device-name] [mount-point] [type] [mount_flags] [fsmgr_flags]
                continue

            flags = parts[4]    # the fsmgr_flags field is the fifth one, thus index 4
            flags = flags.split(',')
            if any(flag.startswith("checkpoint=") for flag in flags):
                return True

        return False

    def reboot(self):
        self.adb.Execute(["reboot"])
        try:
          self.adb.Execute(["wait-for-device"], timeout=900)
        except self.adb.AdbError as e:
          self.fail("Exception thrown waiting for device:" + e.msg())

        # Should not be necessary, but without these retries, test fails
        # regularly on taimen with Android Q
        for i in range(1, 30):
          try:
            self.adb.Execute(["root"])
            break
          except:
            time.sleep(1)

        for i in range(1, 30):
          try:
            self.dut.Execute("ls");
            break
          except:
            time.sleep(1)

    def checkBooted(self):
        for i in range(1, 900):
          out, err, return_code = self.dut.Execute("getprop sys.boot_completed")
          try:
            boot_completed = int(out)
            self.assertEqual(boot_completed, 1)
            return
          except:
            time.sleep(1)

        self.fail("sys.boot_completed not set")

    def testCheckpointEnabled(self):
        out, err, return_code = self.dut.Execute("getprop ro.product.first_api_level")
        try:
          first_api_level = int(out)
          self.assertTrue(first_api_level < 29 or self.isCheckpoint_,
                             "User Data Checkpoint is disabled")
        except:
          pass

    def testRollback(self):
        if not self.isCheckpoint_:
            return

        self.adb.Execute(["root"])

        # Make sure that we are fully booted so we don't get entangled in
        # someone else's checkpoint
        self.checkBooted()

        # Create a file and initiate checkpoint
        self.dut.Execute("setprop persist.vold.dont_commit_checkpoint 1")
        self.dut.Execute("echo " + self._ORIGINALVALUE + " > " + self._CHECKPOINTTESTFILE)
        out, err, return_code = self.dut.Execute("vdc checkpoint startCheckpoint 1")
        self.assertEqual(return_code, 0)
        self.reboot()

        # Modify the file but do not commit
        self.dut.Execute("echo " + self._MODIFIEDVALUE + " > " + self._CHECKPOINTTESTFILE)
        self.reboot()

        # Check the file is unchanged
        out, err, return_code = self.dut.Execute("cat " + self._CHECKPOINTTESTFILE)
        self.assertEqual(out.strip(), self._ORIGINALVALUE)

        # Clean up
        self.dut.Execute("setprop persist.vold.dont_commit_checkpoint 0")
        out, err, return_code = self.dut.Execute("vdc checkpoint commitChanges")
        self.assertEqual(return_code, 0)
        self.reboot()
        self.dut.Execute("rm " + self._CHECKPOINTTESTFILE)

    def testCommit(self):
        if not self.isCheckpoint_:
            return

        self.adb.Execute(["root"])

        # Make sure that we are fully booted so we don't get entangled in
        # someone else's checkpoint
        self.checkBooted()

        # Create a file and initiate checkpoint
        self.dut.Execute("setprop persist.vold.dont_commit_checkpoint 1")
        self.dut.Execute("echo " + self._ORIGINALVALUE + " > " + self._CHECKPOINTTESTFILE)
        out, err, return_code = self.dut.Execute("vdc checkpoint startCheckpoint 1")
        self.assertEqual(return_code, 0)
        self.reboot()

        # Modify the file and commit the checkpoint
        self.dut.Execute("echo " + self._MODIFIEDVALUE + " > " + self._CHECKPOINTTESTFILE)
        self.dut.Execute("setprop persist.vold.dont_commit_checkpoint 0")
        out, err, return_code = self.dut.Execute("vdc checkpoint commitChanges")
        self.assertEqual(return_code, 0)
        self.reboot()

        # Check file has changed
        out, err, return_code = self.dut.Execute("cat " + self._CHECKPOINTTESTFILE)
        self.assertEqual(out.strip(), self._MODIFIEDVALUE)

        # Clean up
        self.dut.Execute("rm " + self._CHECKPOINTTESTFILE)

if __name__ == "__main__":
    # Setting verbosity is required to generate output that the TradeFed test
    # runner can parse.
    unittest.main(verbosity=3)
