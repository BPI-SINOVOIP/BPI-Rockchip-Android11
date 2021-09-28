#!/usr/bin/env python3
#
#   Copyright 2016 - Google
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
    Base Class for Defining Common Telephony Test Functionality
"""

import logging
import os
import re
import shutil
import time

from acts import asserts
from acts import logger as acts_logger
from acts import signals
from acts.base_test import BaseTestClass
from acts.controllers.android_device import DEFAULT_QXDM_LOG_PATH
from acts.controllers.android_device import DEFAULT_SDM_LOG_PATH
from acts.keys import Config
from acts import records
from acts import utils

from acts.test_utils.tel.tel_subscription_utils import \
    initial_set_up_for_subid_infomation
from acts.test_utils.tel.tel_subscription_utils import \
    set_default_sub_for_all_services
from acts.test_utils.tel.tel_subscription_utils import get_subid_from_slot_index
from acts.test_utils.tel.tel_test_utils import build_id_override
from acts.test_utils.tel.tel_test_utils import disable_qxdm_logger
from acts.test_utils.tel.tel_test_utils import enable_connectivity_metrics
from acts.test_utils.tel.tel_test_utils import enable_radio_log_on
from acts.test_utils.tel.tel_test_utils import ensure_phone_default_state
from acts.test_utils.tel.tel_test_utils import ensure_phone_idle
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import force_connectivity_metrics_upload
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import get_screen_shot_log
from acts.test_utils.tel.tel_test_utils import get_sim_state
from acts.test_utils.tel.tel_test_utils import get_tcpdump_log
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_test_utils import print_radio_info
from acts.test_utils.tel.tel_test_utils import reboot_device
from acts.test_utils.tel.tel_test_utils import recover_build_id
from acts.test_utils.tel.tel_test_utils import run_multithread_func
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.test_utils.tel.tel_test_utils import set_phone_screen_on
from acts.test_utils.tel.tel_test_utils import set_phone_silent_mode
from acts.test_utils.tel.tel_test_utils import set_qxdm_logger_command
from acts.test_utils.tel.tel_test_utils import start_qxdm_logger
from acts.test_utils.tel.tel_test_utils import start_qxdm_loggers
from acts.test_utils.tel.tel_test_utils import start_sdm_loggers
from acts.test_utils.tel.tel_test_utils import start_sdm_logger
from acts.test_utils.tel.tel_test_utils import start_tcpdumps
from acts.test_utils.tel.tel_test_utils import stop_qxdm_logger
from acts.test_utils.tel.tel_test_utils import stop_sdm_loggers
from acts.test_utils.tel.tel_test_utils import stop_sdm_logger
from acts.test_utils.tel.tel_test_utils import stop_tcpdumps
from acts.test_utils.tel.tel_test_utils import synchronize_device_time
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_test_utils import wait_for_sim_ready_by_adb
from acts.test_utils.tel.tel_test_utils import wait_for_sims_ready_by_adb
from acts.test_utils.tel.tel_test_utils import activate_wfc_on_device
from acts.test_utils.tel.tel_test_utils import install_googleaccountutil_apk
from acts.test_utils.tel.tel_test_utils import add_google_account
from acts.test_utils.tel.tel_test_utils import install_googlefi_apk
from acts.test_utils.tel.tel_test_utils import activate_google_fi_account
from acts.test_utils.tel.tel_test_utils import check_google_fi_activated
from acts.test_utils.tel.tel_test_utils import check_fi_apk_installed
from acts.test_utils.tel.tel_test_utils import phone_switch_to_msim_mode
from acts.test_utils.tel.tel_test_utils import activate_esim_using_suw
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_BACKGROUND
from acts.test_utils.tel.tel_defines import SINGLE_SIM_CONFIG, MULTI_SIM_CONFIG
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_FOREGROUND
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_RINGING
from acts.test_utils.tel.tel_defines import SIM_STATE_ABSENT
from acts.test_utils.tel.tel_defines import SIM_STATE_UNKNOWN
from acts.test_utils.tel.tel_defines import WIFI_VERBOSE_LOGGING_ENABLED
from acts.test_utils.tel.tel_defines import WIFI_VERBOSE_LOGGING_DISABLED
from acts.test_utils.tel.tel_defines import INVALID_SUB_ID


class TelephonyBaseTest(BaseTestClass):
    # Use for logging in the test cases to facilitate
    # faster log lookup and reduce ambiguity in logging.
    @staticmethod
    def tel_test_wrap(fn):
        def _safe_wrap_test_case(self, *args, **kwargs):
            test_id = "%s:%s:%s" % (self.__class__.__name__, self.test_name,
                                    self.log_begin_time.replace(' ', '-'))
            self.test_id = test_id
            self.result_detail = ""
            self.testsignal_details = ""
            self.testsignal_extras = {}
            tries = int(self.user_params.get("telephony_auto_rerun", 1))
            for ad in self.android_devices:
                ad.log_path = self.log_path
            for i in range(tries + 1):
                result = True
                if i > 0:
                    log_string = "[Test Case] RERUN %s" % self.test_name
                    self.log.info(log_string)
                    self._teardown_test(self.test_name)
                    self._setup_test(self.test_name)
                try:
                    result = fn(self, *args, **kwargs)
                except signals.TestFailure as e:
                    self.testsignal_details = e.details
                    self.testsignal_extras = e.extras
                    result = False
                except signals.TestSignal:
                    raise
                except Exception as e:
                    self.log.exception(e)
                    asserts.fail(self.result_detail)
                if result is False:
                    if i < tries:
                        continue
                else:
                    break
            if self.user_params.get("check_crash", True):
                new_crash = ad.check_crash_report(self.test_name,
                                                  self.begin_time, True)
                if new_crash:
                    msg = "Find new crash reports %s" % new_crash
                    ad.log.error(msg)
                    self.result_detail = "%s %s %s" % (self.result_detail,
                                                       ad.serial, msg)
                    result = False
            if result is not False:
                asserts.explicit_pass(self.result_detail)
            else:
                if self.result_detail:
                    asserts.fail(self.result_detail)
                else:
                    asserts.fail(self.testsignal_details, self.testsignal_extras)

        return _safe_wrap_test_case

    def setup_class(self):
        super().setup_class()
        self.wifi_network_ssid = self.user_params.get(
            "wifi_network_ssid") or self.user_params.get(
                "wifi_network_ssid_2g") or self.user_params.get(
                    "wifi_network_ssid_5g")
        self.wifi_network_pass = self.user_params.get(
            "wifi_network_pass") or self.user_params.get(
                "wifi_network_pass_2g") or self.user_params.get(
                    "wifi_network_ssid_5g")

        self.log_path = getattr(logging, "log_path", None)
        self.qxdm_log = self.user_params.get("qxdm_log", True)
        self.sdm_log = self.user_params.get("sdm_log", False)
        self.enable_radio_log_on = self.user_params.get(
            "enable_radio_log_on", False)
        self.cbrs_esim = self.user_params.get("cbrs_esim", False)
        self.account_util = self.user_params.get("account_util", None)
        self.save_passing_logs = self.user_params.get("save_passing_logs", False)
        if isinstance(self.account_util, list):
            self.account_util = self.account_util[0]
        self.fi_util = self.user_params.get("fi_util", None)
        if isinstance(self.fi_util, list):
            self.fi_util = self.fi_util[0]
        tasks = [(self._init_device, [ad]) for ad in self.android_devices]
        multithread_func(self.log, tasks)
        self.skip_reset_between_cases = self.user_params.get(
            "skip_reset_between_cases", True)
        self.log_path = getattr(logging, "log_path", None)
        self.sim_config = {
                            "config":SINGLE_SIM_CONFIG,
                            "number_of_sims":1
                        }

        for ad in self.android_devices:
            if hasattr(ad, "dsds"):
                self.sim_config = {
                                    "config":MULTI_SIM_CONFIG,
                                    "number_of_sims":2
                                }
                break
        if "anritsu_md8475a_ip_address" in self.user_params:
            return
        qxdm_log_mask_cfg = self.user_params.get("qxdm_log_mask_cfg", None)
        if isinstance(qxdm_log_mask_cfg, list):
            qxdm_log_mask_cfg = qxdm_log_mask_cfg[0]
        if qxdm_log_mask_cfg and "dev/null" in qxdm_log_mask_cfg:
            qxdm_log_mask_cfg = None
        sim_conf_file = self.user_params.get("sim_conf_file")
        if not sim_conf_file:
            self.log.info("\"sim_conf_file\" is not provided test bed config!")
        else:
            if isinstance(sim_conf_file, list):
                sim_conf_file = sim_conf_file[0]
            # If the sim_conf_file is not a full path, attempt to find it
            # relative to the config file.
            if not os.path.isfile(sim_conf_file):
                sim_conf_file = os.path.join(
                    self.user_params[Config.key_config_path.value],
                    sim_conf_file)
                if not os.path.isfile(sim_conf_file):
                    self.log.error("Unable to load user config %s ",
                                   sim_conf_file)

        tasks = [(self._setup_device, [ad, sim_conf_file, qxdm_log_mask_cfg])
                 for ad in self.android_devices]
        return multithread_func(self.log, tasks)

    def _init_device(self, ad):
        synchronize_device_time(ad)
        ad.log_path = self.log_path
        print_radio_info(ad)
        unlock_sim(ad)
        ad.wakeup_screen()
        ad.adb.shell("input keyevent 82")

    def wait_for_sim_ready(self,ad):
        wait_for_sim_ready_on_sim_config = {
              SINGLE_SIM_CONFIG : lambda:wait_for_sim_ready_by_adb(self.log,ad),
              MULTI_SIM_CONFIG : lambda:wait_for_sims_ready_by_adb(self.log,ad)
              }
        if not wait_for_sim_ready_on_sim_config[self.sim_config["config"]]:
            raise signals.TestAbortClass("unable to load the SIM")

    def _setup_device(self, ad, sim_conf_file, qxdm_log_mask_cfg=None):
        ad.qxdm_log = getattr(ad, "qxdm_log", self.qxdm_log)
        ad.sdm_log = getattr(ad, "sdm_log", self.sdm_log)
        if self.user_params.get("enable_connectivity_metrics", False):
            enable_connectivity_metrics(ad)
        if self.user_params.get("build_id_override", False):
            build_postfix = self.user_params.get("build_id_postfix",
                                                 "LAB_TEST")
            build_id_override(
                ad,
                new_build_id=self.user_params.get("build_id_override_with",
                                                  None),
                postfix=build_postfix)
        if self.enable_radio_log_on:
            enable_radio_log_on(ad)
        if "sdm" in ad.model or "msm" in ad.model or "kon" in ad.model:
            phone_mode = "ssss"
            if hasattr(ad, "mtp_dsds"):
                phone_mode = "dsds"
            if ad.adb.getprop("persist.radio.multisim.config") != phone_mode:
                ad.adb.shell("setprop persist.radio.multisim.config %s" \
                             % phone_mode)
                reboot_device(ad)

        stop_qxdm_logger(ad)
        if ad.qxdm_log:
            qxdm_log_mask = getattr(ad, "qxdm_log_mask", None)
            if qxdm_log_mask_cfg:
                qxdm_mask_path = self.user_params.get("qxdm_log_path",
                                                      DEFAULT_QXDM_LOG_PATH)
                ad.adb.shell("mkdir %s" % qxdm_mask_path)
                ad.log.info("Push %s to %s", qxdm_log_mask_cfg, qxdm_mask_path)
                ad.adb.push("%s %s" % (qxdm_log_mask_cfg, qxdm_mask_path))
                mask_file_name = os.path.split(qxdm_log_mask_cfg)[-1]
                qxdm_log_mask = os.path.join(qxdm_mask_path, mask_file_name)
            set_qxdm_logger_command(ad, mask=qxdm_log_mask)
            start_qxdm_logger(ad, utils.get_current_epoch_time())
        elif ad.sdm_log:
            start_sdm_logger(ad)
        else:
            disable_qxdm_logger(ad)
        if not unlock_sim(ad):
            raise signals.TestAbortClass("unable to unlock the SIM")

        # eSIM enablement
        if hasattr(ad, "fi_esim"):
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("Failed to connect to wifi")
            if check_google_fi_activated(ad):
                ad.log.info("Google Fi is already Activated")
            else:
                install_googleaccountutil_apk(ad, self.account_util)
                add_google_account(ad)
                install_googlefi_apk(ad, self.fi_util)
                if not activate_google_fi_account(ad):
                    ad.log.error("Failed to activate Fi")
                check_google_fi_activated(ad)
        if hasattr(ad, "dsds"):
            sim_mode = ad.droid.telephonyGetPhoneCount()
            if sim_mode == 1:
                ad.log.info("Phone in Single SIM Mode")
                if not phone_switch_to_msim_mode(ad):
                    ad.log.error("Failed to switch to Dual SIM Mode")
                    return False
            elif sim_mode == 2:
                ad.log.info("Phone already in Dual SIM Mode")
        if get_sim_state(ad) in (SIM_STATE_ABSENT, SIM_STATE_UNKNOWN):
            ad.log.info("Device has no or unknown SIM in it")
            # eSIM needs activation
            activate_esim_using_suw(ad)
            ensure_phone_idle(self.log, ad)
        elif self.user_params.get("Attenuator"):
            ad.log.info("Device in chamber room")
            ensure_phone_idle(self.log, ad)
            setup_droid_properties(self.log, ad, sim_conf_file)
        else:
            self.wait_for_sim_ready(ad)
            ensure_phone_default_state(self.log, ad)
            setup_droid_properties(self.log, ad, sim_conf_file)

        default_slot = getattr(ad, "default_slot", 0)
        if get_subid_from_slot_index(ad.log, ad, default_slot) != INVALID_SUB_ID:
            ad.log.info("Slot %s is the default slot.", default_slot)
            set_default_sub_for_all_services(ad, default_slot)
        else:
            ad.log.warning("Slot %s is NOT a valid slot. Slot %s will be used by default.",
                default_slot, 1-default_slot)
            set_default_sub_for_all_services(ad, 1-default_slot)

        # Activate WFC on Verizon, AT&T and Canada operators as per # b/33187374 &
        # b/122327716
        activate_wfc_on_device(self.log, ad)

        # Sub ID setup
        initial_set_up_for_subid_infomation(self.log, ad)

        # If device is setup already, skip the following setup procedures
        if getattr(ad, "telephony_test_setup", None):
            return True

        try:
            ad.droid.wifiEnableVerboseLogging(WIFI_VERBOSE_LOGGING_ENABLED)
        except Exception:
            pass

        # Disable Emergency alerts
        # Set chrome browser start with no-first-run verification and
        # disable-fre. Give permission to read from and write to storage.
        for cmd in ("pm disable com.android.cellbroadcastreceiver",
                    "pm grant com.android.chrome "
                    "android.permission.READ_EXTERNAL_STORAGE",
                    "pm grant com.android.chrome "
                    "android.permission.WRITE_EXTERNAL_STORAGE",
                    "rm /data/local/chrome-command-line",
                    "am set-debug-app --persistent com.android.chrome",
                    'echo "chrome --no-default-browser-check --no-first-run '
                    '--disable-fre" > /data/local/tmp/chrome-command-line'):
            ad.adb.shell(cmd, ignore_status=True)

        # Curl for 2016/7 devices
        if not getattr(ad, "curl_capable", False):
            try:
                out = ad.adb.shell("/data/curl --version")
                if not out or "not found" in out:
                    if int(ad.adb.getprop("ro.product.first_api_level")) >= 25:
                        tel_data = self.user_params.get("tel_data", "tel_data")
                        if isinstance(tel_data, list):
                            tel_data = tel_data[0]
                        curl_file_path = os.path.join(tel_data, "curl")
                        if not os.path.isfile(curl_file_path):
                            curl_file_path = os.path.join(
                                self.user_params[Config.key_config_path.value],
                                curl_file_path)
                        if os.path.isfile(curl_file_path):
                            ad.log.info("Pushing Curl to /data dir")
                            ad.adb.push("%s /data" % (curl_file_path))
                            ad.adb.shell(
                                "chmod 777 /data/curl", ignore_status=True)
                else:
                    setattr(ad, "curl_capable", True)
            except Exception:
                ad.log.info("Failed to push curl on this device")

        # Ensure that a test class starts from a consistent state that
        # improves chances of valid network selection and facilitates
        # logging.
        try:
            if not set_phone_screen_on(self.log, ad):
                self.log.error("Failed to set phone screen-on time.")
                return False
            if not set_phone_silent_mode(self.log, ad):
                self.log.error("Failed to set phone silent mode.")
                return False
            ad.droid.telephonyAdjustPreciseCallStateListenLevel(
                PRECISE_CALL_STATE_LISTEN_LEVEL_FOREGROUND, True)
            ad.droid.telephonyAdjustPreciseCallStateListenLevel(
                PRECISE_CALL_STATE_LISTEN_LEVEL_RINGING, True)
            ad.droid.telephonyAdjustPreciseCallStateListenLevel(
                PRECISE_CALL_STATE_LISTEN_LEVEL_BACKGROUND, True)
        except Exception as e:
            self.log.error("Failure with %s", e)
        setattr(ad, "telephony_test_setup", True)
        return True

    def _teardown_device(self, ad):
        try:
            stop_qxdm_logger(ad)
            stop_sdm_logger(ad)
        except Exception as e:
            self.log.error("Failure with %s", e)
        try:
            ad.droid.disableDevicePassword()
        except Exception as e:
            self.log.error("Failure with %s", e)
        if self.user_params.get("enable_connectivity_metrics", False):
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("Failed to connect to wifi")
            force_connectivity_metrics_upload(ad)
            time.sleep(30)
        try:
            ad.droid.wifiEnableVerboseLogging(WIFI_VERBOSE_LOGGING_DISABLED)
        except Exception as e:
            self.log.error("Failure with %s", e)
        try:
            if self.user_params.get("build_id_override",
                                    False) and self.user_params.get(
                                        "recover_build_id", False):
                recover_build_id(ad)
        except Exception as e:
            self.log.error("Failure with %s", e)

    def teardown_class(self):
        tasks = [(self._teardown_device, [ad]) for ad in self.android_devices]
        multithread_func(self.log, tasks)
        return True

    def setup_test(self):
        if getattr(self, "qxdm_log", True):
            if not self.user_params.get("qxdm_log_mask_cfg", None):
                if "wfc" in self.test_name:
                    for ad in self.android_devices:
                        if not getattr(ad, "qxdm_logger_command", None) or (
                                "IMS_DS_CNE_LnX_Golden.cfg" not in getattr(
                                    ad, "qxdm_logger_command", "")):
                            set_qxdm_logger_command(
                                ad, "IMS_DS_CNE_LnX_Golden.cfg")
                else:
                    for ad in self.android_devices:
                        if not getattr(ad, "qxdm_logger_command", None) or (
                                "IMS_DS_CNE_LnX_Golden.cfg" in getattr(
                                    ad, "qxdm_logger_command", "")):
                            set_qxdm_logger_command(ad, None)
            start_qxdm_loggers(self.log, self.android_devices, self.begin_time)
        if getattr(self, "sdm_log", False):
            start_sdm_loggers(self.log, self.android_devices)
        if getattr(self, "tcpdump_log", False) or "wfc" in self.test_name:
            mask = getattr(self, "tcpdump_mask", "all")
            interface = getattr(self, "tcpdump_interface", "wlan0")
            start_tcpdumps(
                self.android_devices,
                begin_time=self.begin_time,
                interface=interface,
                mask=mask)
        else:
            stop_tcpdumps(self.android_devices)
        for ad in self.android_devices:
            if self.skip_reset_between_cases:
                ensure_phone_idle(self.log, ad)
            else:
                ensure_phone_default_state(self.log, ad)
            for session in ad._sl4a_manager.sessions.values():
                ed = session.get_event_dispatcher()
                ed.clear_all_events()
            output = ad.adb.logcat("-t 1")
            match = re.search(r"\d+-\d+\s\d+:\d+:\d+.\d+", output)
            if match:
                ad.test_log_begin_time = match.group(0)

    def teardown_test(self):
        stop_tcpdumps(self.android_devices)

    def on_fail(self, test_name, begin_time):
        self._take_bug_report(test_name, begin_time)

    def on_pass(self, test_name, begin_time):
        if self.save_passing_logs:
            self._take_bug_report(test_name, begin_time)

    def _ad_take_extra_logs(self, ad, test_name, begin_time):
        ad.adb.wait_for_device()
        result = True

        try:
            # get tcpdump and screen shot log
            get_tcpdump_log(ad, test_name, begin_time)
            get_screen_shot_log(ad, test_name, begin_time)
        except Exception as e:
            ad.log.error("Exception error %s", e)
            result = False

        try:
            ad.check_crash_report(test_name, begin_time, log_crash_report=True)
        except Exception as e:
            ad.log.error("Failed to check crash report for %s with error %s",
                         test_name, e)
            result = False

        extra_qxdm_logs_in_seconds = self.user_params.get(
            "extra_qxdm_logs_in_seconds", 60 * 3)
        if getattr(ad, "qxdm_log", True):
            # Gather qxdm log modified 3 minutes earlier than test start time
            if begin_time:
                qxdm_begin_time = begin_time - 1000 * extra_qxdm_logs_in_seconds
            else:
                qxdm_begin_time = None
            try:
                time.sleep(10)
                ad.get_qxdm_logs(test_name, qxdm_begin_time)
            except Exception as e:
                ad.log.error("Failed to get QXDM log for %s with error %s",
                             test_name, e)
                result = False
        if getattr(ad, "sdm_log", False):
            # Gather sdm log modified 3 minutes earlier than test start time
            if begin_time:
                sdm_begin_time = begin_time - 1000 * extra_qxdm_logs_in_seconds
            else:
                sdm_begin_time = None
            try:
                time.sleep(10)
                ad.get_sdm_logs(test_name, sdm_begin_time)
            except Exception as e:
                ad.log.error("Failed to get SDM log for %s with error %s",
                             test_name, e)
                result = False

        return result

    def _take_bug_report(self, test_name, begin_time):
        if self._skip_bug_report(test_name):
            return
        dev_num = getattr(self, "number_of_devices", None) or len(
            self.android_devices)
        tasks = [(self._ad_take_bugreport, (ad, test_name, begin_time))
                 for ad in self.android_devices[:dev_num]]
        tasks.extend([(self._ad_take_extra_logs, (ad, test_name, begin_time))
                      for ad in self.android_devices[:dev_num]])
        run_multithread_func(self.log, tasks)
        for ad in self.android_devices[:dev_num]:
            if getattr(ad, "reboot_to_recover", False):
                reboot_device(ad)
                ad.reboot_to_recover = False
        # Zip log folder
        if not self.user_params.get("zip_log", False): return
        src_dir = os.path.join(self.log_path, test_name)
        os.makedirs(src_dir, exist_ok=True)
        file_name = "%s_%s" % (src_dir, begin_time)
        self.log.info("Zip folder %s to %s.zip", src_dir, file_name)
        shutil.make_archive(file_name, "zip", src_dir)
        shutil.rmtree(src_dir)

    def _block_all_test_cases(self, tests, reason='Failed class setup'):
        """Over-write _block_all_test_cases in BaseTestClass."""
        for (i, (test_name, test_func)) in enumerate(tests):
            signal = signals.TestFailure(reason)
            record = records.TestResultRecord(test_name, self.TAG)
            record.test_begin()
            # mark all test cases as FAIL
            record.test_fail(signal)
            self.results.add_record(record)
            # only gather bug report for the first test case
            if i == 0:
                self.on_fail(test_name, record.begin_time)

    def get_stress_test_number(self):
        """Gets the stress_test_number param from user params.

        Gets the stress_test_number param. If absent, returns default 100.
        """
        return int(self.user_params.get("stress_test_number", 100))
