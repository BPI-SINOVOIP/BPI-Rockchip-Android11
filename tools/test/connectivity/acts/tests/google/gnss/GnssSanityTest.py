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
import time
import os
import re
import fnmatch
from multiprocessing import Process

from acts import utils
from acts import asserts
from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.tel import tel_test_utils as tutils
from acts.test_utils.gnss import gnss_test_utils as gutils
from acts.utils import get_current_epoch_time
from acts.utils import unzip_maintain_permissions
from acts.utils import force_airplane_mode
from acts.test_utils.wifi.wifi_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_test_utils import flash_radio
from acts.test_utils.tel.tel_test_utils import verify_internet_connection
from acts.test_utils.tel.tel_test_utils import abort_all_tests
from acts.test_utils.tel.tel_test_utils import stop_qxdm_logger
from acts.test_utils.tel.tel_test_utils import check_call_state_connected_by_adb
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import hangup_call
from acts.test_utils.tel.tel_test_utils import http_file_download_by_sl4a
from acts.test_utils.tel.tel_test_utils import start_qxdm_logger
from acts.test_utils.tel.tel_test_utils import trigger_modem_crash
from acts.test_utils.gnss.gnss_test_utils import get_baseband_and_gms_version
from acts.test_utils.gnss.gnss_test_utils import set_attenuator_gnss_signal
from acts.test_utils.gnss.gnss_test_utils import _init_device
from acts.test_utils.gnss.gnss_test_utils import check_location_service
from acts.test_utils.gnss.gnss_test_utils import clear_logd_gnss_qxdm_log
from acts.test_utils.gnss.gnss_test_utils import set_mobile_data
from acts.test_utils.gnss.gnss_test_utils import set_wifi_and_bt_scanning
from acts.test_utils.gnss.gnss_test_utils import get_gnss_qxdm_log
from acts.test_utils.gnss.gnss_test_utils import remount_device
from acts.test_utils.gnss.gnss_test_utils import reboot
from acts.test_utils.gnss.gnss_test_utils import check_network_location
from acts.test_utils.gnss.gnss_test_utils import launch_google_map
from acts.test_utils.gnss.gnss_test_utils import check_location_api
from acts.test_utils.gnss.gnss_test_utils import set_battery_saver_mode
from acts.test_utils.gnss.gnss_test_utils import kill_xtra_daemon
from acts.test_utils.gnss.gnss_test_utils import start_gnss_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import process_gnss_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import start_ttff_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import process_ttff_by_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import check_ttff_data
from acts.test_utils.gnss.gnss_test_utils import start_youtube_video
from acts.test_utils.gnss.gnss_test_utils import fastboot_factory_reset
from acts.test_utils.gnss.gnss_test_utils import gnss_trigger_modem_ssr_by_adb
from acts.test_utils.gnss.gnss_test_utils import gnss_trigger_modem_ssr_by_mds
from acts.test_utils.gnss.gnss_test_utils import disable_supl_mode
from acts.test_utils.gnss.gnss_test_utils import connect_to_wifi_network
from acts.test_utils.gnss.gnss_test_utils import check_xtra_download
from acts.test_utils.gnss.gnss_test_utils import gnss_tracking_via_gtw_gpstool
from acts.test_utils.gnss.gnss_test_utils import parse_gtw_gpstool_log
from acts.test_utils.gnss.gnss_test_utils import enable_supl_mode
from acts.test_utils.gnss.gnss_test_utils import start_toggle_gnss_by_gtw_gpstool
from acts.test_utils.tel.tel_test_utils import start_adb_tcpdump
from acts.test_utils.tel.tel_test_utils import stop_adb_tcpdump
from acts.test_utils.tel.tel_test_utils import get_tcpdump_log


