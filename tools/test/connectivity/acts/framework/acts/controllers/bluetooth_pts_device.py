#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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
"""
Prerequisites:
    Windows 10
    Bluetooth PTS installed
    Recommended: Running cmder as Admin: https://cmder.net/

### BEGIN SETUP STEPS###
1. Install latest version of Python for windows:
    https://www.python.org/downloads/windows/

Tested successfully on Python 3.7.3.:
    https://www.python.org/ftp/python/3.7.3/python-3.7.3.exe

2. Launch Powershell and setup PATH:
Setx PATH “%PATH%;C:/Users/<username>/AppData/Local/Programs/Python/Python37-32/Scripts”

3. Launch Cmder as Admin before running any PTS related ACTS tests.


### END SETUP STEPS###


Bluetooth PTS controller.
Mandatory parameters are log_directory and sig_root_directory.

ACTS Config setup:
"BluetoothPtsDevice": {
    "log_directory": "C:\\Users\\fsbtt\\Documents\\Profile Tuning Suite\\Test_Dir",
    "sig_root_directory": "C:\\Program Files (x86)\\Bluetooth SIG"
}

"""
from acts import signals
from datetime import datetime
from threading import Thread

import ctypes
import logging
import os
import subprocess
import time
import xml.etree.ElementTree as ET

from xml.dom import minidom
from xml.etree.ElementTree import Element


class BluetoothPtsDeviceConfigError(signals.ControllerError):
    pass


class BluetoothPtsSnifferError(signals.ControllerError):
    pass


ACTS_CONTROLLER_CONFIG_NAME = "BluetoothPtsDevice"
ACTS_CONTROLLER_REFERENCE_NAME = "bluetooth_pts_device"

# Prefix to identify final verdict string. This is a PTS specific log String.
VERDICT = 'VERDICT/'

# Verdict strings that are specific to PTS.
VERDICT_STRINGS = {
    'RESULT_PASS': 'PASS',
    'RESULT_FAIL': 'FAIL',
    'RESULT_INCONC': 'INCONC',
    'RESULT_INCOMP':
    'INCOMP',  # Initial final verdict meaning that test has not completed yet.
    'RESULT_NONE':
    'NONE',  # Error verdict usually indicating internal PTS error.
}

# Sniffer ready log message.
SNIFFER_READY = 'SNIFFER/Save and clear complete'

# PTS Log Types as defined by PTS:
LOG_TYPE_GENERAL_TEXT = 0
LOG_TYPE_FIRST = 1
LOG_TYPE_START_TEST_CASE = 1
LOG_TYPE_TEST_CASE_ENDED = 2
LOG_TYPE_START_DEFAULT = 3
LOG_TYPE_DEFAULT_ENDED = 4
LOG_TYPE_FINAL_VERDICT = 5
LOG_TYPE_PRELIMINARY_VERDICT = 6
LOG_TYPE_TIMEOUT = 7
LOG_TYPE_ASSIGNMENT = 8
LOG_TYPE_START_TIMER = 9
LOG_TYPE_STOP_TIMER = 10
LOG_TYPE_CANCEL_TIMER = 11
LOG_TYPE_READ_TIMER = 12
LOG_TYPE_ATTACH = 13
LOG_TYPE_IMPLICIT_SEND = 14
LOG_TYPE_GOTO = 15
LOG_TYPE_TIMED_OUT_TIMER = 16
LOG_TYPE_ERROR = 17
LOG_TYPE_CREATE = 18
LOG_TYPE_DONE = 19
LOG_TYPE_ACTIVATE = 20
LOG_TYPE_MESSAGE = 21
LOG_TYPE_LINE_MATCHED = 22
LOG_TYPE_LINE_NOT_MATCHED = 23
LOG_TYPE_SEND_EVENT = 24
LOG_TYPE_RECEIVE_EVENT = 25
LOG_TYPE_OTHERWISE_EVENT = 26
LOG_TYPE_RECEIVED_ON_PCO = 27
LOG_TYPE_MATCH_FAILED = 28
LOG_TYPE_COORDINATION_MESSAGE = 29

PTS_DEVICE_EMPTY_CONFIG_MSG = "Configuration is empty, abort!"


