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

import time
from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import GEN_3G
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import PHONE_TYPE_GSM
from acts.test_utils.tel.tel_defines import RAT_3G
from acts.test_utils.tel.tel_defines import VT_STATE_BIDIRECTIONAL
from acts.test_utils.tel.tel_defines import WAIT_TIME_ANDROID_STATE_SETTLING
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import ensure_network_generation
from acts.test_utils.tel.tel_test_utils import ensure_phone_default_state
from acts.test_utils.tel.tel_test_utils import ensure_phones_idle
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import get_mobile_data_usage
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import remove_mobile_data_usage_limit
from acts.test_utils.tel.tel_test_utils import mms_send_receive_verify
from acts.test_utils.tel.tel_test_utils import mms_receive_verify_after_call_hangup
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_test_utils import set_call_state_listen_level
from acts.test_utils.tel.tel_test_utils import set_mobile_data_usage_limit
from acts.test_utils.tel.tel_test_utils import setup_sim
from acts.test_utils.tel.tel_test_utils import sms_send_receive_verify
from acts.test_utils.tel.tel_video_utils import phone_setup_video
from acts.test_utils.tel.tel_video_utils import is_phone_in_call_video_bidirectional
from acts.test_utils.tel.tel_video_utils import video_call_setup_teardown
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_1x
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_2g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_3g
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_csfb
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_data_general
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_general
from acts.utils import rand_ascii_str

SMS_OVER_WIFI_PROVIDERS = ("vzw", "tmo", "fi", "rogers", "rjio", "eeuk",
                           "dtag")


