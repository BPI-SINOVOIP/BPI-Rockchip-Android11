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

import base64
import concurrent.futures
import copy
import datetime
import functools
import ipaddress
import json
import logging
import os
import random
import re
import signal
import string
import socket
import subprocess
import time
import threading
import traceback
import zipfile
from concurrent.futures import ThreadPoolExecutor

from acts import signals
from acts.controllers import adb
from acts.libs.proc import job

# File name length is limited to 255 chars on some OS, so we need to make sure
# the file names we output fits within the limit.
MAX_FILENAME_LEN = 255


class ActsUtilsError(Exception):
    """Generic error raised for exceptions in ACTS utils."""


class NexusModelNames:
    # TODO(angli): This will be fixed later by angli.
    ONE = 'sprout'
    N5 = 'hammerhead'
    N5v2 = 'bullhead'
    N6 = 'shamu'
    N6v2 = 'angler'
    N6v3 = 'marlin'
    N5v3 = 'sailfish'


class DozeModeStatus:
    ACTIVE = "ACTIVE"
    IDLE = "IDLE"


ascii_letters_and_digits = string.ascii_letters + string.digits
valid_filename_chars = "-_." + ascii_letters_and_digits

models = ("sprout", "occam", "hammerhead", "bullhead", "razor", "razorg",
          "shamu", "angler", "volantis", "volantisg", "mantaray", "fugu",
          "ryu", "marlin", "sailfish")

manufacture_name_to_model = {
    "flo": "razor",
    "flo_lte": "razorg",
    "flounder": "volantis",
    "flounder_lte": "volantisg",
    "dragon": "ryu"
}

GMT_to_olson = {
    "GMT-9": "America/Anchorage",
    "GMT-8": "US/Pacific",
    "GMT-7": "US/Mountain",
    "GMT-6": "US/Central",
    "GMT-5": "US/Eastern",
    "GMT-4": "America/Barbados",
    "GMT-3": "America/Buenos_Aires",
    "GMT-2": "Atlantic/South_Georgia",
    "GMT-1": "Atlantic/Azores",
    "GMT+0": "Africa/Casablanca",
    "GMT+1": "Europe/Amsterdam",
    "GMT+2": "Europe/Athens",
    "GMT+3": "Europe/Moscow",
    "GMT+4": "Asia/Baku",
    "GMT+5": "Asia/Oral",
    "GMT+6": "Asia/Almaty",
    "GMT+7": "Asia/Bangkok",
    "GMT+8": "Asia/Hong_Kong",
    "GMT+9": "Asia/Tokyo",
    "GMT+10": "Pacific/Guam",
    "GMT+11": "Pacific/Noumea",
    "GMT+12": "Pacific/Fiji",
    "GMT+13": "Pacific/Tongatapu",
    "GMT-11": "Pacific/Midway",
    "GMT-10": "Pacific/Honolulu"
}


def abs_path(path):
    """Resolve the '.' and '~' in a path to get the absolute path.

    Args:
        path: The path to expand.

    Returns:
        The absolute path of the input path.
    """
    return os.path.abspath(os.path.expanduser(path))


def get_current_epoch_time():
    """Current epoch time in milliseconds.

    Returns:
        An integer representing the current epoch time in milliseconds.
    """
    return int(round(time.time() * 1000))


def get_current_human_time():
    """Returns the current time in human readable format.

    Returns:
        The current time stamp in Month-Day-Year Hour:Min:Sec format.
    """
    return time.strftime("%m-%d-%Y %H:%M:%S ")


def epoch_to_human_time(epoch_time):
    """Converts an epoch timestamp to human readable time.

    This essentially converts an output of get_current_epoch_time to an output
    of get_current_human_time

    Args:
        epoch_time: An integer representing an epoch timestamp in milliseconds.

    Returns:
        A time string representing the input time.
        None if input param is invalid.
    """
    if isinstance(epoch_time, int):
        try:
            d = datetime.datetime.fromtimestamp(epoch_time / 1000)
            return d.strftime("%m-%d-%Y %H:%M:%S ")
        except ValueError:
            return None


def get_timezone_olson_id():
    """Return the Olson ID of the local (non-DST) timezone.

    Returns:
        A string representing one of the Olson IDs of the local (non-DST)
        timezone.
    """
    tzoffset = int(time.timezone / 3600)
    gmt = None
    if tzoffset <= 0:
        gmt = "GMT+{}".format(-tzoffset)
    else:
        gmt = "GMT-{}".format(tzoffset)
    return GMT_to_olson[gmt]


def get_next_device(test_bed_controllers, used_devices):
    """Gets the next device in a list of testbed controllers

    Args:
        test_bed_controllers: A list of testbed controllers of a particular
            type, for example a list ACTS Android devices.
        used_devices: A list of devices that have been used.  This can be a
            mix of devices, for example a fuchsia device and an Android device.
    Returns:
        The next device in the test_bed_controllers list or None if there are
        no items that are not in the used devices list.
    """
    if test_bed_controllers:
        device_list = test_bed_controllers
    else:
        raise ValueError('test_bed_controllers is empty.')
    for used_device in used_devices:
        if used_device in device_list:
            device_list.remove(used_device)
    if device_list:
        return device_list[0]
    else:
        return None


