#
#   Copyright 2016 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# TODO(b/147454897): Keep the logic in sync with
#                    test/vts-testcase/vndk/utils.py until this file is
#                    removed.
from builtins import str
from builtins import open

import gzip
import logging
import os
import re
import socket
import subprocess
import tempfile
import threading
import time
import traceback

from vts.runners.host import asserts
from vts.runners.host import const
from vts.runners.host import errors
from vts.runners.host import keys
from vts.runners.host import logger as vts_logger
from vts.runners.host import signals
from vts.runners.host import utils
from vts.runners.host.tcp_client import vts_tcp_client
from vts.utils.python.controllers import adb
from vts.utils.python.controllers import fastboot
from vts.utils.python.instrumentation import test_framework_instrumentation as tfi
from vts.utils.python.mirror import mirror_tracker

VTS_CONTROLLER_CONFIG_NAME = "AndroidDevice"
VTS_CONTROLLER_REFERENCE_NAME = "android_devices"

ANDROID_DEVICE_PICK_ALL_TOKEN = "*"
# Key name for adb logcat extra params in config file.
ANDROID_DEVICE_ADB_LOGCAT_PARAM_KEY = "adb_logcat_param"
ANDROID_DEVICE_EMPTY_CONFIG_MSG = "Configuration is empty, abort!"
ANDROID_DEVICE_NOT_LIST_CONFIG_MSG = "Configuration should be a list, abort!"
PORT_RETRY_COUNT = 3
SL4A_APK_NAME = "com.googlecode.android_scripting"

ANDROID_PRODUCT_TYPE_UNKNOWN = "unknown"

# Target-side directory where the VTS binaries are uploaded
DEFAULT_AGENT_BASE_DIR = "/data/local/tmp"
# Name of llkd
LLKD = 'llkd-1'
# Time for which the current is put on sleep when the client is unable to
# make a connection.
THREAD_SLEEP_TIME = 1
# Max number of attempts that the client can make to connect to the agent
MAX_AGENT_CONNECT_RETRIES = 10
# System property for product sku.
PROPERTY_PRODUCT_SKU = "ro.boot.product.hardware.sku"

# The argument to fastboot getvar command to determine whether the device has
# the slot for vbmeta.img
_FASTBOOT_VAR_HAS_VBMETA = "has-slot:vbmeta"

SYSPROP_DEV_BOOTCOMPLETE = "dev.bootcomplete"
SYSPROP_SYS_BOOT_COMPLETED = "sys.boot_completed"
# the name of a system property which tells whether to stop properly configured
# native servers where properly configured means a server's init.rc is
# configured to stop when that property's value is 1.
SYSPROP_VTS_NATIVE_SERVER = "vts.native_server.on"
# Maximum time in seconds to wait for process/system status change.
WAIT_TIMEOUT_SEC = 120

class AndroidDeviceError(signals.ControllerError):
    pass


def create(configs, start_services=True):
    """Creates AndroidDevice controller objects.

    Args:
        configs: A list of dicts, each representing a configuration for an
                 Android device.
        start_services: boolean, controls whether services will be started.

    Returns:
        A list of AndroidDevice objects.
    """
    if not configs:
        raise AndroidDeviceError(ANDROID_DEVICE_EMPTY_CONFIG_MSG)
    elif configs == ANDROID_DEVICE_PICK_ALL_TOKEN:
        ads = get_all_instances()
    elif not isinstance(configs, list):
        raise AndroidDeviceError(ANDROID_DEVICE_NOT_LIST_CONFIG_MSG)
    elif isinstance(configs[0], str):
        # Configs is a list of serials.
        ads = get_instances(configs)
    else:
        # Configs is a list of dicts.
        ads = get_instances_with_configs(configs)
    connected_ads = list_adb_devices()
    for ad in ads:
        ad.enable_vts_agent = start_services
        if ad.serial not in connected_ads:
            raise DoesNotExistError(("Android device %s is specified in config"
                                     " but is not attached.") % ad.serial)
    if start_services:
        _startServicesOnAds(ads)
    return ads


def destroy(ads):
    """Cleans up AndroidDevice objects.

    Args:
        ads: A list of AndroidDevice objects.
    """
    for ad in ads:
        try:
            ad.cleanUp()
        except:
            ad.log.exception("Failed to clean up properly.")


def _startServicesOnAds(ads):
    """Starts long running services on multiple AndroidDevice objects.

    If any one AndroidDevice object fails to start services, cleans up all
    existing AndroidDevice objects and their services.

    Args:
        ads: A list of AndroidDevice objects whose services to start.
    """
    running_ads = []
    for ad in ads:
        running_ads.append(ad)
        try:
            ad.startServices()
        except:
            ad.log.exception("Failed to start some services, abort!")
            destroy(running_ads)
            raise


def _parse_device_list(device_list_str, key):
    """Parses a byte string representing a list of devices. The string is
    generated by calling either adb or fastboot.

    Args:
        device_list_str: Output of adb or fastboot.
        key: The token that signifies a device in device_list_str.

    Returns:
        A list of android device serial numbers.
    """
    clean_lines = str(device_list_str, 'utf-8').strip().split('\n')
    results = []
    for line in clean_lines:
        tokens = line.strip().split('\t')
        if len(tokens) == 2 and tokens[1] == key:
            results.append(tokens[0])
    return results