def create(config):
    if not config:
        raise errors.PTS_DEVICE_EMPTY_CONFIG_MSG
    return get_instance(config)


def destroy(pts):
    try:
        pts[0].clean_up()
    except:
        pts[0].log.error("Failed to clean up properly.")


def get_info(pts_devices):
    """Get information from the BluetoothPtsDevice object.

    Args:
        pts_devices: A list of BluetoothPtsDevice objects although only one
        will ever be specified.

    Returns:
        A dict, representing info for BluetoothPtsDevice object.
    """
    return {
        "address": pts_devices[0].address,
        "sniffer_ready": pts_devices[0].sniffer_ready,
        "ets_manager_library": pts_devices[0].ets_manager_library,
        "log_directory": pts_devices[0].log_directory,
        "pts_installation_directory":
        pts_devices[0].pts_installation_directory,
    }


def get_instance(config):
    """Create BluetoothPtsDevice instance from a dictionary containing
    information related to PTS. Namely the SIG root directory as
    sig_root_directory and the log directory represented by the log_directory.

    Args:
        config: A dict that contains BluetoothPtsDevice device info.

    Returns:
        A list of BluetoothPtsDevice objects.
    """
    result = []
    try:
        log_directory = config.pop("log_directory")
    except KeyError:
        raise BluetoothPtsDeviceConfigError(
            "Missing mandatory log_directory in config.")
    try:
        sig_root_directory = config.pop("sig_root_directory")
    except KeyError:
        example_path = \
            "C:\\\\Program Files (x86)\\\\Bluetooth SIG"
        raise BluetoothPtsDeviceConfigError(
            "Missing mandatory sig_root_directory in config. Example path: {}".
            format(example_path))

    # "C:\\Program Files (x86)\\Bluetooth SIG\\Bluetooth PTS\\bin\\ETSManager.dll"
    ets_manager_library = "{}\\Bluetooth PTS\\bin\\ETSManager.dll".format(
        sig_root_directory)
    # "C:\\Program Files (x86)\\Bluetooth SIG\\Bluetooth PTS\\bin"
    pts_installation_directory = "{}\\Bluetooth PTS\\bin".format(
        sig_root_directory)
    # "C:\\Program Files (x86)\\Bluetooth SIG\\Bluetooth Protocol Viewer"
    pts_sniffer_directory = "{}\\Bluetooth Protocol Viewer".format(
        sig_root_directory)
    result.append(
        BluetoothPtsDevice(ets_manager_library, log_directory,
                           pts_installation_directory, pts_sniffer_directory))
    return result


