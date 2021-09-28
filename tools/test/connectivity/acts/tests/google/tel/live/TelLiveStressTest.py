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
    Test Script for Telephony Stress Call Test
"""

import collections
import json
import os
import random
import time

from acts import context
from acts import signals
from acts.libs.proc import job
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import GEN_3G
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import INCALL_UI_DISPLAY_BACKGROUND
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_SMS_RECEIVE
from acts.test_utils.tel.tel_defines import NETWORK_MODE_WCDMA_ONLY
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GLOBAL
from acts.test_utils.tel.tel_defines import NETWORK_MODE_CDMA
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_ONLY
from acts.test_utils.tel.tel_defines import NETWORK_MODE_TDSCDMA_GSM_WCDMA
from acts.test_utils.tel.tel_defines import WAIT_TIME_AFTER_MODE_CHANGE
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_defines import WAIT_TIME_CHANGE_MESSAGE_SUB_ID
from acts.test_utils.tel.tel_defines import WAIT_TIME_CHANGE_VOICE_SUB_ID
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_CBRS_DATA_SWITCH
from acts.test_utils.tel.tel_lookup_tables import is_rat_svd_capable
from acts.test_utils.tel.tel_test_utils import STORY_LINE
from acts.test_utils.tel.tel_test_utils import active_file_download_test
from acts.test_utils.tel.tel_test_utils import is_phone_in_call
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import ensure_network_generation_for_subscription
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import extract_test_log
from acts.test_utils.tel.tel_test_utils import force_connectivity_metrics_upload
from acts.test_utils.tel.tel_test_utils import get_device_epoch_time
from acts.test_utils.tel.tel_test_utils import get_telephony_signal_strength
from acts.test_utils.tel.tel_test_utils import hangup_call
from acts.test_utils.tel.tel_test_utils import hangup_call_by_adb
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import last_call_drop_reason
from acts.test_utils.tel.tel_test_utils import run_multithread_func
from acts.test_utils.tel.tel_test_utils import set_wfc_mode
from acts.test_utils.tel.tel_test_utils import sms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import start_qxdm_loggers
from acts.test_utils.tel.tel_test_utils import start_sdm_loggers
from acts.test_utils.tel.tel_test_utils import start_adb_tcpdump
from acts.test_utils.tel.tel_test_utils import synchronize_device_time
from acts.test_utils.tel.tel_test_utils import mms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import set_preferred_network_mode_pref
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import verify_internet_connection_by_ping
from acts.test_utils.tel.tel_test_utils import verify_http_connection
from acts.test_utils.tel.tel_test_utils import wait_for_call_id_clearing
from acts.test_utils.tel.tel_test_utils import wait_for_data_connection
from acts.test_utils.tel.tel_test_utils import wait_for_in_call_active
from acts.test_utils.tel.tel_test_utils import is_current_data_on_cbrs
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_2g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_voice_utils import phone_idle_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_idle_volte
from acts.test_utils.tel.tel_voice_utils import get_current_voice_rat
from acts.test_utils.tel.tel_subscription_utils import get_subid_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_operatorname_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_carrierid_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_data
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_message
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_outgoing_call
from acts.test_utils.tel.tel_subscription_utils import set_slways_allow_mms_data
from acts.utils import get_current_epoch_time
from acts.utils import rand_ascii_str

EXCEPTION_TOLERANCE = 5
BINDER_LOGS = ["/sys/kernel/debug/binder"]


class TelLiveStressTest(TelephonyBaseTest):
    def setup_class(self):
        super(TelLiveStressTest, self).setup_class()
        self.dut = self.android_devices[0]
        self.single_phone_test = self.user_params.get("single_phone_test",
                                                      False)
        # supported file download methods: chrome, sl4a, curl
        self.file_download_method = self.user_params.get(
            "file_download_method", "curl")
        self.get_binder_logs = self.user_params.get("get_binder_logs", False)
        if len(self.android_devices) == 1:
            self.single_phone_test = True
        if self.single_phone_test:
            self.android_devices = self.android_devices[:1]
            self.call_server_number = self.user_params.get(
                "call_server_number", STORY_LINE)
            if self.file_download_method == "sl4a":
                # with single device, do not use sl4a file download
                # due to stability issue
                self.file_download_method = "curl"
        else:
            self.android_devices = self.android_devices[:2]
        self.sdm_log = self.user_params.get("sdm_log", False)
        for ad in self.android_devices:
            setattr(ad, "sdm_log", self.sdm_log)
            ad.adb.shell("setprop nfc.debug_enable 1")
            if self.user_params.get("turn_on_tcpdump", False):
                start_adb_tcpdump(ad, interface="any", mask="all")
        self.user_params["telephony_auto_rerun"] = 0
        self.phone_call_iteration = int(
            self.user_params.get("phone_call_iteration", 500))
        self.max_phone_call_duration = int(
            self.user_params.get("max_phone_call_duration", 600))
        self.min_sleep_time = int(self.user_params.get("min_sleep_time", 30))
        self.max_sleep_time = int(self.user_params.get("max_sleep_time", 60))
        self.max_run_time = int(self.user_params.get("max_run_time", 14400))
        self.max_sms_length = int(self.user_params.get("max_sms_length", 1000))
        self.max_mms_length = int(self.user_params.get("max_mms_length", 160))
        self.min_sms_length = int(self.user_params.get("min_sms_length", 1))
        self.min_mms_length = int(self.user_params.get("min_mms_length", 1))
        self.min_phone_call_duration = int(
            self.user_params.get("min_phone_call_duration", 10))
        self.crash_check_interval = int(
            self.user_params.get("crash_check_interval", 300))
        self.cbrs_check_interval = int(
            self.user_params.get("cbrs_check_interval", 100))
        self.dut_incall = False
        self.dsds_esim = self.user_params.get("dsds_esim", False)
        self.cbrs_esim = self.user_params.get("cbrs_esim", False)
        telephony_info = getattr(self.dut, "telephony", {})
        self.dut_capabilities = telephony_info.get("capabilities", [])
        self.dut_wfc_modes = telephony_info.get("wfc_modes", [])
        self.gps_log_file = self.user_params.get("gps_log_file", None)
        return True

    def setup_test(self):
        super(TelLiveStressTest, self).setup_test()
        self.result_info = collections.defaultdict(int)
        self._init_perf_json()
        self.internet_connection_check_method = verify_internet_connection

    def on_fail(self, test_name, begin_time):
        pass

    def _take_bug_report(self, test_name, begin_time):
        if self._skip_bug_report(test_name):
            return
        src_dir = context.get_current_context().get_full_output_path()
        dst_dir = os.path.join(self.log_path, test_name)

        # Extract test_run_info.txt, test_run_debug.txt
        for file_name in ("test_run_info.txt", "test_run_debug.txt"):
            extract_test_log(self.log, os.path.join(src_dir, file_name),
                             os.path.join(dst_dir,
                                          "%s_%s" % (test_name, file_name)),
                             "\[Test Case\] %s " % test_name)
        super()._take_bug_report(test_name, begin_time)

    def _ad_take_extra_logs(self, ad, test_name, begin_time):
        src_file = os.path.join(ad.device_log_path,
                                'adblog_%s_debug.txt' % ad.serial)
        dst_file = os.path.join(ad.device_log_path, test_name,
                                "%s_%s.logcat" % (ad.serial, test_name))
        extract_test_log(self.log, src_file, dst_file, test_name)
        return super()._ad_take_extra_logs(ad, test_name, begin_time)

    def _setup_wfc(self):
        for ad in self.android_devices:
            if not ensure_wifi_connected(
                    self.log,
                    ad,
                    self.wifi_network_ssid,
                    self.wifi_network_pass,
                    retries=3):
                ad.log.error("Bringing up Wifi connection fails.")
                return False
            ad.log.info("Phone WIFI is connected successfully.")
            if not set_wfc_mode(self.log, ad, WFC_MODE_WIFI_PREFERRED):
                ad.log.error("Phone failed to enable Wifi-Calling.")
                return False
            ad.log.info("Phone is set in Wifi-Calling successfully.")
            if not phone_idle_iwlan(self.log, ad):
                ad.log.error("Phone is not in WFC enabled state.")
                return False
            ad.log.info("Phone is in WFC enabled state.")
        return True

    def _setup_wfc_apm(self):
        for ad in self.android_devices:
            if not phone_setup_iwlan(
                    self.log, ad, True, WFC_MODE_CELLULAR_PREFERRED,
                    self.wifi_network_ssid, self.wifi_network_pass):
                ad.log.error("Failed to setup WFC.")
                return False
        return True

    def _setup_lte_volte_enabled(self):
        for ad in self.android_devices:
            if not phone_setup_volte(self.log, ad):
                ad.log.error("Phone failed to enable VoLTE.")
                return False
            ad.log.info("Phone VOLTE is enabled successfully.")
        return True

    def _setup_lte_volte_disabled(self):
        for ad in self.android_devices:
            if not phone_setup_csfb(self.log, ad):
                ad.log.error("Phone failed to setup CSFB.")
                return False
            ad.log.info("Phone VOLTE is disabled successfully.")
        return True

    def _setup_3g(self):
        for ad in self.android_devices:
            if not phone_setup_voice_3g(self.log, ad):
                ad.log.error("Phone failed to setup 3g.")
                return False
            ad.log.info("Phone RAT 3G is enabled successfully.")
        return True

    def _setup_2g(self):
        for ad in self.android_devices:
            if not phone_setup_voice_2g(self.log, ad):
                ad.log.error("Phone failed to setup 2g.")
                return False
            ad.log.info("RAT 2G is enabled successfully.")
        return True

    def _get_network_rat(self, slot_id):
        rat = self.dut.adb.getprop("gsm.network.type")
        if "," in rat:
            if self.dsds_esim:
                rat = rat.split(',')[slot_id]
            else:
                (rat1, rat2) = rat.split(',')
                if rat1 == "Unknown":
                    rat = rat2
                else:
                    rat = rat1
        return rat

    def _send_message(self, max_wait_time=2 * MAX_WAIT_TIME_SMS_RECEIVE):
        slot_id_rx = None
        if self.single_phone_test:
            ads = [self.dut, self.dut]
        else:
            ads = self.android_devices[:]
            random.shuffle(ads)
        slot_id = random.randint(0,1)
        if self.dsds_esim:
            sub_id = get_subid_from_slot_index(self.log, ads[0], slot_id)
            ads[0].log.info("Message - MO - slot_Id %d", slot_id)
            set_subid_for_message(ads[0], sub_id)
            time.sleep(WAIT_TIME_CHANGE_MESSAGE_SUB_ID)
            slot_id_rx = random.randint(0,1)
            ads[1].log.info("Message - MT - slot_id %d", slot_id_rx)
        selection = random.randrange(0, 2)
        message_type_map = {0: "SMS", 1: "MMS"}
        max_length_map = {0: self.max_sms_length, 1: self.max_mms_length}
        min_length_map = {0: self.min_sms_length, 1: self.min_mms_length}
        length = random.randrange(min_length_map[selection],
                                  max_length_map[selection] + 1)
        message_func_map = {
            0: sms_send_receive_verify,
            1: mms_send_receive_verify
        }
        rat = self._get_network_rat(slot_id)
        self.dut.log.info("Network in RAT %s", rat)
        if self.dut_incall and not is_rat_svd_capable(rat.upper()):
            self.dut.log.info("In call data not supported, test SMS only")
            selection = 0
        message_type = message_type_map[selection]
        the_number = self.result_info["%s Total" % message_type] + 1
        begin_time = get_device_epoch_time(self.dut)
        test_name = "%s_No_%s_%s" % (self.test_name, the_number, message_type)
        if self.sdm_log:
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        log_msg = "[Test Case] %s" % test_name
        self.log.info("%s begin", log_msg)
        for ad in self.android_devices:
            if self.user_params.get("turn_on_tcpdump", False):
                start_adb_tcpdump(ad, interface="any", mask="all")
            if not getattr(ad, "messaging_droid", None):
                ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                ad.messaging_ed.start()
            else:
                try:
                    if not ad.messaging_droid.is_live:
                        ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                        ad.messaging_ed.start()
                    else:
                        ad.messaging_ed.clear_all_events()
                except Exception:
                    ad.log.info("Create new sl4a session for messaging")
                    ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                    ad.messaging_ed.start()
            ad.messaging_droid.logI("[BEGIN]%s" % log_msg)

        text = "%s:" % test_name
        text_length = len(text)
        if length < text_length:
            text = text[:length]
        else:
            text += rand_ascii_str(length - text_length)
        message_content_map = {0: [text], 1: [(test_name, text, None)]}

        result = message_func_map[selection](self.log, ads[0], ads[1],
                                             message_content_map[selection],
                                             max_wait_time,
                                             slot_id_rx=slot_id_rx)
        self.log.info("%s end", log_msg)
        for ad in self.android_devices:
            ad.messaging_droid.logI("[END]%s" % log_msg)
        if not result:
            self.result_info["%s Total" % message_type] += 1
            if message_type == "SMS":
                self.log.error("%s fails", log_msg)
                self.result_info["%s Failure" % message_type] += 1
            else:
                rat = self._get_network_rat(slot_id)
                self.dut.log.info("Network in RAT %s", rat)
                if self.dut_incall and not is_rat_svd_capable(rat.upper()):
                    self.dut.log.info(
                        "In call data not supported, MMS failure expected")
                    self.result_info["Expected In-call MMS failure"] += 1
                    return True
                else:
                    self.log.error("%s fails", log_msg)
                    self.result_info["MMS Failure"] += 1
            try:
                self._take_bug_report(test_name, begin_time)
            except Exception as e:
                self.log.exception(e)
            return False
        else:
            self.result_info["%s Total" % message_type] += 1
            self.log.info("%s succeed", log_msg)
            self.result_info["%s Success" % message_type] += 1
            return True

    def _make_phone_call(self, call_verification_func=None):
        ads = self.android_devices[:]
        slot_id_callee = None
        if not self.single_phone_test:
            random.shuffle(ads)
        if self.dsds_esim:
            slot_id = random.randint(0,1)
            sub_id = get_subid_from_slot_index(self.log, ads[0], slot_id)
            ads[0].log.info("Voice - MO - slot_Id %d", slot_id)
            set_subid_for_outgoing_call(ads[0], sub_id)
            time.sleep(WAIT_TIME_CHANGE_VOICE_SUB_ID)
            slot_id_callee = random.randint(0,1)
            ads[1].log.info("Voice - MT - slot_id %d", slot_id_callee)
        the_number = self.result_info["Call Total"] + 1
        duration = random.randrange(self.min_phone_call_duration,
                                    self.max_phone_call_duration)
        result = True
        test_name = "%s_No_%s_phone_call" % (self.test_name, the_number)
        log_msg = "[Test Case] %s" % test_name
        self.log.info("%s for %s seconds begin", log_msg, duration)
        begin_time = get_device_epoch_time(ads[0])
        for ad in self.android_devices:
            if self.user_params.get("turn_on_tcpdump", False):
                start_adb_tcpdump(ad, interface="any", mask="all")
            if not getattr(ad, "droid", None):
                ad.droid, ad.ed = ad.get_droid()
                ad.ed.start()
            else:
                try:
                    if not ad.droid.is_live:
                        ad.droid, ad.ed = ad.get_droid()
                        ad.ed.start()
                    else:
                        ad.ed.clear_all_events()
                except Exception:
                    ad.log.info("Create new sl4a session for phone call")
                    ad.droid, ad.ed = ad.get_droid()
                    ad.ed.start()
            ad.droid.logI("[BEGIN]%s" % log_msg)
        if self.sdm_log:
            for ad in ads:
                ad.adb.shell("i2cset -fy 3 64 6 1 b", ignore_status=True)
                ad.adb.shell("i2cset -fy 3 65 6 1 b", ignore_status=True)
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        if self.cbrs_esim:
            self._cbrs_data_check_test(begin_time, expected_cbrs=True,
                                       test_time="before")
        failure_reasons = set()
        self.dut_incall = True
        if self.single_phone_test:
            call_setup_result = initiate_call(
                self.log,
                self.dut,
                self.call_server_number,
                incall_ui_display=INCALL_UI_DISPLAY_BACKGROUND
            ) and wait_for_in_call_active(self.dut, 60, 3)
        else:
            call_setup_result = call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=call_verification_func,
                verify_callee_func=call_verification_func,
                wait_time_in_call=0,
                incall_ui_display=INCALL_UI_DISPLAY_BACKGROUND,
                slot_id_callee=slot_id_callee)
        if not call_setup_result:
            get_telephony_signal_strength(ads[0])
            if not self.single_phone_test:
                get_telephony_signal_strength(ads[1])
            call_logs = ads[0].search_logcat(
                "ActivityManager: START u0 {act=android.intent.action.CALL",
                begin_time)
            messaging_logs = ads[0].search_logcat(
                "com.google.android.apps.messaging/.ui.conversation.ConversationActivity",
                begin_time)
            if call_logs and messaging_logs:
                if (messaging_logs[-1]["datetime_obj"] -
                        call_logs[-1]["datetime_obj"]).seconds < 5:
                    ads[0].log.info(
                        "Call setup failure due to simultaneous activities")
                    self.result_info[
                        "Call Setup Failure With Simultaneous Activity"] += 1
                    return True
            self.log.error("%s: Setup Call failed.", log_msg)
            failure_reasons.add("Setup")
            result = False
        else:
            elapsed_time = 0
            check_interval = 5
            if self.sdm_log:
                for ad in ads:
                    ad.adb.shell("i2cset -fy 3 64 6 1 b", ignore_status=True)
                    ad.adb.shell("i2cset -fy 3 65 6 1 b", ignore_status=True)
            if self.cbrs_esim:
                time.sleep(5)
                self._cbrs_data_check_test(begin_time, expected_cbrs=False,
                                           test_time="during")
            while (elapsed_time < duration):
                check_interval = min(check_interval, duration - elapsed_time)
                time.sleep(check_interval)
                elapsed_time += check_interval
                time_message = "at <%s>/<%s> second." % (elapsed_time,
                                                         duration)
                for ad in ads:
                    get_telephony_signal_strength(ad)
                    if not call_verification_func(self.log, ad):
                        ad.log.warning("Call is NOT in correct %s state at %s",
                                       call_verification_func.__name__,
                                       time_message)
                        if call_verification_func.__name__ == "is_phone_in_call_iwlan":
                            if is_phone_in_call(self.log, ad):
                                if getattr(ad, "data_rat_state_error_count",
                                           0) < 1:
                                    setattr(ad, "data_rat_state_error_count",
                                            1)
                                    continue
                        failure_reasons.add("Maintenance")
                        last_call_drop_reason(ad, begin_time)
                        hangup_call(self.log, ads[0])
                        result = False
                    else:
                        ad.log.info("Call is in correct %s state at %s",
                                    call_verification_func.__name__,
                                    time_message)
                if not result:
                    break
        if not hangup_call(self.log, ads[0]):
            failure_reasons.add("Teardown")
            result = False
        for ad in ads:
            if not wait_for_call_id_clearing(ad,
                                             []) or ad.droid.telecomIsInCall():
                ad.log.error("Fail to hang up call")
                failure_reasons.add("Teardown")
                result = False
        self.result_info["Call Total"] += 1
        for ad in self.android_devices:
            try:
                ad.droid.logI("[END]%s" % log_msg)
            except:
                pass
        self.log.info("%s end", log_msg)
        self.dut_incall = False
        if self.cbrs_esim:
            time.sleep(30)
            self._cbrs_data_check_test(begin_time, expected_cbrs=True,
                                       test_time="after")
        if not result:
            self.log.info("%s failed", log_msg)
            if self.gps_log_file:
                gps_info = job.run(
                    "tail %s" % self.gps_log_file, ignore_status=True)
                if gps_info.stdout:
                    gps_log_path = os.path.join(self.log_path, test_name)
                    os.makedirs(gps_log_path, exist_ok=True)
                    job.run(
                        "tail %s > %s" %
                        (self.gps_log_file,
                         os.path.join(gps_log_path, "gps_logs.txt")),
                        ignore_status=True)
                    self.log.info("gps log:\n%s", gps_info.stdout)
                else:
                    self.log.warning("Fail to get gps log %s",
                                     self.user_params["gps_log_file"])
            for reason in failure_reasons:
                self.result_info["Call %s Failure" % reason] += 1
            for ad in ads:
                log_path = os.path.join(self.log_path, test_name,
                                        "%s_binder_logs" % ad.serial)
                os.makedirs(log_path, exist_ok=True)
                ad.pull_files(BINDER_LOGS, log_path)
            try:
                self._take_bug_report(test_name, begin_time)
            except Exception as e:
                self.log.exception(e)
            for ad in ads:
                if ad.droid.telecomIsInCall():
                    hangup_call_by_adb(ad)
        else:
            self.log.info("%s test succeed", log_msg)
            self.result_info["Call Success"] += 1
            if self.result_info["Call Total"] % 50 == 0:
                for ad in ads:
                    synchronize_device_time(ad)
                    force_connectivity_metrics_upload(ad)
                    if self.get_binder_logs:
                        log_path = os.path.join(self.log_path, test_name,
                                                "%s_binder_logs" % ad.serial)
                        os.makedirs(log_path, exist_ok=True)
                        ad.pull_files(BINDER_LOGS, log_path)
        return result

    def _prefnetwork_mode_change(self, sub_id):
        # ModePref change to non-LTE
        begin_time = get_device_epoch_time(self.dut)
        if self.sdm_log:
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        self.result_info["Network Change Request Total"] += 1
        test_name = "%s_network_change_iter_%s" % (
            self.test_name, self.result_info["Network Change Request Total"])
        log_msg = "[Test Case] %s" % test_name
        self.log.info("%s begin", log_msg)
        self.dut.droid.logI("[BEGIN]%s" % log_msg)
        network_preference_list = [
            NETWORK_MODE_TDSCDMA_GSM_WCDMA, NETWORK_MODE_WCDMA_ONLY,
            NETWORK_MODE_GLOBAL, NETWORK_MODE_CDMA, NETWORK_MODE_GSM_ONLY
        ]
        network_preference = random.choice(network_preference_list)
        set_preferred_network_mode_pref(self.log, self.dut, sub_id,
                                        network_preference)
        time.sleep(WAIT_TIME_AFTER_MODE_CHANGE)
        self.dut.log.info("Current Voice RAT is %s",
                          get_current_voice_rat(self.log, self.dut))

        # ModePref change back to with LTE
        if not phone_setup_volte(self.log, self.dut):
            self.dut.log.error("Phone failed to enable VoLTE.")
            self.result_info["VoLTE Setup Failure"] += 1
            self.dut.droid.logI("%s end" % log_msg)
            self.dut.log.info("[END]%s", log_msg)
            try:
                self._ad_take_extra_logs(self.dut, test_name, begin_time)
                self._ad_take_bugreport(self.dut, test_name, begin_time)
            except Exception as e:
                self.log.exception(e)
            return False
        else:
            self.result_info["VoLTE Setup Success"] += 1
            return True

    def _mobile_data_toggling(self, setup="volte"):
        # ModePref change to non-LTE
        begin_time = get_device_epoch_time(self.dut)
        if self.sdm_log:
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        result = True
        self.result_info["Data Toggling Request Total"] += 1
        test_name = "%s_data_toggling_iter_%s" % (
            self.test_name, self.result_info["Data Toggling Request Total"])
        log_msg = "[Test Case] %s" % test_name
        self.log.info("%s begin", log_msg)
        self.dut.droid.logI("[BEGIN]%s" % log_msg)
        self.dut.adb.shell("svc data disable")
        time.sleep(WAIT_TIME_AFTER_MODE_CHANGE)
        self.dut.adb.shell("svc data enable")
        if not self._check_data():
            result = False
        elif setup == "volte" and not phone_idle_volte(self.log, self.dut):
            result = False
        self.dut.droid.logI("%s end" % log_msg)
        self.dut.log.info("[END]%s", log_msg)
        if not result:
            self.result_info["Data Toggling Failure"] += 1
            try:
                self._ad_take_extra_logs(self.dut, test_name, begin_time)
                self._ad_take_bugreport(self.dut, test_name, begin_time)
            except Exception as e:
                self.log.exception(e)
            return False
        else:
            self.result_info["Data Toggling Success"] += 1
            return True

    def _get_result_message(self):
        msg_list = [
            "%s: %s" % (count, self.result_info[count])
            for count in sorted(self.result_info.keys())
        ]
        return ", ".join(msg_list)

    def _write_perf_json(self):
        json_str = json.dumps(self.perf_data, indent=4, sort_keys=True)
        with open(self.perf_file, 'w') as f:
            f.write(json_str)

    def _init_perf_json(self):
        self.perf_file = os.path.join(self.log_path, "%s_perf_data_%s.json" %
                                      (self.test_name, self.begin_time))
        self.perf_data = self.dut.build_info.copy()
        self.perf_data["build_fingerprint"] = self.dut.adb.getprop(
            "ro.build.fingerprint")
        self.perf_data["model"] = self.dut.model
        self.perf_data["carrier"] = self.dut.adb.getprop(
            "gsm.sim.operator.alpha")
        self._write_perf_json()

    def _update_perf_json(self):
        for result_key, result_value in self.result_info.items():
            self.perf_data[result_key] = result_value
        self._write_perf_json()

    def crash_check_test(self):
        failure = 0
        while time.time() < self.finishing_time:
            try:
                self.log.info(dict(self.result_info))
                self._update_perf_json()
                begin_time = get_device_epoch_time(self.dut)
                run_time_in_seconds = (begin_time - self.begin_time) / 1000
                test_name = "%s_crash_%s_seconds_after_start" % (
                    self.test_name, run_time_in_seconds)
                time.sleep(self.crash_check_interval)
                for ad in self.android_devices:
                    crash_report = ad.check_crash_report(
                        test_name, begin_time, log_crash_report=True)
                    if crash_report:
                        ad.log.error("Find new crash reports %s", crash_report)
                        failure += 1
                        self.result_info["Crashes"] += len(crash_report)
                        for crash in crash_report:
                            if "ramdump_modem" in crash:
                                self.result_info["Crashes-Modem"] += 1
                        try:
                            ad.take_bug_report(test_name, begin_time)
                        except Exception as e:
                            self.log.exception(e)
            except Exception as e:
                self.log.error("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            self.log.info("Crashes found: %s", failure)
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
        if failure:
            return False
        else:
            return True

    def _cbrs_data_check_test(self, begin_time, expected_cbrs=True,
                              test_time="before"):
        cbrs_fail_count = 0
        the_number = self.result_info["CBRS Total"] + 1
        test_name = "%s_cbrs_%s_call_No_%s" % (self.test_name,
                                               test_time, the_number)
        for ad in self.android_devices:
            current_state = is_current_data_on_cbrs(ad, ad.cbrs)
            if current_state == expected_cbrs:
                self.result_info["CBRS-Check-Pass"] += 1
            else:
                self.result_info["CBRS-Check-Fail"] += 1
                cbrs_fail_count += 1
                try:
                    self._ad_take_extra_logs(ad, test_name, begin_time)
                    self._ad_take_bugreport(ad, test_name, begin_time)
                except Exception as e:
                    self.log.warning(e)
        if cbrs_fail_count > 0:
            ad.log.error("Found %d checks failed, expected cbrs %s",
                         cbrs_fail_count, expected_cbrs)
            cbrs_fail_count += 1
        self.result_info["CBRS Total"] += 1
        return True

    def call_test(self, call_verification_func=None):
        while time.time() < self.finishing_time:
            time.sleep(
                random.randrange(self.min_sleep_time, self.max_sleep_time))
            try:
                self._make_phone_call(call_verification_func)
            except Exception as e:
                self.log.exception("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
            self.log.info("%s", dict(self.result_info))
        if any([
                self.result_info["Call Setup Failure"],
                self.result_info["Call Maintenance Failure"],
                self.result_info["Call Teardown Failure"]
        ]):
            return False
        else:
            return True

    def message_test(self, max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
        while time.time() < self.finishing_time:
            try:
                self._send_message(max_wait_time=max_wait_time)
            except Exception as e:
                self.log.exception("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            self.log.info(dict(self.result_info))
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
            time.sleep(
                random.randrange(self.min_sleep_time, self.max_sleep_time))
        if self.result_info["SMS Failure"] or (
                self.result_info["MMS Failure"] / self.result_info["MMS Total"]
                > 0.3):
            return False
        else:
            return True

    def _data_download(self, file_names=["5MB", "10MB", "20MB", "50MB"]):
        begin_time = get_current_epoch_time()
        slot_id = random.randint(0,1)
        if self.dsds_esim:
            sub_id = get_subid_from_slot_index(self.log, self.dut, slot_id)
            self.dut.log.info("Data - slot_Id %d", slot_id)
            set_subid_for_data(self.dut, sub_id)
            self.dut.droid.telephonyToggleDataConnection(True)
        if self.sdm_log:
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        self.dut.log.info(dict(self.result_info))
        selection = random.randrange(0, len(file_names))
        file_name = file_names[selection]
        self.result_info["Internet Connection Check Total"] += 1

        rat = self._get_network_rat(slot_id)
        if not self.internet_connection_check_method(self.log, self.dut):
            self.dut.log.info("Network in RAT %s", rat)
            if self.dut_incall and not is_rat_svd_capable(rat.upper()):
                self.result_info[
                    "Expected Incall Internet Connection Check Failure"] += 1
                return True
            else:
                self.result_info["Internet Connection Check Failure"] += 1
                test_name = "%s_internet_connection_No_%s_failure" % (
                    self.test_name,
                    self.result_info["Internet Connection Check Failure"])
                try:
                    self._ad_take_extra_logs(self.dut, test_name, begin_time)
                    self._ad_take_bugreport(self.dut, test_name, begin_time)
                except Exception as e:
                    self.log.exception(e)
                return False
        else:
            self.result_info["Internet Connection Check Success"] += 1

        self.result_info["File Download Total"] += 1
        if not active_file_download_test(
                self.log, self.dut, file_name,
                method=self.file_download_method):
            self.result_info["File Download Failure"] += 1
            if self.result_info["File Download Failure"] == 1:
                try:
                    self._ad_take_extra_logs(
                        self.dut, "%s_file_download_failure" % self.test_name,
                        begin_time)
                    self._ad_take_bugreport(
                        self.dut, "%s_file_download_failure" % self.test_name,
                        begin_time)
                except Exception as e:
                    self.log.exception(e)
            return False
        else:
            self.result_info["File Download Success"] += 1
            return True

    def data_test(self):
        while time.time() < self.finishing_time:
            try:
                self._data_download()
            except Exception as e:
                self.log.error("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            self.log.info("%s", dict(self.result_info))
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
            time.sleep(
                random.randrange(self.min_sleep_time, self.max_sleep_time))
        if self.result_info["Internet Connection Check Failure"]:
            return False
        else:
            return True

    def _check_data(self):
        self.result_info["Data Connection Check Total"] += 1
        if not wait_for_data_connection(self.log, self.dut, True):
            self.result_info["Data Connection Setup Failure"] += 1
            return False
        if not self.internet_connection_check_method(self.log, self.dut):
            rat = self.dut.adb.getprop("gsm.network.type")
            self.dut.log.info("Network in RAT %s", rat)
            self.result_info["Internet Connection Check Failure"] += 1
            return False
        return True

    def _data_call_test(self, sub_id, generation):
        self.dut.log.info(dict(self.result_info))
        begin_time = get_device_epoch_time(self.dut)
        if self.sdm_log:
            start_sdm_loggers(self.log, self.android_devices)
        else:
            start_qxdm_loggers(self.log, self.android_devices)
        self.result_info["Network Change Request Total"] += 1
        test_name = "%s_network_change_test_iter_%s" % (
            self.test_name, self.result_info["Network Change Request Total"])
        log_msg = "[Test Case] %s" % test_name
        self.log.info("%s begin", log_msg)
        self.dut.droid.logI("[BEGIN]%s" % log_msg)
        if not ensure_network_generation_for_subscription(
                self.log, self.dut, sub_id,
                generation) or not self._check_data():
            self.result_info["Network Change Failure"] += 1
            self.dut.droid.logI("%s end" % log_msg)
            self.dut.log.info("[END]%s", log_msg)
            try:
                self._ad_take_extra_logs(self.dut, test_name, begin_time)
                self._ad_take_bugreport(self.dut, test_name, begin_time)
            except Exception as e:
                self.log.warning(e)
            return False
        if not self._mobile_data_toggling(setup=None):
            return False
        return True

    def data_call_stress_test(self):
        result = True
        sub_id = self.dut.droid.subscriptionGetDefaultSubId()
        while time.time() < self.finishing_time:
            for generation in (GEN_4G, GEN_3G):
                try:
                    if not self._data_call_test(sub_id, generation):
                        result = False
                except Exception as e:
                    self.log.error("Exception error %s", str(e))
                    self.result_info["Exception Errors"] += 1
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
        return result

    def check_incall_data(self):
        if verify_internet_connection_by_ping(self.log, self.dut):
            self.internet_connection_check_method = verify_internet_connection_by_ping
        elif verify_http_connection(self.log, self.dut):
            self.internet_connection_check_method = verify_http_connection
        else:
            self.dut.log.error("Data test failed")
            raise signals.TestFailure("Data check failed")
        if self.single_phone_test:
            if not initiate_call(
                    self.log, self.dut,
                    self.call_server_number) and wait_for_in_call_active(
                        self.dut, 60, 3):
                self._take_bug_report(self.test_name, self.begin_time)
                raise signals.TestFailure("Unable to make phone call")
        else:
            if not call_setup_teardown(
                    self.log, self.dut, self.android_devices[1],
                    ad_hangup=None):
                self._take_bug_report(self.test_name, self.begin_time)
                raise signals.TestFailure("Unable to make phone call")
        voice_rat = self.dut.droid.telephonyGetCurrentVoiceNetworkType()
        data_rat = self.dut.droid.telephonyGetCurrentDataNetworkType()
        self.dut.log.info("Voice in RAT %s, Data in RAT %s", voice_rat,
                          data_rat)
        try:
            if "wfc" in self.test_name or is_rat_svd_capable(
                    voice_rat.upper()) and is_rat_svd_capable(
                        data_rat.upper()):
                self.dut.log.info("Capable for simultaneous voice and data")

                if not self.internet_connection_check_method(
                        self.log, self.dut):
                    self.dut.log.error("Incall data check failed")
                    raise signals.TestFailure("Incall data check failed")
                else:
                    return True
            else:
                self.dut.log.info(
                    "Not capable for simultaneous voice and data")
                return False
            hangup_call(self.log, self.dut)
        finally:
            for ad in self.android_devices:
                if ad.droid.telecomIsInCall():
                    hangup_call(self.log, ad)

    def parallel_tests(self, setup_func=None, call_verification_func=None):
        self.log.info(self._get_result_message())
        if setup_func and not setup_func():
            msg = "%s setup %s failed" % (self.test_name, setup_func.__name__)
            self.log.error(msg)
            self._take_bug_report("%s%s" % (self.test_name,
                                            setup_func.__name__),
                                  self.begin_time)
            return False
        if not call_verification_func:
            call_verification_func = is_phone_in_call
        self.finishing_time = time.time() + self.max_run_time
        # CBRS setup
        if self.cbrs_esim:
            cbrs_sub_count = 0
            for ad in self.android_devices:
                if not getattr(ad, 'cbrs', {}):
                    setattr(ad, 'cbrs', None)
                for i in range(0, 2):
                    sub_id = get_subid_from_slot_index(ad.log, ad, i)
                    operator = get_operatorname_from_slot_index(ad, i)
                    carrier_id = get_carrierid_from_slot_index(ad, i)
                    ad.log.info("Slot %d - Sub %s - %s - %d", i, sub_id, operator, carrier_id)
                    if carrier_id == 2340:
                        ad.cbrs = sub_id
                        cbrs_sub_count += 1
            if cbrs_sub_count != 2:
                self.log.error("Expecting - 2 CBRS subs, found - %d", cbrs_sub_count)
                raise signals.TestAbortClass("Cannot find all expected CBRS subs")
        # DSDS setup
        if self.dsds_esim:
            for ad in self.android_devices:
                for i in range(0, 2):
                    sub_id = get_subid_from_slot_index(ad.log, ad, i)
                    set_slways_allow_mms_data(ad, sub_id)
                    operator = get_operatorname_from_slot_index(ad, i)
                    ad.log.info("Slot %d - Sub %s - %s", i, sub_id, operator)
        # Actual test trigger
        if not self.dsds_esim and self.check_incall_data():
            self.log.info(
                "==== Start parallel voice/message/data stress test ====")
            self.perf_data["testing method"] = "parallel"
            results = run_multithread_func(
                self.log, [(self.call_test, [call_verification_func]),
                           (self.message_test, []), (self.data_test, []),
                           (self.crash_check_test, [])])
        else:
            self.log.info(
                "==== Start sequential voice/message/data stress test ====")
            self.perf_data["testing method"] = "sequential"
            results = run_multithread_func(
                self.log, [(self.sequential_tests, [call_verification_func]),
                           (self.crash_check_test, [])])
        result_message = self._get_result_message()
        self.log.info(result_message)
        self._update_perf_json()
        self.result_detail = result_message
        return all(results)

    def sequential_tests(self, call_verification_func):
        funcs = [(self._make_phone_call, [call_verification_func]),
                 (self._send_message, []), (self._data_download, [["5MB"]])]
        while time.time() < self.finishing_time:
            selection = random.randrange(0, 3)
            try:
                funcs[selection][0](*funcs[selection][1])
            except Exception as e:
                self.log.error("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            self.log.info("%s", dict(self.result_info))
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
            time.sleep(
                random.randrange(self.min_sleep_time, self.max_sleep_time))
        if any([
                self.result_info["Call Setup Failure"],
                self.result_info["Call Maintenance Failure"],
                self.result_info["Call Teardown Failure"],
                self.result_info["SMS Failure"],
                self.result_info["MMS Failure"],
                self.result_info["Internet Connection Check Failure"]
        ]):
            return False
        return True

    def volte_modechange_volte_test(self):
        sub_id = self.dut.droid.subscriptionGetDefaultSubId()
        result = True
        while time.time() < self.finishing_time:
            try:
                if self._prefnetwork_mode_change(sub_id):
                    run_multithread_func(
                        self.log,
                        [(self._data_download, [["5MB"]]),
                         (self._make_phone_call, [is_phone_in_call_volte]),
                         (self._send_message, [])])
                else:
                    result = False
                if self._mobile_data_toggling():
                    run_multithread_func(
                        self.log,
                        [(self._data_download, [["5MB"]]),
                         (self._make_phone_call, [is_phone_in_call_volte]),
                         (self._send_message, [])])
                else:
                    result = False
            except Exception as e:
                self.log.error("Exception error %s", str(e))
                self.result_info["Exception Errors"] += 1
            self.log.info(dict(self.result_info))
            if self.result_info["Exception Errors"] >= EXCEPTION_TOLERANCE:
                self.log.error("Too many exception errors, quit test")
                return False
        return result

    def parallel_with_network_change_tests(self, setup_func=None):
        if setup_func and not setup_func():
            self.log.error("Test setup %s failed", setup_func.__name__)
            return False
        self.finishing_time = time.time() + self.max_run_time
        results = run_multithread_func(self.log,
                                       [(self.volte_modechange_volte_test, []),
                                        (self.crash_check_test, [])])
        result_message = self._get_result_message()
        self.log.info(result_message)
        self._update_perf_json()
        self.result_detail = result_message
        return all(results)

    def connect_to_wifi(self):
        for ad in self.android_devices:
            if not ensure_wifi_connected(
                    self.log,
                    ad,
                    self.wifi_network_ssid,
                    self.wifi_network_pass,
                    retries=3):
                ad.log.error("Bringing up Wifi connection fails.")
                return False
        ad.log.info("Phone WIFI is connected successfully.")
        return True

    """ Tests Begin """

    @test_tracker_info(uuid="d035e5b9-476a-4e3d-b4e9-6fd86c51a68d")
    @TelephonyBaseTest.tel_test_wrap
    def test_default_parallel_stress(self):
        """ Default state stress test"""
        return self.parallel_tests()

    @test_tracker_info(uuid="798a3c34-db75-4bcf-b8ef-e1116414a7fe")
    @TelephonyBaseTest.tel_test_wrap
    def test_default_parallel_stress_with_wifi(self):
        """ Default state stress test with Wifi enabled."""
        if self.connect_to_wifi():
            return self.parallel_tests()

    @test_tracker_info(uuid="c21e1f17-3282-4f0b-b527-19f048798098")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_volte_parallel_stress(self):
        """ VoLTE on stress test"""
        return self.parallel_tests(
            setup_func=self._setup_lte_volte_enabled,
            call_verification_func=is_phone_in_call_volte)

    @test_tracker_info(uuid="a317c23a-41e0-4ef8-af67-661451cfefcf")
    @TelephonyBaseTest.tel_test_wrap
    def test_csfb_parallel_stress(self):
        """ LTE non-VoLTE stress test"""
        return self.parallel_tests(
            setup_func=self._setup_lte_volte_disabled,
            call_verification_func=is_phone_in_call_csfb)

    @test_tracker_info(uuid="fdb791bf-c414-4333-9fa3-cc18c9b3b234")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_parallel_stress(self):
        """ Wifi calling APM mode off stress test"""
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        return self.parallel_tests(
            setup_func=self._setup_wfc,
            call_verification_func=is_phone_in_call_iwlan)

    @test_tracker_info(uuid="e334c1b3-4378-49bb-bf57-1573fa1b23fa")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_apm_parallel_stress(self):
        """ Wifi calling in APM mode on stress test"""
        return self.parallel_tests(
            setup_func=self._setup_wfc_apm,
            call_verification_func=is_phone_in_call_iwlan)

    @test_tracker_info(uuid="4566eef6-55de-4ac8-87ee-58f2ef41a3e8")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_parallel_stress(self):
        """ 3G stress test"""
        return self.parallel_tests(
            setup_func=self._setup_3g,
            call_verification_func=is_phone_in_call_3g)

    @test_tracker_info(uuid="f34f1a31-3948-4675-8698-372a83b8088d")
    @TelephonyBaseTest.tel_test_wrap
    def test_2g_parallel_stress(self):
        """ 2G call stress test"""
        return self.parallel_tests(
            setup_func=self._setup_2g,
            call_verification_func=is_phone_in_call_2g)

    @test_tracker_info(uuid="af580fca-fea6-4ca5-b981-b8c710302d37")
    @TelephonyBaseTest.tel_test_wrap
    def test_volte_modeprefchange_parallel_stress(self):
        """ VoLTE Mode Pref call stress test"""
        return self.parallel_with_network_change_tests(
            setup_func=self._setup_lte_volte_enabled)

    @test_tracker_info(uuid="10e34247-5fd3-4f87-81bf-3c17a6b71ab2")
    @TelephonyBaseTest.tel_test_wrap
    def test_data_call_stress(self):
        """ Default state stress test"""
        self.finishing_time = time.time() + self.max_run_time
        results = run_multithread_func(self.log,
                                       [(self.data_call_stress_test, []),
                                        (self.crash_check_test, [])])
        result_message = self._get_result_message()
        self.log.info(result_message)
        self._update_perf_json()
        self.result_detail = result_message
        return all(results)

    """ Tests End """