class TelLiveSmsTest(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)

        # Try to put SMS and call on different help device
        # If it is a three phone test bed, use the first one as dut,
        # use the second one as sms/mms help device, use the third one
        # as the active call help device.
        self.caller = self.android_devices[0]
        self.callee = self.android_devices[1]
        self.number_of_devices = 2
        self.message_lengths = (50, 160, 180)
        self.long_message_lengths = (800, 1600)

        is_roaming = False
        for ad in self.android_devices:
            ad.sms_over_wifi = False
            #verizon supports sms over wifi. will add more carriers later
            for sub in ad.telephony["subscription"].values():
                if sub["operator"] in SMS_OVER_WIFI_PROVIDERS:
                    ad.sms_over_wifi = True
            if getattr(ad, 'roaming', False):
                is_roaming = True
        if is_roaming:
            # roaming device does not allow message of length 180
            self.message_lengths = (50, 160)

    def teardown_test(self):
        ensure_phones_idle(self.log, self.android_devices)

    def _sms_test(self, ads):
        """Test SMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        for length in self.message_lengths:
            message_array = [rand_ascii_str(length)]
            if not sms_send_receive_verify(self.log, ads[0], ads[1],
                                           message_array):
                ads[0].log.warning("SMS of length %s test failed", length)
                return False
            else:
                ads[0].log.info("SMS of length %s test succeeded", length)
        self.log.info("SMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True

    def _long_sms_test(self, ads):
        """Test SMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        for length in self.long_message_lengths:
            message_array = [rand_ascii_str(length)]
            if not sms_send_receive_verify(self.log, ads[0], ads[1],
                                           message_array):
                ads[0].log.warning("SMS of length %s test failed", length)
                return False
            else:
                ads[0].log.info("SMS of length %s test succeeded", length)
                time.sleep(30)
        self.log.info("SMS test of length %s characters succeeded.",
                      self.message_lengths)
        return True

    def _mms_test(self, ads, expected_result=True):
        """Test MMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        for length in self.message_lengths:
            message_array = [("Test Message", rand_ascii_str(length), None)]
            if not mms_send_receive_verify(
                    self.log,
                    ads[0],
                    ads[1],
                    message_array,
                    expected_result=expected_result):
                self.log.warning("MMS of body length %s test failed", length)
                return False
            else:
                self.log.info("MMS of body length %s test succeeded", length)
        self.log.info("MMS test of body lengths %s succeeded",
                      self.message_lengths)
        return True

    def _long_mms_test(self, ads):
        """Test MMS between two phones.

        Returns:
            True if success.
            False if failed.
        """
        for length in self.long_message_lengths:
            message_array = [("Test Message", rand_ascii_str(length), None)]
            if not mms_send_receive_verify(self.log, ads[0], ads[1],
                                           message_array):
                self.log.warning("MMS of body length %s test failed", length)
                return False
            else:
                self.log.info("MMS of body length %s test succeeded", length)
                time.sleep(30)
        self.log.info("MMS test of body lengths %s succeeded",
                      self.message_lengths)
        return True

    def _mms_test_after_call_hangup(self, ads):
        """Test MMS send out after call hang up.

        Returns:
            True if success.
            False if failed.
        """
        args = [
            self.log, ads[0], ads[1], [("Test Message", "Basic Message Body",
                                        None)]
        ]
        if get_operator_name(self.log, ads[0]) in ["spt", "Sprint"]:
            args.append(30)
        if not mms_send_receive_verify(*args):
            self.log.info("MMS send in call is suspended.")
            if not mms_receive_verify_after_call_hangup(*args):
                self.log.error(
                    "MMS is not send and received after call release.")
                return False
            else:
                self.log.info("MMS is send and received after call release.")
                return True
        else:
            self.log.info("MMS is send and received successfully in call.")
            return True

    def _sms_test_mo(self, ads):
        return self._sms_test([ads[0], ads[1]])

    def _sms_test_mt(self, ads):
        return self._sms_test([ads[1], ads[0]])

    def _mms_test_mo(self, ads, expected_result=True):
        return self._mms_test(
            [ads[0], ads[1]], expected_result=expected_result)

    def _mms_test_mt(self, ads, expected_result=True):
        return self._mms_test(
            [ads[1], ads[0]], expected_result=expected_result)

    def _long_sms_test_mo(self, ads):
        return self._long_sms_test([ads[0], ads[1]])

    def _long_sms_test_mt(self, ads):
        return self._long_sms_test([ads[1], ads[0]])

    def _long_mms_test_mo(self, ads):
        return self._long_mms_test([ads[0], ads[1]])

    def _long_mms_test_mt(self, ads):
        return self._long_mms_test([ads[1], ads[0]])

    def _mms_test_mo_after_call_hangup(self, ads):
        return self._mms_test_after_call_hangup([ads[0], ads[1]])

    def _mms_test_mt_after_call_hangup(self, ads):
        return self._mms_test_after_call_hangup([ads[1], ads[0]])

    def _mo_sms_in_3g_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_3g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mo(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mt_sms_in_3g_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_3g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mt(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mo_mms_in_3g_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_3g,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mo(ads)
        else:
            return self._mms_test_mo_after_call_hangup(ads)

    def _mt_mms_in_3g_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_3g,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mt(ads)
        else:
            return self._mms_test_mt_after_call_hangup(ads)

    def _mo_sms_in_2g_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mo(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mt_sms_in_2g_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mt(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mo_mms_in_2g_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mo(ads)
        else:
            return self._mms_test_mo_after_call_hangup(ads)

    def _mt_mms_in_2g_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mt(ads)
        else:
            return self._mms_test_mt_after_call_hangup(ads)

    def _mo_sms_in_csfb_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_csfb,
                verify_callee_func=None):
            return False

        if not self._sms_test_mo(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mt_sms_in_csfb_call(self, ads):
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_csfb,
                verify_callee_func=None):
            return False

        if not self._sms_test_mt(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    def _mo_mms_in_csfb_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_csfb,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mo(ads)
        else:
            return self._mms_test_mo_after_call_hangup(ads)

    def _mt_mms_in_csfb_call(self, ads, wifi=False):
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                self.caller,
                self.callee,
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_csfb,
                verify_callee_func=None):
            return False

        if ads[0].sms_over_wifi and wifi:
            return self._mms_test_mt(ads)
        else:
            return self._mms_test_mt_after_call_hangup(ads)

    @test_tracker_info(uuid="480b6ba2-1e5f-4a58-9d88-9b75c8fab1b6")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_general(self):
        """Test SMS basic function between two phone. Phones in any network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(ensure_phone_default_state, (self.log, ads[0])),
                 (ensure_phone_default_state, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="aa87fe73-8236-44c7-865c-3fe3b733eeb4")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_general(self):
        """Test SMS basic function between two phone. Phones in any network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(ensure_phone_default_state, (self.log, ads[0])),
                 (ensure_phone_default_state, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="bb8e1a06-a4b5-4f9b-9ab2-408ace9a1deb")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_general(self):
        """Test MMS basic function between two phone. Phones in any network.

        Airplane mode is off.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(ensure_phone_default_state, (self.log, ads[0])),
                 (ensure_phone_default_state, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="f2779e1e-7d09-43f0-8b5c-87eae5d146be")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_general(self):
        """Test MMS basic function between two phone. Phones in any network.

        Airplane mode is off.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(ensure_phone_default_state, (self.log, ads[0])),
                 (ensure_phone_default_state, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="2c229a4b-c954-4ba3-94ba-178dc7784d03")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_2g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="17fafc41-7e12-47ab-a4cc-fb9bd94e79b9")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_2g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="b4919317-18b5-483c-82f4-ced37a04f28d")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_2g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="cd56bb8a-0794-404d-95bd-c5fd00f4b35a")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_2g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="b39fbc30-9cc2-4d86-a9f4-6f0c1dd0a905")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_2g_wifi(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 2G.
        Connect to Wifi.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone failed to set up 2G.")
            return False
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)
        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="b158a0a7-9697-4b3b-8d5b-f9b6b6bc1c03")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_2g_wifi(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 2G.
        Connect to Wifi.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone failed to set up 2G.")
            return False
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="f094e3da-2523-4f92-a1f3-7cf9edcff850")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_3g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="2186e152-bf83-4d6e-93eb-b4bf9ae2d76e")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_3g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="e716c678-eee9-4a0d-a9cd-ca9eae4fea51")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_3g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="e864a99e-d935-4bd9-95f6-8183cdd3d760")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_3g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="07cdfe26-9021-4af3-8bf6-1abd0cb9e932")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mo_3g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        return self._long_sms_test_mo(ads)

    @test_tracker_info(uuid="740efe0d-fef9-42bc-a732-fe79a3485426")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mt_3g(self):
        """Test SMS basic function between two phone. Phones in 3g network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mt(ads)

    @test_tracker_info(uuid="b0d27de3-1a98-48da-a9c9-c20c8587f256")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mo_3g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mo(ads)

    @test_tracker_info(uuid="fd5a1583-94d2-4b3a-b613-a0a9745daa25")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mt_3g(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mt(ads)

    @test_tracker_info(uuid="c6cfba55-6cde-41cd-93bb-667c317a0127")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_3g_wifi(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Connect to Wifi.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="83c5dd99-f2fe-433d-9775-80a36d0d493b")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_3g_wifi(self):
        """Test MMS basic function between two phone. Phones in 3g network.

        Airplane mode is off. Phone in 3G.
        Connect to Wifi.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])), (phone_setup_3g,
                                                        (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="54a68d6a-dae7-4fe6-b2bb-7c73151a4a73")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_4g_volte(self):
        """Test SMS text function between two phone. Phones in LTE network.

        Airplane mode is off. VoLTE is enabled on PhoneA.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="d0adcd69-37fc-49d1-8dd3-c03dd163fb25")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_4g_volte(self):
        """Test SMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G. VoLTE is enabled on PhoneA.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="8d454a25-a1e5-4872-8193-d435a84d54fa")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_4g_volte(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. VoLTE is enabled on PhoneA.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="79b8239e-9e6a-4781-942b-2df5b060718d")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_4g_volte(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G. VoLTE is enabled on PhoneA.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="5b9e1195-1e42-4405-890f-631e8c58d0c2")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mo_4g_volte(self):
        """Test SMS text function between two phone. Phones in LTE network.

        Airplane mode is off. VoLTE is enabled on PhoneA.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mo(ads)

    @test_tracker_info(uuid="c328cbe7-1899-4ca8-af1c-5eb05683a322")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mt_4g_volte(self):
        """Test SMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G. VoLTE is enabled on PhoneA.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mt(ads)

    @test_tracker_info(uuid="a843c2f7-e4de-4b99-b3a9-f05ecda5fe73")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mo_4g_volte(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. VoLTE is enabled on PhoneA.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mo(ads)

    @test_tracker_info(uuid="26dcba4d-7ddb-438d-84e7-0e754178b5ef")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mt_4g_volte(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G. VoLTE is enabled on PhoneA.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mt(ads)

    @test_tracker_info(uuid="c97687e2-155a-4cf3-9f51-22543b89d53e")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_4g(self):
        """Test SMS basic function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices
        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="e2e01a47-2b51-4d00-a7b2-dbd3c8ffa6ae")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_4g(self):
        """Test SMS basic function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices
        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="90fc6775-de19-49d1-8b8e-e3bc9384c733")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_4g(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="274572bb-ec9f-4c30-aab4-1f4c3f16b372")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_4g(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="44392814-98dd-406a-ae82-5c39e2d082f3")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mo_4g(self):
        """Test SMS basic function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices
        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mo(ads)

    @test_tracker_info(uuid="0f8358a5-a7d5-4dfa-abe0-99fb8b10d48d")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mt_4g(self):
        """Test SMS basic function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices
        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False

        return self._long_sms_test_mt(ads)

    @test_tracker_info(uuid="18edde2b-7db9-40f4-96c4-3286a56d090b")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mo_4g(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mo(ads)

    @test_tracker_info(uuid="49805d08-6f1f-4c90-9bf4-e9acd6f63640")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mt_4g(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mt(ads)

    @test_tracker_info(uuid="c7349fdf-a376-4846-b466-1f329bd1557f")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_4g_wifi(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G.
        Connect to Wifi.
        Send MMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)
        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="1affab34-e03c-49dd-9062-e9ed8eac406b")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_4g_wifi(self):
        """Test MMS text function between two phone. Phones in LTE network.

        Airplane mode is off. Phone in 4G.
        Connect to Wifi.
        Send MMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """

        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="7ee57edb-2962-4d20-b6eb-79cebce91fff")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_volte(self):
        """ Test MO SMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._sms_test_mo(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="5576276b-4ca1-41cc-bb74-31ccd71f9f96")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_volte(self):
        """ Test MT SMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._sms_test_mt(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="3bf8ff74-baa6-4dc6-86eb-c13816fa9bc8")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_volte(self):
        """ Test MO MMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._mms_test_mo(ads):
            self.log.error("MMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="289e6516-5f66-403a-b292-50d067151730")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_volte(self):
        """ Test MT MMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._mms_test_mt(ads):
            self.log.error("MMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="5654d974-3c32-4cce-9d07-0c96213dacc5")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_volte_wifi(self):
        """ Test MO MMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Connect PhoneA to Wifi.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)
        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._mms_test_mo(ads):
            self.log.error("MMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="cbd5ab3d-d76a-4ece-ac09-62efeead7550")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_volte_wifi(self):
        """ Test MT MMS during a MO VoLTE call.

        Make sure PhoneA is in LTE mode (with VoLTE).
        Make sure PhoneB is able to make/receive call.
        Connect PhoneA to Wifi.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_volte, (self.log, ads[0])), (phone_setup_volte,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)
        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_volte,
                verify_callee_func=None):
            return False

        if not self._mms_test_mt(ads):
            self.log.error("MMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="b6e9ce80-8577-48e5-baa7-92780932f278")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_csfb(self):
        """ Test MO SMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mo_sms_in_csfb_call(ads)

    @test_tracker_info(uuid="93f0b58a-01e9-4bc9-944f-729d455597dd")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_csfb(self):
        """ Test MT SMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive receive on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mt_sms_in_csfb_call(ads)

    @test_tracker_info(uuid="bd8e9e80-1955-429f-b122-96b127771bbb")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_csfb(self):
        """ Test MO MMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mo_mms_in_csfb_call(ads)

    @test_tracker_info(uuid="89d65fd2-fc75-4fc5-a018-2d05a4364304")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_csfb(self):
        """ Test MT MMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive receive on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mt_mms_in_csfb_call(ads)

    @test_tracker_info(uuid="9c542b5d-3b8f-4d4a-80de-fb804f066c3d")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_csfb_wifi(self):
        """ Test MO MMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Connect PhoneA to Wifi.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mo_mms_in_csfb_call(ads, wifi=True)

    @test_tracker_info(uuid="c1bed6f5-f65c-4f4d-aa06-0e9f5c867819")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_csfb_wifi(self):
        """ Test MT MMS during a MO csfb call.

        Make sure PhoneA is in LTE mode (no VoLTE).
        Make sure PhoneB is able to make/receive call.
        Connect PhoneA to Wifi.
        Call from PhoneA to PhoneB, accept on PhoneB, receive receive on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_csfb, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mt_mms_in_csfb_call(ads, wifi=True)

    @test_tracker_info(uuid="60996028-b4b2-4a16-9e4b-eb6ef80179a7")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_3g(self):
        """ Test MO SMS during a MO 3G call.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mo_sms_in_3g_call(ads)

    @test_tracker_info(uuid="6b352aac-9b4e-4062-8980-3b1c0e61015b")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_3g(self):
        """ Test MT SMS during a MO 3G call.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mt_sms_in_3g_call(ads)

    @test_tracker_info(uuid="cfae3613-c490-4ce0-b00b-c13286d85027")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_3g(self):
        """ Test MO MMS during a MO 3G call.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Send MMS on PhoneA during the call, MMS is send out after call is released.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mo_mms_in_3g_call(ads)

    @test_tracker_info(uuid="42fc8c16-4a30-4f63-9728-2639f2b79c4c")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_3g(self):
        """ Test MT MMS during a MO 3G call.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mt_mms_in_3g_call(ads)

    @test_tracker_info(uuid="18093f87-aab5-4d86-b178-8085a1651828")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_3g_wifi(self):
        """ Test MO MMS during a 3G call with Wifi on.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Send MMS on PhoneA during the call, MMS is send out after call is released.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mo_mms_in_3g_call(ads, wifi=True)

    @test_tracker_info(uuid="8fe3359a-0857-401f-a043-c47a2a2acb47")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_3g_wifi(self):
        """ Test MT MMS during a 3G call with Wifi On.

        Make sure PhoneA is in 3g.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_3g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mt_mms_in_3g_call(ads, wifi=True)

    @test_tracker_info(uuid="ed720013-e366-448b-8901-bb09d26cea05")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_iwlan(self):
        """ Test MO SMS, Phone in APM, WiFi connected, WFC Cell Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC Cell preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_CELLULAR_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="4d4b0b7b-bf00-44f6-a0ed-23b438c30fc2")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_iwlan(self):
        """ Test MT SMS, Phone in APM, WiFi connected, WFC Cell Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_CELLULAR_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="264e2557-e18c-41c0-8d99-49cee3fe6f07")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_iwlan(self):
        """ Test MO MMS, Phone in APM, WiFi connected, WFC Cell Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC Cell preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_CELLULAR_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="330db618-f074-4bfc-bf5e-78939fbee532")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_iwlan(self):
        """ Test MT MMS, Phone in APM, WiFi connected, WFC Cell Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC Cell preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_CELLULAR_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="875ce520-7a09-4032-8e88-965ce143c1f5")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mo_iwlan(self):
        """ Test MO SMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mo(ads)

    @test_tracker_info(uuid="a317a1b3-16c8-4c2d-bbfd-aebcc0897499")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_long_message_mt_iwlan(self):
        """ Test MT SMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_sms_test_mt(ads)

    @test_tracker_info(uuid="d692c439-6e96-45a6-be0f-1ff81226416c")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mo_iwlan(self):
        """ Test MO MMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mo(ads)

    @test_tracker_info(uuid="a0958a1b-23ea-4353-9af6-7bc5d6a0a3d2")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_long_message_mt_iwlan(self):
        """ Test MT MMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._long_mms_test_mt(ads)

    @test_tracker_info(uuid="94bb8297-f646-4793-9d97-6f82a706127a")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_iwlan_apm_off(self):
        """ Test MO SMS, Phone in APM off, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM off, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], False, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="e4acce6a-75ae-45c1-be85-d3a2eb2da7c2")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_iwlan_apm_off(self):
        """ Test MT SMS, Phone in APM off, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM off, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], False, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="6c003c28-5712-4456-89cb-64d417ab2ce4")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_iwlan_apm_off(self):
        """ Test MO MMS, Phone in APM off, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM off, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], False, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="0ac5c8ff-83e5-49f2-ba71-ebb283feed9e")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_iwlan_apm_off(self):
        """ Test MT MMS, Phone in APM off, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM off, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], False, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="075933a2-df7f-4374-a405-92f96bcc7770")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_apm_wifi_wfc_off(self):
        """ Test MO SMS, Phone in APM, WiFi connected, WFC off.

        Make sure PhoneA APM, WiFi connected, WFC off.
        Make sure PhoneB is able to make/receive call/sms.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices
        phone_setup_voice_general(self.log, ads[0])
        tasks = [(ensure_wifi_connected,
                  (self.log, ads[0], self.wifi_network_ssid,
                   self.wifi_network_pass)), (phone_setup_voice_general,
                                              (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="637af228-29fc-4b74-a963-883f66ddf080")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_apm_wifi_wfc_off(self):
        """ Test MT SMS, Phone in APM, WiFi connected, WFC off.

        Make sure PhoneA APM, WiFi connected, WFC off.
        Make sure PhoneB is able to make/receive call/sms.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices
        phone_setup_voice_general(self.log, ads[0])
        tasks = [(ensure_wifi_connected,
                  (self.log, ads[0], self.wifi_network_ssid,
                   self.wifi_network_pass)), (phone_setup_voice_general,
                                              (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="502aba0d-8895-4807-b394-50a44208ecf7")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_apm_wifi_wfc_off(self):
        """ Test MO MMS, Phone in APM, WiFi connected, WFC off.

        Make sure PhoneA APM, WiFi connected, WFC off.
        Make sure PhoneB is able to make/receive call/sms.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices
        phone_setup_voice_general(self.log, ads[0])
        tasks = [(ensure_wifi_connected,
                  (self.log, ads[0], self.wifi_network_ssid,
                   self.wifi_network_pass)), (phone_setup_voice_general,
                                              (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="235bfdbf-4275-4d89-99f5-41b5b7de8345")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_apm_wifi_wfc_off(self):
        """ Test MT MMS, Phone in APM, WiFi connected, WFC off.

        Make sure PhoneA APM, WiFi connected, WFC off.
        Make sure PhoneB is able to make/receive call/sms.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices
        phone_setup_voice_general(self.log, ads[0])
        tasks = [(ensure_wifi_connected,
                  (self.log, ads[0], self.wifi_network_ssid,
                   self.wifi_network_pass)), (phone_setup_voice_general,
                                              (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="e5a31b94-1cb6-4770-a2bc-5a0ddba51502")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_iwlan(self):
        """ Test MO SMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_iwlan,
                verify_callee_func=None):
            return False

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="d6d30cc5-f75b-42df-b517-401456ee8466")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_iwlan(self):
        """ Test MT SMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_iwlan,
                verify_callee_func=None):
            return False

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="a98a5a97-3864-4ff8-9085-995212eada20")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_iwlan(self):
        """ Test MO MMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_iwlan,
                verify_callee_func=None):
            return False

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="0464a87b-d45b-4b03-9895-17ece360a796")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_iwlan(self):
        """ Test MT MMS, Phone in APM, WiFi connected, WFC WiFi Preferred mode.

        Make sure PhoneA APM, WiFi connected, WFC WiFi preferred mode.
        Make sure PhoneA report iwlan as data rat.
        Make sure PhoneB is able to make/receive call/sms.
        Call from PhoneA to PhoneB, accept on PhoneB.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """

        ads = self.android_devices

        tasks = [(phone_setup_iwlan,
                  (self.log, ads[0], True, WFC_MODE_WIFI_PREFERRED,
                   self.wifi_network_ssid, self.wifi_network_pass)),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call MMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_iwlan,
                verify_callee_func=None):
            return False

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="9f1933bb-c4cb-4655-8655-327c1f38e8ee")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_vt(self):
        """ Test MO SMS, Phone in ongoing VT call.

        Make sure PhoneA and PhoneB in LTE and can make VT call.
        Make Video Call from PhoneA to PhoneB, accept on PhoneB as Video Call.
        Send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_video, (self.log, ads[0])), (phone_setup_video,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        if not video_call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                None,
                video_state=VT_STATE_BIDIRECTIONAL,
                verify_caller_func=is_phone_in_call_video_bidirectional,
                verify_callee_func=is_phone_in_call_video_bidirectional):
            self.log.error("Failed to setup a call")
            return False

        return self._sms_test_mo(ads)

    @test_tracker_info(uuid="0a07e737-4862-4492-9b48-8d94799eab91")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_vt(self):
        """ Test MT SMS, Phone in ongoing VT call.

        Make sure PhoneA and PhoneB in LTE and can make VT call.
        Make Video Call from PhoneA to PhoneB, accept on PhoneB as Video Call.
        Receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_video, (self.log, ads[0])), (phone_setup_video,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        if not video_call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                None,
                video_state=VT_STATE_BIDIRECTIONAL,
                verify_caller_func=is_phone_in_call_video_bidirectional,
                verify_callee_func=is_phone_in_call_video_bidirectional):
            self.log.error("Failed to setup a call")
            return False

        return self._sms_test_mt(ads)

    @test_tracker_info(uuid="55d70548-6aee-40e9-b94d-d10de84fb50f")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_vt(self):
        """ Test MO MMS, Phone in ongoing VT call.

        Make sure PhoneA and PhoneB in LTE and can make VT call.
        Make Video Call from PhoneA to PhoneB, accept on PhoneB as Video Call.
        Send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_video, (self.log, ads[0])), (phone_setup_video,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        if not video_call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                None,
                video_state=VT_STATE_BIDIRECTIONAL,
                verify_caller_func=is_phone_in_call_video_bidirectional,
                verify_callee_func=is_phone_in_call_video_bidirectional):
            self.log.error("Failed to setup a call")
            return False

        return self._mms_test_mo(ads)

    @test_tracker_info(uuid="75f97c9a-4397-42f1-bb00-8fc6d04fdf6d")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_vt(self):
        """ Test MT MMS, Phone in ongoing VT call.

        Make sure PhoneA and PhoneB in LTE and can make VT call.
        Make Video Call from PhoneA to PhoneB, accept on PhoneB as Video Call.
        Receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices

        tasks = [(phone_setup_video, (self.log, ads[0])), (phone_setup_video,
                                                           (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        if not video_call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                None,
                video_state=VT_STATE_BIDIRECTIONAL,
                verify_caller_func=is_phone_in_call_video_bidirectional,
                verify_callee_func=is_phone_in_call_video_bidirectional):
            self.log.error("Failed to setup a call")
            return False

        return self._mms_test_mt(ads)

    @test_tracker_info(uuid="2a72ecc6-702d-4add-a7a2-8c1001628bb6")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_in_call_2g(self):
        """ Test MO SMS during a MO gsm call.

        Make sure PhoneA is in gsm mode.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM SMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mo(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="facd1814-8d69-42a2-9f80-b6a28cc0c9d2")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_in_call_2g(self):
        """ Test MT SMS during a MO gsm call.

        Make sure PhoneA is in gsm mode.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive SMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM SMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        self.log.info("Begin In Call SMS Test.")
        if not call_setup_teardown(
                self.log,
                ads[0],
                ads[1],
                ad_hangup=None,
                verify_caller_func=is_phone_in_call_2g,
                verify_callee_func=None):
            return False

        if not self._sms_test_mt(ads):
            self.log.error("SMS test fail.")
            return False

        return True

    @test_tracker_info(uuid="2bd94d69-3621-4b94-abc7-bd24c4325485")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_2g(self):
        """ Test MO MMS during a MO gsm call.

        Make sure PhoneA is in gsm mode.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM SMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mo_mms_in_2g_call(ads)

    @test_tracker_info(uuid="e20be70d-99d6-4344-a742-f69581b66d8f")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_2g(self):
        """ Test MT MMS during a MO gsm call.

        Make sure PhoneA is in gsm mode.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM MMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)

        return self._mt_mms_in_2g_call(ads)

    @test_tracker_info(uuid="3510d368-4b16-4716-92a3-9dd01842ba79")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_in_call_2g_wifi(self):
        """ Test MO MMS during a MO gsm call.

        Make sure PhoneA is in gsm mode with Wifi connected.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, send MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM MMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mo_mms_in_2g_call(ads)

    @test_tracker_info(uuid="060def89-01bd-4b44-a49b-a4536fe39165")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_in_call_2g_wifi(self):
        """ Test MT MMS during a MO gsm call.

        Make sure PhoneA is in gsm mode with wifi connected.
        Make sure PhoneB is able to make/receive call.
        Call from PhoneA to PhoneB, accept on PhoneB, receive MMS on PhoneA.

        Returns:
            True if pass; False if fail.
        """
        ads = self.android_devices
        # Make sure PhoneA is GSM phone before proceed.
        if (ads[0].droid.telephonyGetPhoneType() != PHONE_TYPE_GSM):
            raise signals.TestSkip("Not GSM phone, abort this GSM MMS test.")

        tasks = [(phone_setup_voice_2g, (self.log, ads[0])),
                 (phone_setup_voice_general, (self.log, ads[1]))]
        if not multithread_func(self.log, tasks):
            self.log.error("Phone Failed to Set Up Properly.")
            return False
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ensure_wifi_connected(self.log, ads[0], self.wifi_network_ssid,
                              self.wifi_network_pass)

        return self._mt_mms_in_2g_call(ads)

    @test_tracker_info(uuid="7de95a56-8055-4c0c-9438-f249403c6078")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mo_general_after_mobile_data_usage_limit_reached(self):
        """Test SMS send after mobile data usage limit is reached.

        Airplane mode is off.
        Set the data limit to the current usage
        Send SMS from PhoneA to PhoneB.
        Verify received message on PhoneB is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices
        try:
            subscriber_id = ads[0].droid.telephonyGetSubscriberId()
            data_usage = get_mobile_data_usage(ads[0], subscriber_id)
            set_mobile_data_usage_limit(ads[0], data_usage, subscriber_id)

            tasks = [(phone_setup_voice_general, (self.log, ads[0])),
                     (phone_setup_voice_general, (self.log, ads[1]))]
            if not multithread_func(self.log, tasks):
                self.log.error("Phone Failed to Set Up Properly.")
                return False
            time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
            return self._sms_test_mo(ads)
        finally:
            remove_mobile_data_usage_limit(ads[0], subscriber_id)

    @test_tracker_info(uuid="df56687f-0932-4b13-952c-ae0ce30b1d7a")
    @TelephonyBaseTest.tel_test_wrap
    def test_sms_mt_general_after_mobile_data_usage_limit_reached(self):
        """Test SMS receive after mobile data usage limit is reached.

        Airplane mode is off.
        Set the data limit to the current usage
        Send SMS from PhoneB to PhoneA.
        Verify received message on PhoneA is correct.

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices
        try:
            subscriber_id = ads[0].droid.telephonyGetSubscriberId()
            data_usage = get_mobile_data_usage(ads[0], subscriber_id)
            set_mobile_data_usage_limit(ads[0], data_usage, subscriber_id)

            tasks = [(phone_setup_voice_general, (self.log, ads[0])),
                     (phone_setup_voice_general, (self.log, ads[1]))]
            if not multithread_func(self.log, tasks):
                self.log.error("Phone Failed to Set Up Properly.")
                return False
            time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
            return self._sms_test_mt(ads)
        finally:
            remove_mobile_data_usage_limit(ads[0], subscriber_id)

    @test_tracker_info(uuid="131f98c6-3b56-44df-b5e7-66f33e2cf117")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mo_general_after_mobile_data_usage_limit_reached(self):
        """Test MMS send after mobile data usage limit is reached.

        Airplane mode is off.
        Set the data limit to the current usage
        Send MMS from PhoneA to PhoneB.
        Verify MMS cannot be send. (Can be send/receive for Verizon)

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices
        expected_result = False
        if get_operator_name(self.log, ads[0]) in ["vzw", "Verizon"]:
            expected_result = True
        ads[0].log.info("Expected Result is %s", expected_result)

        try:
            tasks = [(phone_setup_voice_general, (self.log, ads[0])),
                     (phone_setup_voice_general, (self.log, ads[1]))]
            if not multithread_func(self.log, tasks):
                self.log.error("Phone Failed to Set Up Properly.")
                return False
            subscriber_id = ads[0].droid.telephonyGetSubscriberId()
            data_usage = get_mobile_data_usage(ads[0], subscriber_id)
            set_mobile_data_usage_limit(ads[0], data_usage, subscriber_id)
            log_msg = "expecting successful mms receive" if (
                expected_result) else "expecting mms receive failure"
            if not self._mms_test_mo(ads, expected_result=expected_result):
                ads[0].log.error("Mms test failed, %s", log_msg)
                return False
            else:
                ads[0].log.info("Mms test succeeded, %s", log_msg)
                return True
        finally:
            remove_mobile_data_usage_limit(ads[0], subscriber_id)

    @test_tracker_info(uuid="051e259f-0cb9-417d-9a68-8e8a4266fca1")
    @TelephonyBaseTest.tel_test_wrap
    def test_mms_mt_general_after_mobile_data_usage_limit_reached(self):
        """Test MMS receive after mobile data usage limit is reached.

        Airplane mode is off.
        Set the data limit to the current usage
        Send MMS from PhoneB to PhoneA.
        Verify MMS cannot be received. (Can be send/receive for Verizon)

        Returns:
            True if success.
            False if failed.
        """
        ads = self.android_devices
        expected_result = False
        if get_operator_name(self.log, ads[0]) in ["vzw", "Verizon"]:
            expected_result = True
        try:
            tasks = [(phone_setup_voice_general, (self.log, ads[0])),
                     (phone_setup_voice_general, (self.log, ads[1]))]
            if not multithread_func(self.log, tasks):
                self.log.error("Phone Failed to Set Up Properly.")
                return False
            subscriber_id = ads[0].droid.telephonyGetSubscriberId()
            data_usage = get_mobile_data_usage(ads[0], subscriber_id)
            set_mobile_data_usage_limit(ads[0], data_usage, subscriber_id)
            log_msg = "expecting successful mms receive" if (
                expected_result) else "expecting mms receive failure"
            if not self._mms_test_mt(ads, expected_result=expected_result):
                ads[0].log.error("Mms test failed, %s", log_msg)
                return False
            else:
                ads[0].log.info("Mms test succeeded, %s", log_msg)
                return True
        finally:
            remove_mobile_data_usage_limit(ads[0], subscriber_id)