class BluetoothPtsDevice:
    """Class representing an Bluetooth PTS device and associated functions.

    Each object of this class represents one BluetoothPtsDevice in ACTS.
    """

    _next_action = -1
    _observers = []
    address = ""
    current_implicit_send_description = ""
    devices = []
    extra_answers = []
    log_directory = ""
    log = None
    ics = None
    ixit = None
    profile_under_test = None
    pts_library = None
    pts_profile_mmi_request = ""
    pts_test_result = VERDICT_STRINGS['RESULT_INCOMP']
    sniffer_ready = False
    test_log_directory = ""
    test_log_prefix = ""

    def __init__(self, ets_manager_library, log_directory,
                 pts_installation_directory, pts_sniffer_directory):
        self.log = logging.getLogger()
        if ets_manager_library is not None:
            self.ets_manager_library = ets_manager_library
        self.log_directory = log_directory
        if pts_installation_directory is not None:
            self.pts_installation_directory = pts_installation_directory
        if pts_sniffer_directory is not None:
            self.pts_sniffer_directory = pts_sniffer_directory
        # Define callback functions
        self.USEAUTOIMPLSENDFUNC = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.use_auto_impl_send_func = self.USEAUTOIMPLSENDFUNC(
            self.UseAutoImplicitSend)

        self.DONGLE_MSG_FUNC = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_char_p)
        self.dongle_msg_func = self.DONGLE_MSG_FUNC(self.DongleMsg)

        self.DEVICE_SEARCH_MSG_FUNC = ctypes.CFUNCTYPE(ctypes.c_bool,
                                                       ctypes.c_char_p,
                                                       ctypes.c_char_p,
                                                       ctypes.c_char_p)
        self.dev_search_msg_func = self.DEVICE_SEARCH_MSG_FUNC(
            self.DeviceSearchMsg)

        self.LOGFUNC = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_char_p,
                                        ctypes.c_char_p, ctypes.c_char_p,
                                        ctypes.c_int, ctypes.c_void_p)
        self.log_func = self.LOGFUNC(self.Log)

        self.ONIMPLSENDFUNC = ctypes.CFUNCTYPE(ctypes.c_char_p,
                                               ctypes.c_char_p, ctypes.c_int)
        self.onimplsend_func = self.ONIMPLSENDFUNC(self.ImplicitSend)

        # Helps with PTS reliability.
        os.chdir(self.pts_installation_directory)
        # Load EtsManager
        self.pts_library = ctypes.cdll.LoadLibrary(self.ets_manager_library)
        self.log.info("ETS Manager library {0:s} has been loaded".format(
            self.ets_manager_library))
        # If post-logging is turned on all callbacks to LPLOG-type function
        # will be executed after test execution is complete. It is recommended
        # that post-logging is turned on to avoid simultaneous invocations of
        # LPLOG and LPAUTOIMPLICITSEND callbacks.
        self.pts_library.SetPostLoggingEx(True)

        self.xml_root = Element("ARCHIVE")
        version = Element("VERSION")
        version.text = "2.0"
        self.xml_root.append(version)
        self.xml_pts_pixit = Element("PicsPixit")
        self.xml_pts_pixit.text = ""
        self.xml_pts_running_log = Element("LOG")
        self.xml_pts_running_log.text = ""
        self.xml_pts_running_summary = Element("SUMMARY")
        self.xml_pts_running_summary.text = ""

    def clean_up(self):
        # Since we have no insight to the actual PTS library,
        # catch all Exceptions and log them.
        try:
            self.log.info("Cleaning up Stack...")
            self.pts_library.ExitStackEx(self.profile_under_test)
        except Exception as err:
            self.log.error(
                "Failed to clean up BluetoothPtsDevice: {}".format(err))
        try:
            self.log.info("Unregistering Profile...")
            self.pts_library.UnregisterProfileEx.argtypes = [ctypes.c_char_p]
            self.pts_library.UnregisterProfileEx(
                self.profile_under_test.encode())
            self.pts_library.UnRegisterGetDevInfoEx()
        except Exception as err:
            self.log.error(
                "Failed to clean up BluetoothPtsDevice: {}".format(err))
        try:
            self.log.info("Cleaning up Sniffer")
            self.pts_library.SnifferTerminateEx()
        except Exception as err:
            self.log.error(
                "Failed to clean up BluetoothPtsDevice: {}".format(err))
        self.log.info("Cleanup Done.")

    def write_xml_pts_pixit_values_for_current_test(self):
        """ Writes the current PICS and IXIT values to the XML result.
        """
        self.xml_pts_pixit.text = "ICS VALUES:\n\n"
        for key, value in self.ics.items():
            self.xml_pts_pixit.text += "{} {}\n".format(
                key.decode(), value.decode())
        self.xml_pts_pixit.text += "\nIXIT VALUES:\n\n"
        for key, (_, value) in self.ixit.items():
            self.xml_pts_pixit.text += "{} {}\n".format(
                key.decode(), value.decode())

    def set_ics_and_ixit(self, ics, ixit):
        self.ics = ics
        self.ixit = ixit

    def set_profile_under_test(self, profile):
        self.profile_under_test = profile

    def setup_pts(self):
        """Prepares PTS to run tests. This needs to be called in test classes
        after ICS, IXIT, and setting Profile under test.
        Specifically BluetoothPtsDevice functions:
            set_profile_under_test
            set_ics_and_ixit
        """

        # Register layer to test with callbacks
        self.pts_library.RegisterProfileWithCallbacks.argtypes = [
            ctypes.c_char_p, self.USEAUTOIMPLSENDFUNC, self.ONIMPLSENDFUNC,
            self.LOGFUNC, self.DEVICE_SEARCH_MSG_FUNC, self.DONGLE_MSG_FUNC
        ]
        res = self.pts_library.RegisterProfileWithCallbacks(
            self.profile_under_test.encode(), self.use_auto_impl_send_func,
            self.onimplsend_func, self.log_func, self.dev_search_msg_func,
            self.dongle_msg_func)

        self.log.info(
            "Profile has been registered with result {0:d}".format(res))

        # GetDeviceInfo module is for discovering devices and PTS Dongle address
        # Initialize GetDeviceInfo and register it with callbacks
        # First parameter is PTS executable directory
        self.pts_library.InitGetDevInfoWithCallbacks.argtypes = [
            ctypes.c_char_p, self.DEVICE_SEARCH_MSG_FUNC, self.DONGLE_MSG_FUNC
        ]
        res = self.pts_library.InitGetDevInfoWithCallbacks(
            self.pts_installation_directory.encode(), self.dev_search_msg_func,
            self.dongle_msg_func)
        self.log.info(
            "GetDevInfo has been initialized with result {0:d}".format(res))
        # Initialize PTS dongle
        res = self.pts_library.VerifyDongleEx()
        self.log.info(
            "PTS dongle has been initialized with result {0:d}".format(res))

        # Find PTS dongle address
        self.pts_library.GetDongleBDAddress.restype = ctypes.c_ulonglong
        self.address = self.pts_library.GetDongleBDAddress()
        self.address_str = "{0:012X}".format(self.address)
        self.log.info("PTS BD Address 0x{0:s}".format(self.address_str))

        # Initialize Bluetooth Protocol Viewer communication module
        self.pts_library.SnifferInitializeEx()

        # If Bluetooth Protocol Viewer is not running, start it
        if not self.is_sniffer_running():
            self.log.info("Starting Protocol Viewer")
            args = [
                "{}\Executables\Core\FTS.exe".format(
                    self.pts_sniffer_directory),
                '/PTS Protocol Viewer=Generic',
                '/OEMTitle=Bluetooth Protocol Viewer', '/OEMKey=Virtual'
            ]
            subprocess.Popen(args)
            sniffer_timeout = 10
            while not self.is_sniffer_running():
                time.sleep(sniffer_timeout)

        # Register to recieve Bluetooth Protocol Viewer notofications
        self.pts_library.SnifferRegisterNotificationEx()
        self.pts_library.SetParameterEx.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p
        ]

        for ics_name in self.ics:
            res = self.pts_library.SetParameterEx(
                ics_name, b'BOOLEAN', self.ics[ics_name],
                self.profile_under_test.encode())
            if res:
                self.log.info("ICS {0:s} set successfully".format(
                    str(ics_name)))
            else:
                self.log.error("Setting ICS {0:s} value failed".format(
                    str(ics_name)))

        for ixit_name in self.ixit:
            res = self.pts_library.SetParameterEx(
                ixit_name, (self.ixit[ixit_name])[0],
                (self.ixit[ixit_name])[1], self.profile_under_test.encode())
            if res:
                self.log.info("IXIT {0:s} set successfully".format(
                    str(ixit_name)))
            else:
                self.log.error("Setting IXIT {0:s} value failed".format(
                    str(ixit_name)))

        # Prepare directory to store Bluetooth Protocol Viewer output
        if not os.path.exists(self.log_directory):
            os.makedirs(self.log_directory)

        address_b = self.address_str.encode("utf-8")
        self.pts_library.InitEtsEx.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p
        ]

        implicit_send_path = "{}\\implicit_send3.dll".format(
            self.pts_installation_directory).encode()
        res = self.pts_library.InitEtsEx(self.profile_under_test.encode(),
                                         self.log_directory.encode(),
                                         implicit_send_path, address_b)
        self.log.info("ETS has been initialized with result {0:s}".format(
            str(res)))

        # Initialize Host Stack DLL
        self.pts_library.InitStackEx.argtypes = [ctypes.c_char_p]
        res = self.pts_library.InitStackEx(self.profile_under_test.encode())
        self.log.info("Stack has been initialized with result {0:s}".format(
            str(res)))

        # Select to receive Log messages after test is done
        self.pts_library.SetPostLoggingEx.argtypes = [
            ctypes.c_bool, ctypes.c_char_p
        ]
        self.pts_library.SetPostLoggingEx(True,
                                          self.profile_under_test.encode())

        # Clear Bluetooth Protocol Viewer. Dongle message callback will update
        # sniffer_ready automatically. No need to fail setup if the timeout
        # is exceeded since the logs will still be available just not starting
        # from a clean slate. Just post a warning.
        self.sniffer_ready = False
        self.pts_library.SnifferClearEx()
        end_time = time.time() + 10
        while not self.sniffer_ready and time.time() < end_time:
            time.sleep(1)
        if not self.sniffer_ready:
            self.log.warning("Sniffer not cleared. Continuing.")

    def is_sniffer_running(self):
        """ Looks for running Bluetooth Protocol Viewer process

        Returns:
            Returns True if finds one, False otherwise.
        """
        prog = [
            line.split()
            for line in subprocess.check_output("tasklist").splitlines()
        ]
        [prog.pop(e) for e in [0, 1, 2]]
        for task in prog:
            task_name = task[0].decode("utf-8")
            if task_name == "Fts.exe":
                self.log.info("Found FTS process successfully.")
                # Sleep recommended by PTS.
                time.sleep(1)
                return True
        return False

    def UseAutoImplicitSend(self):
        """Callback method that defines Which ImplicitSend will be used.

        Returns:
            True always to inform PTS to use the local implementation.
        """
        return True

    def DongleMsg(self, msg_str):
        """ Receives PTS dongle messages.

        Specifically this receives the Bluetooth Protocol Viewer completed
        save/clear operations.

        Returns:
            True if sniffer is ready, False otherwise.
        """
        msg = (ctypes.c_char_p(msg_str).value).decode("utf-8")
        self.log.info(msg)
        # Sleep recommended by PTS.
        time.sleep(1)
        if SNIFFER_READY in msg:
            self.sniffer_ready = True
        return True

    def DeviceSearchMsg(self, addr_str, name_str, cod_str):
        """ Receives device search messages

        Each device may return multiple messages
        Each message will contain device address and may contain device name and
        COD.

        Returns:
            True always and reports to the callback appropriately.
        """
        addr = (ctypes.c_char_p(addr_str).value).replace(b'\xed',
                                                         b' ').decode("utf-8")
        name = (ctypes.c_char_p(name_str).value).replace(b'\xed',
                                                         b' ').decode("utf-8")
        cod = (ctypes.c_char_p(cod_str).value).replace(b'\xed',
                                                       b' ').decode("utf-8")
        self.devices.append(
            "Device address = {0:s} name = {1:s} cod = {2:s}".format(
                addr, name, cod))
        return True

    def Log(self, log_time_str, log_descr_str, log_msg_str, log_type, project):
        """ Receives PTS log messages.

        Returns:
            True always and reports to the callback appropriately.
        """
        log_time = (ctypes.c_char_p(log_time_str).value).decode("utf-8")
        log_descr = (ctypes.c_char_p(log_descr_str).value).decode("utf-8")
        log_msg = (ctypes.c_char_p(log_msg_str).value).decode("utf-8")
        if "Verdict Description" in log_descr:
            self.xml_pts_running_summary.text += "\t- {}".format(log_msg)
        if "Final Verdict" in log_descr:
            self.xml_pts_running_summary.text += "{}{}\n".format(
                log_descr.strip(), log_msg.strip())
        full_log_msg = "{}{}{}".format(log_time, log_descr, log_msg)
        self.xml_pts_running_log.text += "{}\n".format(str(full_log_msg))

        if ctypes.c_int(log_type).value == LOG_TYPE_FINAL_VERDICT:
            indx = log_msg.find(VERDICT)
            if indx == 0:
                if self.pts_test_result == VERDICT_STRINGS['RESULT_INCOMP']:
                    if VERDICT_STRINGS['RESULT_INCONC'] in log_msg:
                        self.pts_test_result = VERDICT_STRINGS['RESULT_INCONC']
                    elif VERDICT_STRINGS['RESULT_FAIL'] in log_msg:
                        self.pts_test_result = VERDICT_STRINGS['RESULT_FAIL']
                    elif VERDICT_STRINGS['RESULT_PASS'] in log_msg:
                        self.pts_test_result = VERDICT_STRINGS['RESULT_PASS']
                    elif VERDICT_STRINGS['RESULT_NONE'] in log_msg:
                        self.pts_test_result = VERDICT_STRINGS['RESULT_NONE']
        return True

    def ImplicitSend(self, description, style):
        """ ImplicitSend callback

        Implicit Send Styles:
            MMI_Style_Ok_Cancel1 =     0x11041, Simple prompt           | OK, Cancel buttons      | Default: OK
            MMI_Style_Ok_Cancel2 =     0x11141, Simple prompt           | Cancel button           | Default: Cancel
            MMI_Style_Ok1 =            0x11040, Simple prompt           | OK button               | Default: OK
            MMI_Style_Yes_No1 =        0x11044, Simple prompt           | Yes, No buttons         | Default: Yes
            MMI_Style_Yes_No_Cancel1 = 0x11043, Simple prompt           | Yes, No buttons         | Default: Yes
            MMI_Style_Abort_Retry1 =   0x11042, Simple prompt           | Abort, Retry buttons    | Default: Abort
            MMI_Style_Edit1 =          0x12040, Request for data input  | OK, Cancel buttons      | Default: OK
            MMI_Style_Edit2 =          0x12140, Select item from a list | OK, Cancel buttons      | Default: OK

        Handling
            MMI_Style_Ok_Cancel1
                OK = return "OK"
                Cancel = return 0

            MMI_Style_Ok_Cancel2
                OK = return "OK"
                Cancel = return 0

            MMI_Style_Ok1
                OK = return "OK", this version should not return 0

            MMI_Style_Yes_No1
                Yes = return "OK"
                No = return 0

            MMI_Style_Yes_No_Cancel1
                Yes = return "OK"
                No = return 0
                Cancel = has been deprecated

            MMI_Style_Abort_Retry1
                Abort = return 0
                Retry = return "OK"

            MMI_Style_Edit1
                OK = return expected string
                Cancel = return 0

            MMI_Style_Edit2
                OK = return expected string
                Cancel = return 0

        Receives ImplicitSend messages
        Description format is as following:
        {MMI_ID,Test Name,Layer Name}MMI Action\n\nDescription: MMI Description
        """
        descr_str = (ctypes.c_char_p(description).value).decode("utf-8")
        # Sleep recommended by PTS.
        time.sleep(1)
        indx = descr_str.find('}')
        implicit_send_info = descr_str[1:(indx)]
        self.current_implicit_send_description = descr_str[(indx + 1):]
        items = implicit_send_info.split(',')
        implicit_send_info_id = items[0]
        implicit_send_info_test_case = items[1]
        self.pts_profile_mmi_request = items[2]
        self.log.info(
            "OnImplicitSend() has been called with the following parameters:\n"
        )
        self.log.info("\t\tproject_name = {0:s}".format(
            self.pts_profile_mmi_request))
        self.log.info("\t\tid = {0:s}".format(implicit_send_info_id))
        self.log.info(
            "\t\ttest_case = {0:s}".format(implicit_send_info_test_case))
        self.log.info("\t\tdescription = {0:s}".format(
            self.current_implicit_send_description))
        self.log.info("\t\tstyle = {0:#X}".format(ctypes.c_int(style).value))
        self.log.info("")
        try:
            self.next_action = int(implicit_send_info_id)
        except Exception as err:
            self.log.error(
                "Setting verdict to RESULT_FAIL, exception found: {}".format(
                    err))
            self.pts_test_result = VERDICT_STRINGS['RESULT_FAIL']
        res = b'OK'
        if len(self.extra_answers) > 0:
            res = self.extra_answers.pop(0).encode()
        self.log.info("Sending Response: {}".format(res))
        return res

    def log_results(self, test_name):
        """Log results.

        Saves the sniffer results in cfa format and clears the sniffer.

        Args:
            test_name: string, name of the test run.
        """
        self.pts_library.SnifferCanSaveEx.restype = ctypes.c_bool
        canSave = ctypes.c_bool(self.pts_library.SnifferCanSaveEx()).value
        self.pts_library.SnifferCanSaveAndClearEx.restype = ctypes.c_bool
        canSaveClear = ctypes.c_bool(
            self.pts_library.SnifferCanSaveAndClearEx()).value
        file_name = "\\{}.cfa".format(self.test_log_prefix).encode()
        path = self.test_log_directory.encode() + file_name

        if canSave == True:
            self.pts_library.SnifferSaveEx.argtypes = [ctypes.c_char_p]
            self.pts_library.SnifferSaveEx(path)
        else:
            self.pts_library.SnifferSaveAndClearEx.argtypes = [ctypes.c_char_p]
            self.pts_library.SnifferSaveAndClearEx(path)
        end_time = time.time() + 60
        while self.sniffer_ready == False and end_time > time.time():
            self.log.info("Waiting for sniffer to be ready...")
            time.sleep(1)
        if self.sniffer_ready == False:
            raise BluetoothPtsSnifferError(
                "Sniffer not ready after 60 seconds.")

    def execute_test(self, test_name, test_timeout=60):
        """Execute the input test name.

        Preps PTS to run the test and waits up to 2 minutes for all steps
        in the execution to finish. Cleanup of PTS related objects follows
        any test verdict.

        Args:
            test_name: string, name of the test to execute.
        """
        today = datetime.now()
        self.write_xml_pts_pixit_values_for_current_test()
        # TODO: Find out how to grab the PTS version. Temporarily
        # hardcoded to v.7.4.1.2.
        self.xml_pts_pixit.text = (
            "Test Case Started: {} v.7.4.1.2, {} started on {}\n\n{}".format(
                self.profile_under_test, test_name,
                today.strftime("%A, %B %d, %Y, %H:%M:%S"),
                self.xml_pts_pixit.text))

        self.xml_pts_running_summary.text += "Test case : {} started\n".format(
            test_name)
        log_time_formatted = "{:%Y_%m_%d_%H_%M_%S}".format(datetime.now())
        formatted_test_name = test_name.replace('/', '_')
        formatted_test_name = formatted_test_name.replace('-', '_')
        self.test_log_prefix = "{}_{}".format(formatted_test_name,
                                              log_time_formatted)
        self.test_log_directory = "{}\\{}\\{}".format(self.log_directory,
                                                      self.profile_under_test,
                                                      self.test_log_prefix)
        os.makedirs(self.test_log_directory)
        curr_test = test_name.encode()

        self.pts_library.StartTestCaseEx.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool
        ]
        res = self.pts_library.StartTestCaseEx(
            curr_test, self.profile_under_test.encode(), True)
        self.log.info("Test has been started with result {0:s}".format(
            str(res)))

        # Wait till verdict is received
        self.log.info("Begin Test Execution... waiting for verdict.")
        end_time = time.time() + test_timeout
        while self.pts_test_result == VERDICT_STRINGS[
                'RESULT_INCOMP'] and time.time() < end_time:
            time.sleep(1)
        self.log.info("End Test Execution... Verdict {}".format(
            self.pts_test_result))

        # Clean up after test is done
        self.pts_library.TestCaseFinishedEx.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p
        ]
        res = self.pts_library.TestCaseFinishedEx(
            curr_test, self.profile_under_test.encode())

        self.log_results(test_name)
        self.xml_pts_running_summary.text += "{} finished\n".format(test_name)
        # Add the log results to the XML output
        self.xml_root.append(self.xml_pts_pixit)
        self.xml_root.append(self.xml_pts_running_log)
        self.xml_root.append(self.xml_pts_running_summary)
        rough_string = ET.tostring(self.xml_root,
                                   encoding='utf-8',
                                   method='xml')
        reparsed = minidom.parseString(rough_string)
        with open(
                "{}\\{}.xml".format(self.test_log_directory,
                                    self.test_log_prefix), "w") as writter:
            writter.write(
                reparsed.toprettyxml(indent="  ", encoding="utf-8").decode())

        if self.pts_test_result is VERDICT_STRINGS['RESULT_PASS']:
            return True
        return False

    """Observer functions"""

    def bind_to(self, callback):
        """ Callbacks to add to the observer.
        This is used for DUTS automatic responses (ImplicitSends local
        implementation).
        """
        self._observers.append(callback)

    @property
    def next_action(self):
        return self._next_action

    @next_action.setter
    def next_action(self, action):
        self._next_action = action
        for callback in self._observers:
            callback(self._next_action)

    """End Observer functions"""
