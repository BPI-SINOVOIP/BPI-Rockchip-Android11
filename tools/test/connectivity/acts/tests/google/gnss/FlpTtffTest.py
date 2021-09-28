#!/usr/bin/env python3.5
#
#   Copyright 2019 - The Android Open Source Project
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

from acts import utils
from acts import asserts
from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info
from acts.utils import get_current_epoch_time
from acts.test_utils.wifi.wifi_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_test_utils import start_qxdm_logger
from acts.test_utils.tel.tel_test_utils import stop_qxdm_logger
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import abort_all_tests
from acts.test_utils.gnss.gnss_test_utils import get_baseband_and_gms_version
from acts.test_utils.gnss.gnss_test_utils import _init_device
from acts.test_utils.gnss.gnss_test_utils import check_location_service
from acts.test_utils.gnss.gnss_test_utils import clear_logd_gnss_qxdm_log
from acts.test_utils.gnss.gnss_test_utils import set_mobile_data
from acts.test_utils.gnss.gnss_test_utils import get_gnss_qxdm_log
from acts.test_utils.gnss.gnss_test_utils import set_wifi_and_bt_scanning
from acts.test_utils.gnss.gnss_test_utils import process_gnss_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import start_ttff_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import process_ttff_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import check_ttff_data
from acts.test_utils.gnss.gnss_test_utils import set_attenuator_gnss_signal
from acts.test_utils.gnss.gnss_test_utils import connect_to_wifi_network
from acts.test_utils.gnss.gnss_test_utils import gnss_tracking_via_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import parse_gtw_gpstool_log


