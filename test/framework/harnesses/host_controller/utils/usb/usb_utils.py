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

import fcntl
import logging
import os

from vts.utils.python.common import cmd_utils

# _IO('U', 20)
USBDEVFS_RESET = ord("U") << 8 | 20


def GetDevicesUSBFilePath():
    """Sweeps through connected USB device info and maps serial number to them.

    Returns:
        A dict, serial numbers of the devices as the key, device file path
        corresponding to the serial number as the value.
    """
    ret = {}

    sh_stdout, sh_stderr, ret_code = cmd_utils.ExecuteOneShellCommand("which udevadm")
    if ret_code != 0:
        logging.error("`udevadm` doesn't exist on the host; "
                      "please install 'udev' pkg before retrying.")
        return ret

    sh_stdout, _, _ = cmd_utils.ExecuteOneShellCommand(
        "find /sys/bus/usb/devices/usb*/ -name dev")
    lines = sh_stdout.split()
    for line in lines:
        syspath = os.path.dirname(line)
        devname, _, _ = cmd_utils.ExecuteOneShellCommand(
            "udevadm info -q name -p %s" % syspath)
        if devname.startswith("bus/"):
            serial, _, _ = cmd_utils.ExecuteOneShellCommand(
                "udevadm info -q property --export -p %s | grep ID_SERIAL_SHORT"
                % syspath)
            if serial:
                device_serial = serial.split("=")[1].strip().strip("'")
                ret[device_serial] = os.path.join("/dev", devname.strip())

    return ret


def ResetDeviceUsb(dev_file_path):
    """Invokes ioctl that resets the USB device on the given file path.

    Args:
        dev_file_path: string, abs path to the device file

    Returns:
        True if successful, False otherwise.
    """
    try:
        with open(dev_file_path, "wb") as usb_fd:
            fcntl.ioctl(usb_fd, USBDEVFS_RESET, 0)
    except IOError as e:
        logging.exception(e)
        return False

    return True


def ResetUsbDeviceOfSerial(serial):
    """Resets a USB device file corresponding to the given serial.

    Args:
        serial: string, serial number of the device whose USB device file
                will reset.

    Returns:
        True if successful, False otherwise.
    """
    device_file_path = GetDevicesUSBFilePath()
    if serial in device_file_path:
        logging.error(
            "Device %s not responding. Resetting device file %s.", serial,
            device_file_path[serial])
        return ResetDeviceUsb(device_file_path[serial])
    return False


def ResetUsbDeviceOfSerial_Callback(*args):
    """Wrapper of the ResetUsbDeviceOfSerial(), for handling the *args.

    Args:
        args: tuple of arguments, expected to have the serial number of
              the devices as the first element.

    Returns:
        True if successful, False otherwise.
    """
    try:
        return ResetUsbDeviceOfSerial(args[0])
    except IndexError as e:
        logging.exception(e)
        return False