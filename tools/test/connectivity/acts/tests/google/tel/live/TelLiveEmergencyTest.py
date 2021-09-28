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
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import DEFAULT_DEVICE_PASSWORD
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_lookup_tables import operator_capabilities
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import reset_device_password
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import wait_for_sim_ready_by_adb
from acts.test_utils.tel.tel_voice_utils import phone_setup_csfb
from acts.test_utils.tel.tel_voice_utils import phone_setup_iwlan
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_3g
from acts.test_utils.tel.tel_voice_utils import phone_setup_voice_2g
from TelLiveEmergencyBase import TelLiveEmergencyBase


class TelLiveEmergencyTest(TelLiveEmergencyBase):
    """ Tests Begin """

    @test_tracker_info(uuid="fe75ba2c-e4ea-4fc1-881b-97e7a9a7f48e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer(self):
        """Test emergency call with emergency dialer in user account.

        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Hung up fake emergency call in ringing.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="eb1fa042-518a-4ddb-8e9f-16a6c39c49f1")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_csfb(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in CSFB
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_csfb(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="7a55991a-adc0-432c-b705-8ac9ee249323")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_3g(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 3G
        Add a fake emergency number to emergency number list.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_voice_3g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="cc40611b-6fe5-4952-8bdd-c15d6d995516")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_2g(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 2G
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "2G is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_voice_2g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="a209864c-93fc-455c-aa81-8d3a83f6ad7c")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM on.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Turn off emergency call IMS first.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, True, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="be654073-0107-4b67-a5df-f25ebec7d93e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_off(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM off.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "WFC in non-APM is not supported for carrier %s" %
                self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, False, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        self.set_ims_first("false")
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="8a0978a8-d93e-4f6a-99fe-d0e28bf1be2a")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_dialer(self):
        """Test emergency call with dialer.

        Add system emergency number list with fake emergency number.
        Use dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        return self.fake_emergency_call_test(
            by_emergency_dialer=False) and self.check_emergency_call_back_mode(
                by_emergency_dialer=False)

    @test_tracker_info(uuid="2e6fcc75-ff9e-47b1-9ae8-ed6f9966d0f5")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_apm(self):
        """Test emergency call with emergency dialer in airplane mode.

        Enable airplane mode.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="469bfa60-6e8f-4159-af1f-ab6244073079")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring to DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        self.dut.reboot(stop_at_lock_screen=True)
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="17401c57-0dc2-49b5-b954-a94dbb2d5ad0")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_apm(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.reboot(stop_at_lock_screen=True)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="ccea13ae-6951-4790-a5f7-b5b7a2451c6c")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_setupwizard(self):
        """Test emergency call with emergency dialer in setupwizard.

        Wipe the device and then reboot upto setupwizard.
        Add system emergency number list with fake emergency number.
        Use the emergency dialer to call fake emergency number.
        Verify DUT has in call activity.
        Bring DUT to normal state and make regular call.

        Returns:
            True if success.
            False if failed.
        """
        if not fastboot_wipe(self.dut, skip_setup_wizard=False):
            return False
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.fake_emergency_call_test() and self.check_normal_call()

    @test_tracker_info(uuid="b138da45-45fa-4202-8f19-f8e598e535e1")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_csfb_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in CSFB
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_csfb(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="8199eab0-3656-4fc3-8e9c-7063c24f72c9")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_3g_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 3G
        Add a fake emergency number to emergency number list.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_voice_3g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="6a8ff7e6-29d0-48db-8d27-a7b39aaf4470")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_2g_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 2G
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "2G is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_voice_2g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="583b89c8-be9d-407b-9491-7cc44b4a9d9a")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM on.
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, True, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="a5ac18db-58ae-41b0-b6a8-ed1bdfa8447e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_off_ecbm(self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM off.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "WFC in non-APM is not supported for carrier %s" %
                self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, False, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="1469db59-36f4-4b01-a05a-35ee60be22bc")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_dialer_ecbm(self):
        """Test emergency call with dialer in user account.

        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Use the dialer to call fake emergency number and hung up before connected.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="dca5945d-2847-4662-bab8-583f35a4514e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_apm_ecbm(self):
        """Test emergency call with emergency dialer in airplane mode.

        Enable airplane mode.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Call fake emergency number and hung up before connected.
        Verify DUT voice and data in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="752afef4-bea9-4859-b352-8075f036f859")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_ecbm(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        self.dut.reboot(stop_at_lock_screen=True)
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="45193127-a155-4281-b126-bfef79d70064")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_apm_ecbm(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Call fake emergency number and hung up before connected.
        Verify DUT call and data activity in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.reboot(stop_at_lock_screen=True)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="ccb9ce63-f04f-480e-8c19-43d2a8c3d60f")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_setupwizard_ecbm(self):
        """Test emergency call with emergency dialer in setupwizard.

        Wipe the device and then reboot upto setupwizard.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool true.
        Call fake emergency number and hung up before connected.
        Verify DUT call and data activity in ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not fastboot_wipe(self.dut, skip_setup_wizard=False):
            return False
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.check_emergency_call_back_mode()

    @test_tracker_info(uuid="e0fd7a16-7b3a-4cd9-ab5f-b7e49a531232")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="d08daf23-5efc-43ed-8530-e54fc141f555")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_csfb_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in CSFB
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Turn off emergency call IMS first.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_csfb(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="4ea2f8bb-ce0f-4c7a-b4b2-07259a5c371a")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_3g_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 3G
        Add a fake emergency number to emergency number list.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not phone_setup_voice_3g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="ff11b2cb-0425-4a0b-8748-7ecff0148fe4")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_2g_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in 2G
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "2G is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_voice_2g(self.log, self.dut):
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="13296bdc-4d56-47d3-9d97-00474ca13608")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM on.
        Add system emergency number list with fake emergency number.
        Turn off emergency call IMS first.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, True, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="9179e64a-9236-4b79-b7b3-c5127dd3c21a")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_emergency_dialer_wfc_apm_off_with_ecbm_call_block(
            self):
        """Test emergency call with emergency dialer in user account.

        Configure DUT in WFC APM off.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the emergency dialer to call fake emergency number and hung up before connect.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if CAPABILITY_WFC not in operator_capabilities.get(
                self.dut_operator, operator_capabilities["default"]):
            raise signals.TestSkip(
                "WFC is not supported for carrier %s" % self.dut_operator)
        if self.dut_operator != "tmo":
            raise signals.TestSkip(
                "WFC in non-APM is not supported for carrier %s" %
                self.dut_operator)
        if not phone_setup_iwlan(
                self.log, self.dut, False, WFC_MODE_WIFI_PREFERRED,
                self.wifi_network_ssid, self.wifi_network_pass):
            self.dut.log.error("Failed to setup WFC.")
            return False
        self.set_ims_first("false")
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="8deea62b-a7e8-4747-91c6-9abe3317d0d9")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_by_dialer_with_ecbm_call_block(self):
        """Test emergency call with dialer in user account.

        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Use the dialer to call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        return self.check_emergency_call_back_mode(
            by_emergency_dialer=False, non_emergency_call_allowed=False)

    @test_tracker_info(uuid="561d0a61-84c8-40e3-b2d7-72e221ccf83e")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_apm_with_ecbm_call_block(self):
        """Test emergency call with emergency dialer in airplane mode.

        Enable airplane mode.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="7b8dc0ae-9e2f-456a-bc88-3c920ba0ca71")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_with_ecbm_call_block(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, False)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        self.dut.reboot(stop_at_lock_screen=True)
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="fcdc1510-f60c-4f4e-9be1-a6123365b8ae")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_screen_lock_apm_with_ecbm_call_block(self):
        """Test emergency call with emergency dialer in screen lock phase.

        Enable device password and then reboot upto password query window.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        toggle_airplane_mode_by_adb(self.log, self.dut, True)
        reset_device_password(self.dut, DEFAULT_DEVICE_PASSWORD)
        self.dut.reboot(stop_at_lock_screen=True)
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)

    @test_tracker_info(uuid="85300bba-055c-4ca7-b011-bf1023ffda72")
    @TelephonyBaseTest.tel_test_wrap
    def test_fake_emergency_call_in_setupwizard_with_ecbm_call_block(self):
        """Test emergency call with emergency dialer in setupwizard.

        Wipe the device and then reboot upto setupwizard.
        Add system emergency number list with fake emergency number.
        Configure allow_non_emergency_calls_in_ecm_bool false.
        Call fake emergency number and hung up before connected.
        Verify DUT cannot make call in ecbm mode.
        Verify DUT can make call out of ecbm mode.

        Returns:
            True if success.
            False if failed.
        """
        if not fastboot_wipe(self.dut, skip_setup_wizard=False):
            return False
        if not wait_for_sim_ready_by_adb(self.log, self.dut):
            self.dut.log.error("SIM is not ready")
            return False
        return self.check_emergency_call_back_mode(
            non_emergency_call_allowed=False)


""" Tests End """