class FlpTtffTest(BaseTestClass):
    """ FLP TTFF Tests"""
    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        req_params = ["pixel_lab_network", "standalone_cs_criteria",
                      "qdsp6m_path", "flp_ttff_max_threshold",
                      "pixel_lab_location", "default_gnss_signal_attenuation",
                      "weak_gnss_signal_attenuation"]
        self.unpack_userparams(req_param_names=req_params)
        self.ssid_map = {}
        for network in self.pixel_lab_network:
            SSID = network['SSID']
            self.ssid_map[SSID] = network
        if int(self.ad.adb.shell("settings get global airplane_mode_on")) != 0:
            self.ad.log.info("Force airplane mode off")
            force_airplane_mode(self.ad, False)
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.default_gnss_signal_attenuation)
        _init_device(self.ad)

    def setup_test(self):
        get_baseband_and_gms_version(self.ad)
        clear_logd_gnss_qxdm_log(self.ad)
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.default_gnss_signal_attenuation)
        if not verify_internet_connection(self.ad.log, self.ad, retries=3,
                                          expected_state=True):
            raise signals.TestFailure("Fail to connect to LTE network.")

    def teardown_test(self):
        stop_qxdm_logger(self.ad)
        if int(self.ad.adb.shell("settings get global mobile_data")) != 1:
            set_mobile_data(self.ad, True)
        if int(self.ad.adb.shell(
            "settings get global wifi_scan_always_enabled")) != 1:
            set_wifi_and_bt_scanning(self.ad, True)
        if self.ad.droid.wifiCheckState():
            wifi_toggle_state(self.ad, False)
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.default_gnss_signal_attenuation)

    def on_pass(self, test_name, begin_time):
        self.ad.take_bug_report(test_name, begin_time)
        get_gnss_qxdm_log(self.ad, self.qdsp6m_path)

    def on_fail(self, test_name, begin_time):
        self.ad.take_bug_report(test_name, begin_time)
        get_gnss_qxdm_log(self.ad, self.qdsp6m_path)

    """ Helper Functions """

    def flp_ttff_hs_and_cs(self, criteria, location):
        flp_results = []
        ttff = {"hs": "Hot Start", "cs": "Cold Start"}
        for mode in ttff.keys():
            begin_time = get_current_epoch_time()
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria,
                                        type="flp")
            start_ttff_by_gtw_gpstool(self.ad, ttff_mode=mode, iteration=10)
            ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                    location, type="flp")
            result = check_ttff_data(self.ad, ttff_data, ttff[mode], criteria)
            flp_results.append(result)
        asserts.assert_true(all(flp_results),
                            "FLP TTFF fails to reach designated criteria")

    """ Test Cases """

    @test_tracker_info(uuid="c11ada6a-d7ad-4dc8-9d4a-0ae3cb9dfa8e")
    def test_flp_one_hour_tracking(self):
        """Verify FLP tracking performance of position error.

        Steps:
            1. Launch GTW_GPSTool.
            2. FLP tracking for 60 minutes.

        Expected Results:
            DUT could finish 60 minutes test and output track data.
        """
        start_qxdm_logger(self.ad, get_current_epoch_time())
        gnss_tracking_via_gtw_gpstool(self.ad, self.standalone_cs_criteria,
                                      type="flp", testtime=60)
        parse_gtw_gpstool_log(self.ad, self.pixel_lab_location, type="flp")

    @test_tracker_info(uuid="8bc4e82d-fdce-4ee8-af8c-5e4a925b5360")
    def test_flp_ttff_strong_signal_wifiscan_on_wifi_connect(self):
        """Verify FLP TTFF Hot Start and Cold Start under strong GNSS signals
        with WiFi scanning on and connected.

        Steps:
            1. Enable WiFi scanning in location setting.
            2. Connect to WiFi AP.
            3. TTFF Hot Start for 10 iteration.
            4. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, True)
        wifi_toggle_state(self.ad, True)
        connect_to_wifi_network(
            self.ad, self.ssid_map[self.pixel_lab_network[0]["SSID"]])
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)

    @test_tracker_info(uuid="adc1a0c7-3635-420d-9481-0f5816c58334")
    def test_flp_ttff_strong_signal_wifiscan_on_wifi_not_connect(self):
        """Verify FLP TTFF Hot Start and Cold Start under strong GNSS signals
        with WiFi scanning on and not connected.

        Steps:
            1. Enable WiFi scanning in location setting.
            2. WiFi is not connected.
            3. TTFF Hot Start for 10 iteration.
            4. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, True)
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)

    @test_tracker_info(uuid="3ec3cee2-b881-4c61-9df1-b6b81fcd4527")
    def test_flp_ttff_strong_signal_wifiscan_off(self):
        """Verify FLP TTFF Hot Start and Cold Start with WiFi scanning OFF
           under strong GNSS signals.

        Steps:
            1. Disable WiFi scanning in location setting.
            2. TTFF Hot Start for 10 iteration.
            3. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, False)
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)

    @test_tracker_info(uuid="03c0d34f-8312-48d5-8753-93b09151233a")
    def test_flp_ttff_weak_signal_wifiscan_on_wifi_connect(self):
        """Verify FLP TTFF Hot Start and Cold Start under Weak GNSS signals
        with WiFi scanning on and connected

        Steps:
            1. Set attenuation value to weak GNSS signal.
            2. Enable WiFi scanning in location setting.
            3. Connect to WiFi AP.
            4. TTFF Hot Start for 10 iteration.
            5. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.weak_gnss_signal_attenuation)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, True)
        wifi_toggle_state(self.ad, True)
        connect_to_wifi_network(
            self.ad, self.ssid_map[self.pixel_lab_network[0]["SSID"]])
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)

    @test_tracker_info(uuid="13daf7b3-5ac5-4107-b3dc-a3a8b5589fed")
    def test_flp_ttff_weak_signal_wifiscan_on_wifi_not_connect(self):
        """Verify FLP TTFF Hot Start and Cold Start under Weak GNSS signals
        with WiFi scanning on and not connected.

        Steps:
            1. Set attenuation value to weak GNSS signal.
            2. Enable WiFi scanning in location setting.
            3. WiFi is not connected.
            4. TTFF Hot Start for 10 iteration.
            5. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.weak_gnss_signal_attenuation)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, True)
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)

    @test_tracker_info(uuid="1831f80f-099f-46d2-b484-f332046d5a4d")
    def test_flp_ttff_weak_signal_wifiscan_off(self):
        """Verify FLP TTFF Hot Start and Cold Start with WiFi scanning OFF
           under weak GNSS signals.

        Steps:
            1. Set attenuation value to weak GNSS signal.
            2. Disable WiFi scanning in location setting.
            3. TTFF Hot Start for 10 iteration.
            4. TTFF Cold Start for 10 iteration.

        Expected Results:
            Both FLP TTFF Hot Start and Cold Start results should be within
            flp_ttff_max_threshold.
        """
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.weak_gnss_signal_attenuation)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        set_wifi_and_bt_scanning(self.ad, False)
        self.flp_ttff_hs_and_cs(self.flp_ttff_max_threshold,
                                self.pixel_lab_location)