def find_files(paths, file_predicate):
    """Locate files whose names and extensions match the given predicate in
    the specified directories.

    Args:
        paths: A list of directory paths where to find the files.
        file_predicate: A function that returns True if the file name and
          extension are desired.

    Returns:
        A list of files that match the predicate.
    """
    file_list = []
    if not isinstance(paths, list):
        paths = [paths]
    for path in paths:
        p = abs_path(path)
        for dirPath, subdirList, fileList in os.walk(p):
            for fname in fileList:
                name, ext = os.path.splitext(fname)
                if file_predicate(name, ext):
                    file_list.append((dirPath, name, ext))
    return file_list


def load_config(file_full_path, log_errors=True):
    """Loads a JSON config file.

    Returns:
        A JSON object.
    """
    with open(file_full_path, 'r') as f:
        try:
            return json.load(f)
        except Exception as e:
            if log_errors:
                logging.error("Exception error to load %s: %s", f, e)
            raise


def load_file_to_base64_str(f_path):
    """Loads the content of a file into a base64 string.

    Args:
        f_path: full path to the file including the file name.

    Returns:
        A base64 string representing the content of the file in utf-8 encoding.
    """
    path = abs_path(f_path)
    with open(path, 'rb') as f:
        f_bytes = f.read()
        base64_str = base64.b64encode(f_bytes).decode("utf-8")
        return base64_str


def dump_string_to_file(content, file_path, mode='w'):
    """ Dump content of a string to

    Args:
        content: content to be dumped to file
        file_path: full path to the file including the file name.
        mode: file open mode, 'w' (truncating file) by default
    :return:
    """
    full_path = abs_path(file_path)
    with open(full_path, mode) as f:
        f.write(content)


def list_of_dict_to_dict_of_dict(list_of_dicts, dict_key):
    """Transforms a list of dicts to a dict of dicts.

    For instance:
    >>> list_of_dict_to_dict_of_dict([{'a': '1', 'b':'2'},
    >>>                               {'a': '3', 'b':'4'}],
    >>>                              'b')

    returns:

    >>> {'2': {'a': '1', 'b':'2'},
    >>>  '4': {'a': '3', 'b':'4'}}

    Args:
        list_of_dicts: A list of dictionaries.
        dict_key: The key in the inner dict to be used as the key for the
                  outer dict.
    Returns:
        A dict of dicts.
    """
    return {d[dict_key]: d for d in list_of_dicts}


def dict_purge_key_if_value_is_none(dictionary):
    """Removes all pairs with value None from dictionary."""
    for k, v in dict(dictionary).items():
        if v is None:
            del dictionary[k]
    return dictionary


def find_field(item_list, cond, comparator, target_field):
    """Finds the value of a field in a dict object that satisfies certain
    conditions.

    Args:
        item_list: A list of dict objects.
        cond: A param that defines the condition.
        comparator: A function that checks if an dict satisfies the condition.
        target_field: Name of the field whose value to be returned if an item
            satisfies the condition.

    Returns:
        Target value or None if no item satisfies the condition.
    """
    for item in item_list:
        if comparator(item, cond) and target_field in item:
            return item[target_field]
    return None


def rand_ascii_str(length):
    """Generates a random string of specified length, composed of ascii letters
    and digits.

    Args:
        length: The number of characters in the string.

    Returns:
        The random string generated.
    """
    letters = [random.choice(ascii_letters_and_digits) for i in range(length)]
    return ''.join(letters)


def rand_hex_str(length):
    """Generates a random string of specified length, composed of hex digits

    Args:
        length: The number of characters in the string.

    Returns:
        The random string generated.
    """
    letters = [random.choice(string.hexdigits) for i in range(length)]
    return ''.join(letters)


# Thead/Process related functions.
def concurrent_exec(func, param_list):
    """Executes a function with different parameters pseudo-concurrently.

    This is basically a map function. Each element (should be an iterable) in
    the param_list is unpacked and passed into the function. Due to Python's
    GIL, there's no true concurrency. This is suited for IO-bound tasks.

    Args:
        func: The function that parforms a task.
        param_list: A list of iterables, each being a set of params to be
            passed into the function.

    Returns:
        A list of return values from each function execution. If an execution
        caused an exception, the exception object will be the corresponding
        result.
    """
    with concurrent.futures.ThreadPoolExecutor(max_workers=30) as executor:
        # Start the load operations and mark each future with its params
        future_to_params = {executor.submit(func, *p): p for p in param_list}
        return_vals = []
        for future in concurrent.futures.as_completed(future_to_params):
            params = future_to_params[future]
            try:
                return_vals.append(future.result())
            except Exception as exc:
                print("{} generated an exception: {}".format(
                    params, traceback.format_exc()))
                return_vals.append(exc)
        return return_vals


def exe_cmd(*cmds):
    """Executes commands in a new shell.

    Args:
        cmds: A sequence of commands and arguments.

    Returns:
        The output of the command run.

    Raises:
        OSError is raised if an error occurred during the command execution.
    """
    cmd = ' '.join(cmds)
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            shell=True)
    (out, err) = proc.communicate()
    if not err:
        return out
    raise OSError(err)


