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
    Test Script for Telephony Locked SIM Emergency Call Test
"""

from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import DEFAULT_DEVICE_PASSWORD
from acts.test_utils.tel.tel_defines import GEN_2G
from acts.test_utils.tel.tel_defines import GEN_3G
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_lookup_tables import \
    network_preference_for_generation
from acts.test_utils.tel.tel_lookup_tables import operator_capabilities
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import get_sim_state
from acts.test_utils.tel.tel_test_utils import is_sim_lock_enabled
from acts.test_utils.tel.tel_test_utils import is_sim_locked
from acts.test_utils.tel.tel_test_utils import reboot_device
from acts.test_utils.tel.tel_test_utils import reset_device_password
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from TelLiveEmergencyBase import TelLiveEmergencyBase

EXPECTED_CALL_TEST_RESULT = False


class TelLiveLockedSimTest(TelLiveEmergencyBase):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)
        for ad in self.my_devices:
            if not is_sim_lock_enabled(ad):
                ad.log.info("SIM is not locked")
            else:
                ad.log.info("SIM is locked")
                self.setup_dut(ad)
                return True
        #if there is no locked SIM, reboot the device and check again
        for ad in self.my_devices:
            reboot_device(ad)
            reset_device_password(ad, None)
            if not is_sim_lock_enabled(ad):
                ad.log.info("SIM is not locked")
            else:
                ad.log.info("SIM is locked, set as DUT")
                self.setup_dut(ad)
                return True
        self.log.error("There is no locked SIM in this testbed")
        raise signals.TestAbortClass("No device meets locked SIM requirement")

    def setup_test(self):
        self.expected_call_result = False
        if "wfc" in self.test_name:
            if CAPABILITY_WFC not in operator_capabilities.get(
                    self.dut_operator, operator_capabilities["default"]):
                raise signals.TestSkip(
                    "WFC is not supported for carrier %s" % self.dut_operator)
            unlock_sim(self.dut)
            if "apm_off" not in self.test_name:
                if self.dut_operator != "tmo":
                    raise signals.TestSkip(
                        "WFC in non-APM is not supported for carrier %s" %
                        self.dut_operator)
                if not phone_setup_iwlan(
                        self.log, self.dut, False, WFC_MODE_WIFI_PREFERRED,
                        self.wifi_network_ssid, self.wifi_network_pass):
                    self.dut.log.error("Failed to setup WFC in non-APM.")
                    return False
            else:
                if not phone_setup_iwlan(
                        self.log, self.dut, True, WFC_MODE_WIFI_PREFERRED,
                        self.wifi_network_ssid, self.wifi_network_pass):
                    self.dut.log.error("Failed to setup WFC in APM.")
                    return False
        if not is_sim_locked(self.dut):
            self.dut.reboot(stop_at_lock_screen=True)
            try:
                droid, ed = self.dut.get_droid()
                ed.start()
            except:
                self.dut.log.warning("Failed to start sl4a!")
        self.dut.log.info("SIM at state %s", get_sim_state(self.dut))

    """ Tests Begin """

    @test_tracker_info(uuid="fd7fb69c-6fd4-4874-a4ca-769353b9db25")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_locked_sim(self):
        """Test emergency call with emergency dialer in user account.

        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add storyline number to system emergency number list.
        Use the emergency dialer to call "611".
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="a0b3e7dd-93e0-40e2-99a9-5564d34712fc")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_csfb_locked_sim(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in CSFB
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with storyline number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_4G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)
        self.set_ims_first("false")

        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="b5a5b550-49e6-4026-902a-b155d1209f6d")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_3g_locked_sim(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 3G
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add a fake emergency number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.
        Verify DUT in emergency call back mode.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_3G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="5f083fa7-ddea-44de-8479-4da88d53da65")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_2g_locked_sim(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 2G
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency.
        Verify DUT has in call activity.
        Verify DUT in emergency call back mode.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_2G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="e01870d7-89a6-4641-84c6-8e71142773f8")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_locked_sim(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM on.
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.
        Verify DUT in emergency call back mode.

        Returns:
            True if success.
            False if failed.
        """
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="b2c3de31-79ec-457a-a947-50c28caec214")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_off_locked_sim(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM off.
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.
        Verify DUT in emergency call back mode.

        Returns:
            True if success.
            False if failed.
        """
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="669cf1d9-9513-4f90-b0fd-2f0e8f1cc941")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_dialer_locked_sim(self):
        """Test emergency call with dialer.

        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with fake emergency number.
        Call storyline by dialer.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.fake_emergency_call_test(
            by_emergency_dialer=True) and self.check_normal_call()

    @test_tracker_info(uuid="1990f166-66a7-4092-b448-c179a9194371")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_apm_locked_sim(self):
        """Test emergency call with emergency dialer in airplane mode.

        Enable airplane mode.
        Enable SIM lock on the SIM. Reboot device to SIM pin request page.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="7ffdad34-b8fb-41b0-b0fd-2def5adc67bc")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_locked_sim(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable SIM lock on the SIM.
        Enable device password and then reboot upto password and pin query stage.
        Add system emergency number list with storyline number.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        self.dut.log.info("Set screen lock pin")
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.log.info("Reboot device to screen lock screen")
        self.dut.reboot(stop_at_lock_screen=True)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="12dc1eb6-50ed-4ad9-b195-5d96c6b6952e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_apm_locked_sim(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and airplane mode
        Enable SIM lock on the SIM.
        Reboot upto pin query window.
        Add system emergency number list with story line.
        Use the emergency dialer to call story line.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        self.dut.log.info("Set screen lock pin")
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.log.info("Reboot device to screen lock screen")
        self.dut.reboot(stop_at_lock_screen=True)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="1e01927a-a077-466d-8bf8-52dca87ab87c")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_setupwizard_locked_sim(self):
        """Test emergency call with emergency dialer in setupwizard.

        Enable SIM lock on the SIM.
        Wipe the device and then reboot upto setupwizard.
        Add system emergency number list with story line.
        Use the emergency dialer to call story line.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        if not fastboot_wipe(self.dut, skip_setup_wizard=False):
            return False
        return self.fake_emergency_call_test() and self.check_normal_call()


""" Tests End """
