#!/usr/bin/env python3.4
#
#   Copyright 2018 - Google
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
    Test Script for IMS Settings
"""

import time

from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import CarrierConfigs
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC_MODE_CHANGE
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_WIFI_CONNECTION
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_VOICE
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_IMS_REGISTRATION
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_WFC_ENABLED
from acts.test_utils.tel.tel_defines import RAT_FAMILY_WLAN
from acts.test_utils.tel.tel_defines import RAT_LTE
from acts.test_utils.tel.tel_defines import RAT_UNKNOWN
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_DISABLED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_ONLY
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_subscription_utils import get_outgoing_voice_sub_id
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import dumpsys_carrier_config
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import get_user_config_profile
from acts.test_utils.tel.tel_test_utils import is_droid_in_rat_family
from acts.test_utils.tel.tel_test_utils import revert_default_telephony_setting
from acts.test_utils.tel.tel_test_utils import set_wfc_mode
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import toggle_volte
from acts.test_utils.tel.tel_test_utils import toggle_wfc
from acts.test_utils.tel.tel_test_utils import verify_default_telephony_setting
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import wait_for_ims_registered
from acts.test_utils.tel.tel_test_utils import wait_for_network_rat
from acts.test_utils.tel.tel_test_utils import wait_for_not_network_rat
from acts.test_utils.tel.tel_test_utils import wait_for_state
from acts.test_utils.tel.tel_test_utils import wait_for_voice_attach
from acts.test_utils.tel.tel_test_utils import wait_for_volte_enabled
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_disabled
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_enabled
from acts.test_utils.tel.tel_test_utils import wait_for_wifi_data_connection
from acts.test_utils.tel.tel_test_utils import wifi_reset
from acts.test_utils.tel.tel_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_test_utils import WIFI_SSID_KEY
from acts.test_utils.tel.tel_test_utils import WIFI_PWD_KEY


class TelLiveImsSettingsTest(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)
        self.dut = self.android_devices[0]
        self.number_of_devices = 1
        self.skip_reset_between_cases = False
        subid = get_outgoing_voice_sub_id(self.dut)
        self.carrier_configs = dumpsys_carrier_config(self.dut)[subid]
        self.dut_capabilities = self.dut.telephony["subscription"][
            subid].get("capabilities", [])
        self.dut.log.info("DUT capabilities: %s", self.dut_capabilities)
        if CAPABILITY_VOLTE not in self.dut_capabilities:
            raise signals.TestAbortClass("VoLTE is not supported")
        if CAPABILITY_WFC not in self.dut_capabilities:
            raise signals.TestAbortClass("WFC is not supported")

        self.default_volte = (CAPABILITY_VOLTE in self.dut_capabilities) and (
            self.carrier_configs[CarrierConfigs.
                                 ENHANCED_4G_LTE_ON_BY_DEFAULT_BOOL])
        self.default_wfc_enabled = (
            CAPABILITY_WFC in self.dut_capabilities
        ) and (
            self.carrier_configs[CarrierConfigs.DEFAULT_WFC_IMS_ENABLED_BOOL])
        self.default_wfc_mode = self.carrier_configs.get(
            CarrierConfigs.DEFAULT_WFC_IMS_MODE_INT, None)
        self.dut_wfc_modes = self.dut.telephony[
            "subscription"][subid].get("wfc_modes", [])

    def check_call_in_wfc(self):
        result = True
        if not call_setup_teardown(self.log, self.android_devices[1], self.dut,
                                   self.dut, None, is_phone_in_call_iwlan):
            if not call_setup_teardown(self.log, self.android_devices[1],
                                       self.dut, self.dut, None,
                                       is_phone_in_call_iwlan):
                self.dut.log.error("MT WFC call failed")
                result = False
        if not call_setup_teardown(self.log, self.dut, self.android_devices[1],
                                   self.dut, is_phone_in_call_iwlan):
            self.dut.log.error("MO WFC call failed")
            result = False
        return result

    def check_call_in_volte(self):
        result = True
        if not call_setup_teardown(self.log, self.android_devices[1], self.dut,
                                   self.dut, None, is_phone_in_call_volte):
            if not call_setup_teardown(self.log, self.android_devices[1],
                                       self.dut, self.dut, None,
                                       is_phone_in_call_volte):
                self.dut.log.error("MT VoLTE call failed")
                result = False
        if not call_setup_teardown(self.log, self.dut, self.android_devices[1],
                                   self.dut, is_phone_in_call_volte):
            self.dut.log.error("MO VoLTE call failed")
            result = False
        return result

    def check_call(self):
        result = True
        if not call_setup_teardown(self.log, self.android_devices[1], self.dut,
                                   self.dut):
            if not call_setup_teardown(self.log, self.android_devices[1],
                                       self.dut, self.dut):
                self.dut.log.error("MT call failed")
                result = False
        if not call_setup_teardown(self.log, self.dut, self.android_devices[1],
                                   self.dut):
            self.dut.log.error("MO call failed")
            result = False
        return result

    def change_ims_setting(self,
                           airplane_mode,
                           wifi_enabled,
                           volte_enabled,
                           wfc_enabled,
                           wfc_mode=None):
        result = True
        self.dut.log.info(
            "Setting APM %s, WIFI %s, VoLTE %s, WFC %s, WFC mode %s",
            airplane_mode, wifi_enabled, volte_enabled, wfc_enabled, wfc_mode)
        toggle_airplane_mode_by_adb(self.log, self.dut, airplane_mode)
        if wifi_enabled:
            if not ensure_wifi_connected(self.log, self.dut,
                                         self.wifi_network_ssid,
                                         self.wifi_network_pass):
                self.dut.log.error("Fail to connected to WiFi")
                result = False
        else:
            if not wifi_toggle_state(self.log, self.dut, False):
                self.dut.log.error("Failed to turn off WiFi.")
                result = False
        toggle_volte(self.log, self.dut, volte_enabled)
        toggle_wfc(self.log, self.dut, wfc_enabled)
        if wfc_mode:
            set_wfc_mode(self.log, self.dut, wfc_mode)
        wfc_mode = self.dut.droid.imsGetWfcMode()
        if wifi_enabled or not airplane_mode:
            if not ensure_phone_subscription(self.log, self.dut):
                self.dut.log.error("Failed to find valid subscription")
                result = False
        if airplane_mode:
            if (CAPABILITY_WFC in self.dut_capabilities) and (wifi_enabled
                                                              and wfc_enabled):
                if not wait_for_wfc_enabled(self.log, self.dut):
                    result = False
                elif not self.check_call_in_wfc():
                    result = False
            else:
                if not wait_for_state(
                        self.dut.droid.telephonyGetCurrentVoiceNetworkType,
                        RAT_UNKNOWN):
                    self.dut.log.error(
                        "Voice RAT is %s not UNKNOWN",
                        self.dut.droid.telephonyGetCurrentVoiceNetworkType())
                    result = False
                else:
                    self.dut.log.info("Voice RAT is in UNKKNOWN")
        else:
            if (wifi_enabled and wfc_enabled) and (
                    wfc_mode == WFC_MODE_WIFI_PREFERRED) and (
                        CAPABILITY_WFC in self.dut_capabilities):
                if not wait_for_wfc_enabled(self.log, self.dut):
                    result = False
                if not wait_for_state(
                        self.dut.droid.telephonyGetCurrentVoiceNetworkType,
                        RAT_UNKNOWN):
                    self.dut.log.error(
                        "Voice RAT is %s, not UNKNOWN",
                        self.dut.droid.telephonyGetCurrentVoiceNetworkType())
                if not self.check_call_in_wfc():
                    result = False
            else:
                if not wait_for_wfc_disabled(self.log, self.dut):
                    self.dut.log.error("WFC is not disabled")
                    result = False
                if volte_enabled and CAPABILITY_VOLTE in self.dut_capabilities:
                    if not wait_for_volte_enabled(self.log, self.dut):
                        result = False
                    if not self.check_call_in_volte():
                        result = False
                else:
                    if not wait_for_not_network_rat(
                            self.log,
                            self.dut,
                            RAT_LTE,
                            voice_or_data=NETWORK_SERVICE_VOICE):
                        self.dut.log.error(
                            "Voice RAT is %s",
                            self.dut.droid.telephonyGetCurrentVoiceNetworkType(
                            ))
                        result = False
                    if not wait_for_voice_attach(self.log, self.dut):
                        result = False
                    if not self.check_call():
                        result = False
        user_config_profile = get_user_config_profile(self.dut)
        self.dut.log.info("user_config_profile: %s ",
                          sorted(user_config_profile.items()))
        return result

    def verify_default_ims_setting(self):
        result = True
        airplane_mode = self.dut.droid.connectivityCheckAirplaneMode()
        default_wfc_mode = self.carrier_configs.get(
            CarrierConfigs.DEFAULT_WFC_IMS_MODE_INT, WFC_MODE_DISABLED)
        if self.default_wfc_enabled:
            wait_for_wfc_enabled(self.log, self.dut)
        else:
            wait_for_wfc_disabled(self.log, self.dut)
            if airplane_mode:
                wait_for_network_rat(
                    self.log,
                    self.dut,
                    RAT_UNKNOWN,
                    voice_or_data=NETWORK_SERVICE_VOICE)
            else:
                if self.default_volte:
                    wait_for_volte_enabled(self.log, self.dut)
                else:
                    wait_for_not_network_rat(
                        self.log,
                        self.dut,
                        RAT_UNKNOWN,
                        voice_or_data=NETWORK_SERVICE_VOICE)
        if not ensure_phone_subscription(self.log, self.dut):
            ad.log.error("Failed to find valid subscription")
            result = False
        user_config_profile = get_user_config_profile(self.dut)
        self.dut.log.info("user_config_profile = %s ",
                          sorted(user_config_profile.items()))
        if user_config_profile["VoLTE Enabled"] != self.default_volte:
            self.dut.log.error("VoLTE mode is not %s", self.default_volte)
            result = False
        else:
            self.dut.log.info("VoLTE mode is %s as expected",
                              self.default_volte)
        if user_config_profile["WFC Enabled"] != self.default_wfc_enabled:
            self.dut.log.error("WFC enabled is not %s", default_wfc_enabled)
        if user_config_profile["WFC Enabled"]:
            if user_config_profile["WFC Mode"] != default_wfc_mode:
                self.dut.log.error(
                    "WFC mode is not %s after IMS factory reset",
                    default_wfc_mode)
                result = False
            else:
                self.dut.log.info("WFC mode is %s as expected",
                                  default_wfc_mode)
        if self.default_wfc_enabled and \
            default_wfc_mode == WFC_MODE_WIFI_PREFERRED:
            if not self.check_call_in_wfc():
                result = False
        elif not airplane_mode:
            if self.default_volte:
                if not self.check_call_in_volte():
                    result = False
            else:
                if not self.check_call():
                    result = False
        if result == False:
            user_config_profile = get_user_config_profile(self.dut)
            self.dut.log.info("user_config_profile = %s ",
                              sorted(user_config_profile.items()))
        return result

    @test_tracker_info(uuid="a3a680ba-d1e0-4770-a38c-4de8f15f9171")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_volte_wifi_connected_toggle_wfc(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Enabled + WiFi Connected, Toggling WFC

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE enabled.
        2. Make sure DUT WiFi connected, WFC disabled.
        3. Set DUT WFC enabled (WiFi Preferred), verify DUT WFC available,
            report iwlan rat.
        4. Set DUT WFC disabled, verify DUT WFC unavailable,
            not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_volte(self.log, self.dut):
            self.log.error("Failed to setup VoLTE")
            return False
        if not self.change_ims_setting(False, True, True, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, True, True, False, None):
            return False
        return self.change_ims_setting(False, True, True, True, None)

    @test_tracker_info(uuid="d3ffae75-ae4a-4ed8-9337-9155c413311d")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_wifi_connected_toggle_wfc(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Disabled + WiFi Connected, Toggling WFC

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE disabled.
        2. Make sure DUT WiFi connected, WFC disabled.
        3. Set DUT WFC enabled (WiFi Preferred), verify DUT WFC available,
            report iwlan rat.
        4. Set DUT WFC disabled, verify DUT WFC unavailable,
            not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_csfb(self.log, self.dut):
            self.log.error("Failed to setup LTE")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, True, False, False, None):
            return False
        return self.change_ims_setting(False, True, False, True, None)

    @test_tracker_info(uuid="29d2d7b7-1c31-4a2c-896a-3f6756c620ac")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_wifi_connected_toggle_wfc(self):
        """Test for WiFi Calling settings:
        3G + WiFi Connected, Toggling WFC

        Steps:
        1. Setup DUT Idle, 3G network type.
        2. Make sure DUT WiFi connected, WFC disabled.
        3. Set DUT WFC enabled (WiFi Preferred), verify DUT WFC available,
            report iwlan rat.
        4. Set DUT WFC disabled, verify DUT WFC unavailable,
            not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_voice_3g(self.log, self.dut):
            self.log.error("Failed to setup 3G")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, True, False, False, None):
            return False
        return self.change_ims_setting(False, True, False, True, None)

    @test_tracker_info(uuid="ce2c0208-9ea0-4b31-91f4-d06a62cb927a")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wifi_connected_toggle_wfc(self):
        """Test for WiFi Calling settings:
        APM + WiFi Connected, Toggling WFC

        Steps:
        1. Setup DUT Idle, Airplane mode.
        2. Make sure DUT WiFi connected, WFC disabled.
        3. Set DUT WFC enabled (WiFi Preferred), verify DUT WFC available,
            report iwlan rat.
        4. Set DUT WFC disabled, verify DUT WFC unavailable,
            not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not self.change_ims_setting(True, True, True, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(True, True, True, False, None):
            return False
        return self.change_ims_setting(True, True, True, True, None)

    @test_tracker_info(uuid="681e2448-32a2-434d-abd6-0bc2ab5afd9c")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_volte_wfc_enabled_toggle_wifi(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Enabled + WFC enabled, Toggling WiFi

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE enabled.
        2. Make sure DUT WiFi disconnected, WFC enabled (WiFi Preferred).
        3. DUT connect WiFi, verify DUT WFC available, report iwlan rat.
        4. DUT disconnect WiFi,verify DUT WFC unavailable, not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_volte(self.log, self.dut):
            self.log.error("Failed to setup VoLTE")
            return False
        if not self.change_ims_setting(False, True, True, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, True, True, False, None):
            return False
        return self.change_ims_setting(False, True, True, True, None)

    @test_tracker_info(uuid="63922066-9caa-42e6-bc9f-49f5ac01cbe2")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_wfc_enabled_toggle_wifi(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Disabled + WFC enabled, Toggling WiFi

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE disabled.
        2. Make sure DUT WiFi disconnected, WFC enabled (WiFi Preferred).
        3. DUT connect WiFi, verify DUT WFC available, report iwlan rat.
        4. DUT disconnect WiFi,verify DUT WFC unavailable, not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_csfb(self.log, self.dut):
            self.log.error("Failed to setup CSFB")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, False, False, True, None):
            return False
        return self.change_ims_setting(False, True, False, True, None)

    @test_tracker_info(uuid="8a80a446-2116-4b19-b0ef-f771f30a6d15")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_wfc_enabled_toggle_wifi(self):
        """Test for WiFi Calling settings:
        3G + WFC enabled, Toggling WiFi

        Steps:
        1. Setup DUT Idle, 3G network type.
        2. Make sure DUT WiFi disconnected, WFC enabled (WiFi Preferred).
        3. DUT connect WiFi, verify DUT WFC available, report iwlan rat.
        4. DUT disconnect WiFi,verify DUT WFC unavailable, not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not phone_setup_voice_3g(self.log, self.dut):
            self.log.error("Failed to setup 3G")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, False, False, True, None):
            return False
        return self.change_ims_setting(False, True, False, True, None)

    @test_tracker_info(uuid="9889eebf-cde6-4f47-aec0-9cb204fdf2e5")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wfc_enabled_toggle_wifi(self):
        """Test for WiFi Calling settings:
        APM + WFC enabled, Toggling WiFi

        Steps:
        1. Setup DUT Idle, Airplane mode.
        2. Make sure DUT WiFi disconnected, WFC enabled (WiFi Preferred).
        3. DUT connect WiFi, verify DUT WFC available, report iwlan rat.
        4. DUT disconnect WiFi,verify DUT WFC unavailable, not report iwlan rat.

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        """
        if not self.change_ims_setting(True, True, True, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(True, False, True, True, None):
            return False
        return self.change_ims_setting(True, True, True, True, None)

    @test_tracker_info(uuid="9b23e04b-4f70-4e73-88e7-6376262c739d")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_wfc_enabled_wifi_connected_toggle_volte(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Enabled + WiFi Connected + WFC enabled, toggle VoLTE setting

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE enabled.
        2. Make sure DUT WiFi connected, WFC enabled (WiFi Preferred).
            Verify DUT WFC available, report iwlan rat.
        3. Disable VoLTE on DUT, verify in 2 minutes period,
            DUT does not lost WiFi Calling, DUT still report WFC available,
            rat iwlan.
        4. Enable VoLTE on DUT, verify in 2 minutes period,
            DUT does not lost WiFi Calling, DUT still report WFC available,
            rat iwlan.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFi Calling feature bit return True, network rat is iwlan.
        """
        if not phone_setup_volte(self.log, self.dut):
            self.dut.log.error("Failed to setup VoLTE.")
            return False
        if not self.change_ims_setting(False, True, True, True,
                                       self.default_wfc_mode):
            return False
        if not self.change_ims_setting(False, True, False, True, None):
            return False
        return self.change_ims_setting(False, True, True, True, None)

    @test_tracker_info(uuid="04bdfda4-06f7-41df-9352-a8534bc2a67a")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_volte_wfc_wifi_preferred_to_cellular_preferred(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Enabled + WiFi Connected + WiFi Preferred,
        change WFC to Cellular Preferred

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE enabled.
        2. Make sure DUT WiFi connected, WFC is set to WiFi Preferred.
            Verify DUT WFC available, report iwlan rat.
        3. Change WFC setting to Cellular Preferred.
        4. Verify DUT report WFC not available.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFI Calling feature bit return False, network rat is not iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_volte(self.log, self.dut):
            self.dut.log.error("Failed to setup VoLTE.")
            return False
        if not self.change_ims_setting(False, True, True, True,
                                       WFC_MODE_WIFI_PREFERRED):
            return False
        return self.change_ims_setting(False, True, True, True,
                                       WFC_MODE_CELLULAR_PREFERRED)

    @test_tracker_info(uuid="80d26bdb-992a-4b30-ad51-68308d5af168")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_wfc_wifi_preferred_to_cellular_preferred(self):
        """Test for WiFi Calling settings:
        LTE + WiFi Connected + WiFi Preferred, change WFC to Cellular Preferred

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE disabled.
        2. Make sure DUT WiFi connected, WFC is set to WiFi Preferred.
            Verify DUT WFC available, report iwlan rat.
        3. Change WFC setting to Cellular Preferred.
        4. Verify DUT report WFC not available.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFI Calling feature bit return False, network rat is not iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_csfb(self.log, self.dut):
            self.dut.log.error("Failed to setup LTE.")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_WIFI_PREFERRED):
            return False
        return self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_CELLULAR_PREFERRED)

    @test_tracker_info(uuid="d486c7e3-3d2b-4552-8af8-7b19f6347427")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_wfc_wifi_preferred_to_cellular_preferred(self):
        """Test for WiFi Calling settings:
        3G + WiFi Connected + WiFi Preferred, change WFC to Cellular Preferred

        Steps:
        1. Setup DUT Idle, 3G network type.
        2. Make sure DUT WiFi connected, WFC is set to WiFi Preferred.
            Verify DUT WFC available, report iwlan rat.
        3. Change WFC setting to Cellular Preferred.
        4. Verify DUT report WFC not available.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFI Calling feature bit return False, network rat is not iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_voice_3g(self.dut.log, self.dut):
            self.dut.log.error("Failed to setup 3G.")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_WIFI_PREFERRED):
            return False
        return self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_CELLULAR_PREFERRED)

    @test_tracker_info(uuid="0feb0add-8e22-4c86-b13e-be68659cdd87")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wfc_wifi_preferred_to_cellular_preferred(self):
        """Test for WiFi Calling settings:
        APM + WiFi Connected + WiFi Preferred, change WFC to Cellular Preferred

        Steps:
        1. Setup DUT Idle, airplane mode.
        2. Make sure DUT WiFi connected, WFC is set to WiFi Preferred.
            Verify DUT WFC available, report iwlan rat.
        3. Change WFC setting to Cellular Preferred.
        4. Verify DUT report WFC not available.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_WIFI_PREFERRED):
            return False
        return self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_CELLULAR_PREFERRED)

    @test_tracker_info(uuid="9c8f359f-a084-4413-b8a9-34771af166c5")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_volte_wfc_cellular_preferred_to_wifi_preferred(self):
        """Test for WiFi Calling settings:
        LTE + VoLTE Enabled + WiFi Connected + Cellular Preferred,
        change WFC to WiFi Preferred

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE enabled.
        2. Make sure DUT WiFi connected, WFC is set to Cellular Preferred.
            Verify DUT WFC not available.
        3. Change WFC setting to WiFi Preferred.
        4. Verify DUT report WFC available.

        Expected Results:
        2. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        4. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_volte(self.log, self.dut):
            self.dut.log.error("Failed to setup VoLTE.")
            return False
        if not self.change_ims_setting(False, True, True, True,
                                       WFC_MODE_CELLULAR_PREFERRED):
            return False
        return self.change_ims_setting(False, True, True, True,
                                       WFC_MODE_WIFI_PREFERRED)

    @test_tracker_info(uuid="1894e685-63cf-43aa-91ed-938782ca35a9")
    @TelephonyBaseTest.tel_test_wrap
    def test_lte_wfc_cellular_preferred_to_wifi_preferred(self):
        """Test for WiFi Calling settings:
        LTE + WiFi Connected + Cellular Preferred, change WFC to WiFi Preferred

        Steps:
        1. Setup DUT Idle, LTE network type, VoLTE disabled.
        2. Make sure DUT WiFi connected, WFC is set to Cellular Preferred.
            Verify DUT WFC not available.
        3. Change WFC setting to WiFi Preferred.
        4. Verify DUT report WFC available.

        Expected Results:
        2. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        4. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_csfb(self.log, self.dut):
            self.dut.log.error("Failed to setup LTE.")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_CELLULAR_PREFERRED):
            return False
        return self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_WIFI_PREFERRED)

    @test_tracker_info(uuid="e7fb6a6c-4672-44da-bca2-78b4d96dea9e")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_wfc_cellular_preferred_to_wifi_preferred(self):
        """Test for WiFi Calling settings:
        3G + WiFi Connected + Cellular Preferred, change WFC to WiFi Preferred

        Steps:
        1. Setup DUT Idle, 3G network type.
        2. Make sure DUT WiFi connected, WFC is set to Cellular Preferred.
            Verify DUT WFC not available.
        3. Change WFC setting to WiFi Preferred.
        4. Verify DUT report WFC available.

        Expected Results:
        2. DUT WiFi Calling feature bit return False, network rat is not iwlan.
        4. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not phone_setup_voice_3g(self.log, self.dut):
            self.dut.log.error("Failed to setup 3G.")
            return False
        if not self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_CELLULAR_PREFERRED):
            return False
        return self.change_ims_setting(False, True, False, True,
                                       WFC_MODE_WIFI_PREFERRED)

    @test_tracker_info(uuid="46262b2d-5de9-4984-87e8-42f44469289e")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wfc_cellular_preferred_to_wifi_preferred(self):
        """Test for WiFi Calling settings:
        APM + WiFi Connected + Cellular Preferred, change WFC to WiFi Preferred

        Steps:
        1. Setup DUT Idle, airplane mode.
        2. Make sure DUT WiFi connected, WFC is set to Cellular Preferred.
            Verify DUT WFC not available.
        3. Change WFC setting to WiFi Preferred.
        4. Verify DUT report WFC available.

        Expected Results:
        2. DUT WiFi Calling feature bit return True, network rat is iwlan.
        4. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if WFC_MODE_WIFI_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip("WFC_MODE_WIFI_PREFERRED is not supported")
        if WFC_MODE_CELLULAR_PREFERRED not in self.dut_wfc_modes:
            raise signals.TestSkip(
                "WFC_MODE_CELLULAR_PREFERRED is not supported")
        if not self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_CELLULAR_PREFERRED):
            return False
        return self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_WIFI_PREFERRED)

    @test_tracker_info(uuid="5b514f51-fed9-475e-99d3-17d2165e11a1")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wfc_wifi_preferred_turn_off_apm(self):
        """Test for WiFi Calling settings:
        APM + WiFi Connected + WiFi Preferred + turn off APM

        Steps:
        1. Setup DUT Idle in Airplane mode.
        2. Make sure DUT WiFi connected, set WFC mode to WiFi preferred.
        3. verify DUT WFC available, report iwlan rat.
        4. Turn off airplane mode.
        5. Verify DUT WFC still available, report iwlan rat

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        5. DUT WiFI Calling feature bit return True, network rat is iwlan.
        """
        if not self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_WIFI_PREFERRED):
            return False
        return self.change_ims_setting(False, True, True, True, None)

    @test_tracker_info(uuid="f328cff2-9dec-44b3-ba74-a662b76fcf2a")
    @TelephonyBaseTest.tel_test_wrap
    def test_apm_wfc_cellular_preferred_turn_off_apm(self):
        """Test for WiFi Calling settings:
        APM + WiFi Connected + Cellular Preferred + turn off APM

        Steps:
        1. Setup DUT Idle in Airplane mode.
        2. Make sure DUT WiFi connected, set WFC mode to Cellular preferred.
        3. verify DUT WFC available, report iwlan rat.
        4. Turn off airplane mode.
        5. Verify DUT WFC not available, not report iwlan rat

        Expected Results:
        3. DUT WiFi Calling feature bit return True, network rat is iwlan.
        5. DUT WiFI Calling feature bit return False, network rat is not iwlan.
        """
        if not self.change_ims_setting(True, True, True, True,
                                       WFC_MODE_CELLULAR_PREFERRED):
            return False
        return self.change_ims_setting(False, True, True, True, None)

    @test_tracker_info(uuid="7e30d219-42ee-4309-a95c-2b45b8831d26")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_setup_timing(self):
        """ Measures the time delay in enabling WiFi calling

        Steps:
        1. Make sure DUT idle.
        2. Turn on Airplane Mode, Set WiFi Calling to WiFi_Preferred.
        3. Turn on WiFi, connect to WiFi AP and measure time delay.
        4. Wait for WiFi connected, verify Internet and measure time delay.
        5. Wait for rat to be reported as iwlan and measure time delay.
        6. Wait for ims registered and measure time delay.
        7. Wait for WiFi Calling feature bit to be True and measure time delay.

        Expected results:
        Time Delay in each step should be within pre-defined limit.

        Returns:
            Currently always return True.
        """
        # TODO: b/26338119 Set pass/fail criteria
        time_values = {
            'start': 0,
            'wifi_enabled': 0,
            'wifi_connected': 0,
            'wifi_data': 0,
            'iwlan_rat': 0,
            'ims_registered': 0,
            'wfc_enabled': 0,
            'mo_call_success': 0
        }

        wifi_reset(self.log, self.dut)
        toggle_airplane_mode_by_adb(self.log, self.dut, True)

        set_wfc_mode(self.log, self.dut, WFC_MODE_WIFI_PREFERRED)

        time_values['start'] = time.time()

        self.dut.log.info("Start Time %ss", time_values['start'])

        wifi_toggle_state(self.log, self.dut, True)
        time_values['wifi_enabled'] = time.time()
        self.dut.log.info("WiFi Enabled After %ss",
                          time_values['wifi_enabled'] - time_values['start'])

        network = {WIFI_SSID_KEY: self.wifi_network_ssid}
        if self.wifi_network_pass:
            network[WIFI_PWD_KEY] = self.wifi_network_pass
        try:
            self.dut.droid.wifiConnectByConfig(network)
        except Exception:
            self.dut.log.info("Connecting to wifi by RPC wifiConnect instead")
            self.dut.droid.wifiConnect(network)
        self.dut.droid.wakeUpNow()

        if not wait_for_wifi_data_connection(self.log, self.dut, True,
                                             MAX_WAIT_TIME_WIFI_CONNECTION):
            self.dut.log.error("Failed WiFi connection, aborting!")
            return False
        time_values['wifi_connected'] = time.time()

        self.dut.log.info(
            "WiFi Connected After %ss",
            time_values['wifi_connected'] - time_values['wifi_enabled'])

        if not verify_internet_connection(self.log, self.dut, retries=3):
            self.dut.log.error("Failed to get user-plane traffic, aborting!")
            return False

        time_values['wifi_data'] = time.time()
        self.dut.log.info(
            "WifiData After %ss",
            time_values['wifi_data'] - time_values['wifi_connected'])

        if not wait_for_network_rat(
                self.log,
                self.dut,
                RAT_FAMILY_WLAN,
                voice_or_data=NETWORK_SERVICE_DATA):
            self.dut.log.error("Failed to set-up iwlan, aborting!")
            if is_droid_in_rat_family(self.log, self.dut, RAT_FAMILY_WLAN,
                                      NETWORK_SERVICE_DATA):
                self.dut.log.error(
                    "Never received the event, but droid in iwlan")
            else:
                return False
        time_values['iwlan_rat'] = time.time()
        self.dut.log.info("iWLAN Reported After %ss",
                          time_values['iwlan_rat'] - time_values['wifi_data'])

        if not wait_for_ims_registered(self.log, self.dut,
                                       MAX_WAIT_TIME_IMS_REGISTRATION):
            self.dut.log.error("Never received IMS registered, aborting")
            return False
        time_values['ims_registered'] = time.time()
        self.dut.log.info(
            "Ims Registered After %ss",
            time_values['ims_registered'] - time_values['iwlan_rat'])

        if not wait_for_wfc_enabled(self.log, self.dut,
                                    MAX_WAIT_TIME_WFC_ENABLED):
            self.dut.log.error("Never received WFC feature, aborting")
            return False

        time_values['wfc_enabled'] = time.time()
        self.dut.log.info(
            "Wifi Calling Feature Enabled After %ss",
            time_values['wfc_enabled'] - time_values['ims_registered'])

        set_wfc_mode(self.log, self.dut, WFC_MODE_DISABLED)

        wait_for_not_network_rat(
            self.log,
            self.dut,
            RAT_FAMILY_WLAN,
            voice_or_data=NETWORK_SERVICE_DATA)

        self.dut.log.info("\n\n------------------summary-----------------")
        self.dut.log.info("WiFi Enabled After %.2f seconds",
                          time_values['wifi_enabled'] - time_values['start'])
        self.dut.log.info(
            "WiFi Connected After %.2f seconds",
            time_values['wifi_connected'] - time_values['wifi_enabled'])
        self.dut.log.info(
            "WifiData After %.2f s",
            time_values['wifi_data'] - time_values['wifi_connected'])
        self.dut.log.info("iWLAN Reported After %.2f seconds",
                          time_values['iwlan_rat'] - time_values['wifi_data'])
        self.dut.log.info(
            "Ims Registered After %.2f seconds",
            time_values['ims_registered'] - time_values['iwlan_rat'])
        self.dut.log.info(
            "Wifi Calling Feature Enabled After %.2f seconds",
            time_values['wfc_enabled'] - time_values['ims_registered'])
        self.dut.log.info("\n\n")
        return True

    @test_tracker_info(uuid="135301ea-6d00-4233-98fd-cda706d61eb2")
    @TelephonyBaseTest.tel_test_wrap
    def test_ims_factory_reset(self):
        """Test VOLTE and WFC reset to factory default.

        Steps:
        1. Setup VoLTE, WFC, APM is various mode.
        2. Call IMS factory reset.
        3. Verify VoLTE and WFC are back to factory default.
        4. Verify VoLTE, WFC Voice call can be made successful if enabled.

        """
        result = True
        wifi_enabled = True
        for airplane_mode in (True, False):
            for volte_enabled in (True, False):
                for wfc_enabled in (True, False):
                    if wfc_enabled:
                        wfc_modes = self.dut_wfc_modes
                    else:
                        wfc_modes = [None]
                    for wfc_mode in wfc_modes:
                        if not self.change_ims_setting(
                                airplane_mode, wifi_enabled, volte_enabled,
                                wfc_enabled, wfc_mode):
                            result = False
                        self.dut.log.info("Call IMS factory reset")
                        self.dut.droid.telephonyFactoryReset()
                        if not self.verify_default_ims_setting():
                            result = False
        return result

    @test_tracker_info(uuid="ce60740f-4d8e-4013-a7cf-65589e8a0893")
    @TelephonyBaseTest.tel_test_wrap
    def test_factory_reset_by_fastboot_wipe(self):
        """Verify the network setting after factory reset by wipe.

        Steps:
        1. Config VoLTE, WFC, APM, data_roamingn, mobile_data,
           preferred_network_mode to non-factory default.
        2. Factory reset by fastboot wipe.
        3. Verify network configs back to factory default.

        """
        self.dut.log.info("Set VoLTE off, WFC wifi preferred, APM on")
        toggle_volte(self.log, self.dut, False)
        revert_default_telephony_setting(self.dut)
        self.change_ims_setting(True, True, False, True,
                                WFC_MODE_WIFI_PREFERRED)
        self.dut.log.info("Wipe in fastboot")
        fastboot_wipe(self.dut)
        return verify_default_telephony_setting(
            self.dut) and (self.verify_default_ims_setting())