def require_sl4a(android_devices):
    """Makes sure sl4a connection is established on the given AndroidDevice
    objects.

    Args:
        android_devices: A list of AndroidDevice objects.

    Raises:
        AssertionError is raised if any given android device does not have SL4A
        connection established.
    """
    for ad in android_devices:
        msg = "SL4A connection not established properly on %s." % ad.serial
        assert ad.droid, msg


def _assert_subprocess_running(proc):
    """Checks if a subprocess has terminated on its own.

    Args:
        proc: A subprocess returned by subprocess.Popen.

    Raises:
        ActsUtilsError is raised if the subprocess has stopped.
    """
    ret = proc.poll()
    if ret is not None:
        out, err = proc.communicate()
        raise ActsUtilsError("Process %d has terminated. ret: %d, stderr: %s,"
                             " stdout: %s" % (proc.pid, ret, err, out))


def start_standing_subprocess(cmd, check_health_delay=0, shell=True):
    """Starts a long-running subprocess.

    This is not a blocking call and the subprocess started by it should be
    explicitly terminated with stop_standing_subprocess.

    For short-running commands, you should use exe_cmd, which blocks.

    You can specify a health check after the subprocess is started to make sure
    it did not stop prematurely.

    Args:
        cmd: string, the command to start the subprocess with.
        check_health_delay: float, the number of seconds to wait after the
                            subprocess starts to check its health. Default is 0,
                            which means no check.

    Returns:
        The subprocess that got started.
    """
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            shell=shell,
                            preexec_fn=os.setpgrp)
    logging.debug("Start standing subprocess with cmd: %s", cmd)
    if check_health_delay > 0:
        time.sleep(check_health_delay)
        _assert_subprocess_running(proc)
    return proc


def stop_standing_subprocess(proc, kill_signal=signal.SIGTERM):
    """Stops a subprocess started by start_standing_subprocess.

    Before killing the process, we check if the process is running, if it has
    terminated, ActsUtilsError is raised.

    Catches and ignores the PermissionError which only happens on Macs.

    Args:
        proc: Subprocess to terminate.
    """
    pid = proc.pid
    logging.debug("Stop standing subprocess %d", pid)
    _assert_subprocess_running(proc)
    try:
        os.killpg(pid, kill_signal)
    except PermissionError:
        pass


def wait_for_standing_subprocess(proc, timeout=None):
    """Waits for a subprocess started by start_standing_subprocess to finish
    or times out.

    Propagates the exception raised by the subprocess.wait(.) function.
    The subprocess.TimeoutExpired exception is raised if the process timed-out
    rather then terminating.

    If no exception is raised: the subprocess terminated on its own. No need
    to call stop_standing_subprocess() to kill it.

    If an exception is raised: the subprocess is still alive - it did not
    terminate. Either call stop_standing_subprocess() to kill it, or call
    wait_for_standing_subprocess() to keep waiting for it to terminate on its
    own.

    Args:
        p: Subprocess to wait for.
        timeout: An integer number of seconds to wait before timing out.
    """
    proc.wait(timeout)


def sync_device_time(ad):
    """Sync the time of an android device with the current system time.

    Both epoch time and the timezone will be synced.

    Args:
        ad: The android device to sync time on.
    """
    ad.adb.shell("settings put global auto_time 0", ignore_status=True)
    ad.adb.shell("settings put global auto_time_zone 0", ignore_status=True)
    droid = ad.droid
    droid.setTimeZone(get_timezone_olson_id())
    droid.setTime(get_current_epoch_time())


# Timeout decorator block
class TimeoutError(Exception):
    """Exception for timeout decorator related errors.
    """
    pass


def _timeout_handler(signum, frame):
    """Handler function used by signal to terminate a timed out function.
    """
    raise TimeoutError()


