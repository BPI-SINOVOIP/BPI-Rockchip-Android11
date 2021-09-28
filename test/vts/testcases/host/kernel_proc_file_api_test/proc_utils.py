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

from vts.testcases.vndk import utils

SYSPROP_DEV_BOOTCOMPLETE = "dev.bootcomplete"
SYSPROP_SYS_BOOT_COMPLETED = "sys.boot_completed"


class Shell(object):
    """This class to wrap adb shell command."""

    def __init__(self, serial_number):
        self._serial_number = serial_number

    def Execute(self, *args):
        """Executes a command.

        Args:
            args: Strings, the arguments.

        Returns:
            Stdout as a string, stderr as a string, and return code as an
            integer.
        """
        cmd = ["adb", "-s", self._serial_number, "shell"]
        cmd.extend(args)
        return RunCommand(cmd)


class ADB(object):
    """This class to wrap adb command."""

    def __init__(self, serial_number):
        self._serial_number = serial_number

    def Execute(self, cmd_list, timeout=None):
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

class AndroidDevice(utils.AndroidDevice):
    """This class controls the device via adb commands."""

    def __init__(self, serial_number):
        super(AndroidDevice, self).__init__(serial_number)
        self._serial_number = serial_number
        self.shell = Shell(serial_number)
        self.adb = ADB(serial_number)

    def GetPermission(self, filepath):
        """Get file permission."""
        out, err, r_code = self.shell.Execute('stat -c %%a %s' % filepath)
        if r_code != 0 or err.strip():
            raise IOError("`stat -c %%a '%s'` stdout: %s\nstderr: %s" %
                          (filepath, out, err))
        return out.strip()

    def WaitForBootCompletion(self, timeout=None):
        """Get file permission."""
        start = time.time()
        cmd = ['wait-for-device']
        self.adb.Execute(cmd, timeout)
        while not self.isBootCompleted():
            if time.time() - start >= timeout:
                logging.error("Timeout while waiting for boot completion.")
                return False
            time.sleep(1)
        return True

    def isBootCompleted(self):
        """Checks whether the device has booted.

        Returns:
            True if booted, False otherwise.
        """
        try:
            if (self._GetProp(SYSPROP_SYS_BOOT_COMPLETED) == '1' and
                    self._GetProp(SYSPROP_DEV_BOOTCOMPLETE) == '1'):
                return True
        except Exception as e:
            # adb shell calls may fail during certain period of booting
            # process, which is normal. Ignoring these errors.
            pass
        return False

    def IsShutdown(self, timeout=0):
        """Checks whether the device has booted.

        Returns:
            True if booted, False otherwise.
        """
        start = time.time()
        while (time.time() - start) <= timeout:
            if not self.isBootCompleted():
                return True
            time.sleep(1)
        return self.isBootCompleted()

    def Root(self, timeout=None):
        cmd = ["root"]
        try:
            self.adb.Execute(cmd, timeout=timeout)
            return True
        except subprocess.CalledProcessError as e:
            logging.exception(e)
        return False

    def ReadFileContent(self, filepath):
        """Read the content of a file and perform assertions.

        Args:
            filepath: string, path to file

        Returns:
            string, content of file
        """
        cmd = "cat %s" % filepath
        out, err, r_code = self.shell.Execute(cmd)

        # checks the exit code
        if r_code != 0 or err.strip():
            raise IOError("%s: Error happened while reading the file due to %s."
                          % (filepath, err))
        return out


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

    try:
        return out.decode('UTF-8'), err.decode('UTF-8'), proc.returncode
    except UnicodeDecodeError:
        # ProcUidCpuPowerTimeInStateTest, ProcUidCpuPowerConcurrentActiveTimeTest,
        # and ProcUidCpuPowerConcurrentPolicyTimeTest output could not be decode
        # to UTF-8.
        return out.decode('ISO-8859-1'), err.decode('ISO-8859-1'), proc.returncode
