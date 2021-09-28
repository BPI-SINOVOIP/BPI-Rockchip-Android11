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
    Connectivity Monitor and Telephony Troubleshooter Tests
"""

from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from TelLiveConnectivityMonitorBaseTest import TelLiveConnectivityMonitorBaseTest
from TelLiveConnectivityMonitorBaseTest import ACTIONS
from TelLiveConnectivityMonitorBaseTest import TROUBLES


class TelLiveConnectivityMonitorTest(TelLiveConnectivityMonitorBaseTest):
    """ Tests Begin """

    @test_tracker_info(uuid="fee3d03d-701b-4727-9320-426ff6b29974")
    @TelephonyBaseTest.tel_test_wrap
    def test_volte_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="volte")

    @test_tracker_info(uuid="8c3ee59a-74e5-4885-8f42-8a15d4550d5f")
    @TelephonyBaseTest.tel_test_wrap
    def test_csfb_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="csfb")

    @test_tracker_info(uuid="6cd12786-c048-4925-8745-1d5d30094257")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="3g")

    @test_tracker_info(uuid="51166448-cea6-480b-93d8-7063f940ce0a")
    @TelephonyBaseTest.tel_test_wrap
    def test_2g_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="2g")

    @test_tracker_info(uuid="409f3331-5d64-4793-b300-2b3d3fa50ba5")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_apm_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="wfc_apm")

    @test_tracker_info(uuid="336c383f-ec19-4447-af37-7f9bb0bac4dd")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_non_apm_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="wfc_non_apm")

    @test_tracker_info(uuid="fd8d22ac-66b2-4e91-a922-8ecec45c85e6")
    @TelephonyBaseTest.tel_test_wrap
    def test_vt_call_drop_triggered_suggestion(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.forced_call_drop_test(setup="vt")

    @test_tracker_info(uuid="11c4068e-9710-4a40-8587-79d32a68a37e")
    @TelephonyBaseTest.tel_test_wrap
    def test_volte_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="volte")

    @test_tracker_info(uuid="8c7083e1-7c06-40c9-9a58-485adceb8690")
    @TelephonyBaseTest.tel_test_wrap
    def test_csfb_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="csfb")

    @test_tracker_info(uuid="a7938250-ea3c-4d37-85fe-72edf67c61f7")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="3g")

    @test_tracker_info(uuid="24f498c4-26c5-447f-8e7d-fc3ff1d1e9d5")
    @TelephonyBaseTest.tel_test_wrap
    def test_2g_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="2g")

    @test_tracker_info(uuid="9fd0fc1e-9480-40b7-bd6f-fe6ac95c2018")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_apm_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="wfc_apm")

    @test_tracker_info(uuid="8fd9f1a0-b1e0-4469-8617-608ed0682f91")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_non_apm_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="wfc_non_apm")

    @test_tracker_info(uuid="86056126-9c0b-4702-beb5-49b66368a806")
    @TelephonyBaseTest.tel_test_wrap
    def test_vt_call_drop_after_user_data_wipe(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_wipe(setup="vt")

    @test_tracker_info(uuid="96ee7af3-96cf-48a7-958b-834684b670dc")
    @TelephonyBaseTest.tel_test_wrap
    def test_stats_and_suggestion_after_reboot(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        return self.call_drop_test_after_reboot(setup="volte")

    @test_tracker_info(uuid="6b9c8f45-a3cc-4fa8-9a03-bc439ed5b415")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drops_equally_across_all_types(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_wfc_non_apm()
        return self.call_drop_test_after_same_type_healthy_call(
            setup="wfc_non_apm")

    @test_tracker_info(uuid="f2633204-c2ac-4c57-9465-ef6de3223de3")
    @TelephonyBaseTest.tel_test_wrap
    def test_volte_call_drop_with_wifi_on_cellular_preferred(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_volte()
        self.connect_to_wifi()
        return self.call_drop_triggered_suggestion_test(setup="volte")

    @test_tracker_info(uuid="ec274cb6-0b75-4026-94a7-0228a43a0f5f")
    @TelephonyBaseTest.tel_test_wrap
    def test_csfb_call_drop_with_wifi_on_cellular_preferred(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_csfb()
        self.connect_to_wifi()
        return self.call_drop_triggered_suggestion_test(setup="csfb")

    @test_tracker_info(uuid="b9b439c0-4200-47d6-824b-f12b64dfeecd")
    @TelephonyBaseTest.tel_test_wrap
    def test_3g_call_drop_with_wifi_on_cellular_preferred(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_3g()
        self.connect_to_wifi()
        return self.call_drop_triggered_suggestion_test(setup="3g")

    @test_tracker_info(uuid="a4e43270-f7fa-4709-bbe2-c7368af39227")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_non_apm_toggling_wifi_call_drop(self):
        """Connectivity Monitor Off Test

        Steps:
            1. Verify Connectivity Monitor can be turned off
            2. Force Trigger a call drop : media timeout and ensure it is
               not notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does not report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_volte()
        self.setup_wfc_non_apm()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="wfc_non_apm", triggers=["toggle_wifi", "toogle_wifi"])

    @test_tracker_info(uuid="1c880cf8-082c-4451-b890-22081177d084")
    @TelephonyBaseTest.tel_test_wrap
    def test_wfc_apm_call_toggling_wifi(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_volte()
        self.setup_wfc_apm()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="wfc_apm", triggers=["toggle_wifi", "toggle_wifi"])

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_volte(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               not counted as call drop by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_volte()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="volte", triggers=["toggle_apm"])

    @test_tracker_info(uuid="8e1ba024-3b43-4a7d-adc8-2252da81c55c")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_csfb(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_csfb()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="csfb", triggers=["toggle_apm"])

    @test_tracker_info(uuid="fe6afae4-fa04-435f-8bbc-4a63f5fb525c")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_3g(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_3g()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="3g", triggers=["toggle_apm"])

    @test_tracker_info(uuid="cc089e2b-d0e1-42a3-80de-597986be3d4e")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_2g(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_2g()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="2g", triggers=["toggle_apm"])

    @test_tracker_info(uuid="f8ba9655-572c-4a90-be59-6a5bc9a8fad0")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_wfc_apm(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_wfc_apm()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="wfc_apm", triggers=["toggle_apm"])

    @test_tracker_info(uuid="f2995df9-f56d-442c-977a-141e3269481f")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_wfc_non_apm(
            self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_wfc_non_apm()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="wfc_non_apm", triggers=["toggle_apm"])

    @test_tracker_info(uuid="cb52110c-7470-4886-b71f-e32f0e489cbd")
    @TelephonyBaseTest.tel_test_wrap
    def test_call_drop_by_toggling_apm_with_connectivity_monitor_vt(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_vt()
        return self.call_setup_and_connectivity_monitor_checking(
            setup="vt", triggers=["toggle_apm"])

    @test_tracker_info(uuid="b91a1e8d-3630-4b81-bc8c-c7d3dad42c77")
    @TelephonyBaseTest.tel_test_wrap
    def test_healthy_call_with_connectivity_monitor_volte(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. No call drop during the call
            3. Verify the call summary report

        Expected Results:
            feature work fine, and healthy call is added to report

        Returns:
            True is pass, False if fail.
        """
        self.setup_volte()
        return self.healthy_call_test(setup="volte", count=1)

    @test_tracker_info(uuid="2f581f6a-087f-4d12-a75c-a62778cb741b")
    @TelephonyBaseTest.tel_test_wrap
    def test_healthy_call_with_connectivity_monitor_csfb(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Force Trigger a call drop : media timeout and ensure it is
               notified by Connectivity Monitor

        Expected Results:
            feature work fine, and does report to User about Call Drop

        Returns:
            True is pass, False if fail.
        """
        self.setup_csfb()
        return self.healthy_call_test(setup="csfb", count=1)

    @test_tracker_info(uuid="a5989001-8201-4356-9903-581d0e361b38")
    @TelephonyBaseTest.tel_test_wrap
    def test_healthy_call_with_connectivity_monitor_wfc_apm(self):
        """Telephony Monitor Functional Test

        Steps:
            1. Verify Connectivity Monitor is on
            2. Make a call and hung up the call
            3. Verify the healthy call is added to the call summary report

        Expected Results:
            feature work fine

        Returns:
            True is pass, False if fail.
        """
        self.setup_wfc_apm()
        return self.healthy_call_test(setup="wfc_apm", count=1)


""" Tests End """