def timeout(sec):
    """A decorator used to add time out check to a function.

    This only works in main thread due to its dependency on signal module.
    Do NOT use it if the decorated funtion does not run in the Main thread.

    Args:
        sec: Number of seconds to wait before the function times out.
            No timeout if set to 0

    Returns:
        What the decorated function returns.

    Raises:
        TimeoutError is raised when time out happens.
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            if sec:
                signal.signal(signal.SIGALRM, _timeout_handler)
                signal.alarm(sec)
            try:
                return func(*args, **kwargs)
            except TimeoutError:
                raise TimeoutError(("Function {} timed out after {} "
                                    "seconds.").format(func.__name__, sec))
            finally:
                signal.alarm(0)

        return wrapper

    return decorator


def trim_model_name(model):
    """Trim any prefix and postfix and return the android designation of the
    model name.

    e.g. "m_shamu" will be trimmed to "shamu".

    Args:
        model: model name to be trimmed.

    Returns
        Trimmed model name if one of the known model names is found.
        None otherwise.
    """
    # Directly look up first.
    if model in models:
        return model
    if model in manufacture_name_to_model:
        return manufacture_name_to_model[model]
    # If not found, try trimming off prefix/postfix and look up again.
    tokens = re.split("_|-", model)
    for t in tokens:
        if t in models:
            return t
        if t in manufacture_name_to_model:
            return manufacture_name_to_model[t]
    return None


def force_airplane_mode(ad, new_state, timeout_value=60):
    """Force the device to set airplane mode on or off by adb shell command.

    Args:
        ad: android device object.
        new_state: Turn on airplane mode if True.
            Turn off airplane mode if False.
        timeout_value: max wait time for 'adb wait-for-device'

    Returns:
        True if success.
        False if timeout.
    """

    # Using timeout decorator.
    # Wait for device with timeout. If after <timeout_value> seconds, adb
    # is still waiting for device, throw TimeoutError exception.
    @timeout(timeout_value)
    def wait_for_device_with_timeout(ad):
        ad.adb.wait_for_device()

    try:
        wait_for_device_with_timeout(ad)
        ad.adb.shell("settings put global airplane_mode_on {}".format(
            1 if new_state else 0))
        ad.adb.shell("am broadcast -a android.intent.action.AIRPLANE_MODE")
    except TimeoutError:
        # adb wait for device timeout
        return False
    return True


def get_battery_level(ad):
    """Gets battery level from device

    Returns:
        battery_level: int indicating battery level
    """
    output = ad.adb.shell("dumpsys battery")
    match = re.search(r"level: (?P<battery_level>\S+)", output)
    battery_level = int(match.group("battery_level"))
    return battery_level


def get_device_usb_charging_status(ad):
    """ Returns the usb charging status of the device.

    Args:
        ad: android device object

    Returns:
        True if charging
        False if not charging
     """
    adb_shell_result = ad.adb.shell("dumpsys deviceidle get charging")
    ad.log.info("Device Charging State: {}".format(adb_shell_result))
    return adb_shell_result == 'true'


def disable_usb_charging(ad):
    """ Unplug device from usb charging.

    Args:
        ad: android device object

    Returns:
        True if device is unplugged
        False otherwise
    """
    ad.adb.shell("dumpsys battery unplug")
    if not get_device_usb_charging_status(ad):
        return True
    else:
        ad.log.info("Could not disable USB charging")
        return False


def enable_usb_charging(ad):
    """ Plug device to usb charging.

    Args:
        ad: android device object

    Returns:
        True if device is Plugged
        False otherwise
    """
    ad.adb.shell("dumpsys battery reset")
    if get_device_usb_charging_status(ad):
        return True
    else:
        ad.log.info("Could not enable USB charging")
        return False


def enable_doze(ad):
    """Force the device into doze mode.

    Args:
        ad: android device object.

    Returns:
        True if device is in doze mode.
        False otherwise.
    """
    ad.adb.shell("dumpsys battery unplug")
    ad.adb.shell("dumpsys deviceidle enable")
    ad.adb.shell("dumpsys deviceidle force-idle")
    ad.droid.goToSleepNow()
    time.sleep(5)
    adb_shell_result = ad.adb.shell("dumpsys deviceidle get deep")
    if not adb_shell_result.startswith(DozeModeStatus.IDLE):
        info = ("dumpsys deviceidle get deep: {}".format(adb_shell_result))
        print(info)
        return False
    return True


def disable_doze(ad):
    """Force the device not in doze mode.

    Args:
        ad: android device object.

    Returns:
        True if device is not in doze mode.
        False otherwise.
    """
    ad.adb.shell("dumpsys deviceidle disable")
    ad.adb.shell("dumpsys battery reset")
    adb_shell_result = ad.adb.shell("dumpsys deviceidle get deep")
    if not adb_shell_result.startswith(DozeModeStatus.ACTIVE):
        info = ("dumpsys deviceidle get deep: {}".format(adb_shell_result))
        print(info)
        return False
    return True


def enable_doze_light(ad):
    """Force the device into doze light mode.

    Args:
        ad: android device object.

    Returns:
        True if device is in doze light mode.
        False otherwise.
    """
    ad.adb.shell("dumpsys battery unplug")
    ad.droid.goToSleepNow()
    time.sleep(5)
    ad.adb.shell("cmd deviceidle enable light")
    ad.adb.shell("cmd deviceidle step light")
    adb_shell_result = ad.adb.shell("dumpsys deviceidle get light")
    if not adb_shell_result.startswith(DozeModeStatus.IDLE):
        info = ("dumpsys deviceidle get light: {}".format(adb_shell_result))
        print(info)
        return False
    return True


def disable_doze_light(ad):
    """Force the device not in doze light mode.

    Args:
        ad: android device object.

    Returns:
        True if device is not in doze light mode.
        False otherwise.
    """
    ad.adb.shell("dumpsys battery reset")
    ad.adb.shell("cmd deviceidle disable light")
    adb_shell_result = ad.adb.shell("dumpsys deviceidle get light")
    if not adb_shell_result.startswith(DozeModeStatus.ACTIVE):
        info = ("dumpsys deviceidle get light: {}".format(adb_shell_result))
        print(info)
        return False
    return True


def set_ambient_display(ad, new_state):
    """Set "Ambient Display" in Settings->Display

    Args:
        ad: android device object.
        new_state: new state for "Ambient Display". True or False.
    """
    ad.adb.shell(
        "settings put secure doze_enabled {}".format(1 if new_state else 0))


def set_adaptive_brightness(ad, new_state):
    """Set "Adaptive Brightness" in Settings->Display

    Args:
        ad: android device object.
        new_state: new state for "Adaptive Brightness". True or False.
    """
    ad.adb.shell("settings put system screen_brightness_mode {}".format(
        1 if new_state else 0))


def set_auto_rotate(ad, new_state):
    """Set "Auto-rotate" in QuickSetting

    Args:
        ad: android device object.
        new_state: new state for "Auto-rotate". True or False.
    """
    ad.adb.shell("settings put system accelerometer_rotation {}".format(
        1 if new_state else 0))


def set_location_service(ad, new_state):
    """Set Location service on/off in Settings->Location

    Args:
        ad: android device object.
        new_state: new state for "Location service".
            If new_state is False, turn off location service.
            If new_state if True, set location service to "High accuracy".
    """
    ad.adb.shell("content insert --uri "
                 " content://com.google.settings/partner --bind "
                 "name:s:network_location_opt_in --bind value:s:1")
    ad.adb.shell("content insert --uri "
                 " content://com.google.settings/partner --bind "
                 "name:s:use_location_for_services --bind value:s:1")
    if new_state:
        ad.adb.shell("settings put secure location_mode 3")
    else:
        ad.adb.shell("settings put secure location_mode 0")


def set_mobile_data_always_on(ad, new_state):
    """Set Mobile_Data_Always_On feature bit

    Args:
        ad: android device object.
        new_state: new state for "mobile_data_always_on"
            if new_state is False, set mobile_data_always_on disabled.
            if new_state if True, set mobile_data_always_on enabled.
    """
    ad.adb.shell("settings put global mobile_data_always_on {}".format(
        1 if new_state else 0))


def bypass_setup_wizard(ad):
    """Bypass the setup wizard on an input Android device

    Args:
        ad: android device object.

    Returns:
        True if Android device successfully bypassed the setup wizard.
        False if failed.
    """
    try:
        ad.adb.shell("am start -n \"com.google.android.setupwizard/"
                     ".SetupWizardExitActivity\"")
        logging.debug("No error during default bypass call.")
    except adb.AdbError as adb_error:
        if adb_error.stdout == "ADB_CMD_OUTPUT:0":
            if adb_error.stderr and \
                    not adb_error.stderr.startswith("Error type 3\n"):
                logging.error("ADB_CMD_OUTPUT:0, but error is %s " %
                              adb_error.stderr)
                raise adb_error
            logging.debug("Bypass wizard call received harmless error 3: "
                          "No setup to bypass.")
        elif adb_error.stdout == "ADB_CMD_OUTPUT:255":
            # Run it again as root.
            ad.adb.root_adb()
            logging.debug("Need root access to bypass setup wizard.")
            try:
                ad.adb.shell("am start -n \"com.google.android.setupwizard/"
                             ".SetupWizardExitActivity\"")
                logging.debug("No error during rooted bypass call.")
            except adb.AdbError as adb_error:
                if adb_error.stdout == "ADB_CMD_OUTPUT:0":
                    if adb_error.stderr and \
                            not adb_error.stderr.startswith("Error type 3\n"):
                        logging.error("Rooted ADB_CMD_OUTPUT:0, but error is "
                                      "%s " % adb_error.stderr)
                        raise adb_error
                    logging.debug(
                        "Rooted bypass wizard call received harmless "
                        "error 3: No setup to bypass.")

    # magical sleep to wait for the gservices override broadcast to complete
    time.sleep(3)

    provisioned_state = int(
        ad.adb.shell("settings get global device_provisioned"))
    if provisioned_state != 1:
        logging.error("Failed to bypass setup wizard.")
        return False
    logging.debug("Setup wizard successfully bypassed.")
    return True


def parse_ping_ouput(ad, count, out, loss_tolerance=20):
    """Ping Parsing util.

    Args:
        ad: Android Device Object.
        count: Number of ICMP packets sent
        out: shell output text of ping operation
        loss_tolerance: Threshold after which flag test as false
    Returns:
        False: if packet loss is more than loss_tolerance%
        True: if all good
    """
    result = re.search(
        r"(\d+) packets transmitted, (\d+) received, (\d+)% packet loss", out)
    if not result:
        ad.log.info("Ping failed with %s", out)
        return False

    packet_loss = int(result.group(3))
    packet_xmit = int(result.group(1))
    packet_rcvd = int(result.group(2))
    min_packet_xmit_rcvd = (100 - loss_tolerance) * 0.01
    if (packet_loss > loss_tolerance
            or packet_xmit < count * min_packet_xmit_rcvd
            or packet_rcvd < count * min_packet_xmit_rcvd):
        ad.log.error("%s, ping failed with loss more than tolerance %s%%",
                     result.group(0), loss_tolerance)
        return False
    ad.log.info("Ping succeed with %s", result.group(0))
    return True


def adb_shell_ping(ad,
                   count=120,
                   dest_ip="www.google.com",
                   timeout=200,
                   loss_tolerance=20):
    """Ping utility using adb shell.

    Args:
        ad: Android Device Object.
        count: Number of ICMP packets to send
        dest_ip: hostname or IP address
                 default www.google.com
        timeout: timeout for icmp pings to complete.
    """
    ping_cmd = "ping -W 1"
    if count:
        ping_cmd += " -c %d" % count
    if dest_ip:
        ping_cmd += " %s" % dest_ip
    try:
        ad.log.info("Starting ping test to %s using adb command %s", dest_ip,
                    ping_cmd)
        out = ad.adb.shell(ping_cmd, timeout=timeout, ignore_status=True)
        if not parse_ping_ouput(ad, count, out, loss_tolerance):
            return False
        return True
    except Exception as e:
        ad.log.warning("Ping Test to %s failed with exception %s", dest_ip, e)
        return False


def unzip_maintain_permissions(zip_path, extract_location):
    """Unzip a .zip file while maintaining permissions.

    Args:
        zip_path: The path to the zipped file.
        extract_location: the directory to extract to.
    """
    with zipfile.ZipFile(zip_path, 'r') as zip_file:
        for info in zip_file.infolist():
            _extract_file(zip_file, info, extract_location)


def _extract_file(zip_file, zip_info, extract_location):
    """Extracts a single entry from a ZipFile while maintaining permissions.

    Args:
        zip_file: A zipfile.ZipFile.
        zip_info: A ZipInfo object from zip_file.
        extract_location: The directory to extract to.
    """
    out_path = zip_file.extract(zip_info.filename, path=extract_location)
    perm = zip_info.external_attr >> 16
    os.chmod(out_path, perm)


def get_directory_size(path):
    """Computes the total size of the files in a directory, including subdirectories.

    Args:
        path: The path of the directory.
    Returns:
        The size of the provided directory.
    """
    total = 0
    for dirpath, dirnames, filenames in os.walk(path):
        for filename in filenames:
            total += os.path.getsize(os.path.join(dirpath, filename))
    return total


def get_command_uptime(command_regex):
    """Returns the uptime for a given command.

    Args:
        command_regex: A regex that matches the command line given. Must be
            pgrep compatible.
    """
    pid = job.run('pgrep -f %s' % command_regex).stdout
    runtime = ''
    if pid:
        runtime = job.run('ps -o etime= -p "%s"' % pid).stdout
    return runtime


def get_process_uptime(process):
    """Returns the runtime in [[dd-]hh:]mm:ss, or '' if not running."""
    pid = job.run('pidof %s' % process, ignore_status=True).stdout
    runtime = ''
    if pid:
        runtime = job.run('ps -o etime= -p "%s"' % pid).stdout
    return runtime


def get_device_process_uptime(adb, process):
    """Returns the uptime of a device process."""
    pid = adb.shell('pidof %s' % process, ignore_status=True)
    runtime = ''
    if pid:
        runtime = adb.shell('ps -o etime= -p "%s"' % pid)
    return runtime


def wait_until(func, timeout_s, condition=True, sleep_s=1.0):
    """Executes a function repeatedly until condition is met.

    Args:
      func: The function pointer to execute.
      timeout_s: Amount of time (in seconds) to wait before raising an
                 exception.
      condition: The ending condition of the WaitUntil loop.
      sleep_s: The amount of time (in seconds) to sleep between each function
               execution.

    Returns:
      The time in seconds before detecting a successful condition.

    Raises:
      TimeoutError: If the condition was never met and timeout is hit.
    """
    start_time = time.time()
    end_time = start_time + timeout_s
    count = 0
    while True:
        count += 1
        if func() == condition:
            return time.time() - start_time
        if time.time() > end_time:
            break
        time.sleep(sleep_s)
    raise TimeoutError('Failed to complete function %s in %d seconds having '
                       'attempted %d times.' % (str(func), timeout_s, count))


# Adapted from
# https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#Python
# Available under the Creative Commons Attribution-ShareAlike License
def levenshtein(string1, string2):
    """Returns the Levenshtein distance of two strings.
    Uses Dynamic Programming approach, only keeping track of
    two rows of the DP table at a time.

    Args:
      string1: String to compare to string2
      string2: String to compare to string1

    Returns:
      distance: the Levenshtein distance between string1 and string2
    """

    if len(string1) < len(string2):
        return levenshtein(string2, string1)

    if len(string2) == 0:
        return len(string1)

    previous_row = range(len(string2) + 1)
    for i, char1 in enumerate(string1):
        current_row = [i + 1]
        for j, char2 in enumerate(string2):
            insertions = previous_row[j + 1] + 1
            deletions = current_row[j] + 1
            substitutions = previous_row[j] + (char1 != char2)
            current_row.append(min(insertions, deletions, substitutions))
        previous_row = current_row

    return previous_row[-1]


def string_similarity(s1, s2):
    """Returns a similarity measurement based on Levenshtein distance.

    Args:
      s1: the string to compare to s2
      s2: the string to compare to s1

    Returns:
      result: the similarity metric
    """
    lev = levenshtein(s1, s2)
    try:
        lev_ratio = float(lev) / max(len(s1), len(s2))
        result = (1.0 - lev_ratio) * 100
    except ZeroDivisionError:
        result = 100 if not s2 else 0
    return float(result)


def run_concurrent_actions_no_raise(*calls):
    """Concurrently runs all callables passed in using multithreading.

    Example:

    >>> def test_function_1(arg1, arg2):
    >>>     return arg1, arg2
    >>>
    >>> def test_function_2(arg1, kwarg='kwarg'):
    >>>     raise arg1(kwarg)
    >>>
    >>> run_concurrent_actions_no_raise(
    >>>     lambda: test_function_1('arg1', 'arg2'),
    >>>     lambda: test_function_2(IndexError, kwarg='kwarg'),
    >>> )
    >>> # Output:
    >>> [('arg1', 'arg2'), IndexError('kwarg')]

    Args:
        *calls: A *args list of argumentless callable objects to be called. Note
            that if a function has arguments it can be turned into an
            argumentless function via the lambda keyword or functools.partial.

    Returns:
        An array of the returned values or exceptions received from calls,
        respective of the order given.
    """
    with ThreadPoolExecutor(max_workers=len(calls)) as executor:
        futures = [executor.submit(call) for call in calls]

    results = []
    for future in futures:
        try:
            results.append(future.result())
        except Exception as e:
            results.append(e)
    return results


def run_concurrent_actions(*calls):
    """Runs all callables passed in concurrently using multithreading.

    Examples:

    >>> def test_function_1(arg1, arg2):
    >>>     print(arg1, arg2)
    >>>
    >>> def test_function_2(arg1, kwarg='kwarg'):
    >>>     raise arg1(kwarg)
    >>>
    >>> run_concurrent_actions(
    >>>     lambda: test_function_1('arg1', 'arg2'),
    >>>     lambda: test_function_2(IndexError, kwarg='kwarg'),
    >>> )
    >>> 'The above line raises IndexError("kwarg")'

    Args:
        *calls: A *args list of argumentless callable objects to be called. Note
            that if a function has arguments it can be turned into an
            argumentless function via the lambda keyword or functools.partial.

    Returns:
        An array of the returned values respective of the order of the calls
        argument.

    Raises:
        If an exception is raised in any of the calls, the first exception
        caught will be raised.
    """
    first_exception = None

    class WrappedException(Exception):
        """Raised when a passed-in callable raises an exception."""

    def call_wrapper(call):
        nonlocal first_exception

        try:
            return call()
        except Exception as e:
            logging.exception(e)
            # Note that there is a potential race condition between two
            # exceptions setting first_exception. Even if a locking mechanism
            # was added to prevent this from happening, it is still possible
            # that we capture the second exception as the first exception, as
            # the active thread can swap to the thread that raises the second
            # exception. There is no way to solve this with the tools we have
            # here, so we do not bother. The effects this issue has on the
            # system as a whole are negligible.
            if first_exception is None:
                first_exception = e
            raise WrappedException(e)

    with ThreadPoolExecutor(max_workers=len(calls)) as executor:
        futures = [executor.submit(call_wrapper, call) for call in calls]

    results = []
    for future in futures:
        try:
            results.append(future.result())
        except WrappedException:
            # We do not need to raise here, since first_exception will already
            # be set to the first exception raised by these callables.
            break

    if first_exception:
        raise first_exception

    return results


def test_concurrent_actions(*calls, failure_exceptions=(Exception, )):
    """Concurrently runs all passed in calls using multithreading.

    If any callable raises an Exception found within failure_exceptions, the
    test case is marked as a failure.

    Example:
    >>> def test_function_1(arg1, arg2):
    >>>     print(arg1, arg2)
    >>>
    >>> def test_function_2(kwarg='kwarg'):
    >>>     raise IndexError(kwarg)
    >>>
    >>> test_concurrent_actions(
    >>>     lambda: test_function_1('arg1', 'arg2'),
    >>>     lambda: test_function_2(kwarg='kwarg'),
    >>>     failure_exceptions=IndexError
    >>> )
    >>> 'raises signals.TestFailure due to IndexError being raised.'

    Args:
        *calls: A *args list of argumentless callable objects to be called. Note
            that if a function has arguments it can be turned into an
            argumentless function via the lambda keyword or functools.partial.
        failure_exceptions: A tuple of all possible Exceptions that will mark
            the test as a FAILURE. Any exception that is not in this list will
            mark the tests as UNKNOWN.

    Returns:
        An array of the returned values respective of the order of the calls
        argument.

    Raises:
        signals.TestFailure if any call raises an Exception.
    """
    try:
        return run_concurrent_actions(*calls)
    except signals.TestFailure:
        # Do not modify incoming test failures
        raise
    except failure_exceptions as e:
        raise signals.TestFailure(e)


class SuppressLogOutput(object):
    """Context manager used to suppress all logging output for the specified
    logger and level(s).
    """
    def __init__(self, logger=logging.getLogger(), log_levels=None):
        """Create a SuppressLogOutput context manager

        Args:
            logger: The logger object to suppress
            log_levels: Levels of log handlers to disable.
        """

        self._logger = logger
        self._log_levels = log_levels or [
            logging.DEBUG, logging.INFO, logging.WARNING, logging.ERROR,
            logging.CRITICAL
        ]
        if isinstance(self._log_levels, int):
            self._log_levels = [self._log_levels]
        self._handlers = copy.copy(self._logger.handlers)

    def __enter__(self):
        for handler in self._handlers:
            if handler.level in self._log_levels:
                self._logger.removeHandler(handler)
        return self

    def __exit__(self, *_):
        for handler in self._handlers:
            self._logger.addHandler(handler)


class BlockingTimer(object):
    """Context manager used to block until a specified amount of time has
     elapsed.
     """
    def __init__(self, secs):
        """Initializes a BlockingTimer

        Args:
            secs: Number of seconds to wait before exiting
        """
        self._thread = threading.Timer(secs, lambda: None)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, *_):
        self._thread.join()


def is_valid_ipv4_address(address):
    try:
        socket.inet_pton(socket.AF_INET, address)
    except AttributeError:  # no inet_pton here, sorry
        try:
            socket.inet_aton(address)
        except socket.error:
            return False
        return address.count('.') == 3
    except socket.error:  # not a valid address
        return False

    return True


def is_valid_ipv6_address(address):
    if '%' in address:
        address = address.split('%')[0]
    try:
        socket.inet_pton(socket.AF_INET6, address)
    except socket.error:  # not a valid address
        return False
    return True


def merge_dicts(*dict_args):
    """ Merges args list of dictionaries into a single dictionary.

    Args:
        dict_args: an args list of dictionaries to be merged. If multiple
            dictionaries share a key, the last in the list will appear in the
            final result.
    """
    result = {}
    for dictionary in dict_args:
        result.update(dictionary)
    return result


def ascii_string(uc_string):
    """Converts unicode string to ascii"""
    return str(uc_string).encode('ASCII')


def get_interface_ip_addresses(comm_channel, interface):
    """Gets all of the ip addresses, ipv4 and ipv6, associated with a
       particular interface name.

    Args:
        comm_channel: How to send commands to a device.  Can be ssh, adb serial,
            etc.  Must have the run function implemented.
        interface: The interface name on the device, ie eth0

    Returns:
        A list of dictionaries of the the various IP addresses:
            ipv4_private_local_addresses: Any 192.168, 172.16, or 10
                addresses
            ipv4_public_addresses: Any IPv4 public addresses
            ipv6_link_local_addresses: Any fe80:: addresses
            ipv6_private_local_addresses: Any fd00:: addresses
            ipv6_public_addresses: Any publicly routable addresses
    """
    ipv4_private_local_addresses = []
    ipv4_public_addresses = []
    ipv6_link_local_addresses = []
    ipv6_private_local_addresses = []
    ipv6_public_addresses = []
    all_interfaces_and_addresses = comm_channel.run(
        'ip -o addr | awk \'!/^[0-9]*: ?lo|link\/ether/ {gsub("/", " "); '
        'print $2" "$4}\'').stdout
    ifconfig_output = comm_channel.run('ifconfig %s' % interface).stdout
    for interface_line in all_interfaces_and_addresses.split('\n'):
        if interface != interface_line.split()[0]:
            continue
        on_device_ip = ipaddress.ip_address(interface_line.split()[1])
        if on_device_ip.version() == 4:
            if on_device_ip.is_private():
                ipv4_private_local_addresses.append(str(on_device_ip))
            elif on_device_ip.is_global():
                ipv4_public_addresses.append(str(on_device_ip))
        elif on_device_ip.version() == 6:
            if on_device_ip.is_link_local():
                ipv6_link_local_addresses.append(str(on_device_ip))
            elif on_device_ip.is_private():
                ipv6_private_local_addresses.append(str(on_device_ip))
            elif on_device_ip.is_global():
                ipv6_public_addresses.append(str(on_device_ip))
    return {
        'ipv4_private': ipv4_private_local_addresses,
        'ipv4_public': ipv4_public_addresses,
        'ipv6_link_local': ipv6_link_local_addresses,
        'ipv6_private_local': ipv6_private_local_addresses,
        'ipv6_public': ipv6_public_addresses
    }


def get_interface_based_on_ip(comm_channel, desired_ip_address):
    """Gets the interface for a particular IP

    Args:
        comm_channel: How to send commands to a device.  Can be ssh, adb serial,
            etc.  Must have the run function implemented.
        desired_ip_address: The IP address that is being looked for on a device.

    Returns:
        The name of the test interface.
    """

    desired_ip_address = desired_ip_address.split('%', 1)[0]
    all_ips_and_interfaces = comm_channel.run(
        '(ip -o -4 addr show; ip -o -6 addr show) | '
        'awk \'{print $2" "$4}\'').stdout
    #ipv4_addresses = comm_channel.run(
    #    'ip -o -4 addr show| awk \'{print $2": "$4}\'').stdout
    #ipv6_addresses = comm_channel._ssh_session.run(
    #    'ip -o -6 addr show| awk \'{print $2": "$4}\'').stdout
    #if desired_ip_address in ipv4_addresses:
    #    ip_addresses_to_search = ipv4_addresses
    #elif desired_ip_address in ipv6_addresses:
    #    ip_addresses_to_search = ipv6_addresses
    for ip_address_and_interface in all_ips_and_interfaces.split('\n'):
        if desired_ip_address in ip_address_and_interface:
            return ip_address_and_interface.split()[1][:-1]
    return None


def renew_linux_ip_address(comm_channel, interface):
    comm_channel.run('sudo ifconfig %s down' % interface)
    comm_channel.run('sudo ifconfig %s up' % interface)
    comm_channel.run('sudo dhclient -r %s' % interface)
    comm_channel.run('sudo dhclient %s' % interface)
