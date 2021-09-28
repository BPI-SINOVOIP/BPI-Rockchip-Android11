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
import subprocess
import time

from threading import Timer

class AdbError(Exception):
    """Raised when there is an error in adb operations."""

    def __init__(self, cmd, stdout, stderr, ret_code):
        self.cmd = cmd
        self.stdout = stdout
        self.stderr = stderr
        self.ret_code = ret_code

    def __str__(self):
        return ("Error executing adb cmd '%s'. ret: %d, stdout: %s, stderr: %s"
                ) % (self.cmd, self.ret_code, self.stdout, self.stderr)


class ADB(object):
    """This class to wrap adb command."""

    # Default adb timeout 5 minutes
    DEFAULT_ADB_TIMEOUT = 300

    def __init__(self, serial_number):
        self._serial_number = serial_number

    def Execute(self, cmd_list, timeout=DEFAULT_ADB_TIMEOUT):
        """Executes a command.

        Args:
            args: Strings, the arguments.

        Returns:
            Stdout as a string, stderr as a string, and return code as an
            integer.
        """
        cmd = ["adb", "-s", self._serial_number]
        cmd.extend(cmd_list)
        return RunCommand(cmd, timeout)


def RunCommand(cmd, timeout=None):
    kill = lambda process:process.kill()
    proc = subprocess.Popen(args=cmd,
                            stderr=subprocess.PIPE,
                            stdout=subprocess.PIPE)
    _timer = Timer(timeout, kill, [proc])
    try:
        _timer.start()
        (out, err) = proc.communicate()
    finally:
        _timer.cancel()

    out = out.decode("utf-8")
    err = err.decode("utf-8")

    if proc.returncode != 0:
        raise AdbError(cmd, out, err, proc.returncode)
    return out, err, proc.returncode

