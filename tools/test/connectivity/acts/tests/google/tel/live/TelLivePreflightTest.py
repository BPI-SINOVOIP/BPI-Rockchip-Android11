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
    Test Script for Telephony Pre Flight check.
"""

from acts import signals
from acts import utils

from acts.controllers.android_device import get_info
from acts.libs.ota import ota_updater
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import RAT_FAMILY_WLAN
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_test_utils import abort_all_tests
from acts.test_utils.tel.tel_test_utils import call_setup_teardown
from acts.test_utils.tel.tel_test_utils import ensure_phones_default_state
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import get_user_config_profile
from acts.test_utils.tel.tel_test_utils import is_sim_locked
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import wait_for_network_rat
from acts.test_utils.tel.tel_test_utils import wait_for_wfc_enabled
from acts.test_utils.tel.tel_test_utils import wait_for_wifi_data_connection
from acts.test_utils.tel.tel_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_iwlan
from acts.test_utils.tel.tel_voice_utils import is_phone_in_call_volte
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan_cellular_preferred


class TelLivePreflightTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.user_params["telephony_auto_rerun"] = 0

    def _check_wfc_enabled(self, ad):
        if not wait_for_wifi_data_connection(self.log, ad, True):
            ad.log.error("Failed to connect to WIFI")
            return False
        if not wait_for_wfc_enabled(self.log, ad):
            ad.log.error("WFC is not enabled")
            return False
        if not wait_for_network_rat(
                self.log, ad, RAT_FAMILY_WLAN,
                voice_or_data=NETWORK_SERVICE_DATA):
            ad.log.info("Data rat can not go to iwlan mode successfully")
            return False
        return True

    def _get_call_verification_function(self, ad):
        user_profile = get_user_config_profile(ad)
        if user_profile.get("WFC Enabled", False):
            return is_phone_in_call_iwlan
        if user_profile.get("VoLTE Enabled", False):
            return is_phone_in_call_volte
        return None

    def _set_user_profile_before_ota(self):
        ad = self.android_devices[0]
        if not phone_setup_iwlan_cellular_preferred(
                self.log, ad, self.wifi_network_ssid, self.wifi_network_pass):
            ad.log.error("Failed to setup WFC to %s.",
                         WFC_MODE_CELLULAR_PREFERRED)
        if len(self.android_devices) > 1:
            ad = self.android_devices[1]
            if not phone_setup_iwlan(
                    self.log, ad, True, WFC_MODE_WIFI_PREFERRED,
                    self.wifi_network_ssid, self.wifi_network_pass):
                ad.log.error("Failed to setup WFC to %s.",
                             WFC_MODE_WIFI_PREFERRED)

        if ad.droid.imsIsEnhanced4gLteModeSettingEnabledByPlatform():
            ad.droid.imsSetEnhanced4gMode(False)
            state = ad.droid.imsIsEnhanced4gLteModeSettingEnabledByUser()
            ad.log.info("Enhanced 4G LTE Setting is %s", "on"
                        if state else "off")
            if state:
                ad.log.error("Expecting Enhanced 4G LTE Setting off")

    def _ota_upgrade(self, ad):
        result = True
        self._set_user_profile_before_ota()
        config_profile_before = get_user_config_profile(ad)
        ad.log.info("Before OTA user config is: %s", config_profile_before)
        try:
            ota_updater.update(ad)
        except Exception as e:
            ad.log.error("OTA upgrade failed with %s", e)
            raise
        if is_sim_locked(ad):
            ad.log.info("After OTA, SIM keeps the locked state")
        elif getattr(ad, "is_sim_locked", False):
            ad.log.error("After OTA, SIM loses the locked state")
            result = False
        if not unlock_sim(ad):
            ad.log.error(ad.log, "unable to unlock SIM")
        if not ad.droid.connectivityCheckAirplaneMode():
            if not ensure_phone_subscription(self.log, ad):
                ad.log.error("Subscription check failed")
                result = False
        if config_profile_before.get("WFC Enabled", False):
            self._check_wfc_enabled(ad)
            result = False
        config_profile_after = get_user_config_profile(ad)
        ad.log.info("After OTA user config is: %s", config_profile_after)
        if config_profile_before != config_profile_after:
            ad.log.error("Before: %s, After: %s", config_profile_before,
                         config_profile_after)
            ad.log.error("User config profile changed after OTA")
            result = False
        return result

    """ Tests Begin """

    @test_tracker_info(uuid="cb897221-99e1-4697-927e-02d92d969440")
    @TelephonyBaseTest.tel_test_wrap
    def test_ota_upgrade(self):
        ota_package = self.user_params.get("ota_package")
        if isinstance(ota_package, list):
            ota_package = ota_package[0]
        if ota_package and "dev/null" not in ota_package:
            self.log.info("Upgrade with ota_package %s", ota_package)
            self.log.info("Before OTA upgrade: %s",
                          get_info(self.android_devices))
        else:
            raise signals.TestSkip("No ota_package is defined")
        ota_util = self.user_params.get("ota_util")
        if isinstance(ota_util, list):
            ota_util = ota_util[0]
        if ota_util:
            if "update_engine_client.zip" in ota_util:
                self.user_params["UpdateDeviceOtaTool"] = ota_util
                self.user_params["ota_tool"] = "UpdateDeviceOtaTool"
            else:
                self.user_params["AdbSideloadOtaTool"] = ota_util
                self.user_params["ota_tool"] = "AdbSideloadOtaTool"
        self.log.info("OTA upgrade with %s by %s", ota_package,
                      self.user_params["ota_tool"])
        ota_updater.initialize(self.user_params, self.android_devices)
        tasks = [(self._ota_upgrade, [ad]) for ad in self.android_devices]
        try:
            result = multithread_func(self.log, tasks)
        except Exception as err:
            abort_all_tests(self.log, "Unable to do ota upgrade: %s" % err)
        device_info = get_info(self.android_devices)
        self.log.info("After OTA upgrade: %s", device_info)
        self.results.add_controller_info("AndroidDevice", device_info)
        if len(self.android_devices) > 1:
            caller = self.android_devices[0]
            callee = self.android_devices[1]
            return call_setup_teardown(
                self.log, caller, callee, caller,
                self._get_call_verification_function(caller),
                self._get_call_verification_function(callee)) and result
        return result

    @test_tracker_info(uuid="8390a2eb-a744-4cda-bade-f94a2cc83f02")
    @TelephonyBaseTest.tel_test_wrap
    def test_check_environment(self):
        ad = self.android_devices[0]
        # Check WiFi environment.
        # 1. Connect to WiFi.
        # 2. Check WiFi have Internet access.
        try:
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                abort_all_tests(ad.log, "WiFi connect fail")
            if (not wait_for_wifi_data_connection(self.log, ad, True)
                    or not verify_internet_connection(self.log, ad)):
                abort_all_tests(ad.log, "Data not available on WiFi")
        finally:
            wifi_toggle_state(self.log, ad, False)
        # TODO: add more environment check here.
        return True

    @test_tracker_info(uuid="7bb23ac7-6b7b-4d5e-b8d6-9dd10032b9ad")
    @TelephonyBaseTest.tel_test_wrap
    def test_pre_flight_check(self):
        return ensure_phones_default_state(self.log, self.android_devices)

    @test_tracker_info(uuid="1070b160-902b-43bf-92a0-92cc2d05bb13")
    @TelephonyBaseTest.tel_test_wrap
    def test_check_crash(self):
        result = True
        begin_time = None
        for ad in self.android_devices:
            output = ad.adb.shell("cat /proc/uptime")
            epoch_up_time = utils.get_current_epoch_time() - 1000 * float(
                output.split(" ")[0])
            ad.crash_report_preflight = ad.check_crash_report(
                self.test_name,
                begin_time=epoch_up_time,
                log_crash_report=True)
            if ad.crash_report_preflight:
                msg = "Find crash reports %s before test starts" % (
                    ad.crash_report_preflight)
                ad.log.warn(msg)
                result = False
        return result