def list_adb_devices():
    """List all target devices connected to the host and detected by adb.

    Returns:
        A list of android device serials. Empty if there's none.
    """
    out = adb.AdbProxy().devices()
    return _parse_device_list(out, "device")


def list_fastboot_devices():
    """List all android devices connected to the computer that are in in
    fastboot mode. These are detected by fastboot.

    Returns:
        A list of android device serials. Empty if there's none.
    """
    out = fastboot.FastbootProxy().devices()
    return _parse_device_list(out, "fastboot")


def list_unauthorized_devices():
    """List all unauthorized devices connected to the host and detected by adb.

    Returns:
        A list of unauthorized device serials. Empty if there's none.
    """
    out = adb.AdbProxy().devices()
    return _parse_device_list(out, "unauthorized")


def get_instances(serials):
    """Create AndroidDevice instances from a list of serials.

    Args:
        serials: A list of android device serials.

    Returns:
        A list of AndroidDevice objects.
    """
    results = []
    for s in serials:
        results.append(AndroidDevice(s))
    return results


def get_instances_with_configs(configs):
    """Create AndroidDevice instances from a list of json configs.

    Each config should have the required key-value pair "serial".

    Args:
        configs: A list of dicts each representing the configuration of one
            android device.

    Returns:
        A list of AndroidDevice objects.
    """
    results = []
    for c in configs:
        try:
            serial = c.pop(keys.ConfigKeys.IKEY_SERIAL)
        except KeyError:
            raise AndroidDeviceError(('Required value %s is missing in '
                                      'AndroidDevice config %s.') %
                                     (keys.ConfigKeys.IKEY_SERIAL, c))
        try:
            product_type = c.pop(keys.ConfigKeys.IKEY_PRODUCT_TYPE)
        except KeyError:
            logging.error('Required value %s is missing in '
                          'AndroidDevice config %s.',
                          keys.ConfigKeys.IKEY_PRODUCT_TYPE, c)
            product_type = ANDROID_PRODUCT_TYPE_UNKNOWN

        ad = AndroidDevice(serial, product_type)
        ad.loadConfig(c)
        results.append(ad)
    return results


def get_all_instances(include_fastboot=False):
    """Create AndroidDevice instances for all attached android devices.

    Args:
        include_fastboot: Whether to include devices in bootloader mode or not.

    Returns:
        A list of AndroidDevice objects each representing an android device
        attached to the computer.
    """
    if include_fastboot:
        serial_list = list_adb_devices() + list_fastboot_devices()
        return get_instances(serial_list)
    return get_instances(list_adb_devices())


def filter_devices(ads, func):
    """Finds the AndroidDevice instances from a list that match certain
    conditions.

    Args:
        ads: A list of AndroidDevice instances.
        func: A function that takes an AndroidDevice object and returns True
            if the device satisfies the filter condition.

    Returns:
        A list of AndroidDevice instances that satisfy the filter condition.
    """
    results = []
    for ad in ads:
        if func(ad):
            results.append(ad)
    return results


def get_device(ads, **kwargs):
    """Finds a unique AndroidDevice instance from a list that has specific
    attributes of certain values.

    Example:
        get_device(android_devices, label="foo", phone_number="1234567890")
        get_device(android_devices, model="angler")

    Args:
        ads: A list of AndroidDevice instances.
        kwargs: keyword arguments used to filter AndroidDevice instances.

    Returns:
        The target AndroidDevice instance.

    Raises:
        AndroidDeviceError is raised if none or more than one device is
        matched.
    """

    def _get_device_filter(ad):
        for k, v in kwargs.items():
            if not hasattr(ad, k):
                return False
            elif getattr(ad, k) != v:
                return False
        return True

    filtered = filter_devices(ads, _get_device_filter)
    if not filtered:
        raise AndroidDeviceError(("Could not find a target device that matches"
                                  " condition: %s.") % kwargs)
    elif len(filtered) == 1:
        return filtered[0]
    else:
        serials = [ad.serial for ad in filtered]
        raise AndroidDeviceError("More than one device matched: %s" % serials)


def takeBugReports(ads, test_name, begin_time):
    """Takes bug reports on a list of android devices.

    If you want to take a bug report, call this function with a list of
    android_device objects in on_fail. But reports will be taken on all the
    devices in the list concurrently. Bug report takes a relative long
    time to take, so use this cautiously.

    Args:
        ads: A list of AndroidDevice instances.
        test_name: Name of the test case that triggered this bug report.
        begin_time: Logline format timestamp taken when the test started.
    """
    begin_time = vts_logger.normalizeLogLineTimestamp(begin_time)

    def take_br(test_name, begin_time, ad):
        ad.takeBugReport(test_name, begin_time)

    args = [(test_name, begin_time, ad) for ad in ads]
    utils.concurrent_exec(take_br, args)


