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
"""
Sanity tests for connectivity tests in telephony
"""

import time
from acts.test_decorators import test_tracker_info
from acts.controllers.anritsu_lib._anritsu_utils import AnritsuError
from acts.controllers.anritsu_lib.md8475a import MD8475A
from acts.controllers.anritsu_lib.md8475a import VirtualPhoneStatus
from acts.test_utils.tel.anritsu_utils import set_system_model_1x
from acts.test_utils.tel.anritsu_utils import set_system_model_gsm
from acts.test_utils.tel.anritsu_utils import set_system_model_lte
from acts.test_utils.tel.anritsu_utils import set_system_model_wcdma
from acts.test_utils.tel.anritsu_utils import sms_mo_send
from acts.test_utils.tel.anritsu_utils import sms_mt_receive_verify
from acts.test_utils.tel.anritsu_utils import set_usim_parameters
from acts.test_utils.tel.anritsu_utils import set_post_sim_params
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_ORIGINATED
from acts.test_utils.tel.tel_defines import DIRECTION_MOBILE_TERMINATED
from acts.test_utils.tel.tel_defines import NETWORK_MODE_CDMA
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_ONLY
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_UMTS
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_GSM_WCDMA
from acts.test_utils.tel.tel_defines import RAT_1XRTT
from acts.test_utils.tel.tel_defines import RAT_GSM
from acts.test_utils.tel.tel_defines import RAT_LTE
from acts.test_utils.tel.tel_defines import RAT_WCDMA
from acts.test_utils.tel.tel_defines import RAT_FAMILY_CDMA2000
from acts.test_utils.tel.tel_defines import RAT_FAMILY_GSM
from acts.test_utils.tel.tel_defines import RAT_FAMILY_LTE
from acts.test_utils.tel.tel_defines import RAT_FAMILY_UMTS
from acts.test_utils.tel.tel_test_utils import ensure_network_rat
from acts.test_utils.tel.tel_test_utils import ensure_phones_idle
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_test_utils import toggle_volte
from acts.test_utils.tel.tel_test_utils import set_preferred_apn_by_adb
from acts.test_utils.tel.tel_defines import CALL_TEARDOWN_PHONE
from acts.test_utils.tel.tel_defines import RAT_FAMILY_CDMA2000
from acts.test_utils.tel.tel_defines import RAT_FAMILY_GSM
from acts.test_utils.tel.tel_defines import RAT_FAMILY_LTE
from acts.test_utils.tel.tel_defines import RAT_FAMILY_UMTS
from acts.test_utils.tel.tel_defines import RAT_1XRTT
from acts.test_utils.tel.tel_defines import NETWORK_MODE_CDMA
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_ONLY
from acts.test_utils.tel.tel_defines import NETWORK_MODE_GSM_UMTS
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_CDMA_EVDO
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_CDMA_EVDO_GSM_WCDMA
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_GSM_WCDMA
from acts.utils import rand_ascii_str

SINGLE_PART_LEN = 40
MULTI_PART_LEN = 180
SINGLE_PART_LEN_75 = 75


