#!/usr/bin/env python
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
import sys
import unittest

import target_file_utils
import proc_utils as utils

from proc_tests import ProcAsoundTests
from proc_tests import ProcCmdlineTest
from proc_tests import ProcCpuFileTests
from proc_tests import ProcFsFileTests
from proc_tests import ProcKmsgTest
from proc_tests import ProcMapsTest
from proc_tests import ProcMiscTest
from proc_tests import ProcMemInfoTest
from proc_tests import ProcModulesTest
from proc_tests import ProcQtaguidCtrlTest
from proc_tests import ProcRemoveUidRangeTest
from proc_tests import ProcSimpleFileTests
from proc_tests import ProcShowUidStatTest
from proc_tests import ProcStatTest
from proc_tests import ProcUidIoStatsTest
from proc_tests import ProcUidTimeInStateTest
from proc_tests import ProcUidConcurrentTimeTests
from proc_tests import ProcUidCpuPowerTests
from proc_tests import ProcVersionTest
from proc_tests import ProcVmallocInfoTest
from proc_tests import ProcVmstatTest
from proc_tests import ProcZoneInfoTest

TEST_OBJECTS = {
    ProcAsoundTests.ProcAsoundCardsTest(),
    ProcCmdlineTest.ProcCmdlineTest(),
    ProcCpuFileTests.ProcCpuInfoTest(),
    ProcCpuFileTests.ProcLoadavgTest(),
    ProcFsFileTests.ProcDiskstatsTest(),
    ProcFsFileTests.ProcFilesystemsTest(),
    ProcFsFileTests.ProcMountsTest(),
    ProcFsFileTests.ProcSwapsTest(),
    ProcKmsgTest.ProcKmsgTest(),
    ProcMapsTest.ProcMapsTest(),
    ProcMiscTest.ProcMisc(),
    ProcMemInfoTest.ProcMemInfoTest(),
    ProcModulesTest.ProcModulesTest(),
    ProcQtaguidCtrlTest.ProcQtaguidCtrlTest(),
    ProcRemoveUidRangeTest.ProcRemoveUidRangeTest(),
    ProcSimpleFileTests.ProcCorePattern(),
    ProcSimpleFileTests.ProcCorePipeLimit(),
    ProcSimpleFileTests.ProcDirtyBackgroundBytes(),
    ProcSimpleFileTests.ProcDirtyBackgroundRatio(),
    ProcSimpleFileTests.ProcDirtyExpireCentisecs(),
    ProcSimpleFileTests.ProcDmesgRestrict(),
    ProcSimpleFileTests.ProcDomainname(),
    ProcSimpleFileTests.ProcDropCaches(),
    ProcSimpleFileTests.ProcExtraFreeKbytes(),
    ProcSimpleFileTests.ProcHostname(),
    ProcSimpleFileTests.ProcHungTaskTimeoutSecs(),
    ProcSimpleFileTests.ProcKptrRestrictTest(),
    ProcSimpleFileTests.ProcMaxMapCount(),
    ProcSimpleFileTests.ProcMmapMinAddrTest(),
    ProcSimpleFileTests.ProcMmapRndBitsTest(),
    ProcSimpleFileTests.ProcModulesDisabled(),
    ProcSimpleFileTests.ProcOverCommitMemoryTest(),
    ProcSimpleFileTests.ProcPageCluster(),
    ProcSimpleFileTests.ProcPanicOnOops(),
    ProcSimpleFileTests.ProcPerfEventMaxSampleRate(),
    ProcSimpleFileTests.ProcPerfEventParanoid(),
    ProcSimpleFileTests.ProcPidMax(),
    ProcSimpleFileTests.ProcPipeMaxSize(),
    ProcSimpleFileTests.ProcProtectedHardlinks(),
    ProcSimpleFileTests.ProcProtectedSymlinks(),
    ProcSimpleFileTests.ProcRandomizeVaSpaceTest(),
    ProcSimpleFileTests.ProcSchedChildRunsFirst(),
    ProcSimpleFileTests.ProcSchedLatencyNS(),
    ProcSimpleFileTests.ProcSchedRTPeriodUS(),
    ProcSimpleFileTests.ProcSchedRTRuntimeUS(),
    ProcSimpleFileTests.ProcSchedTunableScaling(),
    ProcSimpleFileTests.ProcSchedWakeupGranularityNS(),
    ProcShowUidStatTest.ProcShowUidStatTest(),
    ProcSimpleFileTests.ProcSuidDumpable(),
    ProcSimpleFileTests.ProcSysKernelRandomBootId(),
    ProcSimpleFileTests.ProcSysRqTest(),
    ProcSimpleFileTests.ProcUptime(),
    ProcStatTest.ProcStatTest(),
    ProcUidIoStatsTest.ProcUidIoStatsTest(),
    ProcUidTimeInStateTest.ProcUidTimeInStateTest(),
    ProcUidConcurrentTimeTests.ProcUidConcurrentActiveTimeTest(),
    ProcUidConcurrentTimeTests.ProcUidConcurrentPolicyTimeTest(),
    ProcUidCpuPowerTests.ProcUidCpuPowerTimeInStateTest(),
    ProcUidCpuPowerTests.ProcUidCpuPowerConcurrentActiveTimeTest(),
    ProcUidCpuPowerTests.ProcUidCpuPowerConcurrentPolicyTimeTest(),
    ProcVersionTest.ProcVersionTest(),
    ProcVmallocInfoTest.ProcVmallocInfoTest(),
    ProcVmstatTest.ProcVmstat(),
    ProcZoneInfoTest.ProcZoneInfoTest(),
}

