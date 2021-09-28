#!/usr/bin/env python3
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

import collections
import logging
import math
import os
import re
import socket
import time
from builtins import open
from builtins import str
from datetime import datetime

from acts import context
from acts import logger as acts_logger
from acts import tracelogger
from acts import utils
from acts.controllers import adb
from acts.controllers import fastboot
from acts.controllers.android_lib import errors
from acts.controllers.android_lib import events as android_events
from acts.controllers.android_lib import logcat
from acts.controllers.android_lib import services
from acts.controllers.sl4a_lib import sl4a_manager
from acts.controllers.utils_lib.ssh import connection
from acts.controllers.utils_lib.ssh import settings
from acts.event import event_bus
from acts.libs.proc import job

ACTS_CONTROLLER_CONFIG_NAME = "AndroidDevice"
ACTS_CONTROLLER_REFERENCE_NAME = "android_devices"

ANDROID_DEVICE_PICK_ALL_TOKEN = "*"
# Key name for adb logcat extra params in config file.
ANDROID_DEVICE_ADB_LOGCAT_PARAM_KEY = "adb_logcat_param"
ANDROID_DEVICE_EMPTY_CONFIG_MSG = "Configuration is empty, abort!"
ANDROID_DEVICE_NOT_LIST_CONFIG_MSG = "Configuration should be a list, abort!"
CRASH_REPORT_PATHS = ("/data/tombstones/", "/data/vendor/ramdump/",
                      "/data/ramdump/", "/data/vendor/ssrdump",
                      "/data/vendor/ramdump/bluetooth", "/data/vendor/log/cbd")
CRASH_REPORT_SKIPS = ("RAMDUMP_RESERVED", "RAMDUMP_STATUS", "RAMDUMP_OUTPUT",
                      "bluetooth")
DEFAULT_QXDM_LOG_PATH = "/data/vendor/radio/diag_logs"
DEFAULT_SDM_LOG_PATH = "/data/vendor/slog/"
BUG_REPORT_TIMEOUT = 1800
PULL_TIMEOUT = 300
PORT_RETRY_COUNT = 3
IPERF_TIMEOUT = 60
SL4A_APK_NAME = "com.googlecode.android_scripting"
WAIT_FOR_DEVICE_TIMEOUT = 180
ENCRYPTION_WINDOW = "CryptKeeper"
DEFAULT_DEVICE_PASSWORD = "1111"
RELEASE_ID_REGEXES = [re.compile(r'\w+\.\d+\.\d+'), re.compile(r'N\w+')]


def create(configs):
    """Creates AndroidDevice controller objects.

    Args:
        configs: A list of dicts, each representing a configuration for an
                 Android device.

    Returns:
        A list of AndroidDevice objects.
    """
    if not configs:
        raise errors.AndroidDeviceConfigError(ANDROID_DEVICE_EMPTY_CONFIG_MSG)
    elif configs == ANDROID_DEVICE_PICK_ALL_TOKEN:
        ads = get_all_instances()
    elif not isinstance(configs, list):
        raise errors.AndroidDeviceConfigError(
            ANDROID_DEVICE_NOT_LIST_CONFIG_MSG)
    elif isinstance(configs[0], str):
        # Configs is a list of serials.
        ads = get_instances(configs)
    else:
        # Configs is a list of dicts.
        ads = get_instances_with_configs(configs)

    ads[0].log.info('The primary device under test is "%s".' % ads[0].serial)

    for ad in ads:
        if not ad.is_connected():
            raise errors.AndroidDeviceError(
                ("Android device %s is specified in config"
                 " but is not attached.") % ad.serial,
                serial=ad.serial)
    _start_services_on_ads(ads)
    for ad in ads:
        if ad.droid:
            utils.set_location_service(ad, False)
            utils.sync_device_time(ad)
    return ads


def destroy(ads):
    """Cleans up AndroidDevice objects.

    Args:
        ads: A list of AndroidDevice objects.
    """
    for ad in ads:
        try:
            ad.clean_up()
        except:
            ad.log.exception("Failed to clean up properly.")


def get_info(ads):
    """Get information on a list of AndroidDevice objects.

    Args:
        ads: A list of AndroidDevice objects.

    Returns:
        A list of dict, each representing info for an AndroidDevice objects.
    """
    device_info = []
    for ad in ads:
        info = {"serial": ad.serial, "model": ad.model}
        info.update(ad.build_info)
        device_info.append(info)
    return device_info


def _start_services_on_ads(ads):
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
            ad.start_services()
        except:
            ad.log.exception('Failed to start some services, abort!')
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
    return re.findall(r"(\S+)\t%s" % key, device_list_str)