class TelLabSmsTest(TelephonyBaseTest):
    phoneNumber = "911"
    SETTLING_TIME = 15

    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        self.md8475a_ip_address = self.user_params[
            "anritsu_md8475a_ip_address"]
        self.ad.sim_card = getattr(self.ad, "sim_card", None)
        self.wlan_option = self.user_params.get("anritsu_wlan_option", False)
        self.md8475_version = self.user_params.get("md8475", "A")

        try:
            self.anritsu = MD8475A(self.md8475a_ip_address, self.wlan_option,
                                   self.md8475_version)
        except AnritsuError:
            self.log.error("Error in connecting to Anritsu Simulator")
            return False
        return True

    def setup_test(self):
        ensure_phones_idle(self.log, self.android_devices)
        self.virtualPhoneHandle = self.anritsu.get_VirtualPhone()
        self.ad.droid.connectivityToggleAirplaneMode(True)
        return True

    def teardown_test(self):
        self.log.info("Stopping Simulation")
        self.anritsu.stop_simulation()
        self.ad.droid.connectivityToggleAirplaneMode(True)
        return True

    def teardown_class(self):
        self.anritsu.disconnect()
        return True

    def _phone_setup_volte(self, ad):
        ad.droid.telephonyToggleDataConnection(True)
        toggle_volte(self.log, ad, True)
        return ensure_network_rat(
            self.log,
            ad,
            NETWORK_MODE_LTE_CDMA_EVDO_GSM_WCDMA,
            RAT_FAMILY_LTE,
            toggle_apm_after_setting=True)

    def _phone_setup_wcdma(self, ad):
        return ensure_network_rat(
            self.log,
            ad,
            NETWORK_MODE_GSM_UMTS,
            RAT_FAMILY_UMTS,
            toggle_apm_after_setting=True)

    def _phone_setup_gsm(self, ad):
        return ensure_network_rat(
            self.log,
            ad,
            NETWORK_MODE_GSM_ONLY,
            RAT_FAMILY_GSM,
            toggle_apm_after_setting=True)

    def _phone_setup_1x(self, ad):
        return ensure_network_rat(
            self.log,
            ad,
            NETWORK_MODE_CDMA,
            RAT_FAMILY_CDMA2000,
            toggle_apm_after_setting=True)


    def _setup_sms(self,
                   set_simulation_func,
                   phone_setup_func,
                   rat,
                   phone_number,
                   message,
                   mo_mt=DIRECTION_MOBILE_ORIGINATED):
        try:
            [self.bts1] = set_simulation_func(self.anritsu, self.user_params,
                                              self.ad.sim_card)
            set_usim_parameters(self.anritsu, self.ad.sim_card)
            if rat == RAT_LTE:
                set_post_sim_params(self.anritsu, self.user_params,
                                    self.ad.sim_card)
            self.anritsu.start_simulation()
            self.anritsu.send_command("IMSSTARTVN 1")
            self.anritsu.send_command("IMSSTARTVN 2")
            self.anritsu.send_command("IMSSTARTVN 3")

            if rat == RAT_LTE:
                preferred_network_setting = NETWORK_MODE_LTE_GSM_WCDMA
                rat_family = RAT_FAMILY_LTE
            elif rat == RAT_WCDMA:
                preferred_network_setting = NETWORK_MODE_GSM_UMTS
                rat_family = RAT_FAMILY_UMTS
            elif rat == RAT_GSM:
                preferred_network_setting = NETWORK_MODE_GSM_ONLY
                rat_family = RAT_FAMILY_GSM
            elif rat == RAT_1XRTT:
                preferred_network_setting = NETWORK_MODE_CDMA
                rat_family = RAT_FAMILY_CDMA2000
            else:
                self.log.error("No valid RAT provided for SMS test.")
                return False

            if phone_setup_func is not None:
                if not phone_setup_func(self.ad):
                    self.log.warning(
                        "phone_setup_func failed. Rebooting UE")
                    self.ad.reboot()
                    time.sleep(30)
                    if self.ad.sim_card == "VzW12349":
                        set_preferred_apn_by_adb(self.ad, "VZWINTERNET")
                    if not phone_setup_func(self.ad):
                        self.log.error("phone_setup_func failed.")
                        return False

            self.anritsu.wait_for_registration_state()
            time.sleep(self.SETTLING_TIME)
            if mo_mt == DIRECTION_MOBILE_ORIGINATED:
                if not sms_mo_send(self.log, self.ad, self.virtualPhoneHandle,
                                   phone_number, message, rat):
                    self.log.error("Phone {} Failed to send SMS"
                                   .format(self.ad.serial))
                    return False
            else:
                if not sms_mt_receive_verify(self.log, self.ad,
                                             self.virtualPhoneHandle,
                                             phone_number, message, rat):
                    self.log.error("Phone {} Failed to receive MT SMS"
                                   .format(self.ad.serial))
                    return False

        except AnritsuError as e:
            self.log.error("Error in connection with Anritsu Simulator: " +
                           str(e))
            return False
        except Exception as e:
            self.log.error("Exception during emergency call procedure: " +
                           str(e))
            return False
        return True

    def insert_string_into_message(self, message, string, index):
        return message[:index] + string + message[index:]

    """ Tests Begin """

    @test_tracker_info(uuid="e7e6187a-3054-4f31-a8e5-cff8b3282674")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_lte(self):
        """ Test MO SMS(less than 160 charcters) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_lte, self._phone_setup_volte,
                               RAT_LTE, self.phoneNumber,
                               rand_ascii_str(SINGLE_PART_LEN),
                               DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="40a65c88-c6b1-4044-ac00-2847c2367821")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_lte(self):
        """ Test MT SMS(less than 160 charcters) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_lte, self._phone_setup_volte,
                               RAT_LTE, self.phoneNumber,
                               rand_ascii_str(SINGLE_PART_LEN),
                               DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="6a439b24-98e5-43a0-84fc-d4050478116d")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart_lte(self):
        """ Test MO SMS(more than 160 charcters) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_lte, self._phone_setup_volte,
                               RAT_LTE, self.phoneNumber,
                               rand_ascii_str(MULTI_PART_LEN),
                               DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="6ba7c165-ff4c-4531-aabe-9695a1d37992")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart_lte(self):
        """ Test MT SMS(more than 160 charcters) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_lte, self._phone_setup_volte,
                               RAT_LTE, self.phoneNumber,
                               rand_ascii_str(MULTI_PART_LEN),
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_eacute_lte(self):
        """ Test MO SMS(single part contains é) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_eacute_lte(self):
        """ Test MT SMS(single part contains é) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart1_eacute_lte(self):
        """ Test MO SMS(multi part contains é in first part) functionality on
        LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart1_eacute_lte(self):
        """ Test MT SMS(multi part contains é in first part) functionality on
        LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart2_eacute_lte(self):
        """ Test MO SMS(multi part contains é in second part) functionality on
        LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 170)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart2_eacute_lte(self):
        """ Test MT SMS(multi part contains é in second part) functionality on
        LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 10)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart12_eacute_lte(self):
        """ Test MO SMS(multi part contains é in both parts) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart12_eacute_lte(self):
        """ Test MT SMS(multi part contains é in both parts) functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_71chars_eacute_lte(self):
        """ Test MO SMS(single part more than 71 characters with é)
        functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_71chars_eacute_lte(self):
        """ Test MT SMS(single part more than 71 characters with é)
        functionality on LTE

        Make Sure Phone is in LTE mode
        Send a SMS from Anritsu
        Verify  Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_lte, RAT_LTE, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="9637923e-bbb5-4839-924d-325719e67f31")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_wcdma(self):
        """ Test MO SMS(less than 160 charcters) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_wcdma, self._phone_setup_wcdma,
            RAT_WCDMA, self.phoneNumber,
            rand_ascii_str(SINGLE_PART_LEN), DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="da8e0850-ca57-4521-84bc-e823f67d01fb")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_wcdma(self):
        """ Test MT SMS(less than 160 charcters) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_wcdma, self._phone_setup_wcdma,
            RAT_WCDMA, self.phoneNumber,
            rand_ascii_str(SINGLE_PART_LEN), DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="5abfb6b6-3790-40d7-afd5-d4e1fd5e48f2")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart_wcdma(self):
        """ Test MO SMS(more than 160 charcters) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_wcdma, self._phone_setup_wcdma,
            RAT_WCDMA, self.phoneNumber,
            rand_ascii_str(MULTI_PART_LEN), DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="1b3882c8-fc63-4f2b-a9d7-2f082e7aac92")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart_wcdma(self):
        """ Test MT SMS(more than 160 charcters) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_wcdma, self._phone_setup_wcdma,
            RAT_WCDMA, self.phoneNumber,
            rand_ascii_str(MULTI_PART_LEN), DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_eacute_wcdma(self):
        """ Test MO SMS(single part contains é) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_eacute_wcdma(self):
        """ Test MT SMS(single part contains é) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart1_eacute_wcdma(self):
        """ Test MO SMS(multi part contains é in first part) functionality on
        WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart1_eacute_wcdma(self):
        """ Test MT SMS(multi part contains é in first part) functionality on
        WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart2_eacute_wcdma(self):
        """ Test MO SMS(multi part contains é in second part) functionality on
        WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 170)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart2_eacute_wcdma(self):
        """ Test MT SMS(multi part contains é in second part) functionality on
        WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 10)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart12_eacute_wcdma(self):
        """ Test MO SMS(multi part contains é in both parts) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart12_eacute_wcdma(self):
        """ Test MT SMS(multi part contains é in both parts) functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_71chars_eacute_wcdma(self):
        """ Test MO SMS(single part more than 71 characters with é)
        functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_71chars_eacute_wcdma(self):
        """ Test MT SMS(single part more than 71 characters with é)
        functionality on WCDMA

        Make Sure Phone is in WCDMA mode
        Send a SMS from Anritsu
        Verify  Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_wcdma, RAT_WCDMA,
                               self.phoneNumber, text,
                               DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="7d6da36b-9ce3-4f19-a276-6f51c2b8b5f7")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_gsm(self):
        """ Test MO SMS(less than 160 charcters) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_gsm, self._phone_setup_gsm,
                               RAT_GSM, self.phoneNumber,
                               rand_ascii_str(SINGLE_PART_LEN),
                               DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="8e0d745a-db61-433c-9179-4b28c1aea0d3")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_gsm(self):
        """ Test MT SMS(less than 160 charcters) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_gsm, self._phone_setup_gsm,
                               RAT_GSM, self.phoneNumber,
                               rand_ascii_str(SINGLE_PART_LEN),
                               DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="35df99ae-f55e-4fd2-993b-13528f50ce4b")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart_gsm(self):
        """ Test MO SMS(more than 160 charcters) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_gsm, self._phone_setup_gsm,
                               RAT_GSM, self.phoneNumber,
                               rand_ascii_str(MULTI_PART_LEN),
                               DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="6bff7780-15f7-4bf5-b7e3-9311d7ba68e7")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart_gsm(self):
        """ Test MT SMS(more than 160 charcters) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(set_system_model_gsm, self._phone_setup_gsm,
                               RAT_GSM, self.phoneNumber,
                               rand_ascii_str(MULTI_PART_LEN),
                               DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_eacute_gsm(self):
        """ Test MO SMS(single part contains é) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_eacute_gsm(self):
        """ Test MT SMS(single part contains é) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN), "single part contains é", 10)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart1_eacute_gsm(self):
        """ Test MO SMS(multi part contains é in first part) functionality on
        GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart1_eacute_gsm(self):
        """ Test MT SMS(multi part contains é in first part) functionality on
        GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in first part", 10)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart2_eacute_gsm(self):
        """ Test MO SMS(multi part contains é in second part) functionality on
        GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 170)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart2_eacute_gsm(self):
        """ Test MT SMS(multi part contains é in second part) functionality on
        GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(MULTI_PART_LEN),
            "multi part contains é in second part", 170)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart12_eacute_gsm(self):
        """ Test MO SMS(multi part contains é in both parts) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart12_eacute_gsm(self):
        """ Test MT SMS(multi part contains é in both parts) functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = rand_ascii_str(MULTI_PART_LEN)
        text = self.insert_string_into_message(text, "é in first part", 50)
        text = self.insert_string_into_message(text, "é in second part", 170)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_71chars_eacute_gsm(self):
        """ Test MO SMS(single part more than 71 characters with é)
        functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_ORIGINATED)

    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_71chars_eacute_gsm(self):
        """ Test MT SMS(single part more than 71 characters with é)
        functionality on GSM

        Make Sure Phone is in GSM mode
        Send a SMS from Anritsu
        Verify  Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        text = self.insert_string_into_message(
            rand_ascii_str(SINGLE_PART_LEN_75),
            "single part more than 71 characters with é", 72)
        return self._setup_sms(set_system_model_gsm, RAT_GSM, self.phoneNumber,
                               text, DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="a044c8f7-4823-4b9e-b2be-572b6c6b1b54")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_singlepart_1x(self):
        """ Test MO SMS(less than 160 charcters) functionality on CDMA1X

        Make Sure Phone is in CDMA1X mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_1x, self._phone_setup_1x,
            RAT_1XRTT, self.phoneNumber,
            rand_ascii_str(SINGLE_PART_LEN), DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="da9afa0e-8a82-4f73-9a49-f506e28edefc")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_singlepart_1x(self):
        """ Test MT SMS(less than 160 charcters) functionality on CDMA1X

        Make Sure Phone is in CDMA1X mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_1x, self._phone_setup_1x,
            RAT_1XRTT, self.phoneNumber,
            rand_ascii_str(SINGLE_PART_LEN), DIRECTION_MOBILE_TERMINATED)

    @test_tracker_info(uuid="dc6c5765-4928-4fec-bb2c-7f127d6e8851")
    @TelephonyBaseTest.tel_test_wrap
    def test_mo_sms_multipart_1x(self):
        """ Test MO SMS(more than 160 charcters) functionality on CDMA1X

        Make Sure Phone is in CDMA1X mode
        Send a SMS from Phone
        Verify Anritsu receives the SMS

        Returns:
            True if pass; False if fail
        """
        return self._setup_sms(
            set_system_model_1x, self._phone_setup_1x,
            RAT_1XRTT, self.phoneNumber,
            rand_ascii_str(MULTI_PART_LEN), DIRECTION_MOBILE_ORIGINATED)

    @test_tracker_info(uuid="7e21506d-788c-48fd-8bb1-1603bc4b5261")
    @TelephonyBaseTest.tel_test_wrap
    def test_mt_sms_multipart_1x(self):
        """ Test MT SMS(more than 160 charcters) functionality on CDMA1X

        Make Sure Phone is in CDMA1X mode
        Send a SMS from Anritsu
        Verify Phone receives the SMS

        Returns:
            True if pass; False if fail
        """
        # TODO: b/26346258 Anritsu is not sending message.
        return self._setup_sms(
            set_system_model_1x, self._phone_setup_1x,
            RAT_1XRTT, self.phoneNumber,
            rand_ascii_str(MULTI_PART_LEN), DIRECTION_MOBILE_TERMINATED)

    """ Tests End """