TEST_OBJECTS_64 = {
    ProcSimpleFileTests.ProcMmapRndCompatBitsTest(),
}


class VtsKernelProcFileApiTest(unittest.TestCase):
    """Test cases which check content of proc files.

    Attributes:
        _PROC_SYS_ABI_SWP_FILE_PATH: the path of a file which decides behaviour of SWP instruction.
    """

    _PROC_SYS_ABI_SWP_FILE_PATH = "/proc/sys/abi/swp"

    def setUp(self):
        """Initializes tests.

        Data file path, device, remote shell instance and temporary directory
        are initialized.
        """
        serial_number = os.environ.get("ANDROID_SERIAL")
        self.assertTrue(serial_number, "$ANDROID_SERIAL is empty.")
        self.dut = utils.AndroidDevice(serial_number)

    def testProcPagetypeinfo(self):
        # TODO(b/109884074): make mandatory once incident_helper is in AOSP.
        out, err, r_code = self.dut.shell.Execute("which incident_helper")
        if r_code != 0:
            logging.info("incident_helper not present")
            return

        filepath = "/proc/pagetypeinfo"
        # Check that incident_helper can parse /proc/pagetypeinfo.
        out, err, r_code = self.dut.shell.Execute(
                "cat %s | incident_helper -s 2001" % filepath)
        self.assertEqual(
                r_code, 0,
            "Failed to parse %s." % filepath)

    def testProcSysrqTrigger(self):
        filepath = "/proc/sysrq-trigger"

        # This command only performs a best effort attempt to remount all
        # filesystems. Check that it doesn't throw an error.
        self.dut.shell.Execute("echo u > %s" % filepath)

        # Reboot the device.
        self.dut.shell.Execute("echo b > %s" % filepath)
        self.assertTrue(self.dut.IsShutdown(10), "Device is still alive.")
        self.assertTrue(self.dut.WaitForBootCompletion(300))
        self.assertTrue(self.dut.Root())

    def testProcUidProcstatSet(self):

        def UidIOStats(uid):
            """Returns I/O stats for a given uid.

            Args:
                uid, uid number.

            Returns:
                list of I/O numbers.
            """
            stats_path = "/proc/uid_io/stats"
            out, err, r_code = self.dut.shell.Execute(
                    "cat %s | grep '^%d'" % (stats_path, uid))
            return out.split()

        def CheckStatsInState(state):
            """Sets VTS (root uid) into a given state and checks the stats.

            Args:
                state, boolean. Use False for foreground,
                and True for background.
            """
            state = 1 if state else 0
            filepath = "/proc/uid_procstat/set"
            root_uid = 0

            # fg write chars are at index 2, and bg write chars are at 6.
            wchar_index = 6 if state else 2
            old_wchar = UidIOStats(root_uid)[wchar_index]
            self.dut.shell.Execute("echo %d %s > %s" % (root_uid, state, filepath))
            # This should increase the number of write syscalls.
            self.dut.shell.Execute("echo foo")
            self.assertLess(
                int(old_wchar),
                int(UidIOStats(root_uid)[wchar_index]),
                "Number of write syscalls has not increased.")

        CheckStatsInState(False)
        CheckStatsInState(True)

    def testProcPerUidTimes(self):
        # TODO: make these files mandatory once they're in AOSP
        try:
            filepaths = self.dut.FindFiles('/proc/uid', 'time_in_state')
        except:
            logging.info("/proc/uid/ directory does not exist and is optional")
            return

        if not filepaths:
            logging.info('per-UID time_in_state files do not exist and are optional')
            return

        for filepath in filepaths:
            self.assertTrue(self.dut.Exists(filepath),
                            '%s does not exist.' % filepath)
            permission = self.dut.GetPermission(filepath)
            self.assertTrue(target_file_utils.IsReadOnly(permission))
            file_content = self.dut.ReadFileContent(filepath)

    def testProcSysAbiSwpInstruction(self):
        """Tests /proc/sys/abi/swp.

        /proc/sys/abi/swp sets the execution behaviour for the obsoleted ARM instruction
        SWP. As per the setting in /proc/sys/abi/swp, the usage of SWP{B}
        can either generate an undefined instruction abort or use software emulation
        or hardware execution.
        """
        if not ('arm' in self.cpu_abi(self.dut) and self.is64Bit(self.dut)):
            logging.info("file not present on non-ARM64 device")
            return

        filepath = '/proc/sys/abi/swp'

        self.assertTrue(self.dut.Exists(filepath), '%s does not exist.' % filepath)
        permission = self.dut.GetPermission(filepath)
        self.assertTrue(target_file_utils.IsReadWrite(permission))

        file_content = self.dut.ReadFileContent(filepath)
        try:
            swp_state = int(file_content)
        except ValueError as e:
            self.fail("Failed to parse %s" % filepath)
        self.assertTrue(swp_state >= 0 and swp_state <= 2,
                        "%s contains incorrect value: %d"
                        % (filepath, swp_state))

    def cpu_abi(self, dut):
        """CPU ABI (Application Binary Interface) of the device."""
        out = dut._GetProp("ro.product.cpu.abi")
        if not out:
            return "unknown"

        cpu_abi = out.lower()
        return cpu_abi

    def is64Bit(self, dut):
        """True if device is 64 bit."""
        out, _, _ = dut.shell.Execute('uname -m')
        return "64" in out