class AndroidDevice(object):
    """Class representing an android device.

    Each object of this class represents one Android device. The object holds
    handles to adb, fastboot, and various RPC clients.

    Attributes:
        serial: A string that's the serial number of the Android device.
        device_command_port: int, the port number used on the Android device
                for adb port forwarding (for command-response sessions).
        device_callback_port: int, the port number used on the Android device
                for adb port reverse forwarding (for callback sessions).
                Set -1 if callback is not needed (e.g., when this class is used
                as an adb library).
        log: A logger project with a device-specific prefix for each line -
             [AndroidDevice|<serial>]
        log_path: A string that is the path where all logs collected on this
                  android device should be stored.
        adb_logcat_process: A process that collects the adb logcat.
        adb_logcat_file_path: A string that's the full path to the adb logcat
                              file collected, if any.
        vts_agent_process: A process that runs the HAL agent.
        adb: An AdbProxy object used for interacting with the device via adb.
        fastboot: A FastbootProxy object used for interacting with the device
                  via fastboot.
        enable_vts_agent: bool, whether VTS agent is used.
        enable_sl4a: bool, whether SL4A is used. (unsupported)
        enable_sl4a_ed: bool, whether SL4A Event Dispatcher is used. (unsupported)
        host_command_port: the host-side port for runner to agent sessions
                           (to send commands and receive responses).
        host_callback_port: the host-side port for agent to runner sessions
                            (to get callbacks from agent).
        hal: HalMirror, in charge of all communications with the HAL layer.
        lib: LibMirror, in charge of all communications with static and shared
             native libs.
        shell: ShellMirror, in charge of all communications with shell.
        shell_default_nohup: bool, whether to use nohup by default in shell commands.
        _product_type: A string, the device product type (e.g., bullhead) if
                       known, ANDROID_PRODUCT_TYPE_UNKNOWN otherwise.
    """

    def __init__(self,
                 serial="",
                 product_type=ANDROID_PRODUCT_TYPE_UNKNOWN,
                 device_callback_port=5010,
                 shell_default_nohup=False):
        self.serial = serial
        self._product_type = product_type
        self.device_command_port = None
        self.device_callback_port = device_callback_port
        self.log = AndroidDeviceLoggerAdapter(logging.getLogger(),
                                              {"serial": self.serial})
        base_log_path = getattr(logging, "log_path", "/tmp/logs/")
        self.log_path = os.path.join(base_log_path, "AndroidDevice%s" % serial)
        self.adb_logcat_process = None
        self.adb_logcat_file_path = None
        self.vts_agent_process = None
        self.adb = adb.AdbProxy(serial)
        self.fastboot = fastboot.FastbootProxy(serial)
        if not self.isBootloaderMode:
            self.rootAdb()
        self.host_command_port = None
        self.host_callback_port = adb.get_available_host_port()
        if self.device_callback_port >= 0:
            self.adb.reverse_tcp_forward(self.device_callback_port,
                                         self.host_callback_port)
        self.hal = None
        self.lib = None
        self.shell = None
        self.shell_default_nohup = shell_default_nohup
        self.fatal_error = False

    def __del__(self):
        self.cleanUp()

    def cleanUp(self):
        """Cleans up the AndroidDevice object and releases any resources it
        claimed.
        """
        self.stopServices()
        self._StartLLKD()
        if self.host_command_port:
            self.adb.forward("--remove tcp:%s" % self.host_command_port,
                             timeout=adb.DEFAULT_ADB_SHORT_TIMEOUT)
            self.host_command_port = None

    @property
    def shell_default_nohup(self):
        """Gets default value for shell nohup option."""
        if not getattr(self, '_shell_default_nohup'):
            self._shell_default_nohup = False
        return self._shell_default_nohup

    @shell_default_nohup.setter
    def shell_default_nohup(self, value):
        """Sets default value for shell nohup option."""
        self._shell_default_nohup = value
        if self.shell:
            self.shell.shell_default_nohup = value

    @property
    def hasVbmetaSlot(self):
        """True if the device has the slot for vbmeta."""
        if not self.isBootloaderMode:
            self.adb.reboot_bootloader()

        out = self.fastboot.getvar(_FASTBOOT_VAR_HAS_VBMETA).strip()
        if ("%s: yes" % _FASTBOOT_VAR_HAS_VBMETA) in out:
            return True
        return False

    @property
    def isBootloaderMode(self):
        """True if the device is in bootloader mode."""
        return self.serial in list_fastboot_devices()

    @property
    def isTcpFastbootdMode(self):
        """True if the device is in tcp fastbootd mode."""
        if self.serial in list_unauthorized_devices():
            if self.fastboot.isFastbootOverTcp(self.serial):
                out = self.fastboot.getvar("is-userspace").strip()
                if ("is-userspace: yes") in out:
                    return True
        return False

    @property
    def isAdbRoot(self):
        """True if adb is running as root for this device."""
        id_str = self.adb.shell("id -un").strip().decode("utf-8")
        return id_str == "root"

    @property
    def verityEnabled(self):
        """True if verity is enabled for this device."""
        try:
            verified = self.getProp("partition.system.verified")
            if not verified:
                return False
        except adb.AdbError:
            # If verity is disabled, there is no property 'partition.system.verified'
            return False
        return True

    @property
    def model(self):
        """The Android code name for the device."""
        # If device is in bootloader mode, get mode name from fastboot.
        if self.isBootloaderMode:
            out = self.fastboot.getvar("product").strip()
            # "out" is never empty because of the "total time" message fastboot
            # writes to stderr.
            lines = out.decode("utf-8").split('\n', 1)
            if lines:
                tokens = lines[0].split(' ')
                if len(tokens) > 1:
                    return tokens[1].lower()
            return None
        model = self.getProp("ro.build.product").lower()
        if model == "sprout":
            return model
        else:
            model = self.getProp("ro.product.name").lower()
            return model

    @property
    def first_api_level(self):
        """Gets the API level that the device was initially launched with."""
        return self.getProp("ro.product.first_api_level")

    @property
    def sdk_version(self):
        """Gets the SDK version that the device is running with."""
        return self.getProp("ro.build.version.sdk")

    def getLaunchApiLevel(self, strict=True):
        """Gets the API level that the device was initially launched with.

        This method reads ro.product.first_api_level from the device. If the
        value is 0, it then reads ro.build.version.sdk.

        Args:
            strict: A boolean, whether to fail the test if the property is
                    not an integer or not defined.

        Returns:
            An integer, the API level.
            0 if the property is not an integer or not defined.
        """
        level_str = self.first_api_level
        try:
            level = int(level_str)
        except ValueError:
            error_msg = "Cannot parse first_api_level: %s" % level_str
            if strict:
                asserts.fail(error_msg)
            logging.error(error_msg)
            return 0

        if level != 0:
            return level

        level_str = self.sdk_version
        try:
            return int(level_str)
        except ValueError:
            error_msg = "Cannot parse version.sdk: %s" % level_str
            if strict:
                asserts.fail(error_msg)
            logging.error(error_msg)
            return 0

    @property
    def kernel_version(self):
        """Gets the kernel verison from the device.

        This method reads the output of command "uname -r" from the device.

        Returns:
            A tuple of kernel version information
            in the format of (version, patchlevel, sublevel).

            It will fail if failed to get the output or correct format
            from the output of "uname -r" command
        """
        cmd = 'uname -r'
        out = self.adb.shell(cmd)
        out = out.strip()

        match = re.match(r"(\d+)\.(\d+)\.(\d+)", out)
        if match is None:
            asserts.fail("Failed to detect kernel version of device. out:%s", out)

        version = int(match.group(1))
        patchlevel = int(match.group(2))
        sublevel = int(match.group(3))
        logging.info("Detected kernel version: %s", match.group(0))
        return (version, patchlevel, sublevel)

    @property
    def vndk_version(self):
        """Gets the VNDK version that the vendor partition is using."""
        return self.getProp("ro.vndk.version")

    @property
    def vndk_lite(self):
        """Checks whether the vendor partition requests lite VNDK
        enforcement.

        Returns:
            bool, True for lite vndk enforcement.
        """
        vndk_lite_str = self.getProp("ro.vndk.lite")
        if vndk_lite_str is None:
            logging.debug('ro.vndk.lite: %s' % vndk_lite_str)
            return False
        return vndk_lite_str.lower() == "true"

    @property
    def cpu_abi(self):
        """CPU ABI (Application Binary Interface) of the device."""
        out = self.getProp("ro.product.cpu.abi")
        if not out:
            return "unknown"

        cpu_abi = out.lower()
        return cpu_abi

    def getCpuAbiList(self, bitness=""):
        """Gets list of supported ABIs from property.

        Args:
            bitness: 32 or 64. If the argument is not specified, this method
                     returns both 32 and 64-bit ABIs.

        Returns:
            A list of strings, the supported ABIs.
        """
        out = self.getProp("ro.product.cpu.abilist" + str(bitness))
        return out.lower().split(",") if out else []

    @property
    def is64Bit(self):
        """True if device is 64 bit."""
        out = self.adb.shell('uname -m')
        return "64" in out

    @property
    def total_memory(self):
        """Total memory on device.

        Returns:
            long, total memory in bytes. -1 if cannot get memory information.
        """
        total_memory_command = 'cat /proc/meminfo | grep MemTotal'
        out = self.adb.shell(total_memory_command)
        value_unit = out.split(':')[-1].strip().split(' ')

        if len(value_unit) != 2:
            logging.error('Cannot get memory information. %s', out)
            return -1

        value, unit = value_unit

        try:
            value = int(value)
        except ValueError:
            logging.error('Unrecognized total memory value: %s', value_unit)
            return -1

        unit = unit.lower()
        if unit == 'kb':
            value *= 1024
        elif unit == 'mb':
            value *= 1024 * 1024
        elif unit == 'b':
            pass
        else:
            logging.error('Unrecognized total memory unit: %s', value_unit)
            return -1

        return value

    @property
    def libPaths(self):
        """List of strings representing the paths to the native library directories."""
        paths_32 = ["/system/lib", "/vendor/lib"]
        if self.is64Bit:
            paths_64 = ["/system/lib64", "/vendor/lib64"]
            paths_64.extend(paths_32)
            return paths_64
        return paths_32

    @property
    def isAdbLogcatOn(self):
        """Whether there is an ongoing adb logcat collection.
        """
        if self.adb_logcat_process:
            return True
        return False

    @property
    def mac_address(self):
        """The MAC address of the device.
        """
        try:
            command = 'cat /sys/class/net/wlan0/address'
            response = self.adb.shell(command)
            return response.strip()
        except adb.AdbError as e:
            logging.exception(e)
            return "unknown"

    @property
    def sim_state(self):
        """The SIM state of the device.
        """
        return self.getProp('gsm.sim.state')

    @property
    def sim_operator(self):
        """The SIM operator of the device.
        """
        return self.getProp('gsm.operator.alpha')

    def getKernelConfig(self, config_name):
        """Gets kernel config from the device.

        Args:
            config_name: A string, the name of the configuration.

        Returns:
            "y" or "m" if the config is set.
            "" if the config is not set.
            None if fails to read config.
        """
        line_prefix = config_name + "="
        with tempfile.NamedTemporaryFile(delete=False) as temp_file:
            config_path = temp_file.name
        try:
            logging.debug("Pull config.gz to %s", config_path)
            self.adb.pull("/proc/config.gz", config_path)
            with gzip.GzipFile(config_path, "rb") as config_file:
                for line in config_file:
                    if line.strip().startswith(line_prefix):
                        logging.debug("Found config: %s", line)
                        return line.strip()[len(line_prefix):]
            logging.debug("%s is not set.", config_name)
            return ""
        except (adb.AdbError, IOError) as e:
            logging.exception("Cannot read kernel config.", e)
            return None
        finally:
            os.remove(config_path)

    def getBinderBitness(self):
        """Returns the value of BINDER_IPC_32BIT in kernel config.

        Returns:
            32 or 64, binder bitness of the device.
            None if fails to read config.
        """
        config_value = self.getKernelConfig("CONFIG_ANDROID_BINDER_IPC_32BIT")
        if config_value is None:
            return None
        elif config_value:
            return 32
        else:
            return 64

    def loadConfig(self, config):
        """Add attributes to the AndroidDevice object based on json config.

        Args:
            config: A dictionary representing the configs.

        Raises:
            AndroidDeviceError is raised if the config is trying to overwrite
            an existing attribute.
        """
        for k, v in config.items():
            if hasattr(self, k):
                raise AndroidDeviceError(
                    "Attempting to set existing attribute %s on %s" %
                    (k, self.serial))
            setattr(self, k, v)

    def rootAdb(self):
        """Changes adb to root mode for this device."""
        if not self.isAdbRoot:
            try:
                self.adb.root()
                self.adb.wait_for_device()
            except adb.AdbError as e:
                # adb wait-for-device is not always possible in the lab
                # continue with an assumption it's done by the harness.
                logging.exception(e)

    def startAdbLogcat(self):
        """Starts a standing adb logcat collection in separate subprocesses and
        save the logcat in a file.
        """
        if self.isAdbLogcatOn:
            raise AndroidDeviceError(("Android device %s already has an adb "
                                      "logcat thread going on. Cannot start "
                                      "another one.") % self.serial)
        event = tfi.Begin("start adb logcat from android_device",
                          tfi.categories.FRAMEWORK_SETUP)

        f_name = "adblog_%s_%s.txt" % (self.model, self.serial)
        utils.create_dir(self.log_path)
        logcat_file_path = os.path.join(self.log_path, f_name)
        try:
            extra_params = self.adb_logcat_param
        except AttributeError:
            extra_params = "-b all"
        cmd = "adb -s %s logcat -v threadtime %s >> %s" % (self.serial,
                                                           extra_params,
                                                           logcat_file_path)
        self.adb_logcat_process = utils.start_standing_subprocess(cmd)
        self.adb_logcat_file_path = logcat_file_path
        event.End()

    def stopAdbLogcat(self):
        """Stops the adb logcat collection subprocess.
        """
        if not self.isAdbLogcatOn:
            raise AndroidDeviceError(
                "Android device %s does not have an ongoing adb logcat collection."
                % self.serial)

        event = tfi.Begin("stop adb logcat from android_device",
                          tfi.categories.FRAMEWORK_TEARDOWN)
        try:
            utils.stop_standing_subprocess(self.adb_logcat_process)
        except utils.VTSUtilsError as e:
            event.Remove("Cannot stop adb logcat. %s" % e)
            logging.error("Cannot stop adb logcat. %s", e)
        self.adb_logcat_process = None
        event.End()

    def takeBugReport(self, test_name, begin_time):
        """Takes a bug report on the device and stores it in a file.

        Args:
            test_name: Name of the test case that triggered this bug report.
            begin_time: Logline format timestamp taken when the test started.
        """
        br_path = os.path.join(self.log_path, "BugReports")
        utils.create_dir(br_path)
        base_name = ",%s,%s.txt" % (begin_time, self.serial)
        test_name_len = utils.MAX_FILENAME_LEN - len(base_name)
        out_name = test_name[:test_name_len] + base_name
        full_out_path = os.path.join(br_path, out_name.replace(' ', '\ '))
        self.log.info("Taking bugreport for %s on %s", test_name, self.serial)
        self.adb.bugreport(" > %s" % full_out_path)
        self.log.info("Bugreport for %s taken at %s", test_name, full_out_path)

    def waitForBootCompletion(self, timeout=900):
        """Waits for Android framework to broadcast ACTION_BOOT_COMPLETED.

        Args:
            timeout: int, seconds to wait for boot completion. Default is
                     15 minutes.

        Returns:
            bool, True if boot completed. False if any error or timeout
        """
        start = time.time()
        try:
            self.adb.wait_for_device(timeout=timeout)
        except adb.AdbError as e:
            # adb wait-for-device is not always possible in the lab
            logging.exception(e)
            return False

        while not self.isBootCompleted():
            if time.time() - start >= timeout:
                logging.error("Timeout while waiting for boot completion.")
                return False
            time.sleep(1)

        return True

    # Deprecated. Use isBootCompleted instead
    def hasBooted(self):
        """Checks whether the device has booted.

        Returns:
            True if booted, False otherwise.
        """
        return self.isBootCompleted()

    def isBootCompleted(self):
        """Checks whether the device has booted.

        Returns:
            True if booted, False otherwise.
        """
        try:
            if (self.getProp(SYSPROP_SYS_BOOT_COMPLETED) == '1' and
                self.getProp(SYSPROP_DEV_BOOTCOMPLETE) == '1'):
                return True
        except adb.AdbError:
            # adb shell calls may fail during certain period of booting
            # process, which is normal. Ignoring these errors.
            pass

        return False

    def isFrameworkRunning(self, check_boot_completion=True):
        """Checks whether Android framework is started.

        This function will first check boot_completed prop. If boot_completed
        is 0, then return False meaning framework not started.
        Then this function will check whether system_server process is running.
        If yes, then return True meaning framework is started.

        The assumption here is if prop boot_completed is 0 then framework
        is stopped.

        There are still cases which can make this function return wrong
        result. For example, boot_completed is set to 0 manually without
        without stopping framework.

        Args:
            check_boot_completion: bool, whether to check boot completion
                                   before checking framework status. This is an
                                   important step for ensuring framework is
                                   started. Under most circumstances this value
                                   should be set to True.
                                   Default True.

        Returns:
            True if started, False otherwise.
        """
        # First, check whether boot has completed.
        if check_boot_completion and not self.isBootCompleted():
            return False

        cmd = 'ps -g system | grep system_server'
        res = self.adb.shell(cmd, no_except=True)

        return 'system_server' in res[const.STDOUT]

    def startFramework(self,
                       wait_for_completion=True,
                       wait_for_completion_timeout=WAIT_TIMEOUT_SEC):
        """Starts Android framework.

        By default this function will wait for framework starting process to
        finish before returning.

        Args:
            wait_for_completion: bool, whether to wait for framework to complete
                                 starting. Default: True
            wait_for_completion_timeout: timeout in seconds for waiting framework
                                 to start. Default: 2 minutes

        Returns:
            bool, True if framework start success. False otherwise.
        """
        logging.debug("starting Android framework")
        self.adb.shell("start")

        if wait_for_completion:
            if not self.waitForFrameworkStartComplete(
                    wait_for_completion_timeout):
                return False

        logging.info("Android framework started.")
        return True

    def start(self, start_native_server=True):
        """Starts Android framework and waits for ACTION_BOOT_COMPLETED.

        Args:
            start_native_server: bool, whether to start the native server.
        Returns:
            bool, True if framework start success. False otherwise.
        """
        if start_native_server:
            self.startNativeServer()
        return self.startFramework()

    def stopFramework(self):
        """Stops Android framework.

        Method will block until stop is complete.
        """
        logging.debug("stopping Android framework")
        self.adb.shell("stop")
        self.setProp(SYSPROP_SYS_BOOT_COMPLETED, 0)
        logging.info("Android framework stopped")

    def stop(self, stop_native_server=False):
        """Stops Android framework.

        Method will block until stop is complete.

        Args:
            stop_native_server: bool, whether to stop the native server.
        """
        self.stopFramework()
        if stop_native_server:
            self.stopNativeServer()

    def waitForFrameworkStartComplete(self, timeout_secs=WAIT_TIMEOUT_SEC):
        """Wait for Android framework to complete starting.

        Args:
            timeout_secs: int, seconds to wait for boot completion. Default is
                          2 minutes.

        Returns:
            bool, True if framework is started. False otherwise or timeout
        """
        start = time.time()

        # First, wait for boot completion and checks
        if not self.waitForBootCompletion(timeout_secs):
            return False

        while not self.isFrameworkRunning(check_boot_completion=False):
            if time.time() - start >= timeout_secs:
                logging.error("Timeout while waiting for framework to start.")
                return False
            time.sleep(1)
        return True

    def startNativeServer(self):
        """Starts all native servers."""
        self.setProp(SYSPROP_VTS_NATIVE_SERVER, "0")

    def stopNativeServer(self):
        """Stops all native servers."""
        self.setProp(SYSPROP_VTS_NATIVE_SERVER, "1")

    def isProcessRunning(self, process_name):
        """Check whether the given process is running.
        Args:
            process_name: string, name of the process.

        Returns:
            bool, True if the process is running.

        Raises:
            AndroidDeviceError, if ps command failed.
        """
        logging.debug("Checking process %s", process_name)
        cmd_result = self.adb.shell.Execute("ps -A")
        if cmd_result[const.EXIT_CODE][0] != 0:
            logging.error("ps command failed (exit code: %s",
                          cmd_result[const.EXIT_CODE][0])
            raise AndroidDeviceError("ps command failed.")
        if (process_name not in cmd_result[const.STDOUT][0]):
            logging.debug("Process %s not running", process_name)
            return False
        return True

    def waitForProcessStop(self, process_names, timeout_secs=WAIT_TIMEOUT_SEC):
        """Wait until the given process is stopped or timeout.

        Args:
            process_names: list of string, name of the processes.
            timeout_secs: int, timeout in secs.

        Returns:
            bool, True if the process stopped within timeout.
        """
        if process_names:
            for process_name in process_names:
                start = time.time()
                while self.isProcessRunning(process_name):
                    if time.time() - start >= timeout_secs:
                        logging.error(
                            "Timeout while waiting for process %s stop.",
                            process_name)
                        return False
                    time.sleep(1)

        return True

    def setProp(self, name, value):
        """Calls setprop shell command.

        Args:
            name: string, the name of a system property to set
            value: any type, value will be converted to string. Quotes in value
                   is not supported at this time; if value contains a quote,
                   this method will log an error and return.

        Raises:
            AdbError, if name contains invalid character
        """
        if name is None or value is None:
            logging.error("name or value of system property "
                          "should not be None. No property is set.")
            return

        value = str(value)

        if "'" in value or "\"" in value:
            logging.error("Quotes in value of system property "
                          "is not yet supported. No property is set.")
            return

        self.adb.shell("setprop %s \"%s\"" % (name, value))

    def getProp(self, name, timeout=adb.DEFAULT_ADB_SHORT_TIMEOUT):
        """Calls getprop shell command.

        Args:
            name: string, the name of a system property to get

        Returns:
            string, value of the property. If name does not exist; an empty
            string will be returned. decode("utf-8") and strip() will be called
            on the output before returning; None will be returned if input
            name is None

        Raises:
            AdbError, if name contains invalid character
        """
        if name is None:
            logging.error("name of system property should not be None.")
            return None

        out = self.adb.shell("getprop %s" % name, timeout=timeout)
        return out.decode("utf-8").strip()

    def reboot(self, restart_services=True):
        """Reboots the device and wait for device to complete booting.

        This is probably going to print some error messages in console. Only
        use if there's no other option.

        Raises:
            AndroidDeviceError is raised if waiting for completion timed
            out.
        """
        if self.isBootloaderMode:
            self.fastboot.reboot()
            return

        if self.isTcpFastbootdMode:
            self.fastboot.reboot()
            return

        if restart_services:
            has_adb_log = self.isAdbLogcatOn
            has_vts_agent = True if self.vts_agent_process else False
            if has_adb_log:
                self.stopAdbLogcat()
            if has_vts_agent:
                self.stopVtsAgent()

        self.adb.reboot()
        self.waitForBootCompletion()
        self.rootAdb()

        if restart_services:
            if has_adb_log:
                self.startAdbLogcat()
            if has_vts_agent:
                self.startVtsAgent()

    def startServices(self):
        """Starts long running services on the android device.

        1. Start adb logcat capture.
        2. Start VtsAgent and create HalMirror unless disabled in config.
        """
        event = tfi.Begin("start vts services",
                          tfi.categories.FRAMEWORK_SETUP)

        self.enable_vts_agent = getattr(self, "enable_vts_agent", True)
        try:
            self.startAdbLogcat()
        except Exception as e:
            msg = "Failed to start adb logcat!"
            event.Remove(msg)
            self.log.error(msg)
            self.log.exception(e)
            raise
        if self.enable_vts_agent:
            self.startVtsAgent()
            self.device_command_port = int(
                self.adb.shell("cat /data/local/tmp/vts_tcp_server_port"))
            logging.debug("device_command_port: %s", self.device_command_port)
            if not self.host_command_port:
                self.host_command_port = adb.get_available_host_port()
            self.adb.tcp_forward(self.host_command_port,
                                 self.device_command_port)
            self.hal = mirror_tracker.MirrorTracker(
                self.host_command_port, self.host_callback_port, True)
            self.lib = mirror_tracker.MirrorTracker(self.host_command_port)
            self.shell = mirror_tracker.MirrorTracker(
                host_command_port=self.host_command_port, adb=self.adb)
            self.shell.shell_default_nohup = self.shell_default_nohup
            self.resource = mirror_tracker.MirrorTracker(self.host_command_port)
        event.End()

    def Heal(self):
        """Performs a self healing.

        Includes self diagnosis that looks for any framework errors.

        Returns:
            bool, True if everything is ok; False otherwise.
        """
        res = True

        if self.shell:
            res &= self.shell.Heal()

        try:
            self.getProp("ro.build.version.sdk")
        except adb.AdbError:
            if self.serial in list_adb_devices():
                self.log.error(
                    "Device is in adb devices, but is not responding!")
            elif self.isBootloaderMode:
                self.log.info("Device is in bootloader/fastbootd mode")
                return True
            elif self.isTcpFastbootdMode:
                self.log.info("Device is in tcp fastbootd mode")
                return True
            else:
                self.log.error("Device is not in adb devices!")
            self.fatal_error = True
            res = False
        else:
            self.fatal_error = False
        if not res:
            self.log.error('Self diagnosis found problem')

        return res

    def stopServices(self):
        """Stops long running services on the android device."""
        if self.adb_logcat_process:
            self.stopAdbLogcat()
        if getattr(self, "enable_vts_agent", True):
            self.stopVtsAgent()
        if self.hal:
            self.hal.CleanUp()

    def _StartLLKD(self):
        """Starts LLKD"""
        if self.fatal_error:
            self.log.error("Device in fatal error state, skip starting llkd")
            return
        try:
            self.adb.shell('start %s' % LLKD)
        except adb.AdbError as e:
            logging.warn('Failed to start llkd')

    def _StopLLKD(self):
        """Stops LLKD"""
        if self.fatal_error:
            self.log.error("Device in fatal error state, skip stop llkd")
            return
        try:
            self.adb.shell('stop %s' % LLKD)
        except adb.AdbError as e:
            logging.warn('Failed to stop llkd')

    def startVtsAgent(self):
        """Start HAL agent on the AndroidDevice.

        This function starts the target side native agent and is persisted
        throughout the test run.
        """
        self.log.info("Starting VTS agent")
        if self.vts_agent_process:
            raise AndroidDeviceError(
                "HAL agent is already running on %s." % self.serial)

        event = tfi.Begin("start vts agent", tfi.categories.FRAMEWORK_SETUP)

        self._StopLLKD()

        event_cleanup = tfi.Begin("start vts agent -- cleanup", tfi.categories.FRAMEWORK_SETUP)
        cleanup_commands = [
            "rm -f /data/local/tmp/vts_driver_*",
            "rm -f /data/local/tmp/vts_agent_callback*"
        ]

        kill_command = "pgrep 'vts_*' | xargs kill"
        cleanup_commands.append(kill_command)
        try:
            self.adb.shell("\"" + " ; ".join(cleanup_commands) + "\"")
        except adb.AdbError as e:
            self.log.warning(
                "A command to setup the env to start the VTS Agent failed %s",
                e)
        event_cleanup.End()

        log_severity = getattr(self, keys.ConfigKeys.KEY_LOG_SEVERITY, "INFO")
        bits = ['64', '32'] if self.is64Bit else ['32']
        file_names = ['vts_hal_agent', 'vts_hal_driver', 'vts_shell_driver']
        for bitness in bits:
            vts_agent_log_path = os.path.join(
                self.log_path, 'vts_agent_%s_%s.log' % (bitness, self.serial))

            chmod_cmd = ' '.join(
                map(lambda file_name: 'chmod 755 {path}/{bit}/{file_name}{bit};'.format(
                        path=DEFAULT_AGENT_BASE_DIR,
                        bit=bitness,
                        file_name=file_name),
                    file_names))

            cmd = ('adb -s {s} shell "{chmod} LD_LIBRARY_PATH={path}/{bitness} '
                   '{path}/{bitness}/vts_hal_agent{bitness} '
                   '--hal_driver_path_32={path}/32/vts_hal_driver32 '
                   '--hal_driver_path_64={path}/64/vts_hal_driver64 '
                   '--spec_dir={path}/spec '
                   '--shell_driver_path_32={path}/32/vts_shell_driver32 '
                   '--shell_driver_path_64={path}/64/vts_shell_driver64 '
                   '-l {severity}" >> {log} 2>&1').format(
                       s=self.serial,
                       chmod=chmod_cmd,
                       bitness=bitness,
                       path=DEFAULT_AGENT_BASE_DIR,
                       log=vts_agent_log_path,
                       severity=log_severity)
            try:
                self.vts_agent_process = utils.start_standing_subprocess(
                    cmd, check_health_delay=1)
                break
            except utils.VTSUtilsError as e:
                logging.exception(e)
                with open(vts_agent_log_path, 'r') as log_file:
                    logging.error("VTS agent output:\n")
                    logging.error(log_file.read())
                # one common cause is that 64-bit executable is not supported
                # in low API level devices.
                if bitness == '32':
                    msg = "unrecognized bitness"
                    event.Remove(msg)
                    logging.error(msg)
                    raise
                else:
                    logging.error('retrying using a 32-bit binary.')
        event.End()

    def stopVtsAgent(self):
        """Stop the HAL agent running on the AndroidDevice.
        """
        if not self.vts_agent_process:
            return
        try:
            utils.stop_standing_subprocess(self.vts_agent_process)
        except utils.VTSUtilsError as e:
            logging.error("Cannot stop VTS agent. %s", e)
        self.vts_agent_process = None

    @property
    def product_type(self):
        """Gets the product type name."""
        return self._product_type

    def getPackagePid(self, package_name):
        """Gets the pid for a given package. Returns None if not running.

        Args:
            package_name: The name of the package.

        Returns:
            The first pid found under a given package name. None if no process
            was found running the package.

        Raises:
            AndroidDeviceError if the output of the phone's process list was
            in an unexpected format.
        """
        for cmd in ("ps -A", "ps"):
            try:
                out = self.adb.shell('%s | grep "S %s"' % (cmd, package_name))
                if package_name not in out:
                    continue
                try:
                    pid = int(out.split()[1])
                    self.log.info('apk %s has pid %s.', package_name, pid)
                    return pid
                except (IndexError, ValueError) as e:
                    # Possible ValueError from string to int cast.
                    # Possible IndexError from split.
                    self.log.warn('Command \"%s\" returned output line: '
                                  '\"%s\".\nError: %s', cmd, out, e)
            except Exception as e:
                self.log.warn(
                    'Device fails to check if %s running with \"%s\"\n'
                    'Exception %s', package_name, cmd, e)
        self.log.debug("apk %s is not running", package_name)
        return None

class AndroidDeviceLoggerAdapter(logging.LoggerAdapter):
    """A wrapper class that attaches a prefix to all log lines from an
    AndroidDevice object.
    """

    def process(self, msg, kwargs):
        """Process every log message written via the wrapped logger object.

        We are adding the prefix "[AndroidDevice|<serial>]" to all log lines.

        Args:
            msg: string, the original log message.
            kwargs: dict, the key value pairs that can be used to modify the
                    original log message.
        """
        msg = "[AndroidDevice|%s] %s" % (self.extra["serial"], msg)
        return (msg, kwargs)

    def warn(self, msg, *args, **kwargs):
        """Function call warper for warn() to warning()."""
        super(AndroidDeviceLoggerAdapter, self).warning(msg, *args, **kwargs)