class GnssSanityTest(BaseTestClass):
    """ GNSS Function Sanity Tests"""
    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        req_params = ["pixel_lab_network", "standalone_cs_criteria",
                      "supl_cs_criteria", "xtra_ws_criteria",
                      "xtra_cs_criteria", "weak_signal_supl_cs_criteria",
                      "weak_signal_xtra_ws_criteria",
                      "weak_signal_xtra_cs_criteria",
                      "default_gnss_signal_attenuation",
                      "weak_gnss_signal_attenuation",
                      "no_gnss_signal_attenuation", "gnss_init_error_list",
                      "gnss_init_error_whitelist", "pixel_lab_location",
                      "legacy_wifi_xtra_cs_criteria", "legacy_projects",
                      "qdsp6m_path", "supl_capabilities"]
        self.unpack_userparams(req_param_names=req_params)
        # create hashmap for SSID
        self.ssid_map = {}
        for network in self.pixel_lab_network:
            SSID = network['SSID']
            self.ssid_map[SSID] = network
        if self.ad.model in self.legacy_projects:
            self.wifi_xtra_cs_criteria = self.legacy_wifi_xtra_cs_criteria
        else:
            self.wifi_xtra_cs_criteria = self.xtra_cs_criteria
        self.flash_new_radio_or_mbn()
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
        stop_adb_tcpdump(self.ad)
        if check_call_state_connected_by_adb(self.ad):
            hangup_call(self.ad.log, self.ad)
        if int(self.ad.adb.shell("settings get global airplane_mode_on")) != 0:
            self.ad.log.info("Force airplane mode off")
            force_airplane_mode(self.ad, False)
        if self.ad.droid.wifiCheckState():
            wifi_toggle_state(self.ad, False)
        if int(self.ad.adb.shell("settings get global mobile_data")) != 1:
            set_mobile_data(self.ad, True)
        if int(self.ad.adb.shell(
            "settings get global wifi_scan_always_enabled")) != 1:
            set_wifi_and_bt_scanning(self.ad, True)
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.default_gnss_signal_attenuation)

    def on_pass(self, test_name, begin_time):
        self.ad.take_bug_report(test_name, begin_time)
        get_gnss_qxdm_log(self.ad, self.qdsp6m_path)
        get_tcpdump_log(self.ad, test_name, begin_time)

    def on_fail(self, test_name, begin_time):
        self.ad.take_bug_report(test_name, begin_time)
        get_gnss_qxdm_log(self.ad, self.qdsp6m_path)
        get_tcpdump_log(self.ad, test_name, begin_time)

    def flash_new_radio_or_mbn(self):
        paths = {}
        path = self.user_params.get("radio_image")
        if isinstance(path, list):
            path = path[0]
        if "dev/null" in path:
            self.ad.log.info("Radio image path is not defined in Test flag.")
            return False
        for path_key in os.listdir(path):
            if fnmatch.fnmatch(path_key, "*.img"):
                paths["radio_image"] = os.path.join(path, path_key)
                os.system("chmod -R 777 %s" % paths["radio_image"])
                self.ad.log.info("radio_image = %s" % paths["radio_image"])
            if fnmatch.fnmatch(path_key, "*.zip"):
                zip_path = os.path.join(path, path_key)
                self.ad.log.info("Unzip %s", zip_path)
                dest_path = os.path.join(path, "mbn")
                unzip_maintain_permissions(zip_path, dest_path)
                paths["mbn_path"] = dest_path
                os.system("chmod -R 777 %s" % paths["mbn_path"])
                self.ad.log.info("mbn_path = %s" % paths["mbn_path"])
                self.ad.log.info(os.listdir(paths["mbn_path"]))
        if not paths.get("radio_image"):
            self.ad.log.info("No radio image is provided on X20. "
                             "Skip flashing radio step.")
            return False
        else:
            get_baseband_and_gms_version(self.ad, "Before flash radio")
            flash_radio(self.ad, paths["radio_image"])
            get_baseband_and_gms_version(self.ad, "After flash radio")
        if not paths.get("mbn_path"):
            self.ad.log.info("No need to push mbn files")
            return False
        else:
            try:
                mcfg_ver = self.ad.adb.shell(
                    "cat /vendor/rfs/msm/mpss/readonly/vendor/mbn/mcfg.version")
                if mcfg_ver:
                    self.ad.log.info("Before push mcfg, mcfg.version = %s",
                                     mcfg_ver)
                else:
                    self.ad.log.info("There is no mcfg.version before push, "
                                     "unmatching device")
                    return False
            except:
                self.ad.log.info("There is no mcfg.version before push, "
                                 "unmatching device")
                return False
            get_baseband_and_gms_version(self.ad, "Before push mcfg")
            try:
                remount_device(self.ad)
                cmd = "%s %s" % (paths["mbn_path"]+"/.",
                                 "/vendor/rfs/msm/mpss/readonly/vendor/mbn/")
                out = self.ad.adb.push(cmd)
                self.ad.log.info(out)
                reboot(self.ad)
            except Exception as e:
                self.ad.log.error("Push mbn files error %s", e)
                return False
            get_baseband_and_gms_version(self.ad, "After push mcfg")
            try:
                new_mcfg_ver = self.ad.adb.shell(
                    "cat /vendor/rfs/msm/mpss/readonly/vendor/mbn/mcfg.version")
                if new_mcfg_ver:
                    self.ad.log.info("New mcfg.version = %s", new_mcfg_ver)
                    if new_mcfg_ver == mcfg_ver:
                        self.ad.log.error("mcfg.version is the same before and "
                                          "after push")
                        return True
                else:
                    self.ad.log.error("Unable to get new mcfg.version")
                    return False
            except Exception as e:
                self.ad.log.error("cat mcfg.version with error %s", e)
                return False

    """ Test Cases """

    @test_tracker_info(uuid="ab859f2a-2c95-4d15-bb7f-bd0e3278340f")
    def test_gnss_one_hour_tracking(self):
        """Verify GNSS tracking performance of signal strength and position
        error.

        Steps:
            1. Launch GTW_GPSTool.
            2. GNSS tracking for 60 minutes.

        Expected Results:
            DUT could finish 60 minutes test and output track data.
        """
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        gnss_tracking_via_gtw_gpstool(self.ad, self.standalone_cs_criteria,
                                      type="gnss", testtime=60)
        parse_gtw_gpstool_log(self.ad, self.pixel_lab_location, type="gnss")

    @test_tracker_info(uuid="499d2091-640a-4735-9c58-de67370e4421")
    def test_gnss_init_error(self):
        """Check if there is any GNSS initialization error after reboot.

        Steps:
            1. Reboot DUT.
            2. Check logcat if the following error pattern shows up.
              "E LocSvc.*", ".*avc.*denied.*u:r:location:s0",
              ".*avc.*denied.*u:r:hal_gnss_qti:s0"

        Expected Results:
            There should be no GNSS initialization error after reboot.
        """
        error_mismatch = True
        for attr in self.gnss_init_error_list:
            error = self.ad.adb.shell("logcat -d | grep -E '%s'" % attr)
            if error:
                for whitelist in self.gnss_init_error_whitelist:
                    if whitelist in error:
                        error = re.sub(".*"+whitelist+".*\n?", "", error)
                        self.ad.log.info("\"%s\" is white-listed and removed "
                                         "from error." % whitelist)
                if error:
                    error_mismatch = False
                    self.ad.log.error("\n%s" % error)
            else:
                self.ad.log.info("NO \"%s\" initialization error found." % attr)
        asserts.assert_true(error_mismatch, "Error message found after GNSS "
                                            "init")

    @test_tracker_info(uuid="ff318483-411c-411a-8b1a-422bd54f4a3f")
    def test_supl_capabilities(self):
        """Verify SUPL capabilities.

        Steps:
            1. Root DUT.
            2. Check SUPL capabilities.

        Expected Results:
            CAPABILITIES=0x37 which supports MSA + MSB.
            CAPABILITIES=0x17 = ON_DEMAND_TIME | MSA | MSB | SCHEDULING
        """
        capabilities_state = str(
            self.ad.adb.shell(
                "cat vendor/etc/gps.conf | grep CAPABILITIES")).split("=")[-1]
        self.ad.log.info("SUPL capabilities - %s" % capabilities_state)
        asserts.assert_true(capabilities_state in self.supl_capabilities,
                            "Wrong default SUPL capabilities is set. Found %s, "
                            "expected any of %r" % (capabilities_state,
                                                    self.supl_capabilities))

    @test_tracker_info(uuid="dcae6979-ddb4-4cad-9d14-fbdd9439cf42")
    def test_sap_valid_modes(self):
        """Verify SAP Valid Modes.

        Steps:
            1. Root DUT.
            2. Check SAP Valid Modes.

        Expected Results:
            SAP=PREMIUM
        """
        sap_state = str(self.ad.adb.shell("cat vendor/etc/izat.conf | grep "
                                          "SAP="))
        self.ad.log.info("SAP Valid Modes - %s" % sap_state)
        asserts.assert_true(sap_state == "SAP=PREMIUM",
                            "Wrong SAP Valid Modes is set")

    @test_tracker_info(uuid="14daaaba-35b4-42d9-8d2c-2a803dd746a6")
    def test_network_location_provider_cell(self):
        """Verify LocationManagerService API reports cell Network Location.

        Steps:
            1. WiFi scanning and Bluetooth scanning in Location Setting are OFF.
            2. Launch GTW_GPSTool.
            3. Verify whether test devices could report cell Network Location.
            4. Repeat Step 2. to Step 3. for 5 times.

        Expected Results:
            Test devices could report cell Network Location.
        """
        test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        set_wifi_and_bt_scanning(self.ad, False)
        for i in range(1, 6):
            test_result = check_network_location(
                self.ad, retries=3, location_type="networkLocationType=cell")
            test_result_all.append(test_result)
            self.ad.log.info("Iteraion %d => %s" % (i, test_result))
        set_wifi_and_bt_scanning(self.ad, True)
        asserts.assert_true(all(test_result_all),
                            "Fail to get networkLocationType=cell")

    @test_tracker_info(uuid="a45bdc7d-29fa-4a1d-ba34-6340b90e308d")
    def test_network_location_provider_wifi(self):
        """Verify LocationManagerService API reports wifi Network Location.

        Steps:
            1. WiFi scanning and Bluetooth scanning in Location Setting are ON.
            2. Launch GTW_GPSTool.
            3. Verify whether test devices could report wifi Network Location.
            4. Repeat Step 2. to Step 3. for 5 times.

        Expected Results:
            Test devices could report wifi Network Location.
        """
        test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        set_wifi_and_bt_scanning(self.ad, True)
        for i in range(1, 6):
            test_result = check_network_location(
                self.ad, retries=3, location_type="networkLocationType=wifi")
            test_result_all.append(test_result)
            self.ad.log.info("Iteraion %d => %s" % (i, test_result))
        asserts.assert_true(all(test_result_all),
                            "Fail to get networkLocationType=wifi")

    @test_tracker_info(uuid="0919d375-baf2-4fe7-b66b-3f72d386f791")
    def test_gmap_location_report_gps_network(self):
        """Verify GnssLocationProvider API reports location to Google Map
           when GPS and Location Accuracy are on.

        Steps:
            1. GPS and NLP are on.
            2. Launch Google Map.
            3. Verify whether test devices could report location.
            4. Repeat Step 2. to Step 3. for 5 times.

        Expected Results:
            Test devices could report location to Google Map.
        """
        test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        for i in range(1, 6):
            launch_google_map(self.ad)
            test_result = check_location_api(self.ad, retries=3)
            self.ad.send_keycode("HOME")
            test_result_all.append(test_result)
            self.ad.log.info("Iteraion %d => %s" % (i, test_result))
        asserts.assert_true(all(test_result_all), "Fail to get location update")

    @test_tracker_info(uuid="513361d2-7d72-41b0-a944-fb259c606b81")
    def test_gmap_location_report_gps(self):
        """Verify GnssLocationProvider API reports location to Google Map
           when GPS is on and Location Accuracy is off.

        Steps:
            1. GPS is on.
            2. Location Accuracy is off.
            3. Launch Google Map.
            4. Verify whether test devices could report location.
            5. Repeat Step 3. to Step 4. for 5 times.

        Expected Results:
            Test devices could report location to Google Map.
        """
        test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        self.ad.adb.shell("settings put secure location_mode 1")
        out = int(self.ad.adb.shell("settings get secure location_mode"))
        self.ad.log.info("Modify current Location Mode to %d" % out)
        for i in range(1, 6):
            launch_google_map(self.ad)
            test_result = check_location_api(self.ad, retries=3)
            self.ad.send_keycode("HOME")
            test_result_all.append(test_result)
            self.ad.log.info("Iteraion %d => %s" % (i, test_result))
        check_location_service(self.ad)
        asserts.assert_true(all(test_result_all), "Fail to get location update")

    @test_tracker_info(uuid="91a65121-b87d-450d-bd0f-387ade450ab7")
    def test_gmap_location_report_battery_saver(self):
        """Verify GnssLocationProvider API reports location to Google Map
           when Battery Saver is enabled.

        Steps:
            1. GPS and NLP are on.
            2. Enable Battery Saver.
            3. Launch Google Map.
            4. Verify whether test devices could report location.
            5. Repeat Step 3. to Step 4. for 5 times.
            6. Disable Battery Saver.

        Expected Results:
            Test devices could report location to Google Map.
        """
        test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        set_battery_saver_mode(self.ad, True)
        for i in range(1, 6):
            launch_google_map(self.ad)
            test_result = check_location_api(self.ad, retries=3)
            self.ad.send_keycode("HOME")
            test_result_all.append(test_result)
            self.ad.log.info("Iteraion %d => %s" % (i, test_result))
        set_battery_saver_mode(self.ad, False)
        asserts.assert_true(all(test_result_all), "Fail to get location update")

    @test_tracker_info(uuid="60c0aeec-0c8f-4a96-bc6c-05cba1260e73")
    def test_supl_ongoing_call(self):
        """Verify SUPL functionality during phone call.

        Steps:
            1. Kill XTRA daemon to support SUPL only case.
            2. Initiate call on DUT.
            3. SUPL TTFF Cold Start for 10 iteration.
            4. DUT hang up call.

        Expected Results:
            All SUPL TTFF Cold Start results should be less than
            supl_cs_criteria.
        """
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        self.ad.droid.setVoiceCallVolume(25)
        initiate_call(self.ad.log, self.ad, "99117")
        time.sleep(5)
        if not check_call_state_connected_by_adb(self.ad):
            raise signals.TestFailure("Call is not connected.")
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.supl_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="df605509-328f-43e8-b6d8-00635bf701ef")
    def test_supl_downloading_files(self):
        """Verify SUPL functionality when downloading files.

        Steps:
            1. Kill XTRA daemon to support SUPL only case.
            2. DUT start downloading files by sl4a.
            3. SUPL TTFF Cold Start for 10 iteration.
            4. DUT cancel downloading files.

        Expected Results:
            All SUPL TTFF Cold Start results should be within supl_cs_criteria.
        """
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        download = Process(target=http_file_download_by_sl4a,
                           args=(self.ad, "https://speed.hetzner.de/10GB.bin",
                                 None, None, True, 3600))
        download.start()
        time.sleep(10)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        download.terminate()
        time.sleep(3)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.supl_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="66b9f9d4-1397-4da7-9e55-8b89b1732017")
    def test_supl_watching_youtube(self):
        """Verify SUPL functionality when watching video on youtube.

        Steps:
            1. Kill XTRA daemon to support SUPL only case.
            2. DUT start watching video on youtube.
            3. SUPL TTFF Cold Start for 10 iteration at the background.
            4. DUT stop watching video on youtube.

        Expected Results:
            All SUPL TTFF Cold Start results should be within supl_cs_criteria.
        """
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        self.ad.droid.setMediaVolume(25)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        start_youtube_video(self.ad,
                            url="https://www.youtube.com/watch?v=AbdVsi1VjQY",
                            retries=3)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.supl_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="a748af8b-e1eb-4ec6-bde3-74bcefa1c680")
    def test_supl_modem_ssr(self):
        """Verify SUPL functionality after modem silent reboot.

        Steps:
            1. Trigger modem crash by adb.
            2. Wait 1 minute for modem to recover.
            3. SUPL TTFF Cold Start for 3 iteration.
            4. Repeat Step 1. to Step 3. for 5 times.

        Expected Results:
            All SUPL TTFF Cold Start results should be within supl_cs_criteria.
        """
        supl_ssr_test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        for times in range(1, 6):
            begin_time = get_current_epoch_time()
            gnss_trigger_modem_ssr_by_mds(self.ad)
            if not verify_internet_connection(self.ad.log, self.ad, retries=3,
                                              expected_state=True):
                raise signals.TestFailure("Fail to connect to LTE network.")
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=3)
            ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                    self.pixel_lab_location)
            supl_ssr_test_result = check_ttff_data(
                self.ad, ttff_data, ttff_mode="Cold Start",
                criteria=self.supl_cs_criteria)
            self.ad.log.info("SUPL after Modem SSR test %d times -> %s"
                             % (times, supl_ssr_test_result))
            supl_ssr_test_result_all.append(supl_ssr_test_result)
        asserts.assert_true(all(supl_ssr_test_result_all),
                            "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="01602e65-8ded-4459-8df1-7df70a1bfe8a")
    def test_gnss_airplane_mode_on(self):
        """Verify Standalone GNSS functionality while airplane mode is on.

        Steps:
            1. Turn on airplane mode.
            2. TTFF Cold Start for 10 iteration.
            3. Turn off airplane mode.

        Expected Results:
            All Standalone TTFF Cold Start results should be within
            standalone_cs_criteria.
        """
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        self.ad.log.info("Turn airplane mode on")
        force_airplane_mode(self.ad, True)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.standalone_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="23731b0d-cb80-4c79-a877-cfe7c2faa447")
    def test_gnss_mobile_data_off(self):
        """Verify Standalone GNSS functionality while mobile radio is off.

        Steps:
            1. Disable mobile data.
            2. TTFF Cold Start for 10 iteration.
            3. Enable mobile data.

        Expected Results:
            All Standalone TTFF Cold Start results should be within
            standalone_cs_criteria.
        """
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        set_mobile_data(self.ad, False)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.standalone_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="085b86a9-0212-4c0f-8ca1-2e467a0a2e6e")
    def test_supl_without_gnss_signal(self):
        """Verify SUPL functionality after no GNSS signal for awhile.

        Steps:
            1. Get location fixed.
            2  Let device do GNSS tracking for 1 minute.
            3. Set attenuation value to block GNSS signal.
            4. Let DUT stay in no GNSS signal for 5 minutes.
            5. Set attenuation value to regain GNSS signal.
            6. Try to get location reported again.
            7. Repeat Step 1. to Step 6. for 5 times.

        Expected Results:
            After setting attenuation value to 10 (GPS signal regain),
            DUT could get location fixed again.
        """
        supl_no_gnss_signal_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        for times in range(1, 6):
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            self.ad.log.info("Let device do GNSS tracking for 1 minute.")
            time.sleep(60)
            set_attenuator_gnss_signal(self.ad, self.attenuators,
                                       self.no_gnss_signal_attenuation)
            self.ad.log.info("Let device stay in no GNSS signal for 5 minutes.")
            time.sleep(300)
            set_attenuator_gnss_signal(self.ad, self.attenuators,
                                       self.default_gnss_signal_attenuation)
            supl_no_gnss_signal = check_location_api(self.ad, retries=3)
            start_gnss_by_gtw_gpstool(self.ad, False)
            self.ad.log.info("SUPL without GNSS signal test %d times -> %s"
                             % (times, supl_no_gnss_signal))
            supl_no_gnss_signal_all.append(supl_no_gnss_signal)
        asserts.assert_true(all(supl_no_gnss_signal_all),
                            "Fail to get location update")

    @test_tracker_info(uuid="3ff2f2fa-42d8-47fa-91de-060816cca9df")
    def test_supl_weak_gnss_signal(self):
        """Verify SUPL TTFF functionality under weak GNSS signal.

        Steps:
            1. Set attenuation value to weak GNSS signal.
            2. Kill XTRA daemon to support SUPL only case.
            3. SUPL TTFF Cold Start for 10 iteration.

        Expected Results:
            All SUPL TTFF Cold Start results should be less than
            weak_signal_supl_cs_criteria.
        """
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.weak_gnss_signal_attenuation)
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        kill_xtra_daemon(self.ad)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                self.pixel_lab_location)
        result = check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                 criteria=self.weak_signal_supl_cs_criteria)
        asserts.assert_true(result, "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="4ad4a371-949a-42e1-b1f4-628c79fa8ddc")
    def test_supl_factory_reset(self):
        """Verify SUPL functionality after factory reset.

        Steps:
            1. Factory reset device.
            2. Kill XTRA daemon to support SUPL only case.
            3. SUPL TTFF Cold Start for 10 iteration.
            4. Repeat Step 1. to Step 3. for 3 times.

        Expected Results:
            All SUPL TTFF Cold Start results should be within supl_cs_criteria.
        """
        for times in range(1, 4):
            fastboot_factory_reset(self.ad)
            self.ad.unlock_screen(password=None)
            _init_device(self.ad)
            begin_time = get_current_epoch_time()
            start_qxdm_logger(self.ad, begin_time)
            start_adb_tcpdump(self.ad)
            kill_xtra_daemon(self.ad)
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
            ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                    self.pixel_lab_location)
            if not check_ttff_data(self.ad, ttff_data, ttff_mode="Cold Start",
                                   criteria=self.supl_cs_criteria):
                raise signals.TestFailure("SUPL after Factory Reset test %d "
                                          "times -> FAIL" % times)
            self.ad.log.info("SUPL after Factory Reset test %d times -> "
                             "PASS" % times)

    @test_tracker_info(uuid="ea3096cf-4f72-4e91-bfb3-0bcbfe865ab4")
    def test_xtra_ttff_mobile_data(self):
        """Verify XTRA TTFF functionality with mobile data.

        Steps:
            1. Disable SUPL mode.
            2. TTFF Warm Start for 10 iteration.
            3. TTFF Cold Start for 10 iteration.

        Expected Results:
            XTRA TTFF Warm Start results should be within xtra_ws_criteria.
            XTRA TTFF Cold Start results should be within xtra_cs_criteria.
        """
        xtra_result = []
        disable_supl_mode(self.ad)
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="ws", iteration=10)
        ws_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        ws_result = check_ttff_data(self.ad,
                                    ws_ttff_data,
                                    ttff_mode="Warm Start",
                                    criteria=self.xtra_ws_criteria)
        xtra_result.append(ws_result)
        begin_time = get_current_epoch_time()
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        cs_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        cs_result = check_ttff_data(self.ad,
                                    cs_ttff_data,
                                    ttff_mode="Cold Start",
                                    criteria=self.xtra_cs_criteria)
        xtra_result.append(cs_result)
        asserts.assert_true(all(xtra_result),
                            "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="c91ba740-220e-41de-81e5-43af31f63907")
    def test_xtra_ttff_weak_gnss_signal(self):
        """Verify XTRA TTFF functionality under weak GNSS signal.

        Steps:
            1. Set attenuation value to weak GNSS signal.
            2. TTFF Warm Start for 10 iteration.
            3. TTFF Cold Start for 10 iteration.

        Expected Results:
            XTRA TTFF Warm Start results should be within
            weak_signal_xtra_ws_criteria.
            XTRA TTFF Cold Start results should be within
            weak_signal_xtra_cs_criteria.
        """
        xtra_result = []
        disable_supl_mode(self.ad)
        set_attenuator_gnss_signal(self.ad, self.attenuators,
                                   self.weak_gnss_signal_attenuation)
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="ws", iteration=10)
        ws_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        ws_result = check_ttff_data(self.ad,
                                    ws_ttff_data,
                                    ttff_mode="Warm Start",
                                    criteria=self.weak_signal_xtra_ws_criteria)
        xtra_result.append(ws_result)
        begin_time = get_current_epoch_time()
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        cs_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        cs_result = check_ttff_data(self.ad,
                                    cs_ttff_data,
                                    ttff_mode="Cold Start",
                                    criteria=self.weak_signal_xtra_cs_criteria)
        xtra_result.append(cs_result)
        asserts.assert_true(all(xtra_result),
                            "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="beeb3454-bcb2-451e-83fb-26289e89b515")
    def test_xtra_ttff_wifi(self):
        """Verify XTRA TTFF functionality with WiFi.

        Steps:
            1. Disable SUPL mode and turn airplane mode on.
            2. Connect to WiFi.
            3. TTFF Warm Start for 10 iteration.
            4. TTFF Cold Start for 10 iteration.

        Expected Results:
            XTRA TTFF Warm Start results should be within xtra_ws_criteria.
            XTRA TTFF Cold Start results should be within xtra_cs_criteria.
        """
        xtra_result = []
        disable_supl_mode(self.ad)
        begin_time = get_current_epoch_time()
        start_qxdm_logger(self.ad, begin_time)
        start_adb_tcpdump(self.ad)
        self.ad.log.info("Turn airplane mode on")
        force_airplane_mode(self.ad, True)
        wifi_toggle_state(self.ad, True)
        connect_to_wifi_network(
            self.ad, self.ssid_map[self.pixel_lab_network[0]["SSID"]])
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="ws", iteration=10)
        ws_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        ws_result = check_ttff_data(self.ad,
                                    ws_ttff_data,
                                    ttff_mode="Warm Start",
                                    criteria=self.xtra_ws_criteria)
        xtra_result.append(ws_result)
        begin_time = get_current_epoch_time()
        process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
        start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=10)
        cs_ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                   self.pixel_lab_location)
        cs_result = check_ttff_data(self.ad,
                                    cs_ttff_data,
                                    ttff_mode="Cold Start",
                                    criteria=self.wifi_xtra_cs_criteria)
        xtra_result.append(cs_result)
        asserts.assert_true(all(xtra_result),
                            "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="1745b8a4-5925-4aa0-809a-1b17e848dc9c")
    def test_xtra_modem_ssr(self):
        """Verify XTRA functionality after modem silent reboot.

        Steps:
            1. Trigger modem crash by adb.
            2. Wait 1 minute for modem to recover.
            3. XTRA TTFF Cold Start for 3 iteration.
            4. Repeat Step1. to Step 3. for 5 times.

        Expected Results:
            All XTRA TTFF Cold Start results should be within xtra_cs_criteria.
        """
        disable_supl_mode(self.ad)
        xtra_ssr_test_result_all = []
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        for times in range(1, 6):
            begin_time = get_current_epoch_time()
            gnss_trigger_modem_ssr_by_mds(self.ad)
            if not verify_internet_connection(self.ad.log, self.ad, retries=3,
                                              expected_state=True):
                raise signals.TestFailure("Fail to connect to LTE network.")
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            start_ttff_by_gtw_gpstool(self.ad, ttff_mode="cs", iteration=3)
            ttff_data = process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                    self.pixel_lab_location)
            xtra_ssr_test_result = check_ttff_data(
                self.ad, ttff_data, ttff_mode="Cold Start",
                criteria=self.xtra_cs_criteria)
            self.ad.log.info("XTRA after Modem SSR test %d times -> %s"
                             % (times, xtra_ssr_test_result))
            xtra_ssr_test_result_all.append(xtra_ssr_test_result)
        asserts.assert_true(all(xtra_ssr_test_result_all),
                            "TTFF fails to reach designated criteria")

    @test_tracker_info(uuid="4d6e81e1-3abb-4e03-b732-7b6b497a2258")
    def test_xtra_download_mobile_data(self):
        """Verify XTRA data could be downloaded via mobile data.

        Steps:
            1. Delete all GNSS aiding data.
            2. Get location fixed.
            3. Verify whether XTRA is downloaded and injected.
            4. Repeat Step 1. to Step 3. for 5 times.

        Expected Results:
            XTRA data is properly downloaded and injected via mobile data.
        """
        mobile_xtra_result_all = []
        disable_supl_mode(self.ad)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        for i in range(1, 6):
            begin_time = get_current_epoch_time()
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            time.sleep(5)
            start_gnss_by_gtw_gpstool(self.ad, False)
            mobile_xtra_result = check_xtra_download(self.ad, begin_time)
            self.ad.log.info("Iteration %d => %s" % (i, mobile_xtra_result))
            mobile_xtra_result_all.append(mobile_xtra_result)
        asserts.assert_true(all(mobile_xtra_result_all),
                            "Fail to Download XTRA file")

    @test_tracker_info(uuid="625ac665-1446-4406-a722-e6a19645222c")
    def test_xtra_download_wifi(self):
        """Verify XTRA data could be downloaded via WiFi.

        Steps:
            1. Connect to WiFi.
            2. Delete all GNSS aiding data.
            3. Get location fixed.
            4. Verify whether XTRA is downloaded and injected.
            5. Repeat Step 2. to Step 4. for 5 times.

        Expected Results:
            XTRA data is properly downloaded and injected via WiFi.
        """
        wifi_xtra_result_all = []
        disable_supl_mode(self.ad)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        self.ad.log.info("Turn airplane mode on")
        force_airplane_mode(self.ad, True)
        wifi_toggle_state(self.ad, True)
        connect_to_wifi_network(
            self.ad, self.ssid_map[self.pixel_lab_network[0]["SSID"]])
        for i in range(1, 6):
            begin_time = get_current_epoch_time()
            process_gnss_by_gtw_gpstool(self.ad, self.standalone_cs_criteria)
            time.sleep(5)
            start_gnss_by_gtw_gpstool(self.ad, False)
            wifi_xtra_result = check_xtra_download(self.ad, begin_time)
            wifi_xtra_result_all.append(wifi_xtra_result)
            self.ad.log.info("Iteraion %d => %s" % (i, wifi_xtra_result))
        asserts.assert_true(all(wifi_xtra_result_all),
                            "Fail to Download XTRA file")

    @test_tracker_info(uuid="2a9f2890-3c0a-48b8-821d-bf97e36355e9")
    def test_quick_toggle_gnss_state(self):
        """Verify GNSS can still work properly after quick toggle GNSS off
        to on.

        Steps:
            1. Launch GTW_GPSTool.
            2. Go to "Advance setting"
            3. Set Cycle = 10 & Time-out = 60
            4. Go to "Toggle GPS" tab
            5. Execute "Start"

        Expected Results:
            No single Timeout is seen in 10 iterations.
        """
        enable_supl_mode(self.ad)
        reboot(self.ad)
        start_qxdm_logger(self.ad, get_current_epoch_time())
        start_adb_tcpdump(self.ad)
        start_toggle_gnss_by_gtw_gpstool(self.ad, iteration=10)