def run_proc_file_test(test_object):
    """Reads from the file and checks that it parses and the content is valid.

    Args:
        test_object: inherits KernelProcFileTestBase, contains the test functions
    """
    def test(self):
        if test_object in TEST_OBJECTS_64 and not self.is64Bit(self.dut):
            logging.info("Skip test for 64-bit kernel.")
            return
        test_object.set_api_level(self.dut)
        filepath = test_object.get_path()
        if not self.dut.Exists(filepath) and test_object.file_optional(dut=self.dut):
            logging.info("%s does not exist and is optional." % filepath)
            return
        self.assertTrue(self.dut.Exists(filepath), '%s does not exist.' % filepath)
        permission = self.dut.GetPermission(filepath)
        self.assertTrue(test_object.get_permission_checker())
        self.assertTrue(test_object.get_permission_checker()(permission),
                        "%s: File has invalid permissions (%s)" % (filepath,
                                                                   permission))

        logging.info("Testing format of %s", filepath)
        self.assertTrue(
            test_object.prepare_test(self.dut), "Setup failed!")

        if not test_object.test_format():
            return

        file_content = self.dut.ReadFileContent(filepath)
        try:
            parse_result = test_object.parse_contents(file_content)
        except (SyntaxError, ValueError, IndexError) as e:
            self.fail("Failed to parse! " + str(e))
        self.assertTrue(
            test_object.result_correct(parse_result), "Results not valid!")
    return test


if __name__ == "__main__":
    try:
        for test_object in TEST_OBJECTS.union(TEST_OBJECTS_64):
            test_func = run_proc_file_test(test_object)
            setattr(VtsKernelProcFileApiTest, 'test_' +
                    test_object.__class__.__name__, test_func)
        suite = unittest.TestLoader().loadTestsFromTestCase(
            VtsKernelProcFileApiTest)
        results = unittest.TextTestRunner(verbosity=2).run(suite)
    finally:
        if results.failures:
            sys.exit(1)
