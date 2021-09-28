#!/usr/bin/env python3.4
#
#   Copyright 2020 - Google
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
    Test Script for Telephony Factory Data Reset In Sanity
"""

import collections
import time

from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_data_utils import wifi_tethering_setup_teardown
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import CAPABILITY_OMADM
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_TETHERING_ENTITLEMENT_CHECK
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import TETHERING_MODE_WIFI
from acts.test_utils.tel.tel_defines import WAIT_TIME_AFTER_FDR
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import get_model_name
from acts.test_utils.tel.tel_test_utils import get_outgoing_voice_sub_id
from acts.test_utils.tel.tel_test_utils import is_droid_in_network_generation
from acts.test_utils.tel.tel_test_utils import mms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import sms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import wait_for_network_generation
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import wait_for_state
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_idle_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte

from acts.utils import get_current_epoch_time
from acts.utils import rand_ascii_str


class TelLiveStressFdrTest(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)

        self.stress_test_number = int(
            self.user_params.get("stress_test_number", 100))
        self.skip_reset_between_cases = False

        self.dut = self.android_devices[0]
        if len(self.android_devices) > 1:
            self.ad_reference = self.android_devices[1]
        else:
            raise signals.TestSkip("Single DUT is not supported")
        self.user_params["check_crash"] = False
        self.skip_reset_between_cases = False

        # self.dut_capabilities = self.dut.telephony.get("capabilities", [])
        sub_id = get_outgoing_voice_sub_id(self.dut)
        self.dut_capabilities = self.dut.telephony["subscription"][sub_id].get("capabilities", [])
        self.dut.log.info("dut_capabilitiese: %s", self.dut_capabilities)

        self.dut_wfc_modes = self.dut.telephony.get("wfc_modes", [])
        self.default_testing_func_names = []
        for method in ("_check_volte", "_check_csfb",
                       "_check_tethering", "_check_3g"):
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
        for method in ("_check_subscription", "_check_data", "_check_mms_mt",
                       "_check_sms_mt", "_check_call_setup_teardown",
                       "_check_sms", "_check_mms"):
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

    def _set_volte_provisioning(self):
        if CAPABILITY_OMADM in self.dut_capabilities:
            provisioned = self.dut.droid.imsIsVolteProvisionedOnDevice()
            if provisioned:
                self.dut.log.info("Volte is provioned")
                return
            self.dut.log.info("VoLTE provisioning is not set, setting\
                VoLTE provision")
            self.dut.droid.imsSetVolteProvisioning(True)

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
        for retries in range(3):
            try:
                if not self.dut.droid.carrierConfigIsTetheringModeAllowed(
                        TETHERING_MODE_WIFI,
                        MAX_WAIT_TIME_TETHERING_ENTITLEMENT_CHECK):
                    self.log.error("Tethering Entitlement check failed.")
                    if retries == 2: return False
                    time.sleep(10)
            except Exception as e:
                if retries == 2:
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

    def _fdr_stress_test(self, *args):
        """Fdr Reliability Test

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
            test_msg = "FDR Stress Test %s Iteration <%s> / <%s>" % (
                self.test_name, i, self.stress_test_number)
            self.log.info(test_msg)
            fastboot_wipe(self.dut)
            self.log.info("%s wait %s secs for radio up.",
                self.dut.serial, WAIT_TIME_AFTER_FDR)
            time.sleep(WAIT_TIME_AFTER_FDR)
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


    """ Tests Begin """


    @test_tracker_info(uuid="a09a5d44-ffcc-4890-b0ba-a30a75e05fbd")
    @TelephonyBaseTest.tel_test_wrap
    def test_fdr_stress_volte(self):
        """FDR with VoLTE Test

        Steps:
            1. Fdr DUT.
            2. Wait for DUT to camp on LTE, Verify Data.
            3. Check VoLTE is enabled by default, check IMS registration.
               Wait for DUT report VoLTE enabled, make VoLTE call.
               And verify VoLTE SMS. (if support VoLTE)
            4. Check crashes.
            5. Repeat Step 1~4 for N times.

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        if CAPABILITY_VOLTE not in self.dut_capabilities:
            raise signals.TestSkip("VOLTE is not supported")
        if not self._check_volte():
            self.dut.log.error("VoLTE test failed before fdr test")
            return False
        func_names = ["_check_volte_enabled"]
        return self._fdr_stress_test(*func_names)

    """ Tests End """
