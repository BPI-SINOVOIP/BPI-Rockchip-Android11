#!/usr/bin/env python3.4
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
    Test Script for Telephony Pre Check In Sanity
"""

import collections
import time

from acts import signals
from acts.test_decorators import test_tracker_info
from acts.controllers.sl4a_lib.sl4a_types import Sl4aNetworkInfo
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_data_utils import wifi_tethering_setup_teardown
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from acts.test_utils.tel.tel_defines import CAPABILITY_VT
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import CAPABILITY_OMADM
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_TETHERING_ENTITLEMENT_CHECK
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import RAT_FAMILY_WLAN
from acts.test_utils.tel.tel_defines import TETHERING_MODE_WIFI
from acts.test_utils.tel.tel_defines import WAIT_TIME_AFTER_REBOOT
from acts.test_utils.tel.tel_defines import WAIT_TIME_AFTER_CRASH
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_defines import VT_STATE_BIDIRECTIONAL
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import get_model_name
from acts.test_utils.tel.tel_test_utils import get_outgoing_voice_sub_id
from acts.test_utils.tel.tel_test_utils import get_slot_index_from_subid
from acts.test_utils.tel.tel_test_utils import is_droid_in_network_generation
from acts.test_utils.tel.tel_test_utils import is_sim_locked
from acts.test_utils.tel.tel_test_utils import mms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import power_off_sim
from acts.test_utils.tel.tel_test_utils import power_on_sim
from acts.test_utils.tel.tel_test_utils import reboot_device
from acts.test_utils.tel.tel_test_utils import sms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.tel.tel_test_utils import trigger_modem_crash
from acts.test_utils.tel.tel_test_utils import trigger_modem_crash_by_modem
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_enabled
from acts.test_utils.tel.tel_test_utils import wait_for_cell_data_connection
from acts.test_utils.tel.tel_test_utils import wait_for_network_generation
from acts.test_utils.tel.tel_test_utils import wait_for_network_rat
from acts.test_utils.tel.tel_test_utils import wait_for_wifi_data_connection
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import wait_for_state
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_idle_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_video_utils import video_call_setup_teardown
from acts.test_utils.tel.tel_video_utils import phone_setup_video
from acts.test_utils.tel.tel_video_utils import \
    is_phone_in_call_video_bidirectional

from acts.utils import get_current_epoch_time
from acts.utils import rand_ascii_str


class TelLiveRebootStressTest(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)

        self.stress_test_number = int(
            self.user_params.get("stress_test_number", 10))
        self.skip_reset_between_cases = False

        self.dut = self.android_devices[0]
        self.ad_reference = self.android_devices[1] if len(
            self.android_devices) > 1 else None
        self.dut_model = get_model_name(self.dut)
        self.user_params["check_crash"] = False
        self.skip_reset_between_cases = False

        self.dut_subID = get_outgoing_voice_sub_id(self.dut)
        self.dut_capabilities = self.dut.telephony["subscription"][self.dut_subID].get("capabilities", [])
        self.dut_wfc_modes = self.dut.telephony["subscription"][self.dut_subID].get("wfc_modes", [])
        self.default_testing_func_names = []
        for method in ("_check_volte", "_check_3g"):
            func = getattr(self, method)
            try:
                check_result = func()
            except Exception as e:
                self.dut.log.error("%s failed with %s", method, e)
                check_result = False
            self.dut.log.info("%s is %s before tests start", method,
                              check_result)
            if check_result:
                self.default_testing_func_names.append(method)
        self.dut.log.info("To be tested: %s", self.default_testing_func_names)

    def teardown_test(self):
        self._set_volte_provisioning()

    def feature_validator(self, *args):
        failed_tests = []
        for method in ("_check_subscription", "_check_data",
                       "_check_call_setup_teardown", "_check_sms"):
            func = getattr(self, method)
            if not func():
                self.log.error("%s failed", method)
                failed_tests.append(method)
        for method in args:
            func = getattr(self, method)
            try:
                func_result = func()
            except Exception as e:
                self.log.error("%s check failed with %s", method, e)
                func_result = False
            if not func_result:
                self.log.error("%s failed", method)
                failed_tests.append(method)
            else:
                self.log.info("%s succeeded", method)
        if failed_tests:
            self.log.error("%s failed", failed_tests)
        return failed_tests

    def _check_subscription(self):
        if not ensure_phone_subscription(self.log, self.dut):
            self.dut.log.error("Subscription check failed")
            return False
        else:
            return True

    def _check_volte_provisioning(self):
        if CAPABILITY_OMADM in self.dut_capabilities:
            if not wait_for_state(self.dut.droid.imsIsVolteProvisionedOnDevice,
                                  True):
                self.dut.log.error("VoLTE provisioning is disabled.")
                return False
            else:
                self.dut.log.info("VoLTE provision is enabled")
                return True
        return True

    def _check_volte_provisioning_disabled(self):
        if CAPABILITY_OMADM in self.dut_capabilities:
            if not wait_for_state(self.dut.droid.imsIsVolteProvisionedOnDevice,
                                  False):
                self.dut.log.error("VoLTE provisioning is not disabled.")
                return False
            else:
                self.dut.log.info("VoLTE provision is disabled")
                return True
        return True

    def _set_volte_provisioning(self):
        if CAPABILITY_OMADM in self.dut_capabilities:
            provisioned = self.dut.droid.imsIsVolteProvisionedOnDevice()
            if provisioned:
                self.dut.log.info("Volte is provioned")
                return
            self.dut.log.info("Volte is not provisioned")
            self.dut.log.info("Set VoLTE Provisioning bit")
            self.dut.droid.imsSetVolteProvisioning(True)

    def _clear_volte_provisioning(self):
        if CAPABILITY_OMADM in self.dut_capabilities:
            self.dut.log.info("Clear VoLTE Provisioning bit")
            self.dut.droid.imsSetVolteProvisioning(False)
            if self.dut.droid.imsIsVolteProvisionedOnDevice():
                self.dut.log.error("VoLTE is still provisioned")
                return False
            else:
                self.dut.log.info("VoLTE provisioning is disabled")
        return True

    def _check_call_setup_teardown(self):
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut):
            self.log.error("Phone call test failed.")
            return False
        return True

    def _check_sms(self):
        if not sms_send_receive_verify(self.log, self.dut, self.ad_reference,
                                       [rand_ascii_str(180)]):
            self.log.error("SMS send test failed")
            return False
        else:
            self.log.info("SMS send test passed")
            return True

    def _check_mms(self):
        message_array = [("Test Message", rand_ascii_str(180), None)]
        if not mms_send_receive_verify(self.log, self.dut, self.ad_reference,
                                       message_array):
            self.log.error("MMS test sendfailed")
            return False
        else:
            self.log.info("MMS send test passed")
            return True

    def _check_sms_mt(self):
        if not sms_send_receive_verify(self.log, self.ad_reference, self.dut,
                                       [rand_ascii_str(180)]):
            self.log.error("SMS receive test failed")
            return False
        else:
            self.log.info("SMS receive test passed")
            return True

    def _check_mms_mt(self):
        message_array = [("Test Message", rand_ascii_str(180), None)]
        if not mms_send_receive_verify(self.log, self.ad_reference, self.dut,
                                       message_array):
            self.log.error("MMS receive test failed")
            return False
        else:
            self.log.info("MMS receive test passed")
            return True

    def _check_data(self):
        if not verify_internet_connection(self.log, self.dut):
            self.dut.log.error("Data connection is not available.")
            return False
        return True

    def _get_list_average(self, input_list):
        total_sum = float(sum(input_list))
        total_count = float(len(input_list))
        if input_list == []:
            return False
        return float(total_sum / total_count)

    def _check_lte_data(self):
        if not is_droid_in_network_generation(self.log, self.dut, GEN_4G,
                                              NETWORK_SERVICE_DATA):
            self.dut.log.error("Data is not on 4G network")
            return False
        if not verify_internet_connection(self.log, self.dut):
            self.log.error("Data not available on cell.")
            return False
        return True

    def _check_volte(self):
        if CAPABILITY_VOLTE in self.dut_capabilities:
            self._set_volte_provisioning()
            if not self._check_volte_provisioning():
                return False
            self.log.info("Check VoLTE")
            if not wait_for_state(self.dut.droid.imsIsVolteProvisionedOnDevice,
                                  True):
                self.dut.log.error("VoLTE provisioning is disabled.")
                return False
            if not phone_setup_volte(self.log, self.dut):
                self.log.error("Failed to setup VoLTE.")
                return False
            time.sleep(5)
            if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                       self.dut, is_phone_in_call_volte):
                self.log.error("VoLTE Call Failed.")
                return False
            if not self._check_lte_data():
                return False
        else:
            self.dut.log.info("VoLTE is not supported")
            return False
        return True

    def _check_csfb(self):
        if not phone_setup_csfb(self.log, self.dut):
            self.log.error("Failed to setup CSFB.")
            return False
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_csfb):
            self.dut.log.error("CSFB Call Failed.")
            return False
        if not wait_for_network_generation(
                self.log, self.dut, GEN_4G,
                voice_or_data=NETWORK_SERVICE_DATA):
            self.dut.log.error("Data service failed to camp to 4G")
            return False
        if not verify_internet_connection(self.log, self.dut):
            self.log.error("Data not available on cell.")
            return False
        return True

    def _check_volte_enabled(self):
        if phone_idle_volte(self.log, self.dut):
            self.dut.log.info("VoLTE is enabled")
        else:
            self.dut.log.error("VoLTE is not enabled")
            return False
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_volte):
            self.log.error("VoLTE Call Failed.")
            return False
        if not self._check_lte_data():
            return False
        return True

    def _check_csfb_enabled(self):
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_csfb):
            self.log.error("CSFB Call Failed.")
            return False
        if not wait_for_network_generation(
                self.log, self.dut, GEN_4G,
                voice_or_data=NETWORK_SERVICE_DATA):
            self.dut.log.error("Data service failed to camp to 4G")
            return False
        if not verify_internet_connection(self.log, self.dut):
            self.log.error("Data not available on cell.")
            return False
        return True

    def _check_vt(self):
        if CAPABILITY_VT in self.dut_capabilities:
            self.log.info("Check VT")
            for ad in (self.dut, self.ad_reference):
                if not phone_setup_video(self.log, ad):
                    ad.log.error("Failed to setup VT.")
                    return False
            time.sleep(5)
            if not video_call_setup_teardown(
                    self.log,
                    self.dut,
                    self.ad_reference,
                    self.dut,
                    video_state=VT_STATE_BIDIRECTIONAL,
                    verify_caller_func=is_phone_in_call_video_bidirectional,
                    verify_callee_func=is_phone_in_call_video_bidirectional):
                self.log.error("VT Call Failed.")
                return False
            else:
                return True
        return False

    def _check_vt_enabled(self):
        if not video_call_setup_teardown(
                self.log,
                self.dut,
                self.ad_reference,
                self.dut,
                video_state=VT_STATE_BIDIRECTIONAL,
                verify_caller_func=is_phone_in_call_video_bidirectional,
                verify_callee_func=is_phone_in_call_video_bidirectional):
            self.log.error("VT Call Failed.")
            return False
        return True

    def _check_wfc_apm(self):
        if CAPABILITY_WFC in self.dut_capabilities:
            self.log.info("Check WFC in APM")
            if not phone_setup_iwlan(
                    self.log, self.dut, True, WFC_MODE_CELLULAR_PREFERRED,
                    self.wifi_network_ssid, self.wifi_network_pass):
                self.log.error("Failed to setup WFC.")
                return False
            if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                       self.dut, is_phone_in_call_iwlan):
                self.log.error("WFC Call Failed.")
                return False
            else:
                return True
        return False

    def _check_wfc_nonapm(self):
        if CAPABILITY_WFC not in self.dut_capabilities and (
                WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes):
            return False
        self.log.info("Check WFC in NonAPM")
        if not phone_setup_iwlan(
                self.log, self.dut, False, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.log.error("Failed to setup WFC.")
            return False
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_iwlan):
            self.log.error("WFC Call Failed.")
            return False
        else:
            return True

    def _check_wfc_enabled(self):
        if not wait_for_wifi_data_connection(self.log, self.dut, True):
            self.dut.log.error("Failed to connect to WIFI")
            return False
        if not wait_for_wfc_enabled(self.log, self.dut):
            self.dut.log.error("WFC is not enabled")
            return False
        if not wait_for_network_rat(
                self.log,
                self.dut,
                RAT_FAMILY_WLAN,
                voice_or_data=NETWORK_SERVICE_DATA):
            ad.log.info("Data rat can not go to iwlan mode successfully")
            return False
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_iwlan):
            self.log.error("WFC Call Failed.")
            return False
        return True

    def _check_3g(self):
        self.log.info("Check 3G data and CS call")
        if not phone_setup_voice_3g(self.log, self.dut):
            self.log.error("Failed to setup 3G")
            return False
        if not verify_internet_connection(self.log, self.dut):
            self.log.error("Data not available on cell.")
            return False
        if not call_setup_teardown(self.log, self.dut, self.ad_reference,
                                   self.dut, is_phone_in_call_3g):
            self.log.error("WFC Call Failed.")
            return False
        if not sms_send_receive_verify(self.log, self.dut, self.ad_reference,
                                       [rand_ascii_str(50)]):
            self.log.error("SMS failed in 3G")
            return False
        return True

    def _check_tethering(self):
        self.log.info("Check tethering")
        for i in range(3):
            try:
                if not self.dut.droid.carrierConfigIsTetheringModeAllowed(
                        TETHERING_MODE_WIFI,
                        MAX_WAIT_TIME_TETHERING_ENTITLEMENT_CHECK):
                    self.log.error("Tethering Entitlement check failed.")
                    if i == 2: return False
                    time.sleep(10)
            except Exception as e:
                if i == 2:
                    self.dut.log.error(e)
                    return False
                time.sleep(10)
        if not wifi_tethering_setup_teardown(
                self.log,
                self.dut, [self.ad_reference],
                check_interval=5,
                check_iteration=1):
            self.log.error("Tethering check failed.")
            return False
        return True

    def _check_data_roaming_status(self):
        if not self.dut.droid.telephonyIsDataEnabled():
            self.log.info("Enabling Cellular Data")
            telephonyToggleDataConnection(True)
        else:
            self.log.info("Cell Data is Enabled")
        self.log.info("Waiting for cellular data to be connected")
        if not wait_for_cell_data_connection(self.log, self.dut, state=True):
            self.log.error("Failed to enable cell data")
            return False
        self.log.info("Cellular data connected, checking NetworkInfos")
        roaming_state = self.dut.droid.telephonyCheckNetworkRoaming()
        for network_info in self.dut.droid.connectivityNetworkGetAllInfo():
            sl4a_network_info = Sl4aNetworkInfo.from_dict(network_info)
            if sl4a_network_info.isRoaming:
                self.log.warning("We don't expect to be roaming")
            if sl4a_network_info.isRoaming != roaming_state:
                self.log.error(
                    "Mismatched Roaming Status Information Telephony: {}, NetworkInfo {}".
                    format(roaming_state, sl4a_network_info.isRoaming))
                self.log.error(network_info)
                return False
        return True

    def _reboot_stress_test(self, *args):
        """Reboot Reliability Test

        Arguments:
            function_name: function to be checked

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        self.number_of_devices = 2
        fail_count = collections.defaultdict(int)
        test_result = True

        for i in range(1, self.stress_test_number + 1):
            begin_time = get_current_epoch_time()
            test_name = "%s_iteration_%s" % (self.test_name, i)
            log_msg = "[Test Case] %s" % test_name
            self.log.info("%s begin", log_msg)
            self.dut.droid.logI("%s begin" % log_msg)
            test_msg = "Reboot Stress Test %s Iteration <%s> / <%s>" % (
                self.test_name, i, self.stress_test_number)
            self.log.info(test_msg)
            reboot_device(self.dut)
            self.log.info("{} wait {}s for radio up.".format(
                self.dut.serial, WAIT_TIME_AFTER_REBOOT))
            time.sleep(WAIT_TIME_AFTER_REBOOT)
            failed_tests = self.feature_validator(*args)
            for test in failed_tests:
                fail_count[test] += 1

            crash_report = self.dut.check_crash_report(
                "%s_%s" % (self.test_name, i),
                begin_time,
                log_crash_report=True)
            if crash_report:
                fail_count["crashes"] += 1
            if failed_tests or crash_report:
                self.log.error("%s FAIL with %s and crashes %s", test_msg,
                               failed_tests, crash_report)
                self._take_bug_report(test_name, begin_time)
            else:
                self.log.info("%s PASS", test_msg)
            self.log.info("Total failure count: %s", dict(fail_count))
            self.log.info("%s end", log_msg)
            self.dut.droid.logI("%s end" % log_msg)

        for failure, count in fail_count.items():
            if count:
                self.log.error("%s failure count = %s in total %s iterations",
                               failure, count, self.stress_test_number)
                test_result = False
        return test_result

    def _crash_recovery_test(self, process, *args):
        """Crash Recovery Test

        Arguments:
            process: the process to be killed. For example:
                "rild", "netmgrd", "com.android.phone", "imsqmidaemon",
                "imsdatadaemon", "ims_rtp_daemon",
                "com.android.ims.rcsservice", "system_server", "cnd",
                "modem"

        Expected Results:
            All Features should work as intended post crash recovery

        Returns:
            True is pass, False if fail.
        """
        self.number_of_devices = 2

        try:
            self.dut.droid.logI("======== Trigger %s crash ========" % process)
        except:
            pass
        if process == "modem":
            self.user_params["check_crash"] = False
            self.dut.log.info("======== Crash modem from kernal ========")
            trigger_modem_crash(self.dut)
        elif process == "modem-crash":
            self.user_params["check_crash"] = False
            self.dut.log.info("======== Crash modem from modem ========")
            trigger_modem_crash_by_modem(self.dut)
        elif process == "sim":
            self.dut.log.info("======== Power cycle SIM slot ========")
            self.user_params["check_crash"] = True
            sub_id = get_outgoing_voice_sub_id(self.dut)
            slot_index = get_slot_index_from_subid(self.log, self.dut, sub_id)
            if not power_off_sim(self.dut, slot_index):
                self.dut.log.warning("Fail to power off SIM")
                raise signals.TestSkip("Power cycle SIM not working")
            if not power_on_sim(self.dut, slot_index):
                self.dut.log.error("Fail to power on SIM slot")
                setattr(self.dut, "reboot_to_recover", True)
                return False
        else:
            if process == "rild":
                if int(self.dut.adb.getprop(
                        "ro.product.first_api_level")) >= 28:
                    process = "qcrild"
            self.dut.log.info("======== Killing process <%s> ========",
                              process)
            process_pid = self.dut.adb.shell("pidof %s" % process)
            self.dut.log.info("Pid of %s is %s", process, process_pid)
            if not process_pid:
                self.dut.log.error("Process %s not running", process)
                return False
            if process in ("netd", "system_server"):
                self.dut.stop_services()
            self.dut.adb.shell("kill -9 %s" % process_pid, ignore_status=True)
            self.dut.log.info("Wait %s sec for process %s come up.",
                              WAIT_TIME_AFTER_CRASH, process)
            time.sleep(WAIT_TIME_AFTER_CRASH)
            if process in ("netd", "system_server"):
                self.dut.ensure_screen_on()
                try:
                    self.dut.start_services()
                except Exception as e:
                    self.dut.log.warning(e)
            process_pid_new = self.dut.adb.shell("pidof %s" % process)
            if process_pid == process_pid_new:
                self.dut.log.error(
                    "Process %s has the same pid: old:%s new:%s", process,
                    process_pid, process_pid_new)
        try:
            self.dut.droid.logI(
                "======== Start testing after triggering %s crash ========" %
                process)
        except Exception:
            self.dut.ensure_screen_on()
            self.dut.start_services()
            if is_sim_locked(self.dut):
                unlock_sim(self.dut)

        if process == "ims_rtp_daemon":
            if not self._check_wfc_enabled:
                failed_tests = ["_check_wfc_enabled"]
            else:
                failed_tests = []
        else:
            failed_tests = []

        begin_time = get_current_epoch_time()
        failed_tests.extend(self.feature_validator(*args))
        crash_report = self.dut.check_crash_report(
            self.test_name, begin_time, log_crash_report=True)
        if failed_tests or crash_report:
            if failed_tests:
                self.dut.log.error("%s failed after %s restart", failed_tests,
                                   process)
                setattr(self.dut, "reboot_to_recover", True)
            if crash_report:
                self.dut.log.error("Crash %s found after %s restart",
                                   crash_report, process)
            return False
        else:
            return True

    """ Tests Begin """

    @test_tracker_info(uuid="4d9b425b-f804-45f4-8f47-0ba3f01a426b")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress(self):
        """Reboot with VoLTE Test

        Steps:
            1. Reboot DUT.
            2. Wait for DUT to camp
            3. Verify Subscription, Call, Data, Messaging, Tethering
            4. Check crashes.
            5. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        return self._reboot_stress_test(*self.default_testing_func_names)

    @test_tracker_info(uuid="8b0e2c06-02bf-40fd-a374-08860e482757")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_check_phone_call_only(self):
        """Reboot with VoLTE Test

        Steps:
            1. Reboot DUT with volte enabled.
            2. Wait for DUT to camp on LTE, Verify Data.
            3. Check VoLTE is enabled by default, check IMS registration.
               Wait for DUT report VoLTE enabled, make VoLTE call.
               And verify VoLTE SMS. (if support VoLTE)
            4. Check crashes.
            5. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if not self._check_call_setup_teardown():
            self.dut.log.error("Call setup test failed before reboot test")
            return False
        func_names = [
            "_check_subscription", "_check_data", "_check_call_setup_teardown"
        ]
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="39a822e5-0360-44ce-97c7-f75468eba8d7")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_volte_enabled(self):
        """Reboot with VoLTE Test

        Steps:
            1. Reboot DUT with volte enabled.
            2. Wait for DUT to camp on LTE, Verify Data.
            3. Check VoLTE is enabled by default, check IMS registration.
               Wait for DUT report VoLTE enabled, make VoLTE call.
               And verify VoLTE SMS. (if support VoLTE)
            4. Check crashes.
            5. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_VOLTE not in self.dut_capabilities:
            raise signals.TestSkip("VOLTE is not supported")
        if not self._check_volte():
            self.dut.log.error("VoLTE test failed before reboot test")
            return False
        func_names = ["_check_volte_enabled"]
        if "_check_vt" in self.default_testing_func_names:
            func_names.append("_check_vt_enabled")
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="3dace255-01a6-46ba-87e0-35396d406c95")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_csfb(self):
        """Reboot with VoLTE Test

        Steps:
            1. Reboot DUT with CSFB.
            2. Wait for DUT to camp on LTE, Verify Data.
            3. Check call in CSFB after rebooting.
            4. Check crashes.
            5. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if not self._check_csfb():
            self.dut.log.error("CSFB test failed before reboot test")
            return False
        func_names = ["_check_csfb_enabled"]
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="326f5ba4-8819-49bc-af87-6b3c07532de3")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_volte_provisioning_disabled(self):
        """Reboot with VoLTE Test

        Steps:
            1. Reboot DUT with volte provisioning disabled.
            2. Wait for DUT to camp on LTE, Verify Data.
            3. Check VoLTE is disabled after rebooting.
            4. Check crashes.
            5. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_OMADM not in self.dut_capabilities:
            raise signals.TestSkip("OMADM is not supported")
        self._clear_volte_provisioning()
        if not self._check_csfb():
            self.dut.log.error("CSFB test failed before reboot test")
            return False
        func_names = [
            "_check_volte_provisioning_disabled", "_check_csfb_enabled"
        ]
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="6c243b53-379a-4cda-9848-84fcec4019bd")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_wfc_apm(self):
        """Reboot with WFC in APM Test

        Steps:
            1. Reboot DUT with wfc in apm mode.
            2. Check phone call.
            3. Check crashes.
            4. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_WFC not in self.dut_capabilities:
            raise signals.TestSkip("WFC is not supported")
        if "_check_wfc_apm" not in self.default_testing_func_names:
            raise signals.TestSkip("WFC in airplane mode is not supported")
        func_names = ["_check_data", "_check_wfc_enabled"]
        if "_check_vt" in self.default_testing_func_names:
            func_names.append("_check_vt_enabled")
        if not self._check_wfc_apm():
            self.dut.log.error("WFC in APM test failed before reboot test")
            return False
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="d0439c53-98fa-4303-b097-12ba2462295d")
    @TelephonyBaseTest.tel_test_wrap
    def test_reboot_stress_wfc_nonapm(self):
        """Reboot with WFC in APM Test

        Steps:
            1. Reboot DUT with wfc in apm mode.
            2. Check phone call .
            3. Check crashes.
            4. Repeat Step 1~4 for N times. (before reboot, clear Provisioning
                bit if provisioning is supported)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_WFC not in self.dut_capabilities and (
                WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes):
            raise signals.TestSkip("WFC_NONAPM is not supported")
        if "_check_wfc_nonapm" not in self.default_testing_func_names:
            raise signals.TestSkip("WFC in non-airplane mode is not working")
        func_names = ["_check_wfc_enabled"]
        if "_check_vt" in self.default_testing_func_names:
            func_names.append("_check_vt_enabled")
        if not self._check_wfc_nonapm():
            self.dut.log.error("WFC test failed before reboot test")
            return False
        return self._reboot_stress_test(*func_names)

    @test_tracker_info(uuid="08752fac-dbdb-4d5b-91f6-4ffc3a3ac6d6")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_modem(self):
        """Crash Recovery Test

        Steps:
            1. Crash modem
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("modem",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="ce5f4d63-7f3d-48b7-831d-2c1d5db60733")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_crash_modem_from_modem(self):
        """Crash Recovery Test

        Steps:
            1. Crash modem
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        if (not self.dut.is_apk_installed("com.google.mdstest")) or (
                self.dut.adb.getprop("ro.build.version.release")[0] in
            ("8", "O", "7", "N")) or self.dut.model in ("angler", "bullhead",
                                                        "sailfish", "marlin"):
            raise signals.TestSkip(
                "com.google.mdstest not installed or supported")
        return self._crash_recovery_test("modem-crash",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="489284e8-77c9-4961-97c8-b6f1a833ff90")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_rild(self):
        """Crash Recovery Test

        Steps:
            1. Crash rild
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("rild",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="e1b34b2c-99e6-4966-a11c-88cedc953b47")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_netmgrd(self):
        """Crash Recovery Test

        Steps:
            1. Crash netmgrd
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("netmgrd",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="6d6908b7-7eca-42e3-b165-2621714f1822")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_qtidataservice(self):
        """Crash Recovery Test

        Steps:
            1. Crash qtidataservice
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("qtidataservice",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="fa34f994-bc49-4444-9187-87691c94b4f4")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_phone(self):
        """Crash Recovery Test

        Steps:
            1. Crash com.android.phone
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("com.android.phone",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="6f5a24bb-3cf3-4362-9675-36a6be90282f")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_imsqmidaemon(self):
        """Crash Recovery Test

        Steps:
            1. Crash imsqmidaemon
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("imsqmidaemon",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="7a8dc971-054b-47e7-9e57-3bb7b39937d3")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_imsdatadaemon(self):
        """Crash Recovery Test

        Steps:
            1. Crash imsdatadaemon
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("imsdatadaemon",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="350ca58c-01f2-4a61-baff-530b8b24f1f6")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_ims_rtp_daemon(self):
        """Crash Recovery Test

        Steps:
            1. Crash imsdatadaemon
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_WFC not in self.dut_capabilities:
            raise signals.TestSkip("WFC is not supported")
        if WFC_MODE_WIFI_PREFERRED in self.dut_wfc_modes:
            self._check_wfc_nonapm()
        else:
            self._check_wfc_apm()
        return self._crash_recovery_test("ims_rtp_daemon",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="af78f33a-2b50-4c55-a302-3701b655c557")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_ims_rcsservice(self):
        """Crash Recovery Test

        Steps:
            1. Crash imsdatadaemon
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("com.android.ims.rcsservice",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="8119aeef-84ba-415c-88ea-6eba35bd91fd")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_system_server(self):
        """Crash Recovery Test

        Steps:
            1. Crash system_server
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("system_server",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="c3891aca-9e1a-4e37-9f2f-23f12ef0a86f")
    @TelephonyBaseTest.tel_test_wrap
    def test_crash_recovery_cnd(self):
        """Crash Recovery Test

        Steps:
            1. Crash cnd
            2. Post crash recovery, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        return self._crash_recovery_test("cnd",
                                         *self.default_testing_func_names)

    @test_tracker_info(uuid="c1b661b9-d5cf-4a22-90a9-3fd55ddc2f3f")
    @TelephonyBaseTest.tel_test_wrap
    def test_sim_slot_power_cycle(self):
        """SIM slot power cycle Test

        Steps:
            1. Power cycle SIM slot to simulate SIM resit
            2. Post power cycle SIM, verify Voice, Data, SMS, VoLTE, VT

        Expected Results:
            No crash happens in functional test, features work fine.

        Returns:
            True is pass, False if fail.
        """
        if self.dut.adb.getprop("ro.build.version.release")[0] in (
                "8", "O", "7", "N") or self.dut.model in ("angler", "bullhead",
                                                          "marlin",
                                                          "sailfish"):
            raise signals.TestSkip("Power off SIM is not supported")
        return self._crash_recovery_test("sim",
                                         *self.default_testing_func_names)


""" Tests End """
