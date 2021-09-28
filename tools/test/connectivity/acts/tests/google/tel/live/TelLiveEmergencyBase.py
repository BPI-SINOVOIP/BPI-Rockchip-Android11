#!/usr/bin/env python3.4
#
#   Copyright 2017 - Google
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
    Test Script for Telephony Pre Check In Sanity
"""

import re
import time
from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import DEFAULT_DEVICE_PASSWORD
from acts.test_utils.tel.tel_defines import PHONE_TYPE_CDMA
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_lookup_tables import operator_capabilities
from acts.test_utils.tel.tel_test_utils import abort_all_tests
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import dumpsys_last_call_info
from acts.test_utils.tel.tel_test_utils import dumpsys_last_call_number
from acts.test_utils.tel.tel_test_utils import dumpsys_new_call_info
from acts.test_utils.tel.tel_test_utils import ensure_phone_default_state
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import get_service_state_by_adb
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import hangup_call_by_adb
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import is_sim_lock_enabled
from acts.test_utils.tel.tel_test_utils import initiate_emergency_dialer_call_by_adb
from acts.test_utils.tel.tel_test_utils import last_call_drop_reason
from acts.test_utils.tel.tel_test_utils import reset_device_password
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import wait_for_cell_data_connection
from acts.test_utils.tel.tel_test_utils import wait_for_sim_ready_by_adb
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from acts.utils import get_current_epoch_time

CARRIER_OVERRIDE_CMD = (
    "am broadcast -a com.google.android.carrier.action.LOCAL_OVERRIDE -n "
    "com.google.android.carrier/.ConfigOverridingReceiver --ez")
IMS_FIRST = "carrier_use_ims_first_for_emergency_bool"
ALLOW_NON_EMERGENCY_CALL = "allow_non_emergency_calls_in_ecm_bool"
BLOCK_DURATION_CMD = "duration_blocking_disabled_after_emergency_int"
BLOCK_DURATION = 300


class TelLiveEmergencyBase(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)
        self.number_of_devices = 1
        fake_number = self.user_params.get("fake_emergency_number", "411")
        self.fake_emergency_number = fake_number.strip("+").replace("-", "")
        self.my_devices = self.android_devices[:]

        for ad in self.android_devices:
            if not is_sim_lock_enabled(ad):
                self.setup_dut(ad)
                return True
        self.log.error("No device meets SIM READY or LOADED requirement")
        raise signals.TestAbortClass("No device meets SIM requirement")

    def setup_dut(self, ad):
        self.dut = ad
        output = self.dut.adb.shell("dumpsys carrier_config")
        self.default_settings = {}
        for setting in (IMS_FIRST, ALLOW_NON_EMERGENCY_CALL,
                        BLOCK_DURATION_CMD):
            values = re.findall(r"%s = (\S+)" % setting, output)
            if values:
                self.default_settings[setting] = values[-1]
            else:
                self.default_settings[setting] = ""
        self.dut.adb.shell(" ".join(
            [CARRIER_OVERRIDE_CMD, BLOCK_DURATION_CMD,
             "%s" % BLOCK_DURATION]))
        self.dut_operator = get_operator_name(self.log, ad)
        if self.dut_operator == "tmo":
            self.fake_emergency_number = "611"
        elif self.dut_operator == "vzw":
            self.fake_emergency_number = "922"
        elif self.dut_operator == "spt":
            self.fake_emergency_number = "526"
        if len(self.my_devices) > 1:
            self.android_devices.remove(ad)
            self.android_devices.insert(0, ad)

    def teardown_class(self):
        self.android_devices = self.my_devices
        TelephonyBaseTest.teardown_class(self)

    def setup_test(self):
        if not unlock_sim(self.dut):
            abort_all_tests(self.dut.log, "unable to unlock SIM")
        self.expected_call_result = True

    def teardown_test(self):
        self.dut.ensure_screen_on()
        self.dut.exit_setup_wizard()
        reset_device_password(self.dut, None)
        output = self.dut.adb.shell("dumpsys carrier_config")
        for setting, state in self.default_settings.items():
            values = re.findall(r"%s = (\S+)" % setting, output)
            if values and values[-1] != state:
                self.dut.adb.shell(" ".join(
                    [CARRIER_OVERRIDE_CMD, setting, state]))
        ensure_phone_default_state(self.log, self.dut)

    def change_emergency_number_list(self):
        test_number = "%s:%s" % (self.fake_emergency_number,
                                 self.fake_emergency_number)
        output = self.dut.adb.getprop("ril.test.emergencynumber")
        if output != test_number:
            cmd = "setprop ril.test.emergencynumber %s" % test_number
            self.dut.log.info(cmd)
            self.dut.adb.shell(cmd)
        for _ in range(5):
            existing = self.dut.adb.getprop("ril.ecclist")
            self.dut.log.info("Existing ril.ecclist is: %s", existing)
            if self.fake_emergency_number in existing:
                return True
            emergency_numbers = "%s,%s" % (existing,
                                           self.fake_emergency_number)
            cmd = "setprop ril.ecclist %s" % emergency_numbers
            self.dut.log.info(cmd)
            self.dut.adb.shell(cmd)
            # After some system events, ril.ecclist might change
            # wait sometime for it to settle
            time.sleep(10)
            if self.fake_emergency_number in existing:
                return True
        return False

    def change_qcril_emergency_source_mcc_table(self):
        # This will add the fake number into emergency number list for a mcc
        # in qcril. Please note, the fake number will be send as an emergency
        # number by modem and reach the real 911 by this
        qcril_database_path = self.dut.adb.shell("find /data -iname  qcril.db")
        if not qcril_database_path: return
        mcc = self.dut.droid.telephonyGetNetworkOperator()
        mcc = mcc[:3]
        self.dut.log.info("Add %s mcc %s in qcril_emergency_source_mcc_table")
        self.dut.adb.shell(
            "sqlite3 %s \"INSERT INTO qcril_emergency_source_mcc_table VALUES('%s','%s','','')\""
            % (qcril_database_path, mcc, self.fake_emergency_number))

    def fake_emergency_call_test(self, by_emergency_dialer=True, attemps=3):
        self.dut.log.info("ServiceState is in %s",
                          get_service_state_by_adb(self.log, self.dut))
        if by_emergency_dialer:
            dialing_func = initiate_emergency_dialer_call_by_adb
            callee = self.fake_emergency_number
        else:
            dialing_func = initiate_call
            # Initiate_call method has to have "+" in front
            # otherwise the number will be in dialer without dial out
            # with sl4a fascade. Need further investigation
            callee = "+%s" % self.fake_emergency_number
        for i in range(attemps):
            begin_time = get_current_epoch_time()
            result = True
            if not self.change_emergency_number_list():
                self.dut.log.error("Unable to add number to ril.ecclist")
                return False
            time.sleep(1)
            last_call_number = dumpsys_last_call_number(self.dut)
            call_result = dialing_func(self.log, self.dut, callee)
            time.sleep(3)
            hangup_call_by_adb(self.dut)
            if not call_result:
                last_call_drop_reason(self.dut, begin_time)
            self.dut.send_keycode("BACK")
            self.dut.send_keycode("BACK")
            if not dumpsys_new_call_info(self.dut, last_call_number):
                result = False
            if call_result == self.expected_call_result:
                self.dut.log.info("Call to %s returns %s as expected", callee,
                                  self.expected_call_result)
            else:
                self.dut.log.info("Call to %s returns %s", callee,
                                  not self.expected_call_result)
                result = False
            if result:
                return True
            ecclist = self.dut.adb.getprop("ril.ecclist")
            self.dut.log.info("ril.ecclist = %s", ecclist)
            if self.fake_emergency_number in ecclist:
                if i == attemps - 1:
                    self.dut.log.error("%s is in ril-ecclist, but call failed",
                                       self.fake_emergency_number)
                else:
                    self.dut.log.warning(
                        "%s is in ril-ecclist, but call failed, try again",
                        self.fake_emergency_number)
            else:
                if i == attemps - 1:
                    self.dut.log.error("Fail to write %s to ril-ecclist",
                                       self.fake_emergency_number)
                else:
                    self.dut.log.info("%s is not in ril-ecclist",
                                      self.fake_emergency_number)
        self.dut.log.info("fake_emergency_call_test result is %s", result)
        return result

    def set_ims_first(self, state):
        output = self.dut.adb.shell(
            "dumpsys carrier_config | grep %s | tail -1" % IMS_FIRST)
        self.dut.log.info(output)
        if state in output: return
        cmd = " ".join([CARRIER_OVERRIDE_CMD, IMS_FIRST, state])
        self.dut.log.info(cmd)
        self.dut.adb.shell(cmd)
        self.dut.log.info(
            self.dut.adb.shell(
                "dumpsys carrier_config | grep %s | tail -1" % IMS_FIRST))

    def set_allow_non_emergency_call(self, state):
        output = self.dut.adb.shell(
            "dumpsys carrier_config | grep %s | tail -1" %
            ALLOW_NON_EMERGENCY_CALL)
        self.dut.log.info(output)
        if state in output: return
        cmd = " ".join([CARRIER_OVERRIDE_CMD, ALLOW_NON_EMERGENCY_CALL, state])
        self.dut.log.info(cmd)
        self.dut.adb.shell(cmd)
        self.dut.log.info(
            self.dut.adb.shell("dumpsys carrier_config | grep %s | tail -1" %
                               ALLOW_NON_EMERGENCY_CALL))

    def check_emergency_call_back_mode(self,
                                       by_emergency_dialer=True,
                                       non_emergency_call_allowed=True,
                                       attemps=3):
        state = "true" if non_emergency_call_allowed else "false"
        self.set_allow_non_emergency_call(state)
        result = True
        for _ in range(attemps):
            if not self.change_emergency_number_list():
                self.dut.log.error("Unable to add number to ril.ecclist")
                return False
            last_call_number = dumpsys_last_call_number(self.dut)
            self.dut.log.info("Hung up fake emergency call in ringing")
            if by_emergency_dialer:
                initiate_emergency_dialer_call_by_adb(
                    self.log, self.dut, self.fake_emergency_number, timeout=0)
            else:
                callee = "+%s" % self.fake_emergency_number
                self.dut.droid.telecomCallNumber(callee)
            time.sleep(3)
            hangup_call_by_adb(self.dut)
            ecclist = self.dut.adb.getprop("ril.ecclist")
            self.dut.log.info("ril.ecclist = %s", ecclist)
            if self.fake_emergency_number in ecclist:
                break
        call_info = dumpsys_new_call_info(self.dut, last_call_number)
        if not call_info:
            return False
        call_tech = call_info.get("callTechnologies", "")
        if "CDMA" in call_tech:
            expected_ecbm = True
            expected_data = False
            expected_call = non_emergency_call_allowed
        elif "GSM" in call_tech:
            expected_ecbm = True
            expected_data = True
            expected_call = True
        else:
            expected_ecbm = False
            expected_data = True
            expected_call = True
        last_call_number = dumpsys_last_call_number(self.dut)
        begin_time = get_current_epoch_time()
        self.dut.ensure_screen_on()
        self.dut.exit_setup_wizard()
        reset_device_password(self.dut, None)
        call_check = call_setup_teardown(
            self.log, self.dut, self.android_devices[1], ad_hangup=self.dut)
        if call_check != expected_call:
            self.dut.log.error("Regular phone call is %s, expecting %s",
                               call_check, expected_call)
            result = False
        call_info = dumpsys_new_call_info(self.dut, last_call_number)
        if not call_info:
            self.dut.log.error("New call is not in dumpsys telecom")
            return False
        if expected_ecbm:
            if "ecbm" in call_info["callProperties"]:
                self.dut.log.info("New call is in ecbm.")
            else:
                self.dut.log.error(
                    "New call is not in emergency call back mode.")
                result = False
        else:
            if "ecbm" in call_info["callProperties"]:
                self.dut.log.error("New call is in emergency call back mode")
                result = False
        if not expected_data:
            if self.dut.droid.telephonyGetDataConnectionState():
                self.dut.log.info(
                    "Data connection is off as expected in ECB mode")
                self.dut.log.info("Wait for getting out of ecbm")
                if not wait_for_cell_data_connection(self.log, self.dut, True,
                                                     400):
                    self.dut.log.error(
                        "Data connection didn't come back after 5 minutes")
                    result = False
                #if not self.dut.droid.telephonyGetDataConnectionState():
                #    self.dut.log.error(
                #        "Data connection is not coming back")
                #    result = False
                elif not verify_internet_connection(self.log, self.dut):
                    self.dut.log.error(
                        "Internet connection check failed after getting out of ECB"
                    )
                    result = False

            else:
                self.dut.log.error("Data connection is not off in ECB mode")
                if not verify_internet_connection(self.log, self.dut, False):
                    self.dut.log.error("Internet connection is not off")
                    result = False
        else:
            if self.dut.droid.telephonyGetDataConnectionState():
                self.dut.log.info("Data connection is on as expected")
                if not verify_internet_connection(self.log, self.dut):
                    self.dut.log.error("Internet connection check failed")
                    result = False
            else:
                self.dut.log.error("Data connection is off, expecting on")
                result = False
        if expected_call:
            return result
        elapsed_time = (get_current_epoch_time() - begin_time) / 1000
        if elapsed_time < BLOCK_DURATION:
            time.sleep(BLOCK_DURATION - elapsed_time + 10)
        if not call_setup_teardown(
                self.log, self.dut, self.android_devices[1],
                ad_hangup=self.dut):
            self.dut.log.error("Regular phone call failed after out of ecbm")
            result = False
        return result

    def check_normal_call(self):
        result = True
        if "wfc" not in self.test_name:
            toggle_airplane_mode_by_adb(self.log, self.dut, False)
        self.dut.ensure_screen_on()
        self.dut.exit_setup_wizard()
        reset_device_password(self.dut, None)
        begin_time = get_current_epoch_time()
        if not call_setup_teardown(
                self.log,
                self.android_devices[1],
                self.dut,
                ad_hangup=self.android_devices[1]):
            self.dut.log.error("Regular MT phone call fails")
            self.dut.log.info("call_info = %s", dumpsys_last_call_info(
                self.dut))
            result = False
        if not call_setup_teardown(
                self.log, self.dut, self.android_devices[1],
                ad_hangup=self.dut):
            self.dut.log.error("Regular MO phone call fails")
            self.dut.log.info("call_info = %s", dumpsys_last_call_info(
                self.dut))
            result = False
        return result
