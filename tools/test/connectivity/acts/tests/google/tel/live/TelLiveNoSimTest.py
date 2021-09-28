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
    Test Script for Telephony Pre Check In Sanity
"""

from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import DEFAULT_DEVICE_PASSWORD
from acts.test_utils.tel.tel_defines import GEN_2G
from acts.test_utils.tel.tel_defines import GEN_3G
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import SIM_STATE_ABSENT
from acts.test_utils.tel.tel_defines import SIM_STATE_UNKNOWN
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import get_sim_state
from acts.test_utils.tel.tel_lookup_tables import \
    network_preference_for_generation
from acts.test_utils.tel.tel_test_utils import reset_device_password
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from TelLiveEmergencyBase import TelLiveEmergencyBase


class TelLiveNoSimTest(TelLiveEmergencyBase):
    def setup_class(self):
        for ad in self.my_devices:
            sim_state = get_sim_state(ad)
            ad.log.info("Sim state is %s", sim_state)
            if sim_state in (SIM_STATE_ABSENT, SIM_STATE_UNKNOWN):
                ad.log.info("Device has no or unknown SIM in it, set as DUT")
                self.setup_dut(ad)
                return True
        self.log.error("No device meets SIM absent or unknown requirement")
        raise signals.TestAbortClass(
            "No device meets SIM absent or unknown requirement")

    def setup_test(self):
        self.expected_call_result = False

    def teardown_test(self):
        self.dut.ensure_screen_on()
        self.dut.exit_setup_wizard()
        reset_device_password(self.dut, None)

    """ Tests Begin """

    @test_tracker_info(uuid="91bc0c02-c1f2-4112-a7f8-c91617bff53e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_no_sim(self):
        """Test fake emergency call with emergency dialer in user account.

        Add fake emergency number to system emergency number list.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="fccb7d1d-279d-4933-8ffe-528bd7fdf5f1")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_csfb_no_sim(self):
        """Change network preference to use 4G.

        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_4G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)

        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="de6332f9-9682-45ee-b8fd-99a24c821ca9")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_3g_no_sim(self):
        """Change network preference to use 3G.

        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_3G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)

        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="d173f152-a43a-4be8-a289-affc0817ca8e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_2g_no_sim(self):
        """Change network preference to use 2G.

        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        network_preference = network_preference_for_generation(
            GEN_2G, None, self.dut.droid.telephonyGetPhoneType())
        self.dut.log.info("Set network preference to %s", network_preference)
        self.dut.droid.telephonySetPreferredNetworkTypes(network_preference)

        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="cdf7ddad-480f-4757-83bd-a74321b799f7")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_dialer_no_sim(self):
        """Test emergency call with dialer.

        Add storyline number to system emergency number list.
        Call storyline by dialer.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        return self.fake_emergency_call_test(by_emergency_dialer=False)

    @test_tracker_info(uuid="e147960a-4227-41e2-bd06-65001ad5e0cd")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_apm_no_sim(self):
        """Test emergency call with emergency dialer in airplane mode.

        Enable airplane mode.
        Add storyline number to system emergency number list.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="34068bc8-bfa0-4c7b-9450-e189a0b93c8a")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_no_sim(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add storyline number to system emergency number list.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.reboot(stop_at_lock_screen=True)
        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="1ef97f8a-eb3d-45b7-b947-ac409bb70587")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_apm_no_sim(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add storyline number to system emergency number list.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.reboot(stop_at_lock_screen=True)
        return self.fake_emergency_call_test()

    @test_tracker_info(uuid="50f8b3d9-b126-4419-b5e5-b37b850deb8e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_setupwizard_no_sim(self):
        """Test emergency call with emergency dialer in setupwizard.

        Wipe the device and then reboot upto setupwizard.
        Add storyline number to system emergency number list.
        Use the emergency dialer to call storyline.
        Verify DUT has in call activity.

        Returns:
            True if success.
            False if failed.
        """
        if not fastboot_wipe(self.dut, skip_setup_wizard=False):
            return False
        return self.fake_emergency_call_test()


""" Tests End """