def list_adb_devices():
    """List all android devices connected to the computer that are detected by
    adb.

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
            serial = c.pop('serial')
        except KeyError:
            raise errors.AndroidDeviceConfigError(
                "Required value 'serial' is missing in AndroidDevice config %s."
                % c)
        ssh_config = c.pop('ssh_config', None)
        ssh_connection = None
        if ssh_config is not None:
            ssh_settings = settings.from_config(ssh_config)
            ssh_connection = connection.SshConnection(ssh_settings)
        ad = AndroidDevice(serial, ssh_connection=ssh_connection)
        ad.load_config(c)
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
        raise ValueError(
            "Could not find a target device that matches condition: %s." %
            kwargs)
    elif len(filtered) == 1:
        return filtered[0]
    else:
        serials = [ad.serial for ad in filtered]
        raise ValueError("More than one device matched: %s" % serials)


def take_bug_reports(ads, test_name, begin_time):
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

    def take_br(test_name, begin_time, ad):
        ad.take_bug_report(test_name, begin_time)

    args = [(test_name, begin_time, ad) for ad in ads]
    utils.concurrent_exec(take_br, args)


class AndroidDevice:
    """Class representing an android device.

    Each object of this class represents one Android device in ACTS, including
    handles to adb, fastboot, and sl4a clients. In addition to direct adb
    commands, this object also uses adb port forwarding to talk to the Android
    device.

    Attributes:
        serial: A string that's the serial number of the Android device.
        log_path: A string that is the path where all logs collected on this
                  android device should be stored.
        log: A logger adapted from root logger with added token specific to an
             AndroidDevice instance.
        adb_logcat_process: A process that collects the adb logcat.
        adb: An AdbProxy object used for interacting with the device via adb.
        fastboot: A FastbootProxy object used for interacting with the device
                  via fastboot.
    """

    def __init__(self, serial='', ssh_connection=None):
        self.serial = serial
        # logging.log_path only exists when this is used in an ACTS test run.
        log_path_base = getattr(logging, 'log_path', '/tmp/logs')
        self.log_dir = 'AndroidDevice%s' % serial
        self.log_path = os.path.join(log_path_base, self.log_dir)
        self.log = tracelogger.TraceLogger(
            AndroidDeviceLoggerAdapter(logging.getLogger(),
                                       {'serial': serial}))
        self._event_dispatchers = {}
        self._services = []
        self.register_service(services.AdbLogcatService(self))
        self.register_service(services.Sl4aService(self))
        self.adb_logcat_process = None
        self.adb = adb.AdbProxy(serial, ssh_connection=ssh_connection)
        self.fastboot = fastboot.FastbootProxy(
            serial, ssh_connection=ssh_connection)
        if not self.is_bootloader:
            self.root_adb()
        self._ssh_connection = ssh_connection
        self.skip_sl4a = False
        self.crash_report = None
        self.data_accounting = collections.defaultdict(int)
        self._sl4a_manager = sl4a_manager.Sl4aManager(self.adb)
        self.last_logcat_timestamp = None
        # Device info cache.
        self._user_added_device_info = {}
        self._sdk_api_level = None

    def clean_up(self):
        """Cleans up the AndroidDevice object and releases any resources it
        claimed.
        """
        self.stop_services()
        for service in self._services:
            service.unregister()
        self._services.clear()
        if self._ssh_connection:
            self._ssh_connection.close()

    def register_service(self, service):
        """Registers the service on the device. """
        service.register()
        self._services.append(service)

    # TODO(angli): This function shall be refactored to accommodate all services
    # and not have hard coded switch for SL4A when b/29157104 is done.
    def start_services(self, skip_setup_wizard=True):
        """Starts long running services on the android device.

        1. Start adb logcat capture.
        2. Start SL4A if not skipped.

        Args:
            skip_setup_wizard: Whether or not to skip the setup wizard.
        """
        if skip_setup_wizard:
            self.exit_setup_wizard()

        event_bus.post(android_events.AndroidStartServicesEvent(self))

    def stop_services(self):
        """Stops long running services on the android device.

        Stop adb logcat and terminate sl4a sessions if exist.
        """
        event_bus.post(
            android_events.AndroidStopServicesEvent(self), ignore_errors=True)

    def is_connected(self):
        out = self.adb.devices()
        devices = _parse_device_list(out, "device")
        return self.serial in devices

    @property
    def build_info(self):
        """Get the build info of this Android device, including build id and
        build type.

        This is not available if the device is in bootloader mode.

        Returns:
            A dict with the build info of this Android device, or None if the
            device is in bootloader mode.
        """
        if self.is_bootloader:
            self.log.error("Device is in fastboot mode, could not get build "
                           "info.")
            return

        build_id = self.adb.getprop("ro.build.id")
        incremental_build_id = self.adb.getprop("ro.build.version.incremental")
        valid_build_id = False
        for regex in RELEASE_ID_REGEXES:
            if re.match(regex, build_id):
                valid_build_id = True
                break
        if not valid_build_id:
            build_id = incremental_build_id

        info = {
            "build_id": build_id,
            "incremental_build_id": incremental_build_id,
            "build_type": self.adb.getprop("ro.build.type")
        }
        return info

    @property
    def device_info(self):
        """Information to be pulled into controller info.

        The latest serial, model, and build_info are included. Additional info
        can be added via `add_device_info`.
        """
        info = {
            'serial': self.serial,
            'model': self.model,
            'build_info': self.build_info,
            'user_added_info': self._user_added_device_info,
            'flavor': self.flavor
        }
        return info

    def sdk_api_level(self):
        if self._sdk_api_level is not None:
            return self._sdk_api_level
        if self.is_bootloader:
            self.log.error(
                'Device is in fastboot mode. Cannot get build info.')
            return
        self._sdk_api_level = int(
            self.adb.shell('getprop ro.build.version.sdk'))
        return self._sdk_api_level

    @property
    def is_bootloader(self):
        """True if the device is in bootloader mode.
        """
        return self.serial in list_fastboot_devices()

    @property
    def is_adb_root(self):
        """True if adb is running as root for this device.
        """
        try:
            return "0" == self.adb.shell("id -u")
        except adb.AdbError:
            # Wait a bit and retry to work around adb flakiness for this cmd.
            time.sleep(0.2)
            return "0" == self.adb.shell("id -u")

    @property
    def model(self):
        """The Android code name for the device."""
        # If device is in bootloader mode, get mode name from fastboot.
        if self.is_bootloader:
            out = self.fastboot.getvar("product").strip()
            # "out" is never empty because of the "total time" message fastboot
            # writes to stderr.
            lines = out.split('\n', 1)
            if lines:
                tokens = lines[0].split(' ')
                if len(tokens) > 1:
                    return tokens[1].lower()
            return None
        model = self.adb.getprop("ro.build.product").lower()
        if model == "sprout":
            return model
        else:
            return self.adb.getprop("ro.product.name").lower()

    @property
    def flavor(self):
        """Returns the specific flavor of Android build the device is using."""
        return self.adb.getprop("ro.build.flavor").lower()

    @property
    def droid(self):
        """Returns the RPC Service of the first Sl4aSession created."""
        if len(self._sl4a_manager.sessions) > 0:
            session_id = sorted(self._sl4a_manager.sessions.keys())[0]
            return self._sl4a_manager.sessions[session_id].rpc_client
        else:
            return None

    @property
    def ed(self):
        """Returns the event dispatcher of the first Sl4aSession created."""
        if len(self._sl4a_manager.sessions) > 0:
            session_id = sorted(self._sl4a_manager.sessions.keys())[0]
            return self._sl4a_manager.sessions[
                session_id].get_event_dispatcher()
        else:
            return None

    @property
    def sl4a_sessions(self):
        """Returns a dictionary of session ids to sessions."""
        return list(self._sl4a_manager.sessions)

    @property
    def is_adb_logcat_on(self):
        """Whether there is an ongoing adb logcat collection.
        """
        if self.adb_logcat_process:
            if self.adb_logcat_process.is_running():
                return True
            else:
                # if skip_sl4a is true, there is no sl4a session
                # if logcat died due to device reboot and sl4a session has
                # not restarted there is no droid.
                if self.droid:
                    self.droid.logI('Logcat died')
                self.log.info("Logcat to %s died", self.log_path)
                return False
        return False

    @property
    def device_log_path(self):
        """Returns the directory for all Android device logs for the current
        test context and serial.
        """
        return context.get_current_context().get_full_output_path(self.serial)

    def update_sdk_api_level(self):
        self._sdk_api_level = None
        self.sdk_api_level()

    def load_config(self, config):
        """Add attributes to the AndroidDevice object based on json config.

        Args:
            config: A dictionary representing the configs.

        Raises:
            AndroidDeviceError is raised if the config is trying to overwrite
            an existing attribute.
        """
        for k, v in config.items():
            # skip_sl4a value can be reset from config file
            if hasattr(self, k) and k != "skip_sl4a":
                raise errors.AndroidDeviceError(
                    "Attempting to set existing attribute %s on %s" %
                    (k, self.serial),
                    serial=self.serial)
            setattr(self, k, v)

    def root_adb(self):
        """Change adb to root mode for this device if allowed.

        If executed on a production build, adb will not be switched to root
        mode per security restrictions.
        """
        self.adb.root()
        self.adb.wait_for_device()

    def get_droid(self, handle_event=True):
        """Create an sl4a connection to the device.

        Return the connection handler 'droid'. By default, another connection
        on the same session is made for EventDispatcher, and the dispatcher is
        returned to the caller as well.
        If sl4a server is not started on the device, try to start it.

        Args:
            handle_event: True if this droid session will need to handle
                events.

        Returns:
            droid: Android object used to communicate with sl4a on the android
                device.
            ed: An optional EventDispatcher to organize events for this droid.

        Examples:
            Don't need event handling:
            >>> ad = AndroidDevice()
            >>> droid = ad.get_droid(False)

            Need event handling:
            >>> ad = AndroidDevice()
            >>> droid, ed = ad.get_droid()
        """
        session = self._sl4a_manager.create_session()
        droid = session.rpc_client
        if handle_event:
            ed = session.get_event_dispatcher()
            return droid, ed
        return droid

    def get_package_pid(self, package_name):
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
                out = self.adb.shell(
                    '%s | grep "S %s"' % (cmd, package_name),
                    ignore_status=True)
                if package_name not in out:
                    continue
                try:
                    pid = int(out.split()[1])
                    self.log.info('apk %s has pid %s.', package_name, pid)
                    return pid
                except (IndexError, ValueError) as e:
                    # Possible ValueError from string to int cast.
                    # Possible IndexError from split.
                    self.log.warn(
                        'Command \"%s\" returned output line: '
                        '\"%s\".\nError: %s', cmd, out, e)
            except Exception as e:
                self.log.warn(
                    'Device fails to check if %s running with \"%s\"\n'
                    'Exception %s', package_name, cmd, e)
        self.log.debug("apk %s is not running", package_name)
        return None

    def get_dispatcher(self, droid):
        """Return an EventDispatcher for an sl4a session

        Args:
            droid: Session to create EventDispatcher for.

        Returns:
            ed: An EventDispatcher for specified session.
        """
        return self._sl4a_manager.sessions[droid.uid].get_event_dispatcher()

    def _is_timestamp_in_range(self, target, log_begin_time, log_end_time):
        low = acts_logger.logline_timestamp_comparator(log_begin_time,
                                                       target) <= 0
        high = acts_logger.logline_timestamp_comparator(log_end_time,
                                                        target) >= 0
        return low and high

    def cat_adb_log(self,
                    tag,
                    begin_time,
                    end_time=None,
                    dest_path="AdbLogExcerpts"):
        """Takes an excerpt of the adb logcat log from a certain time point to
        current time.

        Args:
            tag: An identifier of the time period, usually the name of a test.
            begin_time: Epoch time of the beginning of the time period.
            end_time: Epoch time of the ending of the time period, default None
            dest_path: Destination path of the excerpt file.
        """
        log_begin_time = acts_logger.epoch_to_log_line_timestamp(begin_time)
        if end_time is None:
            log_end_time = acts_logger.get_log_line_timestamp()
        else:
            log_end_time = acts_logger.epoch_to_log_line_timestamp(end_time)
        self.log.debug("Extracting adb log from logcat.")
        logcat_path = os.path.join(self.device_log_path,
                                   'adblog_%s_debug.txt' % self.serial)
        if not os.path.exists(logcat_path):
            self.log.warning("Logcat file %s does not exist." % logcat_path)
            return
        adb_excerpt_dir = os.path.join(self.log_path, dest_path)
        os.makedirs(adb_excerpt_dir, exist_ok=True)
        out_name = '%s,%s.txt' % (acts_logger.normalize_log_line_timestamp(
            log_begin_time), self.serial)
        tag_len = utils.MAX_FILENAME_LEN - len(out_name)
        out_name = '%s,%s' % (tag[:tag_len], out_name)
        adb_excerpt_path = os.path.join(adb_excerpt_dir, out_name)
        with open(adb_excerpt_path, 'w', encoding='utf-8') as out:
            in_file = logcat_path
            with open(in_file, 'r', encoding='utf-8', errors='replace') as f:
                while True:
                    line = None
                    try:
                        line = f.readline()
                        if not line:
                            break
                    except:
                        continue
                    line_time = line[:acts_logger.log_line_timestamp_len]
                    if not acts_logger.is_valid_logline_timestamp(line_time):
                        continue
                    if self._is_timestamp_in_range(line_time, log_begin_time,
                                                   log_end_time):
                        if not line.endswith('\n'):
                            line += '\n'
                        out.write(line)
        return adb_excerpt_path

    def search_logcat(self, matching_string, begin_time=None):
        """Search logcat message with given string.

        Args:
            matching_string: matching_string to search.

        Returns:
            A list of dictionaries with full log message, time stamp string
            and time object. For example:
            [{"log_message": "05-03 17:39:29.898   968  1001 D"
                              "ActivityManager: Sending BOOT_COMPLETE user #0",
              "time_stamp": "2017-05-03 17:39:29.898",
              "datetime_obj": datetime object}]
        """
        logcat_path = os.path.join(self.device_log_path,
                                   'adblog_%s_debug.txt' % self.serial)
        if not os.path.exists(logcat_path):
            self.log.warning("Logcat file %s does not exist." % logcat_path)
            return
        output = job.run(
            "grep '%s' %s" % (matching_string, logcat_path),
            ignore_status=True)
        if not output.stdout or output.exit_status != 0:
            return []
        if begin_time:
            log_begin_time = acts_logger.epoch_to_log_line_timestamp(
                begin_time)
            begin_time = datetime.strptime(log_begin_time,
                                           "%Y-%m-%d %H:%M:%S.%f")
        result = []
        logs = re.findall(r'(\S+\s\S+)(.*)', output.stdout)
        for log in logs:
            time_stamp = log[0]
            time_obj = datetime.strptime(time_stamp, "%Y-%m-%d %H:%M:%S.%f")
            if begin_time and time_obj < begin_time:
                continue
            result.append({
                "log_message": "".join(log),
                "time_stamp": time_stamp,
                "datetime_obj": time_obj
            })
        return result

    def start_adb_logcat(self):
        """Starts a standing adb logcat collection in separate subprocesses and
        save the logcat in a file.
        """
        if self.is_adb_logcat_on:
            self.log.warn(
                'Android device %s already has a running adb logcat thread. ' %
                self.serial)
            return
        # Disable adb log spam filter. Have to stop and clear settings first
        # because 'start' doesn't support --clear option before Android N.
        self.adb.shell("logpersist.stop --clear")
        self.adb.shell("logpersist.start")
        if hasattr(self, 'adb_logcat_param'):
            extra_params = self.adb_logcat_param
        else:
            extra_params = "-b all"

        self.adb_logcat_process = logcat.create_logcat_keepalive_process(
            self.serial, self.log_dir, extra_params)
        self.adb_logcat_process.start()

    def stop_adb_logcat(self):
        """Stops the adb logcat collection subprocess.
        """
        if not self.is_adb_logcat_on:
            self.log.warn(
                'Android device %s does not have an ongoing adb logcat ' %
                self.serial)
            return
        # Set the last timestamp to the current timestamp. This may cause
        # a race condition that allows the same line to be logged twice,
        # but it does not pose a problem for our logging purposes.
        self.adb_logcat_process.stop()
        self.adb_logcat_process = None

    def get_apk_uid(self, apk_name):
        """Get the uid of the given apk.

        Args:
        apk_name: Name of the package, e.g., com.android.phone.

        Returns:
        Linux UID for the apk.
        """
        output = self.adb.shell(
            "dumpsys package %s | grep userId=" % apk_name, ignore_status=True)
        result = re.search(r"userId=(\d+)", output)
        if result:
            return result.group(1)
        else:
            None

    def is_apk_installed(self, package_name):
        """Check if the given apk is already installed.

        Args:
        package_name: Name of the package, e.g., com.android.phone.

        Returns:
        True if package is installed. False otherwise.
        """

        try:
            return bool(
                self.adb.shell(
                    'pm list packages | grep -w "package:%s"' % package_name))

        except Exception as err:
            self.log.error(
                'Could not determine if %s is installed. '
                'Received error:\n%s', package_name, err)
            return False

    def is_sl4a_installed(self):
        return self.is_apk_installed(SL4A_APK_NAME)

    def is_apk_running(self, package_name):
        """Check if the given apk is running.

        Args:
            package_name: Name of the package, e.g., com.android.phone.

        Returns:
        True if package is installed. False otherwise.
        """
        for cmd in ("ps -A", "ps"):
            try:
                out = self.adb.shell(
                    '%s | grep "S %s"' % (cmd, package_name),
                    ignore_status=True)
                if package_name in out:
                    self.log.info("apk %s is running", package_name)
                    return True
            except Exception as e:
                self.log.warn(
                    "Device fails to check is %s running by %s "
                    "Exception %s", package_name, cmd, e)
                continue
        self.log.debug("apk %s is not running", package_name)
        return False

    def is_sl4a_running(self):
        return self.is_apk_running(SL4A_APK_NAME)

    def force_stop_apk(self, package_name):
        """Force stop the given apk.

        Args:
        package_name: Name of the package, e.g., com.android.phone.

        Returns:
        True if package is installed. False otherwise.
        """
        try:
            self.adb.shell(
                'am force-stop %s' % package_name, ignore_status=True)
        except Exception as e:
            self.log.warn("Fail to stop package %s: %s", package_name, e)

    def stop_sl4a(self):
        # TODO(markdr): Move this into sl4a_manager.
        return self.force_stop_apk(SL4A_APK_NAME)

    def start_sl4a(self):
        self._sl4a_manager.start_sl4a_service()

    def take_bug_report(self, test_name, begin_time):
        """Takes a bug report on the device and stores it in a file.

        Args:
            test_name: Name of the test case that triggered this bug report.
            begin_time: Epoch time when the test started.
        """
        self.adb.wait_for_device(timeout=WAIT_FOR_DEVICE_TIMEOUT)
        new_br = True
        try:
            stdout = self.adb.shell("bugreportz -v")
            # This check is necessary for builds before N, where adb shell's ret
            # code and stderr are not propagated properly.
            if "not found" in stdout:
                new_br = False
        except adb.AdbError:
            new_br = False
        br_path = self.device_log_path
        os.makedirs(br_path, exist_ok=True)
        time_stamp = acts_logger.normalize_log_line_timestamp(
            acts_logger.epoch_to_log_line_timestamp(begin_time))
        out_name = "AndroidDevice%s_%s" % (
            self.serial, time_stamp.replace(" ", "_").replace(":", "-"))
        out_name = "%s.zip" % out_name if new_br else "%s.txt" % out_name
        full_out_path = os.path.join(br_path, out_name)
        # in case device restarted, wait for adb interface to return
        self.wait_for_boot_completion()
        self.log.info("Taking bugreport for %s.", test_name)
        if new_br:
            out = self.adb.shell("bugreportz", timeout=BUG_REPORT_TIMEOUT)
            if not out.startswith("OK"):
                raise errors.AndroidDeviceError(
                    'Failed to take bugreport on %s: %s' % (self.serial, out),
                    serial=self.serial)
            br_out_path = out.split(':')[1].strip().split()[0]
            self.adb.pull("%s %s" % (br_out_path, full_out_path))
        else:
            self.adb.bugreport(
                " > {}".format(full_out_path), timeout=BUG_REPORT_TIMEOUT)
        self.log.info("Bugreport for %s taken at %s.", test_name,
                      full_out_path)
        self.adb.wait_for_device(timeout=WAIT_FOR_DEVICE_TIMEOUT)

    def get_file_names(self,
                       directory,
                       begin_time=None,
                       skip_files=[],
                       match_string=None):
        """Get files names with provided directory."""
        cmd = "find %s -type f" % directory
        if begin_time:
            current_time = utils.get_current_epoch_time()
            seconds = int(math.ceil((current_time - begin_time) / 1000.0))
            cmd = "%s -mtime -%ss" % (cmd, seconds)
        if match_string:
            cmd = "%s -iname %s" % (cmd, match_string)
        for skip_file in skip_files:
            cmd = "%s ! -iname %s" % (cmd, skip_file)
        out = self.adb.shell(cmd, ignore_status=True)
        if not out or "No such" in out or "Permission denied" in out or \
            "Not a directory" in out:
            return []
        files = out.split("\n")
        self.log.debug("Find files in directory %s: %s", directory, files)
        return files

    @property
    def external_storage_path(self):
        """
        The $EXTERNAL_STORAGE path on the device. Most commonly set to '/sdcard'
        """
        return self.adb.shell('echo $EXTERNAL_STORAGE')

    def pull_files(self, device_paths, host_path=None):
        """Pull files from devices.

        Args:
            device_paths: List of paths on the device to pull from.
            host_path: Destination path
        """
        if isinstance(device_paths, str):
            device_paths = [device_paths]
        if not host_path:
            host_path = self.log_path
        for device_path in device_paths:
            self.log.info(
                'Pull from device: %s -> %s' % (device_path, host_path))
            self.adb.pull(
                "%s %s" % (device_path, host_path), timeout=PULL_TIMEOUT)

    def check_crash_report(self,
                           test_name=None,
                           begin_time=None,
                           log_crash_report=False):
        """check crash report on the device."""
        crash_reports = []
        for crash_path in CRASH_REPORT_PATHS:
            try:
                cmd = 'cd %s' % crash_path
                self.adb.shell(cmd)
            except Exception as e:
                self.log.debug("received exception %s", e)
                continue
            crashes = self.get_file_names(
                crash_path,
                skip_files=CRASH_REPORT_SKIPS,
                begin_time=begin_time)
            if crash_path == "/data/tombstones/" and crashes:
                tombstones = crashes[:]
                for tombstone in tombstones:
                    if self.adb.shell(
                            'cat %s | grep "crash_dump failed to dump process"'
                            % tombstone):
                        crashes.remove(tombstone)
            if crashes:
                crash_reports.extend(crashes)
        if crash_reports and log_crash_report:
            test_name = test_name or time.strftime("%Y-%m-%d-%Y-%H-%M-%S")
            crash_log_path = os.path.join(self.log_path, test_name,
                                          "Crashes_%s" % self.serial)
            os.makedirs(crash_log_path, exist_ok=True)
            self.pull_files(crash_reports, crash_log_path)
        return crash_reports

    def get_qxdm_logs(self, test_name="", begin_time=None):
        """Get qxdm logs."""
        # Sleep 10 seconds for the buffered log to be written in qxdm log file
        time.sleep(10)
        log_path = getattr(self, "qxdm_log_path", DEFAULT_QXDM_LOG_PATH)
        qxdm_logs = self.get_file_names(
            log_path, begin_time=begin_time, match_string="*.qmdl")
        if qxdm_logs:
            qxdm_log_path = os.path.join(self.device_log_path,
                                         "QXDM_%s" % self.serial)
            os.makedirs(qxdm_log_path, exist_ok=True)
            self.log.info("Pull QXDM Log %s to %s", qxdm_logs, qxdm_log_path)
            self.pull_files(qxdm_logs, qxdm_log_path)
            self.adb.pull(
                "/firmware/image/qdsp6m.qdb %s" % qxdm_log_path,
                timeout=PULL_TIMEOUT,
                ignore_status=True)
        else:
            self.log.error("Didn't find QXDM logs in %s." % log_path)
        if "Verizon" in self.adb.getprop("gsm.sim.operator.alpha"):
            omadm_log_path = os.path.join(self.device_log_path,
                                          "OMADM_%s" % self.serial)
            os.makedirs(omadm_log_path, exist_ok=True)
            self.log.info("Pull OMADM Log")
            self.adb.pull(
                "/data/data/com.android.omadm.service/files/dm/log/ %s" %
                omadm_log_path,
                timeout=PULL_TIMEOUT,
                ignore_status=True)

    def get_sdm_logs(self, test_name="", begin_time=None):
        """Get sdm logs."""
        # Sleep 10 seconds for the buffered log to be written in sdm log file
        time.sleep(10)
        log_path = getattr(self, "sdm_log_path", DEFAULT_SDM_LOG_PATH)
        sdm_logs = self.get_file_names(
            log_path, begin_time=begin_time, match_string="*.sdm*")
        if sdm_logs:
            sdm_log_path = os.path.join(self.device_log_path,
                                        "SDM_%s" % self.serial)
            os.makedirs(sdm_log_path, exist_ok=True)
            self.log.info("Pull SDM Log %s to %s", sdm_logs, sdm_log_path)
            self.pull_files(sdm_logs, sdm_log_path)
        else:
            self.log.error("Didn't find SDM logs in %s." % log_path)
        if "Verizon" in self.adb.getprop("gsm.sim.operator.alpha"):
            omadm_log_path = os.path.join(self.device_log_path,
                                          "OMADM_%s" % self.serial)
            os.makedirs(omadm_log_path, exist_ok=True)
            self.log.info("Pull OMADM Log")
            self.adb.pull(
                "/data/data/com.android.omadm.service/files/dm/log/ %s" %
                omadm_log_path,
                timeout=PULL_TIMEOUT,
                ignore_status=True)

    def start_new_session(self, max_connections=None, server_port=None):
        """Start a new session in sl4a.

        Also caches the droid in a dict with its uid being the key.

        Returns:
            An Android object used to communicate with sl4a on the android
                device.

        Raises:
            Sl4aException: Something is wrong with sl4a and it returned an
            existing uid to a new session.
        """
        session = self._sl4a_manager.create_session(
            max_connections=max_connections, server_port=server_port)

        self._sl4a_manager.sessions[session.uid] = session
        return session.rpc_client

    def terminate_all_sessions(self):
        """Terminate all sl4a sessions on the AndroidDevice instance.

        Terminate all sessions and clear caches.
        """
        self._sl4a_manager.terminate_all_sessions()

    def run_iperf_client_nb(self,
                            server_host,
                            extra_args="",
                            timeout=IPERF_TIMEOUT,
                            log_file_path=None):
        """Start iperf client on the device asynchronously.

        Return status as true if iperf client start successfully.
        And data flow information as results.

        Args:
            server_host: Address of the iperf server.
            extra_args: A string representing extra arguments for iperf client,
                e.g. "-i 1 -t 30".
            log_file_path: The complete file path to log the results.

        """
        cmd = "iperf3 -c {} {}".format(server_host, extra_args)
        if log_file_path:
            cmd += " --logfile {} &".format(log_file_path)
        self.adb.shell_nb(cmd)

    def run_iperf_client(self,
                         server_host,
                         extra_args="",
                         timeout=IPERF_TIMEOUT):
        """Start iperf client on the device.

        Return status as true if iperf client start successfully.
        And data flow information as results.

        Args:
            server_host: Address of the iperf server.
            extra_args: A string representing extra arguments for iperf client,
                e.g. "-i 1 -t 30".

        Returns:
            status: true if iperf client start successfully.
            results: results have data flow information
        """
        out = self.adb.shell(
            "iperf3 -c {} {}".format(server_host, extra_args), timeout=timeout)
        clean_out = out.split('\n')
        if "error" in clean_out[0].lower():
            return False, clean_out
        return True, clean_out

    def run_iperf_server(self, extra_args=""):
        """Start iperf server on the device

        Return status as true if iperf server started successfully.

        Args:
            extra_args: A string representing extra arguments for iperf server.

        Returns:
            status: true if iperf server started successfully.
            results: results have output of command
        """
        out = self.adb.shell("iperf3 -s {}".format(extra_args))
        clean_out = out.split('\n')
        if "error" in clean_out[0].lower():
            return False, clean_out
        return True, clean_out

    def wait_for_boot_completion(self):
        """Waits for Android framework to broadcast ACTION_BOOT_COMPLETED.

        This function times out after 15 minutes.
        """
        timeout_start = time.time()
        timeout = 15 * 60

        self.adb.wait_for_device(timeout=WAIT_FOR_DEVICE_TIMEOUT)
        while time.time() < timeout_start + timeout:
            try:
                completed = self.adb.getprop("sys.boot_completed")
                if completed == '1':
                    return
            except adb.AdbError:
                # adb shell calls may fail during certain period of booting
                # process, which is normal. Ignoring these errors.
                pass
            time.sleep(5)
        raise errors.AndroidDeviceError(
            'Device %s booting process timed out.' % self.serial,
            serial=self.serial)

    def reboot(self, stop_at_lock_screen=False):
        """Reboots the device.

        Terminate all sl4a sessions, reboot the device, wait for device to
        complete booting, and restart an sl4a session if restart_sl4a is True.

        Args:
            stop_at_lock_screen: whether to unlock after reboot. Set to False
                if want to bring the device to reboot up to password locking
                phase. Sl4a checking need the device unlocked after rebooting.
        """
        if self.is_bootloader:
            self.fastboot.reboot()
            return
        self.stop_services()
        self.log.info("Rebooting")
        self.adb.reboot()

        timeout_start = time.time()
        timeout = 2 * 60
        # b/111791239: Newer versions of android sometimes return early after
        # `adb reboot` is called. This means subsequent calls may make it to
        # the device before the reboot goes through, return false positives for
        # getprops such as sys.boot_completed.
        while time.time() < timeout_start + timeout:
            try:
                self.adb.get_state()
                time.sleep(.1)
            except adb.AdbError:
                # get_state will raise an error if the device is not found. We
                # want the device to be missing to prove the device has kicked
                # off the reboot.
                break
        self.wait_for_boot_completion()
        self.root_adb()
        skip_sl4a = self.skip_sl4a
        self.skip_sl4a = self.skip_sl4a or stop_at_lock_screen
        self.start_services()
        self.skip_sl4a = skip_sl4a

    def restart_runtime(self):
        """Restarts android runtime.

        Terminate all sl4a sessions, restarts runtime, wait for framework
        complete restart, and restart an sl4a session if restart_sl4a is True.
        """
        self.stop_services()
        self.log.info("Restarting android runtime")
        self.adb.shell("stop")
        # Reset the boot completed flag before we restart the framework
        # to correctly detect when the framework has fully come up.
        self.adb.shell("setprop sys.boot_completed 0")
        self.adb.shell("start")
        self.wait_for_boot_completion()
        self.root_adb()

        self.start_services()

    def get_ipv4_address(self, interface='wlan0', timeout=5):
        for timer in range(0, timeout):
            try:
                ip_string = self.adb.shell('ifconfig %s|grep inet' % interface)
                break
            except adb.AdbError as e:
                if timer + 1 == timeout:
                    self.log.warning(
                        'Unable to find IP address for %s.' % interface)
                    return None
                else:
                    time.sleep(1)
        result = re.search('addr:(.*) Bcast', ip_string)
        if result != None:
            ip_address = result.group(1)
            try:
                socket.inet_aton(ip_address)
                return ip_address
            except socket.error:
                return None
        else:
            return None

    def get_ipv4_gateway(self, timeout=5):
        for timer in range(0, timeout):
            try:
                gateway_string = self.adb.shell(
                    'dumpsys wifi | grep mDhcpResults')
                break
            except adb.AdbError as e:
                if timer + 1 == timeout:
                    self.log.warning('Unable to find gateway')
                    return None
                else:
                    time.sleep(1)
        result = re.search('Gateway (.*) DNS servers', gateway_string)
        if result != None:
            ipv4_gateway = result.group(1)
            try:
                socket.inet_aton(ipv4_gateway)
                return ipv4_gateway
            except socket.error:
                return None
        else:
            return None

    def send_keycode(self, keycode):
        self.adb.shell("input keyevent KEYCODE_%s" % keycode)

    def get_my_current_focus_window(self):
        """Get the current focus window on screen"""
        output = self.adb.shell(
            'dumpsys window windows | grep -E mCurrentFocus',
            ignore_status=True)
        if not output or "not found" in output or "Can't find" in output or (
                "mCurrentFocus=null" in output):
            result = ''
        else:
            result = output.split(' ')[-1].strip("}")
        self.log.debug("Current focus window is %s", result)
        return result

    def get_my_current_focus_app(self):
        """Get the current focus application"""
        dumpsys_cmd = [
            'dumpsys window | grep -E mFocusedApp',
            'dumpsys window windows | grep -E mFocusedApp'
        ]
        for cmd in dumpsys_cmd:
            output = self.adb.shell(cmd, ignore_status=True)
            if not output or "not found" in output or "Can't find" in output or (
                    "mFocusedApp=null" in output):
                result = ''
            else:
                result = output.split(' ')[-2]
                break
        self.log.debug("Current focus app is %s", result)
        return result

    def is_window_ready(self, window_name=None):
        current_window = self.get_my_current_focus_window()
        if window_name:
            return window_name in current_window
        return current_window and ENCRYPTION_WINDOW not in current_window

    def wait_for_window_ready(self,
                              window_name=None,
                              check_interval=5,
                              check_duration=60):
        elapsed_time = 0
        while elapsed_time < check_duration:
            if self.is_window_ready(window_name=window_name):
                return True
            time.sleep(check_interval)
            elapsed_time += check_interval
        self.log.info("Current focus window is %s",
                      self.get_my_current_focus_window())
        return False

    def is_user_setup_complete(self):
        return "1" in self.adb.shell("settings get secure user_setup_complete")

    def is_screen_awake(self):
        """Check if device screen is in sleep mode"""
        return "Awake" in self.adb.shell("dumpsys power | grep mWakefulness=")

    def is_screen_emergency_dialer(self):
        """Check if device screen is in emergency dialer mode"""
        return "EmergencyDialer" in self.get_my_current_focus_window()

    def is_screen_in_call_activity(self):
        """Check if device screen is in in-call activity notification"""
        return "InCallActivity" in self.get_my_current_focus_window()

    def is_setupwizard_on(self):
        """Check if device screen is in emergency dialer mode"""
        return "setupwizard" in self.get_my_current_focus_app()

    def is_screen_lock_enabled(self):
        """Check if screen lock is enabled"""
        cmd = ("sqlite3 /data/system/locksettings.db .dump"
               " | grep lockscreen.password_type | grep -v alternate")
        out = self.adb.shell(cmd, ignore_status=True)
        if "unable to open" in out:
            self.root_adb()
            out = self.adb.shell(cmd, ignore_status=True)
        if ",0,'0'" not in out and out != "":
            self.log.info("Screen lock is enabled")
            return True
        return False

    def is_waiting_for_unlock_pin(self):
        """Check if device is waiting for unlock pin to boot up"""
        current_window = self.get_my_current_focus_window()
        current_app = self.get_my_current_focus_app()
        if ENCRYPTION_WINDOW in current_window:
            self.log.info("Device is in CrpytKeeper window")
            return True
        if "StatusBar" in current_window and (
            (not current_app) or "FallbackHome" in current_app):
            self.log.info("Device is locked")
            return True
        return False

    def ensure_screen_on(self):
        """Ensure device screen is powered on"""
        if self.is_screen_lock_enabled():
            for _ in range(2):
                self.unlock_screen()
                time.sleep(1)
                if self.is_waiting_for_unlock_pin():
                    self.unlock_screen(password=DEFAULT_DEVICE_PASSWORD)
                    time.sleep(1)
                if not self.is_waiting_for_unlock_pin(
                ) and self.wait_for_window_ready():
                    return True
            return False
        else:
            self.wakeup_screen()
            return True

    def wakeup_screen(self):
        if not self.is_screen_awake():
            self.log.info("Screen is not awake, wake it up")
            self.send_keycode("WAKEUP")

    def go_to_sleep(self):
        if self.is_screen_awake():
            self.send_keycode("SLEEP")

    def send_keycode_number_pad(self, number):
        self.send_keycode("NUMPAD_%s" % number)

    def unlock_screen(self, password=None):
        self.log.info("Unlocking with %s", password or "swipe up")
        # Bring device to SLEEP so that unlock process can start fresh
        self.send_keycode("SLEEP")
        time.sleep(1)
        self.send_keycode("WAKEUP")
        if ENCRYPTION_WINDOW not in self.get_my_current_focus_app():
            self.send_keycode("MENU")
        if password:
            self.send_keycode("DEL")
            for number in password:
                self.send_keycode_number_pad(number)
            self.send_keycode("ENTER")
            self.send_keycode("BACK")

    def exit_setup_wizard(self):
        # Handling Android TV's setupwizard is ignored for now.
        if 'feature:com.google.android.tv.installed' in self.adb.shell(
                'pm list features'):
            return
        if not self.is_user_setup_complete() or self.is_setupwizard_on():
            # b/116709539 need this to prevent reboot after skip setup wizard
            self.adb.shell(
                "am start -a com.android.setupwizard.EXIT", ignore_status=True)
            self.adb.shell(
                "pm disable %s" % self.get_setupwizard_package_name())
        # Wait up to 5 seconds for user_setup_complete to be updated
        end_time = time.time() + 5
        while time.time() < end_time:
            if self.is_user_setup_complete() or not self.is_setupwizard_on():
                return

        # If fail to exit setup wizard, set local.prop and reboot
        if not self.is_user_setup_complete() and self.is_setupwizard_on():
            self.adb.shell("echo ro.test_harness=1 > /data/local.prop")
            self.adb.shell("chmod 644 /data/local.prop")
            self.reboot(stop_at_lock_screen=True)

    def get_setupwizard_package_name(self):
        """Finds setupwizard package/.activity

        Bypass setupwizard or setupwraith depending on device.

         Returns:
            packageName/.ActivityName
        """
        packages_to_skip = "'setupwizard|setupwraith'"
        android_package_name = "com.google.android"
        package = self.adb.shell(
            "pm list packages -f | grep -E {} | grep {}".format(
                packages_to_skip, android_package_name))
        wizard_package = package.split('=')[1]
        activity = package.split('=')[0].split('/')[-2]
        self.log.info("%s/.%sActivity" % (wizard_package, activity))
        return "%s/.%sActivity" % (wizard_package, activity)

    def push_system_file(self, src_file_path, dst_file_path, push_timeout=300):
        """Pushes a file onto the read-only file system.

        For speed, the device is left in root mode after this call, and leaves
        verity disabled. To re-enable verity, call ensure_verity_enabled().

        Args:
            src_file_path: The path to the system app to install.
            dst_file_path: The destination of the file.
            push_timeout: How long to wait for the push to finish.
        Returns:
            Whether or not the install was successful.
        """
        self.adb.ensure_root()
        try:
            self.ensure_verity_disabled()
            self.adb.remount()
            out = self.adb.push(
                '%s %s' % (src_file_path, dst_file_path), timeout=push_timeout)
            if 'error' in out:
                self.log.error('Unable to push system file %s to %s due to %s',
                               src_file_path, dst_file_path, out)
                return False
            return True
        except Exception as e:
            self.log.error('Unable to push system file %s to %s due to %s',
                           src_file_path, dst_file_path, e)
            return False

    def ensure_verity_enabled(self):
        """Ensures that verity is enabled.

        If verity is not enabled, this call will reboot the phone. Note that
        this only works on debuggable builds.
        """
        user = self.adb.get_user_id()
        # The below properties will only exist if verity has been enabled.
        system_verity = self.adb.getprop('partition.system.verified')
        vendor_verity = self.adb.getprop('partition.vendor.verified')
        if not system_verity or not vendor_verity:
            self.adb.ensure_root()
            self.adb.enable_verity()
            self.reboot()
            self.adb.ensure_user(user)

    def ensure_verity_disabled(self):
        """Ensures that verity is disabled.

        If verity is enabled, this call will reboot the phone.
        """
        user = self.adb.get_user_id()
        # The below properties will only exist if verity has been enabled.
        system_verity = self.adb.getprop('partition.system.verified')
        vendor_verity = self.adb.getprop('partition.vendor.verified')
        if system_verity or vendor_verity:
            self.adb.ensure_root()
            self.adb.disable_verity()
            self.reboot()
            self.adb.ensure_user(user)


class AndroidDeviceLoggerAdapter(logging.LoggerAdapter):
    def process(self, msg, kwargs):
        msg = "[AndroidDevice|%s] %s" % (self.extra["serial"], msg)
        return (msg, kwargs)
