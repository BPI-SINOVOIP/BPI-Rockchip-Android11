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
    Test Script for DSDS devices
"""

import time
import random
import collections

from queue import Empty
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_ORIGINATED
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_TERMINATED
from acts.test_utils.tel.tel_defines import GEN_3G
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import INVALID_WIFI_RSSI
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CALL_DROP
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_NW_SELECTION
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_VOICE
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_BACKGROUND
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_FOREGROUND
from acts.test_utils.tel.tel_defines import PRECISE_CALL_STATE_LISTEN_LEVEL_RINGING
from acts.test_utils.tel.tel_defines import RAT_LTE
from acts.test_utils.tel.tel_defines import RAT_IWLAN
from acts.test_utils.tel.tel_defines import RAT_WCDMA
from acts.test_utils.tel.tel_defines import WAIT_TIME_BETWEEN_REG_AND_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_IN_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_WIFI_RSSI_CALIBRATION_SCREEN_ON
from acts.test_utils.tel.tel_defines import WAIT_TIME_WIFI_RSSI_CALIBRATION_WIFI_CONNECTED
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_DISABLED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_ONLY
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_defines import WIFI_WEAK_RSSI_VALUE
from acts.test_utils.tel.tel_defines import EventNetworkCallback
from acts.test_utils.tel.tel_defines import NetworkCallbackAvailable
from acts.test_utils.tel.tel_defines import NetworkCallbackLost
from acts.test_utils.tel.tel_defines import SignalStrengthContainer
from acts.test_utils.tel.tel_defines import WAIT_TIME_CHANGE_DATA_SUB_ID
from acts.test_utils.tel.tel_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_test_utils import ensure_network_generation
from acts.test_utils.tel.tel_test_utils import ensure_phones_default_state
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import get_network_rat
from acts.test_utils.tel.tel_test_utils import get_phone_number
from acts.test_utils.tel.tel_test_utils import get_phone_number_for_subscription
from acts.test_utils.tel.tel_test_utils import hangup_call
from acts.test_utils.tel.tel_test_utils import hangup_call_by_adb
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import is_network_call_back_event_match
from acts.test_utils.tel.tel_test_utils import is_phone_in_call
from acts.test_utils.tel.tel_test_utils import is_phone_not_in_call
from acts.test_utils.tel.tel_test_utils import set_wfc_mode
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.tel.tel_test_utils import toggle_volte
from acts.test_utils.tel.tel_test_utils import wait_and_answer_call
from acts.test_utils.tel.tel_test_utils import wait_for_cell_data_connection
from acts.test_utils.tel.tel_test_utils import wait_for_droid_not_in_call
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_disabled
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_enabled
from acts.test_utils.tel.tel_test_utils import wait_for_wifi_data_connection
from acts.test_utils.tel.tel_test_utils import verify_http_connection
from acts.test_utils.tel.tel_test_utils import get_telephony_signal_strength
from acts.test_utils.tel.tel_test_utils import get_lte_rsrp
from acts.test_utils.tel.tel_test_utils import get_wifi_signal_strength
from acts.test_utils.tel.tel_test_utils import wait_for_state
from acts.test_utils.tel.tel_test_utils import is_phone_in_call
from acts.test_utils.tel.tel_test_utils import start_qxdm_loggers
from acts.test_utils.tel.tel_test_utils import start_qxdm_logger
from acts.test_utils.tel.tel_test_utils import active_file_download_test
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import test_data_browsing_success_using_sl4a
from acts.test_utils.tel.tel_test_utils import test_data_browsing_failure_using_sl4a
from acts.test_utils.tel.tel_test_utils import sms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import mms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_2g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_not_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_general
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from acts.test_utils.tel.tel_voice_utils import phone_idle_3g
from acts.test_utils.tel.tel_voice_utils import phone_idle_3g_for_subscription
from acts.test_utils.tel.tel_voice_utils import phone_idle_2g
from acts.test_utils.tel.tel_voice_utils import phone_idle_csfb
from acts.test_utils.tel.tel_voice_utils import phone_idle_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_idle_not_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_idle_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_2g
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_outgoing_call
from acts.test_utils.tel.tel_subscription_utils import set_incoming_voice_sub_id
from acts.test_utils.tel.tel_subscription_utils import get_subid_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_operatorname_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_default_data_sub_id
from acts.test_utils.tel.tel_subscription_utils import perform_dds_switch
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_data
from acts.test_utils.tel.tel_subscription_utils import set_dds_on_slot_0
from acts.test_utils.tel.tel_subscription_utils import set_dds_on_slot_1
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_message
from acts.test_utils.tel.tel_subscription_utils import set_slways_allow_mms_data
from acts.utils import get_current_epoch_time
from acts.utils import rand_ascii_str


DEFAULT_LONG_DURATION_CALL_TOTAL_DURATION = 1 * 60 * 60  # default value 1 hour
DEFAULT_PING_DURATION = 120  # in seconds


class TelLiveDSDSVoiceTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.number_of_devices = 2
        self.stress_test_number = self.get_stress_test_number()
        self.dds_operator = self.user_params.get("dds_operator", None)
        self.message_lengths = (50, 160, 180)
        self.long_message_lengths = (800, 1600)


    def on_fail(self, test_name, begin_time):
        if test_name.startswith('test_stress'):
            return
        super().on_fail(test_name, begin_time)


    def _msim_call_sequence(self, ads, mo_mt, slot_id,
                            msim_phone_setup_func,
                            verify_msim_initial_idle_func,
                            verify_data_initial_func,
                            verify_msim_in_call_state_func,
                            verify_data_in_call_func,
                            incall_msim_setting_check_func,
                            verify_data_final_func, expected_result):
        """_msim_call_sequence

        Args:
            ads: list of android devices. This list should have 2 ad.
            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.
            slot_id: either 0 or 1

        Returns:
            if expected_result is True,
                Return True if call sequence finish without exception.
            if expected_result is string,
                Return True if expected exception happened. Otherwise False.

        """

        class _MSIMCallSequenceException(Exception):
            pass

        if (len(ads) != 2) or (mo_mt not in [
                DIRECTION_MOBILE_ORIGINATED, DIRECTION_MOBILE_TERMINATED
        ]):
            self.log.error("Invalid parameters.")
            return False

        sub_id = get_subid_from_slot_index(ads[0].log, ads[0], slot_id)
        if mo_mt == DIRECTION_MOBILE_ORIGINATED:
            ad_caller = ads[0]
            ad_callee = ads[1]
            set_subid_for_outgoing_call(ads[0], sub_id)
            caller_number = get_phone_number(self.log, ad_caller)
            callee_number = get_phone_number(self.log, ad_callee)
            mo_operator = get_operatorname_from_slot_index(ads[0], slot_id)
            mt_operator = get_operator_name(ads[1].log, ads[1])
        else:
            ad_caller = ads[1]
            ad_callee = ads[0]
            caller_number = get_phone_number(self.log, ad_caller)
            callee_number = get_phone_number_for_subscription(ads[0].log,
                                                          ads[0], sub_id)
            setattr(ads[0], "incoming_voice_sub_id", sub_id)
            mt_operator = get_operatorname_from_slot_index(ads[0], slot_id)
            mo_operator = get_operator_name(ads[1].log, ads[1])

        self.log.info("-->Begin msim_call_sequence: %s to %s<--",
                      caller_number, callee_number)
        self.log.info("--> %s to %s <--", mo_operator, mt_operator)

        try:
            # Setup
            if msim_phone_setup_func and not msim_phone_setup_func():
                raise _MSIMCallSequenceException("msim_phone_setup_func fail.")
            if not phone_setup_voice_general(self.log, ads[1]):
                raise _MSIMCallSequenceException(
                    "phone_setup_voice_general fail.")
            time.sleep(WAIT_TIME_BETWEEN_REG_AND_CALL)

            # Ensure idle status correct
            if verify_msim_initial_idle_func and not \
                verify_msim_initial_idle_func():
                raise _MSIMCallSequenceException(
                    "verify_msim_initial_idle_func fail.")

            # Ensure data checks are performed
            if verify_data_initial_func and not \
                verify_data_initial_func():
                raise _MSIMCallSequenceException(
                    "verify_data_initial_func fail.")

            # Make MO/MT call.
            if not initiate_call(self.log, ad_caller, callee_number):
                raise _MSIMCallSequenceException("initiate_call fail.")
            if not wait_and_answer_call(self.log, ad_callee, caller_number):
                raise _MSIMCallSequenceException("wait_and_answer_call fail.")
            time.sleep(1)

            # Check state, wait 30 seconds, check again.
            if (verify_msim_in_call_state_func and not
                    verify_msim_in_call_state_func()):
                raise _MSIMCallSequenceException(
                    "verify_msim_in_call_state_func fail.")

            if is_phone_not_in_call(self.log, ads[1]):
                raise _MSIMCallSequenceException("PhoneB not in call.")

            # Ensure data checks are performed
            if verify_data_in_call_func and not \
                verify_data_in_call_func():
                raise _MSIMCallSequenceException(
                    "verify_data_in_call_func fail.")

            time.sleep(WAIT_TIME_IN_CALL)

            if (verify_msim_in_call_state_func and not
                    verify_msim_in_call_state_func()):
                raise _MSIMCallSequenceException(
                    "verify_msim_in_call_state_func fail after 30 seconds.")
            if is_phone_not_in_call(self.log, ads[1]):
                raise _MSIMCallSequenceException(
                    "PhoneB not in call after 30 seconds.")

            # in call change setting and check
            if (incall_msim_setting_check_func and not
                    incall_msim_setting_check_func()):
                raise _MSIMCallSequenceException(
                    "incall_msim_setting_check_func fail.")

            # Hangup call
            if is_phone_in_call(self.log, ads[0]):
                if not hangup_call(self.log, ads[0]):
                    raise _MSIMCallSequenceException("hangup_call fail.")
            else:
                if incall_msim_setting_check_func is None:
                    raise _MSIMCallSequenceException("Unexpected call drop.")

            # Ensure data checks are performed
            if verify_data_final_func and not \
                verify_data_final_func():
                raise _MSIMCallSequenceException(
                    "verify_data_final_func fail.")

        except _MSIMCallSequenceException as e:
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
        self.log.info("msim_call_sequence finished, return %s",
                      expected_result is True)
        return (expected_result is True)


    def _msim_sms_sequence(self, ads, mo_mt, slot_id,
                           msim_phone_setup_func,
                           verify_actual_messaging_test, expected_result):
        """_msim_call_sequence

        Args:
            ads: list of android devices. This list should have 2 ad.
            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.
            slot_id: either 0 or 1

        Returns:
            if expected_result is True,
                Return True if call sequence finish without exception.
            if expected_result is string,
                Return True if expected exception happened. Otherwise False.

        """

        class _MSIMSmsSequenceException(Exception):
            pass

        if (len(ads) != 2) or (mo_mt not in [
                DIRECTION_MOBILE_ORIGINATED, DIRECTION_MOBILE_TERMINATED
        ]):
            self.log.error("Invalid parameters.")
            return False

        sub_id = get_subid_from_slot_index(ads[0].log, ads[0], slot_id)
        set_slways_allow_mms_data(ads[0], sub_id)
        if mo_mt == DIRECTION_MOBILE_ORIGINATED:
            ad_sender = ads[0]
            ad_receiver = ads[1]
            set_subid_for_message(ads[0], sub_id)
            sender_number = get_phone_number(self.log, ad_sender)
            receiver_number = get_phone_number(self.log, ad_receiver)
            mo_operator = get_operatorname_from_slot_index(ads[0], slot_id)
            mt_operator = get_operator_name(ads[1].log, ads[1])
        else:
            ad_sender = ads[1]
            ad_receiver = ads[0]
            sender_number = get_phone_number(self.log, ad_sender)
            receiver_number = get_phone_number_for_subscription(ads[0].log,
                                                          ads[0], sub_id)
            setattr(ads[0], "incoming_message_sub_id", sub_id)
            mt_operator = get_operatorname_from_slot_index(ads[0], slot_id)
            mo_operator = get_operator_name(ads[1].log, ads[1])

        self.log.info("-->Begin msim_sms_sequence: %s to %s<--",
                      sender_number, receiver_number)
        self.log.info("--> %s to %s <--", mo_operator, mt_operator)

        try:
            # Setup
            if msim_phone_setup_func and not msim_phone_setup_func():
                raise _MSIMSmsSequenceException("msim_phone_setup_func fail.")
            if not phone_setup_voice_general(self.log, ads[1]):
                raise _MSIMSmsSequenceException(
                    "phone_setup_voice_general fail.")

            time.sleep(WAIT_TIME_BETWEEN_REG_AND_CALL)

            # Send MO/MT sms.
            if not verify_actual_messaging_test():
                raise _MSIMSmsSequenceException("sms_test fail.")

            time.sleep(1)

        except _MSIMSmsSequenceException as e:
            if str(e) == expected_result:
                self.log.info("Expected exception: <%s>, return True.", e)
                return True
            else:
                self.log.info("Unexpected exception: <%s>, return False.", e)
                return False

        self.log.info("msim_sms_sequence finished, return %s",
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

    def _phone_idle_3g_slot0(self):
        sub_id = get_subid_from_slot_index(self.log, self.android_devices[0], 0)
        return phone_idle_3g_for_subscription(self.log,
                                              self.android_devices[0], sub_id)

    def _phone_idle_3g_slot1(self):
        sub_id = get_subid_from_slot_index(self.log, self.android_devices[0], 1)
        return phone_idle_3g_for_subscription(self.log,
                                              self.android_devices[0], sub_id)
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
        return phone_setup_volte(self.log, self.android_devices[0])

    def _phone_setup_3g(self):
        return phone_setup_voice_3g(self.log, self.android_devices[0])

    def _phone_setup_2g(self):
        return phone_setup_voice_2g(self.log, self.android_devices[0])

    def _set_dds_on_slot_0(self):
        return set_dds_on_slot_0(self.android_devices[0])

    def _set_dds_on_slot_1(self):
        return set_dds_on_slot_1(self.android_devices[0])

    def _test_data_browsing_success_using_sl4a(self):
        return test_data_browsing_success_using_sl4a(self.log,
                                                     self.android_devices[0])

    def _test_data_browsing_failure_using_sl4a(self):
        return test_data_browsing_failure_using_sl4a(self.log,
                                                     self.android_devices[0])

    def _sms_test_msim_mo(self):
        """Test SMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        ad_sender = self.android_devices[0]
        ad_receiver = self.android_devices[1]
        for length in self.message_lengths:
            message_array = [rand_ascii_str(length)]
            if not sms_send_receive_verify(self.log, ad_sender, ad_receiver,
                                           message_array):
                ad_sender.log.warning("SMS of length %s test failed", length)
                return False
            else:
                ad_sender.log.info("SMS of length %s test succeeded", length)
        self.log.info("SMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True


    def _sms_test_msim_mt(self):
        """Test SMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        ad_sender = self.android_devices[1]
        ad_receiver = self.android_devices[0]
        for length in self.message_lengths:
            message_array = [rand_ascii_str(length)]
            if not sms_send_receive_verify(self.log, ad_sender, ad_receiver,
                                           message_array):
                ad_sender.log.warning("SMS of length %s test failed", length)
                return False
            else:
                ad_sender.log.info("SMS of length %s test succeeded", length)
        self.log.info("SMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True


    def _mms_test_msim_mo(self):
        """Test MMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        ad_sender = self.android_devices[0]
        ad_receiver = self.android_devices[1]
        for length in self.message_lengths:
            message_array = [("Test Message", rand_ascii_str(length), None)]
            if not mms_send_receive_verify(self.log, ad_sender, ad_receiver,
                                           message_array):
                ad_sender.log.warning("MMS of length %s test failed", length)
                return False
            else:
                ad_sender.log.info("MMS of length %s test succeeded", length)
        self.log.info("MMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True


    def _mms_test_msim_mt(self):
        """Test MMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        ad_sender = self.android_devices[1]
        ad_receiver = self.android_devices[0]
        for length in self.message_lengths:
            message_array = [("Test Message", rand_ascii_str(length), None)]
            if not mms_send_receive_verify(self.log, ad_sender, ad_receiver,
                                           message_array):
                ad_sender.log.warning("MMS of length %s test failed", length)
                return False
            else:
                ad_sender.log.info("MMS of length %s test succeeded", length)
        self.log.info("MMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True


    """ Tests Begin """


    @test_tracker_info(uuid="3af77c9e-b3bf-438f-bbce-97977a454402")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_concurrency_with_dds_volte(self):
        """ Test MSIM MO Concurrency with DDS

        Make Sure DDS is on same slot as Voice
        Verify Data Browsing works fine before call
        Call from PhoneA to PhoneB, call should succeed
        Verify Data Browsing works fine during call
        Terminate call
        Verify Data Browsing works fine after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_volte, self._set_dds_on_slot_0,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_success_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_volte, self._set_dds_on_slot_1,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_success_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="110af62d-fc08-42d4-ae52-5985755bff35")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_concurrency_without_dds_volte(self):
        """ Test MSIM MO Concurrency without DDS

        Make Sure DDS is NOT on same slot as Voice
        Verify Data Browsing works fine before call
        Call from PhoneA to PhoneB, call should succeed
        Verify Data Browsing fails during call
        Terminate call
        Verify Data Browsing works fine after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_volte, self._set_dds_on_slot_1,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_failure_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_volte, self._set_dds_on_slot_0,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_failure_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="35c90533-8e10-4d2b-af30-fe54aec380f2")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_concurrency_with_dds_volte(self):
        """ Test MSIM MT Concurrency with DDS

        Make Sure DDS is on same slot as Voice
        Verify Data Browsing works fine before call
        Call from PhoneB to PhoneA, call should succeed
        Verify Data Browsing works fine during call
        Terminate call
        Verify Data Browsing works fine after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_volte, self._set_dds_on_slot_0,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_success_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_volte, self._set_dds_on_slot_1,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_success_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="f67fab80-c010-4f73-b225-793d7db2c528")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_concurrency_without_dds_volte(self):
        """ Test MSIM MT Concurrency without DDS

        Make Sure DDS is NOT on same slot as Voice
        Verify Data Browsing works fine before call
        Call from PhoneB to PhoneA, call should succeed
        Verify Data Browsing fails during call
        Terminate call
        Verify Data Browsing works fine after call

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_volte, self._set_dds_on_slot_1,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_failure_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_volte, self._set_dds_on_slot_0,
            self._test_data_browsing_success_using_sl4a,
            self._is_phone_in_call_volte,
            self._test_data_browsing_failure_using_sl4a, None,
            self._test_data_browsing_success_using_sl4a, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="5a3ff3c0-5956-4b18-86a1-61ac60546330")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_voice_general(self):
        """ Test MSIM to SSIM MO Voice General

        Make Sure PhoneA Slot0 is attached to Voice
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneA to PhoneB, call should succeed
        Make Sure PhoneA Slot1 is attached to Voice
        Call from PhoneA to PhoneB, call should succeed

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_voice_general, None, None, self._is_phone_in_call,
            None, None, None, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_voice_general, None, None, self._is_phone_in_call,
            None, None, None, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="5adc17d4-8258-42aa-82ab-0ef92ac7b660")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_to_ssim_voice_general(self):
        """ Test SSIM to MSIM MT Voice General

        Make Sure PhoneA Slot0 is attached to Voice
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneB to PhoneA, call should succeed
        Make Sure PhoneA Slot1 is attached to Voice
        Call from PhoneB to PhoneA, call should succeed

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_voice_general, None, None, self._is_phone_in_call,
            None, None, None, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_voice_general, None, None, self._is_phone_in_call,
            None, None, None, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="3988e411-b9f8-4798-b3a6-dd3bf0d2035c")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_volte(self):
        """ Test MSIM to SSIM MO VoLTE

        Make Sure PhoneA Slot0 is on VoLTE
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneA to PhoneB, call should succeed on VoLTE
        Make Sure PhoneA Slot1 is on VoLTE
        Call from PhoneA to PhoneB, call should succeed on VoLTE

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0, self._phone_setup_volte,
            self._phone_idle_volte, None, self._is_phone_in_call_volte, None,
            None, None, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1, self._phone_setup_volte,
            self._phone_idle_volte, None, self._is_phone_in_call_volte, None,
            None, None, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="4fe74a11-6f13-4305-becd-ad77e13a3193")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_to_ssim_volte(self):
        """ Test SSIM to MSIM MT VoLTE

        Make Sure PhoneA Slot0 is on VoLTE
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneB to PhoneA, call should succeed on VoLTE
        Make Sure PhoneA Slot1 is on VoLTE
        Call from PhoneB to PhoneA, call should succeed on VoLTE

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0, self._phone_setup_volte,
            self._phone_idle_volte, None, self._is_phone_in_call_volte, None,
            None, None, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1, self._phone_setup_volte,
            self._phone_idle_volte, None, self._is_phone_in_call_volte, None,
            None, None, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="ffc7a414-8ec3-4bf4-a509-0bec59f2a3b9")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_3g(self):
        """ Test MSIM to SSIM MO 3G

        Make Sure PhoneA Slot0 is on 3G
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneA to PhoneB, call should succeed on 3G
        Make Sure PhoneA Slot1 is on 3G
        Call from PhoneA to PhoneB, call should succeed on 3G

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0, self._phone_setup_3g,
            self._phone_idle_3g, None, self._is_phone_in_call_3g, None, None,
            None, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1, self._phone_setup_3g,
            self._phone_idle_3g, None, self._is_phone_in_call_3g, None, None,
            None, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="21221072-01f2-4951-ad55-fe1f4fe1b044")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_to_ssim_3g(self):
        """ Test SSIM to MSIM MT 3G

        Make Sure PhoneA Slot0 is on 3G
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneB to PhoneA, call should succeed on 3G
        Make Sure PhoneA Slot1 is on 3G
        Call from PhoneB to PhoneA, call should succeed on 3G

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0, self._phone_setup_3g,
            self._phone_idle_3g_slot0, None, self._is_phone_in_call_3g, None, None,
            None, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1, self._phone_setup_3g,
            self._phone_idle_3g_slot1, None, self._is_phone_in_call_3g, None, None,
            None, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="65448385-42fe-429c-ad8e-6c75be93f83e")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_2g(self):
        """ Test MSIM to SSIM MO 2G

        Make Sure PhoneA Slot0 is on 2G
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneA to PhoneB, call should succeed on 2G
        Make Sure PhoneA Slot1 is on 2G
        Call from PhoneA to PhoneB, call should succeed on 2G

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0, self._phone_setup_2g,
            self._phone_idle_2g, None, self._is_phone_in_call_2g, None, None,
            None, True)

        mo_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1, self._phone_setup_2g,
            self._phone_idle_2g, None, self._is_phone_in_call_2g, None, None,
            None, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="44d1ef0a-c981-4db1-958c-0af06eb0c503")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_to_ssim_2g(self):
        """ Test SSIM to MSIM MT 2G

        Make Sure PhoneA Slot0 is on 2G
        Make Sure PhoneB is able to make MO/MT call
        Call from PhoneB to PhoneA, call should succeed on 2G
        Make Sure PhoneA Slot1 is on 2G
        Call from PhoneB to PhoneA, call should succeed on 2G

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0, self._phone_setup_2g,
            self._phone_idle_2g, None, self._is_phone_in_call_2g, None, None,
            None, True)

        mt_result_1 = self._msim_call_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1, self._phone_setup_2g,
            self._phone_idle_2g, None, self._is_phone_in_call_2g, None, None,
            None, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="e982c5ea-63f6-43dd-8269-3932eb79136d")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_sms_general(self):
        """ Test MSIM MO SMS on both slots

        Airplane mode is off. Phone in default state.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_voice_general, self._sms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_voice_general, self._sms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="f4dc44c5-8edf-493f-91ca-8998e9c1bfc7")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_sms_lte(self):
        """ Test MSIM MO SMS on both slots

        Airplane mode is off. Phone in 4G.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_volte, self._sms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_volte, self._sms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="c49ddf5d-eb86-4884-adb5-03d6166e9b2e")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_sms_3g(self):
        """ Test MSIM MO SMS on both slots

        Airplane mode is off. Phone in 3G.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_3g, self._sms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_3g, self._sms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))

    @test_tracker_info(uuid="27b0f53f-86e9-4608-820c-e9756906c9fd")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_sms_2g(self):
        """ Test MSIM MO SMS on both slots

        Airplane mode is off. Phone in 2G.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_2g, self._sms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_2g, self._sms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="daac5b32-faf2-411c-bcab-720bcb92b7be")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_sms_general(self):
        """ Test SSIM to MSIM MT SMS

        Airplane mode is off. Phone in default state.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_voice_general, self._sms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_voice_general, self._sms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="2948c245-e1ff-4612-bed0-31c07eb878be")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_sms_lte(self):
        """ Test SSIM to MSIM MT SMS

        Airplane mode is off. Phone in LTE state.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_volte, self._sms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_volte, self._sms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="b4df4acd-8122-4af1-ae89-e0c32d7c5e5f")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_sms_3g(self):
        """ Test SSIM to MSIM MT SMS

        Airplane mode is off. Phone in 3G state.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_3g, self._sms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_3g, self._sms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="7b6febe4-97c2-4e98-b656-fc2981ccc8a7")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_sms_2g(self):
        """ Test SSIM to MSIM MT SMS

        Airplane mode is off. Phone in 2G state.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_2g, self._sms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_2g, self._sms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="1aeb2b27-fbd5-4f58-9b5f-21e10e1e577c")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_mms_general(self):
        """ Test MSIM MO MMS on both slots

        Airplane mode is off. Phone in default state.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_voice_general, self._mms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_voice_general, self._mms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="c3f3cd1a-6314-4bb4-bf8b-37bc4360dfa3")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_mms_lte(self):
        """ Test MSIM MO MMS on both slots

        Airplane mode is off. Phone in 4G.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_volte, self._mms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_volte, self._mms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="c99d3ee5-394e-4230-aae7-0f092b8bde84")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_mms_3g(self):
        """ Test MSIM MO MMS on both slots

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_3g, self._mms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_3g, self._mms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="54d5de46-9691-4b23-bcab-9ac5f96841b9")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mo_to_ssim_mms_2g(self):
        """ Test MSIM MO MMS on both slots

        Airplane mode is off. Phone in 2G.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mo_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 0,
            self._phone_setup_2g, self._mms_test_msim_mo, True)

        mo_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_ORIGINATED, 1,
            self._phone_setup_2g, self._mms_test_msim_mo, True)

        self.log.info("MO Slot0: %s, MO Slot1: %s", mo_result_0, mo_result_1)
        return ((mo_result_0 is True) and (mo_result_1 is True))


    @test_tracker_info(uuid="ffcad40f-420d-46ac-affb-f8f559173b0e")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_mms_general(self):
        """ Test MSIM MT MMS on both slots

        Airplane mode is off. Phone in default state.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_voice_general, self._mms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_voice_general, self._mms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="73bfcedf-5725-4f91-a245-aeae5471f75c")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_mms_lte(self):
        """ Test MSIM MT MMS on both slots

        Airplane mode is off. Phone in 4G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_volte, self._mms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_volte, self._mms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="76befdc8-df06-4c50-8f20-d6171c9c91f4")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_mms_3g(self):
        """ Test MSIM MT MMS on both slots

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_3g, self._mms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_3g, self._mms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    @test_tracker_info(uuid="855631c8-30fd-43f5-9a1b-0461b4c4ed86")
    @TelephonyBaseTest.tel_test_wrap
    def test_msim_mt_from_ssim_mms_2g(self):
        """ Test MSIM MT MMS on both slots

        Airplane mode is off. Phone in 2G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        ads = [self.android_devices[0], self.android_devices[1]]
        mt_result_0 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 0,
            self._phone_setup_2g, self._mms_test_msim_mt, True)

        mt_result_1 = self._msim_sms_sequence(
            ads, DIRECTION_MOBILE_TERMINATED, 1,
            self._phone_setup_2g, self._mms_test_msim_mt, True)

        self.log.info("MT Slot0: %s, MT Slot1: %s", mt_result_0, mt_result_1)
        return ((mt_result_0 is True) and (mt_result_1 is True))


    def _test_stress_msim(self, mo_mt, dds_switch=False):
        """ Test MSIM/SSIM Voice General Stress

            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.
            slot_id: either 0 or 1

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
        for i in range(0,2):
            sub_id = get_subid_from_slot_index(ads[0].log, ads[0], i)
            operator = get_operatorname_from_slot_index(ads[0], i)
            self.log.info("Slot %d - Sub %s - %s", i, sub_id, operator)
            if self.dds_operator == operator:
                ads[0].log.info("Setting DDS on %s", operator)
                set_subid_for_data(ads[0], sub_id)
                ads[0].droid.telephonyToggleDataConnection(True)

        self.log.info("Total iteration = %d.", total_iteration)
        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "Stress Call Test Iteration: <%s> / <%s>" % (
                i, total_iteration)
            begin_time = get_current_epoch_time()
            self.log.info(msg)
            start_qxdm_loggers(self.log, self.android_devices, begin_time)

            if dds_switch:
                if not perform_dds_switch(ads[0]):
                    ads[0].log.error("DDS Switch Failed")
                    fail_count["dds_switch"] += 1

            result_0 = self._msim_call_sequence(
                ads, mo_mt, 0,
                self._phone_setup_voice_general, None, None,
                self._is_phone_in_call, None, None, None, True)
            if not result_0:
                fail_count["slot_0"] += 1

            result_1 = self._msim_call_sequence(
                ads, mo_mt, 1,
                self._phone_setup_voice_general, None, None,
                self._is_phone_in_call, None, None, None, True)
            if not result_1:
                fail_count["slot_1"] += 1

            self.log.info("Slot0: %s, Slot1: %s", result_0, result_1)
            iteration_result = ((result_0 is True) and (result_1 is True))
            if iteration_result:
                self.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
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

    @test_tracker_info(uuid="22e3130e-9d46-45e2-a999-36a091acadcf")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mt_calls(self):
        """ Test MSIM to SSIM stress

        Call from PhoneB to PhoneA Slot0
        Call from PhoneB to PhoneA Slot1
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim(DIRECTION_MOBILE_TERMINATED)


    @test_tracker_info(uuid="e7c97ffb-6b1c-4819-9f3c-29b1c87b0ead")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mt_calls(self):
        """ Test MSIM to SSIM stress

        Perform DDS Switch
        Call from PhoneB to PhoneA Slot0
        Call from PhoneB to PhoneA Slot0
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim(DIRECTION_MOBILE_TERMINATED,
                                      dds_switch=True)


    @test_tracker_info(uuid="bc80622a-09fb-48ed-9340-c5de92df1c69")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mo_calls(self):
        """ Test MSIM to SSIM stress

        Call from PhoneA Slot0 to PhoneB
        Call from PhoneA Slot1 to PhoneB
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim(DIRECTION_MOBILE_ORIGINATED)


    @test_tracker_info(uuid="b1af7036-7d91-4f6f-83c9-a763092790b4")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mo_calls(self):
        """ Test MSIM to SSIM stress with DDS

        Switch DDS
        Call from PhoneA Slot0 to PhoneB
        Call from PhoneA Slot1 to PhoneB
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim(DIRECTION_MOBILE_ORIGINATED,
                                      dds_switch=True)


    def _test_stress_msim_sms(self, mo_mt, sms_mms, dds_switch=False):
        """ Test MSIM/SSIM SMS Stress

            mo_mt: indicating this call sequence is MO or MT.
                Valid input: DIRECTION_MOBILE_ORIGINATED and
                DIRECTION_MOBILE_TERMINATED.
            slot_id: either 0 or 1

        Returns:
            True if pass; False if fail.
        """
        if (mo_mt not in [DIRECTION_MOBILE_ORIGINATED,
                          DIRECTION_MOBILE_TERMINATED]):
            self.log.error("Invalid parameters.")
            return False
        ads = [self.android_devices[0], self.android_devices[1]]
        if mo_mt == DIRECTION_MOBILE_ORIGINATED:
            sms_test_func = self._mms_test_msim_mo
            if sms_mms == "sms":
                sms_test_func = self._sms_test_msim_mo
        else:
            sms_test_func = self._mms_test_msim_mt
            if sms_mms == "sms":
                sms_test_func = self._sms_test_msim_mt

        total_iteration = self.stress_test_number
        fail_count = collections.defaultdict(int)
        for i in range(0,2):
            sub_id = get_subid_from_slot_index(ads[0].log, ads[0], i)
            operator = get_operatorname_from_slot_index(ads[0], i)
            self.log.info("Slot %d - Sub %s - %s", i, sub_id, operator)
            if self.dds_operator == operator:
                ads[0].log.info("Setting DDS on %s", operator)
                set_subid_for_data(ads[0], sub_id)
                ads[0].droid.telephonyToggleDataConnection(True)

        self.log.info("Total iteration = %d.", total_iteration)
        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "Stress Call Test Iteration: <%s> / <%s>" % (
                i, total_iteration)
            begin_time = get_current_epoch_time()
            self.log.info(msg)
            start_qxdm_loggers(self.log, self.android_devices, begin_time)

            if dds_switch:
                if not perform_dds_switch(ads[0]):
                    ads[0].log.error("DDS Switch Failed")
                    fail_count["dds_switch"] += 1

            result_0 = self._msim_sms_sequence(
                ads, mo_mt, 0, self._phone_setup_voice_general, sms_test_func,
                True)
            if not result_0:
                fail_count["slot_0"] += 1

            result_1 = self._msim_sms_sequence(
                ads, mo_mt, 1, self._phone_setup_voice_general, sms_test_func,
                True)
            if not result_1:
                fail_count["slot_1"] += 1

            self.log.info("Slot0: %s, Slot1: %s", result_0, result_1)
            iteration_result = ((result_0 is True) and (result_1 is True))
            if iteration_result:
                self.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
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


    @test_tracker_info(uuid="2396723d-4ad1-4a0d-8ee9-98847cf99f34")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mo_sms(self):
        """ Test MSIM to SSIM SMS MO stress

        Airplane mode is off. Phone in default state.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_ORIGINATED, "sms")


    @test_tracker_info(uuid="6b2f0796-dd1c-4e18-9e87-e37bb45f1bba")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mo_sms(self):
        """ Test MSIM to SSIM SMS MO stress

        Airplane mode is off. Phone in default state.
        Switch DDS
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_ORIGINATED, "sms",
                                          dds_switch=True)


    @test_tracker_info(uuid="3b2d6fe8-eb6a-43ff-b3e1-a16a12f108c6")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mo_mms(self):
        """ Test MSIM to SSIM MMS MO stress

        Airplane mode is off. Phone in default state.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_ORIGINATED, "mms")


    @test_tracker_info(uuid="2b2fae63-d2f2-49c8-974f-3af88391904f")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mo_mms(self):
        """ Test MSIM to SSIM MMS MO stress

        Airplane mode is off. Phone in default state.
        Switch DDS
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_ORIGINATED, "mms",
                                          dds_switch=True)

    @test_tracker_info(uuid="71a51e15-ccfa-417d-a3fb-9c6eba214e45")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mt_sms(self):
        """ Test MSIM SMS MT from SSIM stress

        Airplane mode is off. Phone in default state.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_TERMINATED, "sms")


    @test_tracker_info(uuid="ac61311d-4100-498e-893d-7669b5de1226")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mt_sms(self):
        """ Test MSIM SMS MT from SSIM stress

        Airplane mode is off. Phone in default state.
        Switch DDS
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_TERMINATED, "sms",
                                          dds_switch=True)


    @test_tracker_info(uuid="33316449-d802-472d-8a51-85322f83a501")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_msim_mt_mms(self):
        """ Test MSIM MMS MT from SSIM stress

        Airplane mode is off. Phone in default state.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_TERMINATED, "mms")


    @test_tracker_info(uuid="25fecd17-b4a1-4010-acf6-ae72fbe905df")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_msim_mt_mms(self):
        """ Test MSIM MMS MT from SSIM stress

        Airplane mode is off. Phone in default state.
        Switch DDS
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.
        Above steps to be done for 3 different message lengths
        Above steps to be done for slot0 and slot1

        Returns:
            True if pass; False if fail.
        """
        return self._test_stress_msim_sms(DIRECTION_MOBILE_TERMINATED, "mms",
                                          dds_switch=True)


    def _test_msim_file_download_stress(self):
        ad = self.android_devices[0]
        total_iteration = self.stress_test_number
        fail_count = collections.defaultdict(int)
        for i in range(0,2):
            sub_id = get_subid_from_slot_index(ad.log, ad, i)
            operator = get_operatorname_from_slot_index(ad, i)
            ad.log.info("Slot %d - Sub %s - %s", i, sub_id, operator)

        file_names = ["5MB", "10MB"]
        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "File DL Iteration: <%s> / <%s>" % (i, total_iteration)
            self.log.info(msg)
            begin_time = get_current_epoch_time()
            start_qxdm_logger(ad, begin_time)
            current_dds = perform_dds_switch(ad)
            if not current_dds:
                ad.log.error("DDS Switch Failed")
                fail_count["dds_switch"] += 1
                ad.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
                current_iteration += 1
                continue
            time.sleep(15)
            if not verify_internet_connection(ad.log, ad):
                ad.log.warning("No Data after DDS. Waiting 1 more minute")
                time.sleep(60)
            try:
                selection = random.randrange(0, len(file_names))
                file_name = file_names[selection]
                iteration_result = active_file_download_test(
                                       ad.log, ad, file_name)
                if not iteration_result:
                    fail_count["%s" % current_dds] += 1
            except Exception as e:
                ad.log.error("Exception error %s", str(e))
                iteration_result = False

            if iteration_result:
                ad.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
                ad.log.error("%s file download failure", file_name)
                ad.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
            current_iteration += 1

        test_result = True
        for failure, count in fail_count.items():
            if count:
                ad.log.error("%s: %s %s failures in %s iterations",
                               self.test_name, count, failure,
                               total_iteration)
                test_result = False
        return test_result


    @test_tracker_info(uuid="82e10a34-5018-453a-bf20-52d3b225a36e")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_file_download(self):
        """ Test File DL stress on both DDS

        Switch DDS
        File Download on alternate slot
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_msim_file_download_stress()


    def _test_msim_pings_dds_stress(self):
        ad = self.android_devices[0]
        total_iteration = self.stress_test_number
        fail_count = collections.defaultdict(int)
        for i in range(0,2):
            sub_id = get_subid_from_slot_index(ad.log, ad, i)
            operator = get_operatorname_from_slot_index(ad, i)
            ad.log.info("Slot %d - Sub %s - %s", i, sub_id, operator)

        current_iteration = 1
        for i in range(1, total_iteration + 1):
            msg = "Ping test Iteration: <%s> / <%s>" % (i, total_iteration)
            self.log.info(msg)
            begin_time = get_current_epoch_time()
            start_qxdm_logger(ad, begin_time)

            current_dds = perform_dds_switch(ad)
            if not current_dds:
                ad.log.error("DDS Switch Failed")
                fail_count["dds_switch"] += 1
                ad.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
                current_iteration += 1
                continue

            ad.log.info("Waiting for 30 secs before verifying data")
            time.sleep(30)
            iteration_result = verify_internet_connection(ad.log, ad)
            if not iteration_result:
                fail_count["%s" % current_dds] += 1

            if iteration_result:
                ad.log.info(">----Iteration : %d/%d succeed.----<",
                    i, total_iteration)
            else:
                ad.log.error(">----Iteration : %d/%d failed.----<",
                    i, total_iteration)
                self._take_bug_report("%s_IterNo_%s" % (self.test_name, i),
                                      begin_time)
            current_iteration += 1

        test_result = True
        for failure, count in fail_count.items():
            if count:
                ad.log.error("%s: %s %s failures in %s iterations",
                               self.test_name, count, failure,
                               total_iteration)
                test_result = False
        return test_result


    @test_tracker_info(uuid="7a5127dd-71e0-45cf-bb8a-52081b92ca7b")
    @TelephonyBaseTest.tel_test_wrap
    def test_stress_dds_switch_pings(self):
        """ Test pings stress on both DDS

        Switch DDS
        ICMP Pings on alternate slot
        Repeat above steps

        Returns:
            True if pass; False if fail.
        """
        return self._test_msim_pings_dds_stress()
