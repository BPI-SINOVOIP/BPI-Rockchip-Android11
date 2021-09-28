#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging

from host_controller import common
from host_controller.command_processor import base_command_processor

from vts.utils.python.common import cmd_utils
from vts.utils.python.controllers import adb
from vts.utils.python.controllers import android_device

# Default index of setStreamVolume() from IAudioService.aidl (1 based)
SETSTREAMVOLUME_INDEX_DEFAULT = 3

# Indices of each stream type (can be checked with "adb shell dumpsys audio" on the command line)
STREAM_TYPE_CALL = 0
STREAM_TYPE_RINGTONE = 2
STREAM_TYPE_MEDIA = 3
STREAM_TYPE_ALARM = 4

STREAM_TYPE_LIST = [
    STREAM_TYPE_CALL,
    STREAM_TYPE_RINGTONE,
    STREAM_TYPE_MEDIA,
    STREAM_TYPE_ALARM,
]


class CommandDUT(base_command_processor.BaseCommandProcessor):
    """Command processor for DUT command.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "dut"
    command_detail = "Performs certain operations on DUT (Device Under Test)."

    # @Override
    def SetUp(self):
        """Initializes the parser for dut command."""
        self.arg_parser.add_argument(
            "--operation",
            choices=("wifi_on", "wifi_off", 'volume_mute', 'volume_max'),
            default="",
            required=True,
            help="Operation to perform.")
        self.arg_parser.add_argument(
            "--serial", default="", required=True, help="The device serial.")
        self.arg_parser.add_argument(
            "--ap",
            default="",  # Required only for wifi_on
            help="Access point (AP) name for 'wifi_on' operation.")
        self.arg_parser.add_argument(
            "--volume_levels",
            type=int,
            default=30,  # Required only for volume_mute and volume_max
            help="The number of volume control levels.")
        self.arg_parser.add_argument(
            "--version",
            type=float,
            default=8.0,
            help="System version information of the device on which "
            "the test will run.")

    # @Override
    def Run(self, arg_line):
        """Performs the requested operation on the selected DUT."""
        args = self.arg_parser.ParseLine(arg_line)
        device = android_device.AndroidDevice(
            args.serial, device_callback_port=-1)
        boot_complete = device.waitForBootCompletion()
        if not boot_complete:
            logging.error("Device %s failed to bootup.", args.serial)
            self.console.device_status[
                args.serial] = common._DEVICE_STATUS_DICT["error"]
            self.console.vti_endpoint_client.SetJobStatusFromLeasedTo(
                "bootup-err")
            return False

        adb_proxy = adb.AdbProxy(serial=args.serial)
        adb_proxy.root()
        try:
            if args.operation == "wifi_on":
                adb_proxy.shell("svc wifi enable")
                if args.ap:
                    adb_proxy.install(
                        "../testcases/DATA/app/WifiUtil/WifiUtil.apk")
                    adb_proxy.shell(
                        "am instrument -e method \"connectToNetwork\" "
                        "-e ssid %s "
                        "-w com.android.tradefed.utils.wifi/.WifiUtil" %
                        args.ap)
            elif args.operation == "wifi_off":
                adb_proxy.shell("svc wifi disable")
            elif args.operation == "volume_mute":
                for _ in range(args.volume_levels):
                    adb_proxy.shell("input keyevent 25")
                self.SetOtherVolumes(adb_proxy, 0, args.version)
            elif args.operation == "volume_max":
                for _ in range(args.volume_levels):
                    adb_proxy.shell("input keyevent 24")
                self.SetOtherVolumes(adb_proxy, args.volume_levels,
                                     args.version)
        except adb.AdbError as e:
            logging.exception(e)
            return False

    def SetOtherVolumes(self, adb_proxy, volume_level, version=None):
        """Sets device's call/media/alarm volumes a certain level.

        Args:
            adb_proxy: AdbProxy, used for interacting with the device via adb.
            volume_level: int, volume level value.
            version: float, Android system version value. The index of
                     setStreamVolume() depends on the Android version.
        """
        setStreamVolume_index = SETSTREAMVOLUME_INDEX_DEFAULT
        if version and version >= 9.0:
            setStreamVolume_index = 7
        for stream_type in STREAM_TYPE_LIST:
            adb_volume_command = "service call audio %s i32 %s i32 %s i32 1" % (
                setStreamVolume_index, stream_type, volume_level)
            adb_proxy.shell(adb_volume_command)
