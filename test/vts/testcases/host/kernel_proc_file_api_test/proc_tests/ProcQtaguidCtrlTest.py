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
import re

from proc_tests import KernelProcFileTestBase
import target_file_utils


class ProcQtaguidCtrlTest(KernelProcFileTestBase.KernelProcFileTestBase):
    '''/proc/net/xt_qtaguid/ctrl provides information about tagged sockets.

    File content consists of possibly several lines of socket info followed by a
    single line of events info, followed by a terminating newline.'''

    def parse_contents(self, contents):
        result = []
        lines = contents.split('\n')
        if len(lines) == 0 or lines[-1] != '':
            raise SyntaxError
        for line in lines[:-2]:
            parsed = self.parse_line(
                "sock={:x} tag=0x{:x} (uid={:d}) pid={:d} f_count={:d}", line)
            if any(map(lambda x: x < 0, parsed)):
                raise SyntaxError("Negative numbers not allowed!")
            result.append(parsed)
        parsed = self.parse_line(
            "events: sockets_tagged={:d} sockets_untagged={:d} counter_set_changes={:d} "
            "delete_cmds={:d} iface_events={:d} match_calls={:d} match_calls_prepost={:d} "
            "match_found_sk={:d} match_found_sk_in_ct={:d} match_found_no_sk_in_ct={:d} "
            "match_no_sk={:d} match_no_sk_{:w}={:d}", lines[-2])
        if parsed[-2] not in {"file", "gid"}:
            raise SyntaxError("match_no_sk_{file|gid} incorrect")
        del parsed[-2]
        if any(map(lambda x: x < 0, parsed)):
            raise SyntaxError("Negative numbers not allowed!")
        result.append(parsed)
        return result

    def get_path(self):
        return "/proc/net/xt_qtaguid/ctrl"

    def file_optional(self, shell=None, dut=None):
        """Specifies if the /proc/net/xt_qtaguid/ctrl file is mandatory.

        For device running kernel 4.9 or above, it should use the eBPF cgroup
        filter to monitor networking stats instead. So it may not have
        xt_qtaguid module and /proc/net/xt_qtaguid/ctrl file on device.
        But for device that still has xt_qtaguid module, this file is mandatory.

        Same logic as checkKernelSupport in file:
        test/vts-testcase/kernel/api/qtaguid/SocketTagUserSpace.cpp

        Returns:
            True when the kernel is 4.9 or newer, otherwise False is returned
        """
        (version, patchlevel, sublevel) = self._kernel_version(dut)
        if version == 4 and patchlevel >= 9 or version > 4:
            return True
        else:
            return False

    def _kernel_version(self, dut):
        """Gets the kernel version from the device.

        This method reads the output of command "uname -r" from the device.

        Returns:
            A tuple of kernel version information
            in the format of (version, patchlevel, sublevel).

            It will fail if failed to get the output or correct format
            from the output of "uname -r" command
        """
        cmd = 'uname -r'
        out, _, _ = dut.shell.Execute(cmd)
        out = out.strip()

        match = re.match(r"(\d+)\.(\d+)\.(\d+)", out)
        if match is None:
            raise RuntimeError("Failed to detect kernel version of device. out:%s" % out)

        version = int(match.group(1))
        patchlevel = int(match.group(2))
        sublevel = int(match.group(3))
        logging.info("Detected kernel version: %s", match.group(0))
        return (version, patchlevel, sublevel)

    def get_permission_checker(self):
        """Get r/w file permission checker.
        """
        return target_file_utils.IsReadWrite
