#!/usr/bin/env python3.4
#
#   Copyright 2019 - Google
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
    Test Script for CBRS devices
"""

import time
import collections

from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_ORIGINATED
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_TERMINATED
from acts.test_utils.tel.tel_defines import WAIT_TIME_BETWEEN_REG_AND_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_IN_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_CBRS_DATA_SWITCH
from acts.test_utils.tel.tel_defines import EventActiveDataSubIdChanged
from acts.test_utils.tel.tel_defines import NetworkCallbackAvailable
from acts.test_utils.tel.tel_defines import EventNetworkCallback
from acts.test_utils.tel.tel_test_utils import get_phone_number
from acts.test_utils.tel.tel_test_utils import hangup_call
from acts.test_utils.tel.tel_test_utils import hangup_call_by_adb
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import is_phone_not_in_call
from acts.test_utils.tel.tel_test_utils import wait_and_answer_call
from acts.test_utils.tel.tel_test_utils import is_phone_in_call
from acts.test_utils.tel.tel_test_utils import start_qxdm_loggers
from acts.test_utils.tel.tel_test_utils import test_data_browsing_success_using_sl4a
from acts.test_utils.tel.tel_test_utils import test_data_browsing_failure_using_sl4a
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import is_current_data_on_cbrs
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.tel.tel_test_utils import STORY_LINE
from acts.test_utils.tel.tel_test_utils import get_device_epoch_time
from acts.test_utils.tel.tel_test_utils import start_qxdm_logger
from acts.test_utils.tel.tel_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_2g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_not_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_general
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte_for_subscription
from acts.test_utils.tel.tel_voice_utils import phone_setup_cdma
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from acts.test_utils.tel.tel_voice_utils import phone_idle_3g
from acts.test_utils.tel.tel_voice_utils import phone_idle_2g
from acts.test_utils.tel.tel_voice_utils import phone_idle_csfb
from acts.test_utils.tel.tel_voice_utils import phone_idle_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_idle_not_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_idle_volte
from acts.test_utils.tel.tel_subscription_utils import get_subid_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_operatorname_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_cbrs_and_default_sub_id
from acts.utils import get_current_epoch_time
from queue import Empty

WAIT_TIME_BETWEEN_ITERATION = 5
WAIT_TIME_BETWEEN_HANDOVER = 10
TIME_PERMITTED_FOR_CBRS_SWITCH = 2

class TelLiveCBRSTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.number_of_devices = 2
        self.stress_test_number = self.get_stress_test_number()
        self.message_lengths = (50, 160, 180)
        self.long_message_lengths = (800, 1600)
        self.cbrs_subid = None
        self.default_subid = None
        self.switch_time_dict = {}
        self.stress_test_number = int(
            self.user_params.get("stress_test_number", 5))


    def on_fail(self, test_name, begin_time):
        if test_name.startswith('test_stress'):
            return
        super().on_fail(test_name, begin_time)


    def _cbrs_call_sequence(self, ads, mo_mt,
                            cbrs_phone_setup_func,
                            verify_cbrs_initial_idle_func,
                            verify_data_initial_func,
                            verify_cbrs_in_call_state_func,
                            verify_data_in_call_func,
                            incall_cbrs_setting_check_func,
                            verify_data_final_func,
                            verify_cbrs_final_func,
                            expected_result):
        """_cbrs_call_sequence

        Args:
            ads: list of android devices. This list should have 2 ad.
            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.

        Returns:
            if expected_result is True,
                Return True if call sequence finish without exception.
            if expected_result is string,
                Return True if expected exception happened. Otherwise False.

        """

        class _CBRSCallSequenceException(Exception):
            pass

        if (len(ads) != 2) or (mo_mt not in [
                DIRECTION_MOBILE_ORIGINATED, DIRECTION_MOBILE_TERMINATED
        ]):
            self.log.error("Invalid parameters.")
            return False

        self.cbrs_subid, self.default_subid = get_cbrs_and_default_sub_id(ads[0])

        if mo_mt == DIRECTION_MOBILE_ORIGINATED:
            ad_caller = ads[0]
            ad_callee = ads[1]
            caller_number = get_phone_number(self.log, ad_caller)
            callee_number = get_phone_number(self.log, ad_callee)
            mo_operator = get_operator_name(ads[0].log, ads[0])
            mt_operator = get_operator_name(ads[1].log, ads[1])
        else:
            ad_caller = ads[1]
            ad_callee = ads[0]
            caller_number = get_phone_number(self.log, ad_caller)
            callee_number = get_phone_number(self.log, ad_callee)
            mt_operator = get_operator_name(ads[0].log, ads[0])
            mo_operator = get_operator_name(ads[1].log, ads[1])

        self.log.info("-->Begin cbrs_call_sequence: %s to %s<--",
                      caller_number, callee_number)
        self.log.info("--> %s to %s <--", mo_operator, mt_operator)

        try:
            # Setup
            if cbrs_phone_setup_func and not cbrs_phone_setup_func():
                raise _CBRSCallSequenceException("cbrs_phone_setup_func fail.")
            if not phone_setup_voice_general(self.log, ads[1]):
                raise _CBRSCallSequenceException(
                    "phone_setup_voice_general fail.")
            time.sleep(WAIT_TIME_BETWEEN_REG_AND_CALL)

            # Ensure idle status correct
            if verify_cbrs_initial_idle_func and not \
                verify_cbrs_initial_idle_func():
                raise _CBRSCallSequenceException(
                    "verify_cbrs_initial_idle_func fail.")

            # Ensure data checks are performed
            if verify_data_initial_func and not \
                verify_data_initial_func():
                raise _CBRSCallSequenceException(
                    "verify_data_initial_func fail.")

            # Make MO/MT call.
            if not initiate_call(self.log, ad_caller, callee_number):
                raise _CBRSCallSequenceException("initiate_call fail.")
            if not wait_and_answer_call(self.log, ad_callee, caller_number):
                raise _CBRSCallSequenceException("wait_and_answer_call fail.")
            time.sleep(WAIT_TIME_FOR_CBRS_DATA_SWITCH)

            # Check state, wait 30 seconds, check again.
            if (verify_cbrs_in_call_state_func and not
                    verify_cbrs_in_call_state_func()):
                raise _CBRSCallSequenceException(
                    "verify_cbrs_in_call_state_func fail.")

            if is_phone_not_in_call(self.log, ads[1]):
                raise _CBRSCallSequenceException("PhoneB not in call.")

            # Ensure data checks are performed
            if verify_data_in_call_func and not \
                verify_data_in_call_func():
                raise _CBRSCallSequenceException(
                    "verify_data_in_call_func fail.")

            time.sleep(WAIT_TIME_IN_CALL)

            if (verify_cbrs_in_call_state_func and not
                    verify_cbrs_in_call_state_func()):
                raise _CBRSCallSequenceException(
                    "verify_cbrs_in_call_state_func fail after 30 seconds.")
            if is_phone_not_in_call(self.log, ads[1]):
                raise _CBRSCallSequenceException(
                    "PhoneB not in call after 30 seconds.")

            # in call change setting and check
            if (incall_cbrs_setting_check_func and not
                    incall_cbrs_setting_check_func()):
                raise _CBRSCallSequenceException(
                    "incall_cbrs_setting_check_func fail.")

            # Hangup call
            if is_phone_in_call(self.log, ads[0]):
                if not hangup_call(self.log, ads[0]):
                    raise _CBRSCallSequenceException("hangup_call fail.")
            else:
                if incall_cbrs_setting_check_func is None:
                    raise _CBRSCallSequenceException("Unexpected call drop.")

            time.sleep(WAIT_TIME_FOR_CBRS_DATA_SWITCH)

            # Ensure data checks are performed
            if verify_data_final_func and not \
                verify_data_final_func():
                raise _CBRSCallSequenceException(
                    "verify_data_final_func fail.")

            # Ensure data checks are performed
            if verify_cbrs_final_func and not \
                verify_cbrs_final_func():
                raise _CBRSCallSequenceException(
                    "verify_cbrs_final_func fail.")

        except _CBRSCallSequenceException as e:
            if str(e) == expected_result:
                self.log.info("Expected exception: <%s>, return True.", e)
                return True
            else:
                self.log.info("Unexpected exception: <%s>, return False.", e)
                return False
        finally:
            for ad in ads:
                if ad.droid.telecomIsInCall():
                    hangup_call_by_adb(ad)
        self.log.info("cbrs_call_sequence finished, return %s",
                      expected_result is True)
        return (expected_result is True)


    def _phone_idle_iwlan(self):
        return phone_idle_iwlan(self.log, self.android_devices[0])

    def _phone_idle_not_iwlan(self):
        return phone_idle_not_iwlan(self.log, self.android_devices[0])

    def _phone_idle_volte(self):
        return phone_idle_volte(self.log, self.android_devices[0])

    def _phone_idle_csfb(self):
        return phone_idle_csfb(self.log, self.android_devices[0])

    def _phone_idle_3g(self):
        return phone_idle_3g(self.log, self.android_devices[0])

    def _phone_idle_2g(self):
        return phone_idle_2g(self.log, self.android_devices[0])

    def _is_phone_in_call_iwlan(self):
        return is_phone_in_call_iwlan(self.log, self.android_devices[0])

    def _is_phone_in_call_not_iwlan(self):
        return is_phone_in_call_not_iwlan(self.log, self.android_devices[0])

    def _is_phone_not_in_call(self):
        if is_phone_in_call(self.log, self.android_devices[0]):
            self.log.info("{} in call.".format(self.android_devices[0].serial))
            return False
        self.log.info("{} not in call.".format(self.android_devices[0].serial))
        return True

    def _is_phone_in_call_volte(self):
        return is_phone_in_call_volte(self.log, self.android_devices[0])

    def _is_phone_in_call_3g(self):
        return is_phone_in_call_3g(self.log, self.android_devices[0])

    def _is_phone_in_call_2g(self):
        return is_phone_in_call_2g(self.log, self.android_devices[0])

    def _is_phone_in_call_csfb(self):
        return is_phone_in_call_csfb(self.log, self.android_devices[0])

    def _is_phone_in_call(self):
        return is_phone_in_call(self.log, self.android_devices[0])

    def _phone_setup_voice_general(self):
        return phone_setup_voice_general(self.log, self.android_devices[0])

    def _phone_setup_volte(self):
        return phone_setup_volte_for_subscription(self.log,
                                                  self.android_devices[0],
                                                  self.default_subid)

    def _phone_setup_1x(self):
        return phone_setup_cdma(self.log, self.android_devices[0])

    def _phone_setup_2g(self):
        return phone_setup_voice_2g(self.log, self.android_devices[0])


    def _test_data_browsing_success_using_sl4a(self):
        return test_data_browsing_success_using_sl4a(self.log,
                                                     self.android_devices[0])

    def _test_data_browsing_failure_using_sl4a(self):
        return test_data_browsing_failure_using_sl4a(self.log,
                                                     self.android_devices[0])

    def _is_current_data_on_cbrs(self):
        return is_current_data_on_cbrs(self.android_devices[0],
                                       self.cbrs_subid)

    def _is_current_data_on_default(self):
        return not is_current_data_on_cbrs(self.android_devices[0],
                                           self.cbrs_subid)

    def _get_list_average(self, input_list):
        total_sum = float(sum(input_list))
        total_count = float(len(input_list))
        if input_list == []:
            return False
        return float(total_sum / total_count)


    """ Tests Begin """


    @test_tracker_info(uuid="f7a3db92-2f1b-4131-99bc-b323dbce812c")
    @TelephonyBaseTest.tel_test_wrap
    def test_cbrs_mo_voice_data_concurrency_lte(self):
        """ Test CBRS Data with MO Voice Call on LTE

        PhoneA should be on LTE with VoLTE enabled
        Verify Data Browsing works fine on cbrs before call
        Call from PhoneA to PhoneB, call should succeed
        Verify Data Browsing works fine on pSIM during call
        Terminate call
        Verify Data Browsing works fine on cbrs after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        result = self._cbrs_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED,
            self._phone_setup_volte, self._is_current_data_on_cbrs,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._is_current_data_on_default,
            self._test_data_browsing_success_using_sl4a,
            self._test_data_browsing_success_using_sl4a,
            self._is_current_data_on_cbrs, True)

        self.log.info("CBRS MO Result: %s", result)
        return result


    @test_tracker_info(uuid="17acce7a-de9c-4540-b2d3-2c98367a0b4e")
    @TelephonyBaseTest.tel_test_wrap
    def test_cbrs_mt_voice_data_concurrency_lte(self):
        """ Test CBRS Data with MT Voice Call on LTE

        PhoneA should be on LTE with VoLTE enabled
        Verify Data Browsing works fine on cbrs before call
        Call from PhoneB to PhoneA, call should succeed
        Verify Data Browsing works fine on pSIM during call
        Terminate call
        Verify Data Browsing works fine on cbrs after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        result = self._cbrs_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED,
            self._phone_setup_volte, self._is_current_data_on_cbrs,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._is_current_data_on_default,
            self._test_data_browsing_success_using_sl4a,
            self._test_data_browsing_success_using_sl4a,
            self._is_current_data_on_cbrs, True)

        self.log.info("CBRS MT Result: %s", result)
        return result

    @test_tracker_info(uuid="dc2608fc-b99d-419b-8989-e1f8cdeb04da")
    @TelephonyBaseTest.tel_test_wrap
    def test_cbrs_mo_voice_data_concurrency_1x(self):
        """ Test CBRS Data with MO Voice Call on 3G

        PhoneA should be on UMTS
        Verify Data Browsing works fine on cbrs before call
        Call from PhoneA to PhoneB, call should succeed
        Verify Data Browsing works fine on pSIM during call
        Terminate call
        Verify Data Browsing works fine on cbrs after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        result = self._cbrs_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED,
            self._phone_setup_1x, self._is_current_data_on_cbrs,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_3g,
            self._is_current_data_on_default,
            self._test_data_browsing_failure_using_sl4a,
            self._test_data_browsing_success_using_sl4a,
            self._is_current_data_on_cbrs, True)

        self.log.info("CBRS MO Result: %s", result)
        return result


    @test_tracker_info(uuid="cd3a6613-ca37-43c7-8364-7e4e627ca558")
    @TelephonyBaseTest.tel_test_wrap
    def test_cbrs_mt_voice_data_concurrency_1x(self):
        """ Test CBRS Data with MT Voice Call on 3G

        PhoneA should be on UMTS
        Verify Data Browsing works fine on cbrs before call
        Call from PhoneA to PhoneA, call should succeed
        Verify Data Browsing works fine on pSIM during call
        Terminate call
        Verify Data Browsing works fine on cbrs after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        result = self._cbrs_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED,
            self._phone_setup_1x, self._is_current_data_on_cbrs,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_3g,
            self._is_current_data_on_default,
            self._test_data_browsing_failure_using_sl4a,
            self._test_data_browsing_success_using_sl4a,
            self._is_current_data_on_cbrs, True)

        self.log.info("CBRS MT Result: %s", result)
        return result


    def _test_stress_cbrs(self, mo_mt):
        """ Test CBRS/SSIM VoLTE Stress

            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.

        Returns:
            True if pass; False if fail.
        """
        if (mo_mt not in [DIRECTION_MOBILE_ORIGINATED,
                          DIRECTION_MOBILE_TERMINATED]):
            self.log.error("Invalid parameters.")
            return False
        ads = [self.android_devices[0], self.android_devices[1]]
        total_iteration = self.stress_test_number
        fail_count = collections.defaultdict(int)
        self.cbrs_subid, self.default_subid = get_cbrs_and_default_sub_id(ads[0])
        self.log.info("Total iteration = %d.", total_iteration)
        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "Stress Call Test Iteration: <%s> / <%s>" % (
                i, total_iteration)
            begin_time = get_current_epoch_time()
            self.log.info(msg)
            start_qxdm_loggers(self.log, self.android_devices, begin_time)
            iteration_result = self._cbrs_call_sequence(
                ads, mo_mt,
                self._phone_setup_volte,
                self._is_current_data_on_cbrs,
                self._test_data_browsing_success_using_sl4a,
                self._is_phone_in_call_volte,
                self._is_current_data_on_default,
                self._test_data_browsing_success_using_sl4a,
                self._test_data_browsing_success_using_sl4a,
                self._is_current_data_on_cbrs, True)
            self.log.info("Result: %s", iteration_result)
            if iteration_result:
                self.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
                fail_count["cbrs_fail"] += 1
                self.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
            current_iteration += 1
        test_result = True
        for failure, count in fail_count.items():
            if count:
                self.log.error("%s: %s %s failures in %s iterations",
                               self.test_name, count, failure,
                               total_iteration)
                test_result = False
        return test_result

    @test_tracker_info(uuid="860dc00d-5be5-4cdd-aeb1-a89edfa83342")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_cbrs_mt_calls_lte(self):
        """ Test SSIM to CBRS stress

        Call from PhoneB to PhoneA
        Perform CBRS Data checks
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_cbrs(DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="54366c70-c9cb-4eed-bd1c-a37c83d5c0ae")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_cbrs_mo_calls_lte(self):
        """ Test CBRS to SSIM stress

        Call from PhoneA to PhoneB
        Perform CBRS Data checks
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_cbrs(DIRECTION_MOBILE_ORIGINATED)


    def _cbrs_default_data_switch_timing(self, ad, method, validation=False):
        result = True
        callback_key = None
        if not getattr(ad, "cbrs_droid", None):
            ad.cbrs_droid, ad.cbrs_ed = ad.get_droid()
            ad.cbrs_ed.start()
        else:
            try:
                if not ad.cbrs_droid.is_live:
                    ad.cbrs_droid, ad.cbrs_ed = ad.get_droid()
                    ad.cbrs_ed.start()
                else:
                    ad.cbrs_ed.clear_all_events()
                ad.cbrs_droid.logI("Start test_stress_cbrsdataswitch test")
            except Exception:
                ad.log.info("Create new sl4a session for CBRS")
                ad.cbrs_droid, ad.cbrs_ed = ad.get_droid()
                ad.cbrs_ed.start()
        if validation:
            ad.cbrs_droid.telephonyStartTrackingActiveDataChange()
        else:
            callback_key = ad.cbrs_droid.connectivityRegisterDefaultNetworkCallback()
            ad.cbrs_droid.connectivityNetworkCallbackStartListeningForEvent(
                callback_key, NetworkCallbackAvailable)
        time.sleep(WAIT_TIME_FOR_CBRS_DATA_SWITCH)
        try:
            ad.cbrs_ed.clear_events(EventActiveDataSubIdChanged)
            ad.cbrs_ed.clear_events(EventNetworkCallback)
            initiate_time_before = get_device_epoch_time(ad)
            ad.log.debug("initiate_time_before: %d", initiate_time_before)
            if method == "api":
                ad.log.info("Setting DDS to default sub %s", self.default_subid)
                ad.droid.telephonySetPreferredOpportunisticDataSubscription(
                    2147483647, validation)
            else:
                ad.log.info("Making a Voice Call to %s", STORY_LINE)
                ad.droid.telecomCallNumber(STORY_LINE, False)
            try:
                if validation:
                    events = ad.cbrs_ed.pop_events("(%s)" %
                        (EventActiveDataSubIdChanged),
                        WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                    for event in events:
                        ad.log.info("Got event %s", event["name"])
                        if event["name"] == EventActiveDataSubIdChanged:
                            if event.get("data") and \
                                    event["data"] == self.default_subid:
                                ad.log.info("%s has data switched to: %s sub",
                                            event["name"], event["data"])
                                initiate_time_after = event["time"]
                            break
                else:
                    events = ad.cbrs_ed.pop_events("(%s)" %
                       (EventNetworkCallback), WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                    for event in events:
                        ad.log.info("Got event %s", event["name"])
                        if event["name"] == EventNetworkCallback:
                            if event.get("data") and event["data"].get("networkCallbackEvent"):
                                ad.log.info("%s %s has data switched to: %s sub",
                                            event["name"],
                                            event["data"]["networkCallbackEvent"],
                                            self.default_subid)
                                initiate_time_after = event["time"]
                            break
            except Empty:
                ad.log.error("No %s or %s event for DataSwitch received in %d seconds",
                             EventActiveDataSubIdChanged, EventNetworkCallback,
                             WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                return False
            time_interval = (initiate_time_after - initiate_time_before) / 1000
            self.switch_time_dict['cbrs_default_switch'].append(time_interval)
            if time_interval > TIME_PERMITTED_FOR_CBRS_SWITCH:
                ad.log.error("Time for CBRS->Default - %.2f secs", time_interval)
                result = False
            else:
                ad.log.info("Time for CBRS->Default - %.2f secs", time_interval)
            time.sleep(WAIT_TIME_BETWEEN_HANDOVER)
            ad.cbrs_ed.clear_events(EventActiveDataSubIdChanged)
            ad.cbrs_ed.clear_events(EventNetworkCallback)
            hangup_time_before = get_device_epoch_time(ad)
            ad.log.debug("hangup_time_before: %d", hangup_time_before)
            if method == "api":
                ad.log.info("Setting DDS to cbrs sub %s", self.cbrs_subid)
                ad.droid.telephonySetPreferredOpportunisticDataSubscription(
                    self.cbrs_subid, validation)
            else:
                ad.log.info("Ending Call")
                ad.droid.telecomEndCall()
            try:
                if validation:
                    events = ad.cbrs_ed.pop_events("(%s)" %
                        (EventActiveDataSubIdChanged), WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                    for event in events:
                        ad.log.info("Got event %s", event["name"])
                        if event["name"] == EventActiveDataSubIdChanged:
                            if event.get("data") and \
                                    event["data"] == self.cbrs_subid:
                                ad.log.info("%s has data switched to: %s sub",
                                            event["name"], event["data"])
                                hangup_time_after = event["time"]
                            break
                else:
                    events = ad.cbrs_ed.pop_events("(%s)" %
                        (EventNetworkCallback), WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                    for event in events:
                        ad.log.info("Got event %s", event["name"])
                        if event["name"] == EventNetworkCallback:
                            if event.get("data") and event["data"].get("networkCallbackEvent"):
                                ad.log.info("%s %s has data switched to: %s sub",
                                            event["name"],
                                            event["data"]["networkCallbackEvent"],
                                            self.cbrs_subid)
                                hangup_time_after = event["time"]
                            break
            except Empty:
                ad.log.error("No %s event for DataSwitch received in %d seconds",
                             EventActiveDataSubIdChanged,
                             WAIT_TIME_FOR_CBRS_DATA_SWITCH)
                return False
            time_interval = (hangup_time_after - hangup_time_before) / 1000
            self.switch_time_dict['default_cbrs_switch'].append(time_interval)
            if time_interval > TIME_PERMITTED_FOR_CBRS_SWITCH:
                ad.log.error("Time for Default->CBRS - %.2f secs", time_interval)
                result = False
            else:
                ad.log.info("Time for Default->CBRS - %.2f secs", time_interval)
        except Exception as e:
            self.log.error("Exception error %s", e)
            raise
        finally:
            if validation:
                ad.cbrs_droid.telephonyStopTrackingActiveDataChange()
            elif callback_key:
                ad.cbrs_droid.connectivityNetworkCallbackStopListeningForEvent(
                    callback_key, NetworkCallbackAvailable)
        return result


    def _test_stress_cbrsdataswitch_timing(self, ad, method, validation=False):
        setattr(self, "number_of_devices", 1)
        ad.adb.shell("pm disable com.google.android.apps.scone")
        wifi_toggle_state(self.log, ad, True)
        self.cbrs_subid, self.default_subid = get_cbrs_and_default_sub_id(ad)
        toggle_airplane_mode(ad.log, ad, new_state=False, strict_checking=False)
        if self._is_current_data_on_cbrs():
            ad.log.info("Current Data is on CBRS, proceeding with test")
        else:
            ad.log.error("Current Data not on CBRS, forcing it now..")
            ad.droid.telephonySetPreferredOpportunisticDataSubscription(
                self.cbrs_subid, False)
        ad.droid.telephonyUpdateAvailableNetworks(self.cbrs_subid)
        total_iteration = self.stress_test_number
        fail_count = collections.defaultdict(int)
        self.switch_time_dict = {'cbrs_default_switch':[],
                                 'default_cbrs_switch': []}
        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "Stress CBRS Test Iteration: <%s> / <%s>" % (
                i, total_iteration)
            begin_time = get_current_epoch_time()
            self.log.info(msg)
            start_qxdm_logger(ad, begin_time)
            iteration_result = self._cbrs_default_data_switch_timing(
                ad, method, validation)
            self.log.info("Result: %s", iteration_result)
            if iteration_result:
                self.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
                fail_count["cbrsdataswitch_fail"] += 1
                self.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
            current_iteration += 1
            time.sleep(WAIT_TIME_BETWEEN_ITERATION)
        test_result = True
        for time_task, time_list in self.switch_time_dict.items():
            ad.log.info("%s %s", time_task, time_list)
            avg_time = self._get_list_average(time_list)
            ad.log.info("Average %s for %d iterations = %.2f seconds",
                                time_task, self.stress_test_number, avg_time)
        for failure, count in fail_count.items():
            if count:
                self.log.error("%s: %s %s failures in %s iterations",
                               self.test_name, count, failure,
                               total_iteration)
                test_result = False
        ad.adb.shell("pm enable com.google.android.apps.scone")
        return test_result


    @test_tracker_info(uuid="c66e307b-8456-4a86-af43-d715597ce802")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_cbrsdataswitch_via_api_without_validation(self):
        """ Test CBRS to Default Data Switch Stress Test

        By default, data is expected to be on CBRS
        Using TelephonyManagerAPI, switch data to default
        Measure time to switch
        Using TelephonyManagerAPI, switch data to cbrs
        Measure time to switch
        Switching to be done with validation set to False
        Repeat above steps
        Calculate average time to switch

        Returns:
            True if pass; False if fail.
        """
        ad = self.android_devices[0]
        self._test_stress_cbrsdataswitch_timing(ad, "api", validation=False)


    @test_tracker_info(uuid="a0ae7691-4890-4e94-8a01-b54ba5b6f489")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_cbrsdataswitch_via_api_with_validation(self):
        """ Test CBRS to Default Data Switch Stress Test

        By default, data is expected to be on CBRS
        Using TelephonyManagerAPI, switch data to default
        Measure time to switch
        Using TelephonyManagerAPI, switch data to cbrs
        Measure time to switch
        Switching to be done with validation set to True
        Repeat above steps
        Calculate average time to switch

        Returns:
            True if pass; False if fail.
        """
        ad = self.android_devices[0]
        self._test_stress_cbrsdataswitch_timing(ad, "api", validation=True)


    @test_tracker_info(uuid="1e69b57a-f2f7-42c6-8389-2194356c599c")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_cbrsdataswitch_via_call(self):
        """ Test CBRS to Default Data Switch Stress Test

        By default, data is expected to be on CBRS
        Initiate MO Phone, data will switch to default
        Measure time to switch
        Hangup MO Phone, data will switch to cbrs
        Measure time to switch
        Repeat above steps
        Calculate average time to switch

        Returns:
            True if pass; False if fail.
        """
        ad = self.android_devices[0]
        self._test_stress_cbrsdataswitch_timing(ad, "call")
