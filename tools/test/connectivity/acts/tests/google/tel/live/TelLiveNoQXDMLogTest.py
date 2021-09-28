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
    Test Script for Telephony Pre Check In Sanity
"""

import collections
import time
import os
import re

from acts import signals
from acts.utils import unzip_maintain_permissions
from acts.utils import exe_cmd
from acts.controllers.android_device import SL4A_APK_NAME
from acts.controllers.android_device import list_adb_devices
from acts.controllers.android_device import list_fastboot_devices
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_BOOT_COMPLETE
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_CARRIERCONFIG_CHANGE
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_CARRIERID_CHANGE
from acts.test_utils.tel.tel_defines import VZW_CARRIER_CONFIG_VERSION
from acts.test_utils.tel.tel_defines import ATT_CARRIER_CONFIG_VERSION
from acts.test_utils.tel.tel_defines import CARRIER_ID_METADATA_URL
from acts.test_utils.tel.tel_defines import CARRIER_ID_CONTENT_URL
from acts.test_utils.tel.tel_defines import CARRIER_ID_VERSION
from acts.test_utils.tel.tel_defines import CARRIER_ID_METADATA_URL_P
from acts.test_utils.tel.tel_defines import CARRIER_ID_CONTENT_URL_P
from acts.test_utils.tel.tel_defines import CARRIER_ID_VERSION_P
from acts.test_utils.tel.tel_lookup_tables import device_capabilities
from acts.test_utils.tel.tel_lookup_tables import operator_capabilities
from acts.test_utils.tel.tel_test_utils import lock_lte_band_by_mds
from acts.test_utils.tel.tel_test_utils import get_model_name
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import reboot_device
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.tel.tel_test_utils import trigger_modem_crash_by_modem
from acts.test_utils.tel.tel_test_utils import bring_up_sl4a
from acts.test_utils.tel.tel_test_utils import fastboot_wipe
from acts.test_utils.tel.tel_test_utils import get_carrier_config_version
from acts.test_utils.tel.tel_test_utils import get_carrier_id_version
from acts.test_utils.tel.tel_test_utils import adb_disable_verity
from acts.test_utils.tel.tel_test_utils import install_carriersettings_apk
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import cleanup_configupdater
from acts.test_utils.tel.tel_test_utils import pull_carrier_id_files
from acts.test_utils.tel.tel_test_utils import wifi_toggle_state
from acts.test_utils.tel.tel_voice_utils import phone_setup_volte
from acts.test_utils.tel.tel_subscription_utils import get_cbrs_and_default_sub_id
from acts.utils import get_current_epoch_time
from acts.keys import Config

class TelLiveNoQXDMLogTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.dut = self.android_devices[0]
        if len(self.android_devices) > 1:
            self.ad_reference = self.android_devices[1]
            setattr(self.ad_reference, "qxdm_log", False)
        else:
            self.ad_reference = None
        setattr(self.dut, "qxdm_log", False)
        self.stress_test_number = int(
            self.user_params.get("stress_test_number", 5))
        self.skip_reset_between_cases = False
        self.dut_model = get_model_name(self.dut)
        self.dut_operator = get_operator_name(self.log, self.dut)
        self.dut_capabilities = set(
            device_capabilities.get(
                self.dut_model, device_capabilities["default"])) & set(
                    operator_capabilities.get(
                        self.dut_operator, operator_capabilities["default"]))
        self.dut.log.info("DUT capabilities: %s", self.dut_capabilities)
        self.user_params["check_crash"] = False
        self.skip_reset_between_cases = False

    def _get_list_average(self, input_list):
        total_sum = float(sum(input_list))
        total_count = float(len(input_list))
        if input_list == []:
            return False
        return float(total_sum / total_count)

    def _telephony_bootup_time_test(self):
        """Telephony Bootup Perf Test

        Arguments:
            check_lte_data: whether to check the LTE data.
            check_volte: whether to check Voice over LTE.
            check_wfc: whether to check Wifi Calling.

        Expected Results:
            Time

        Returns:
            True is pass, False if fail.
        """
        self.number_of_devices = 1
        ad = self.dut
        toggle_airplane_mode(self.log, ad, False)
        if not phone_setup_volte(self.log, ad):
            ad.log.error("Failed to setup VoLTE.")
            return False
        fail_count = collections.defaultdict(int)
        test_result = True
        keyword_time_dict = {}

        text_search_mapping = {
            'boot_complete': "ModemService: Received: android.intent.action.BOOT_COMPLETED",
            'Voice_Reg': "< VOICE_REGISTRATION_STATE {.regState = REG_HOME",
            'Data_Reg': "< DATA_REGISTRATION_STATE {.regState = REG_HOME",
            'Data_Call_Up': "onSetupConnectionCompleted result=SUCCESS",
            'VoLTE_Enabled': "isVolteEnabled=true",
        }

        text_obj_mapping = {
            "boot_complete": None,
            "Voice_Reg": None,
            "Data_Reg": None,
            "Data_Call_Up": None,
            "VoLTE_Enabled": None,
        }
        blocked_for_calculate = ["boot_complete"]
        for i in range(1, self.stress_test_number + 1):
            ad.log.info("Telephony Bootup Time Test %s Iteration: %d / %d",
                        self.test_name, i, self.stress_test_number)
            begin_time = get_current_epoch_time()
            ad.log.debug("Begin Time is %s", begin_time)
            ad.log.info("reboot!")
            reboot_device(ad)
            iteration_result = "pass"

            time.sleep(WAIT_TIME_FOR_BOOT_COMPLETE)

            dict_match = ad.search_logcat(
                text_search_mapping['boot_complete'], begin_time=begin_time)
            if len(dict_match) != 0:
                text_obj_mapping['boot_complete'] = dict_match[0][
                    'datetime_obj']
                ad.log.debug("Datetime for boot_complete is %s",
                             text_obj_mapping['boot_complete'])
                bootup_time = dict_match[0]['datetime_obj'].strftime('%s')
                bootup_time = int(bootup_time) * 1000
                ad.log.info("Bootup Time is %d", bootup_time)
            else:
                ad.log.error("TERMINATE- boot_complete not seen in logcat")
                return False

            for tel_state in text_search_mapping:
                if tel_state == "boot_complete":
                    continue
                dict_match = ad.search_logcat(
                    text_search_mapping[tel_state], begin_time=bootup_time)
                if len(dict_match) != 0:
                    text_obj_mapping[tel_state] = dict_match[0]['datetime_obj']
                    ad.log.debug("Datetime for %s is %s", tel_state,
                                 text_obj_mapping[tel_state])
                else:
                    ad.log.error("Cannot Find Text %s in logcat",
                                 text_search_mapping[tel_state])
                    blocked_for_calculate.append(tel_state)
                    ad.log.debug("New Blocked %s", blocked_for_calculate)

            ad.log.info("List Blocked %s", blocked_for_calculate)
            for tel_state in text_search_mapping:
                if tel_state not in blocked_for_calculate:
                    time_diff = text_obj_mapping[tel_state] - \
                                text_obj_mapping['boot_complete']
                    ad.log.info("Time Diff is %d for %s", time_diff.seconds,
                                tel_state)
                    if tel_state in keyword_time_dict:
                        keyword_time_dict[tel_state].append(time_diff.seconds)
                    else:
                        keyword_time_dict[tel_state] = [
                            time_diff.seconds,
                        ]
                    ad.log.debug("Keyword Time Dict %s", keyword_time_dict)

            ad.log.info("Telephony Bootup Time Test %s Iteration: %d / %d %s",
                        self.test_name, i, self.stress_test_number,
                        iteration_result)
        ad.log.info("Final Keyword Time Dict %s", keyword_time_dict)
        for tel_state in text_search_mapping:
            if tel_state not in blocked_for_calculate:
                avg_time = self._get_list_average(keyword_time_dict[tel_state])
                if avg_time < 12.0:
                    ad.log.info("Average %s for %d iterations = %.2f seconds",
                                tel_state, self.stress_test_number, avg_time)
                else:
                    ad.log.error("Average %s for %d iterations = %.2f seconds",
                                 tel_state, self.stress_test_number, avg_time)
                    fail_count[tel_state] += 1

        ad.log.info("Bootup Time Dict: %s", keyword_time_dict)
        ad.log.info("fail_count: %s", dict(fail_count))
        for failure, count in fail_count.items():
            if count:
                ad.log.error("%s %s failures in %s iterations", count, failure,
                             self.stress_test_number)
                test_result = False
        return test_result

    def _cbrs_bootup_time_test(self):
        """CBRS Bootup Perf Test

        Expected Results:
            Time

        Returns:
            True is pass, False if fail.
        """
        self.number_of_devices = 1
        ad = self.dut
        cbrs_subid, default_subid = get_cbrs_and_default_sub_id(ad)
        toggle_airplane_mode(self.log, ad, False)

        fail_count = collections.defaultdict(int)
        test_result = True
        keyword_time_dict = {}

        text_search_mapping = {
            'boot_complete': "ModemService: Received: android.intent.action.BOOT_COMPLETED",
            'cbrs_active': "notifyPreferredDataSubIdChanged to %s" % cbrs_subid,
        }

        text_obj_mapping = {
            "boot_complete": None,
            "cbrs_active": None,
        }
        blocked_for_calculate = ["boot_complete"]
        for i in range(1, self.stress_test_number + 1):
            ad.log.info("CBRS Bootup Time Test %s Iteration: %d / %d",
                        self.test_name, i, self.stress_test_number)
            begin_time = get_current_epoch_time()
            ad.log.debug("Begin Time is %s", begin_time)
            ad.log.info("reboot!")
            reboot_device(ad)
            iteration_result = "pass"

            time.sleep(WAIT_TIME_FOR_BOOT_COMPLETE)

            dict_match = ad.search_logcat(
                text_search_mapping['boot_complete'], begin_time=begin_time)
            if len(dict_match) != 0:
                text_obj_mapping['boot_complete'] = dict_match[0][
                    'datetime_obj']
                ad.log.debug("Datetime for boot_complete is %s",
                             text_obj_mapping['boot_complete'])
                bootup_time = dict_match[0]['datetime_obj'].strftime('%s')
                bootup_time = int(bootup_time) * 1000
                ad.log.info("Bootup Time is %d", bootup_time)
            else:
                ad.log.error("TERMINATE- boot_complete not seen in logcat")
                return False

            for tel_state in text_search_mapping:
                if tel_state == "boot_complete":
                    continue
                dict_match = ad.search_logcat(
                    text_search_mapping[tel_state], begin_time=bootup_time)
                if len(dict_match) != 0:
                    text_obj_mapping[tel_state] = dict_match[0]['datetime_obj']
                    ad.log.debug("Datetime for %s is %s", tel_state,
                                 text_obj_mapping[tel_state])
                else:
                    ad.log.error("Cannot Find Text %s in logcat",
                                 text_search_mapping[tel_state])
                    blocked_for_calculate.append(tel_state)
                    ad.log.debug("New Blocked %s", blocked_for_calculate)

            ad.log.info("List Blocked %s", blocked_for_calculate)
            for tel_state in text_search_mapping:
                if tel_state not in blocked_for_calculate:
                    time_diff = text_obj_mapping[tel_state] - \
                                text_obj_mapping['boot_complete']
                    ad.log.info("Time Diff is %d for %s", time_diff.seconds,
                                tel_state)
                    if tel_state in keyword_time_dict:
                        keyword_time_dict[tel_state].append(time_diff.seconds)
                    else:
                        keyword_time_dict[tel_state] = [
                            time_diff.seconds,
                        ]
                    ad.log.debug("Keyword Time Dict %s", keyword_time_dict)

            ad.log.info("CBRS Bootup Time Test %s Iteration: %d / %d %s",
                        self.test_name, i, self.stress_test_number,
                        iteration_result)
        ad.log.info("Final Keyword Time Dict %s", keyword_time_dict)
        for tel_state in text_search_mapping:
            if tel_state not in blocked_for_calculate:
                avg_time = self._get_list_average(keyword_time_dict[tel_state])
                if avg_time < 12.0:
                    ad.log.info("Average %s for %d iterations = %.2f seconds",
                                tel_state, self.stress_test_number, avg_time)
                else:
                    ad.log.error("Average %s for %d iterations = %.2f seconds",
                                 tel_state, self.stress_test_number, avg_time)
                    fail_count[tel_state] += 1

        ad.log.info("Bootup Time Dict: %s", keyword_time_dict)
        ad.log.info("fail_count: %s", dict(fail_count))
        for failure, count in fail_count.items():
            if count:
                ad.log.error("%s %s failures in %s iterations", count, failure,
                             self.stress_test_number)
                test_result = False
        return test_result

    """ Tests Begin """

    @test_tracker_info(uuid="109d59ff-a488-4a68-87fd-2d8d0c035326")
    @TelephonyBaseTest.tel_test_wrap
    def test_bootup_optimized_stress(self):
        """Bootup Optimized Reliability Test

        Steps:
            1. Reboot DUT.
            2. Parse logcat for time taken by Voice, Data, VoLTE
            3. Repeat Step 1~2 for N times. (before reboot)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        return self._telephony_bootup_time_test()

    @test_tracker_info(uuid="d29e6e62-3d54-4a58-b67f-2ba0de3d0a19")
    @TelephonyBaseTest.tel_test_wrap
    def test_bootup_cbrs_stress(self):
        """Bootup Optimized Reliability Test

        Steps:
            1. Reboot DUT.
            2. Parse logcat for time taken by CBRS data
            3. Repeat Step 1~2 for N times. (before reboot)

        Expected Results:
            No crash happens in stress test.

        Returns:
            True is pass, False if fail.
        """
        return self._cbrs_bootup_time_test()

    @test_tracker_info(uuid="67f50d11-a987-4e79-9a20-1569d365511b")
    @TelephonyBaseTest.tel_test_wrap
    def test_modem_power_anomaly_file_existence(self):
        """Verify if the power anomaly file exists

        1. Collect Bugreport
        2. unzip bugreport
        3. remane the .bin file to .tar
        4. unzip dumpstate.tar
        5. Verify if the file exists

        """
        ad = self.android_devices[0]
        cmd = ("am broadcast -a "
               "com.google.gservices.intent.action.GSERVICES_OVERRIDE "
               "-e \"ce.cm.power_anomaly_data_enable\" \"true\"")
        ad.adb.shell(cmd)
        time.sleep(60)
        begin_time = get_current_epoch_time()
        for i in range(3):
            try:
                ad.take_bug_report(self.test_name, begin_time)
                bugreport_path = ad.device_log_path
                break
            except Exception as e:
                ad.log.error("bugreport attempt %s error: %s", i + 1, e)
        ad.log.info("Bugreport Path is %s" % bugreport_path)
        try:
            list_of_files = os.listdir(bugreport_path)
            ad.log.info(list_of_files)
            for filename in list_of_files:
                if ".zip" in filename:
                    ad.log.info(filename)
                    file_path = os.path.join(bugreport_path, filename)
                    ad.log.info(file_path)
                    unzip_maintain_permissions(file_path, bugreport_path)
            dumpstate_path = os.path.join(bugreport_path,
                                          "dumpstate_board.bin")
            if os.path.isfile(dumpstate_path):
                os.rename(dumpstate_path,
                          bugreport_path + "/dumpstate_board.tar")
                os.chmod(bugreport_path + "/dumpstate_board.tar", 0o777)
                current_dir = os.getcwd()
                os.chdir(bugreport_path)
                exe_cmd("tar -xvf %s" %
                        (bugreport_path + "/dumpstate_board.tar"))
                os.chdir(current_dir)
            else:
                ad.log.info("The dumpstate_path file %s does not exist" % dumpstate_path)
            if os.path.isfile(bugreport_path + "/power_anomaly_data.txt"):
                ad.log.info("Modem Power Anomaly File Exists!!")
                return True
            ad.log.info("Modem Power Anomaly File DO NOT Exist!!")
            return False
        except Exception as e:
            ad.log.error(e)
            return False

    @TelephonyBaseTest.tel_test_wrap
    def test_lock_lte_band_4(self):
        """Set LTE band lock 4"""
        if not self.dut.is_apk_installed("com.google.mdstest"):
            raise signals.TestSkip("mdstest is not installed")
        return lock_lte_band_by_mds(self.dut, "4")

    @TelephonyBaseTest.tel_test_wrap
    def test_lock_lte_band_13(self):
        """Set LTE band lock 4"""
        if not self.dut.is_apk_installed("com.google.mdstest"):
            raise signals.TestSkip("mdstest is not installed")
        return lock_lte_band_by_mds(self.dut, "13")

    @test_tracker_info(uuid="e2a8cb1e-7998-4912-9e16-d9e5f1daee5d")
    @TelephonyBaseTest.tel_test_wrap
    def test_modem_ssr_rampdump_generation(self):
        """Trigger Modem SSR Crash and Verify if Ramdumps are generated

        1. Empty the rampdump dir
        2. Trigger ModemSSR
        3. Verify if rampdumps are getting generated or not

        """
        if not self.dut.is_apk_installed("com.google.mdstest"):
            raise signals.TestSkip("mdstest is not installed")
        try:
            ad = self.android_devices[0]
            ad.adb.shell("rm -rf /data/vendor/ssrdump/*", ignore_status=True)
            if not trigger_modem_crash_by_modem(ad):
                ad.log.error("Failed to trigger Modem SSR, aborting...")
                return False
            out = ad.adb.shell("ls -l /data/vendor/ssrdump/ramdump_modem_*",
                               ignore_status=True)
            if "No such file" in out or not out:
                ad.log.error("Ramdump Modem File not found post SSR\n %s", out)
                return False
            ad.log.info("Ramdump Modem File found post SSR\n %s", out)
            return True
        except Exception as e:
            ad.log.error(e)
            return False
        finally:
            ad.adb.shell("rm -rf /data/vendor/ssrdump/*", ignore_status=True)

    @test_tracker_info(uuid="e12b2f00-7e18-47d6-b310-aabb96b165a3")
    @TelephonyBaseTest.tel_test_wrap
    def test_factory_modem_offline(self):
        """Trigger Modem Factory Offline and verify if Modem Offline

        1. Device in Fastboot
        2. Wipe Userdata and set the fastboot command for factory
        3. Device will bootup in adb, verify Modem Offline
        4. Reboot again, and verify camping

        """
        try:
            ad = self.android_devices[0]
            skip_setup_wizard=True

            # Pull sl4a apk from device
            out = ad.adb.shell("pm path %s" % SL4A_APK_NAME)
            result = re.search(r"package:(.*)", out)
            if not result:
                ad.log.error("Couldn't find sl4a apk")
                return False
            else:
                sl4a_apk = result.group(1)
                ad.log.info("Get sl4a apk from %s", sl4a_apk)
                ad.pull_files([sl4a_apk], "/tmp/")
            ad.stop_services()

            # Fastboot Wipe
            if ad.serial in list_adb_devices():
                ad.log.info("Reboot to bootloader")
                ad.adb.reboot("bootloader", ignore_status=True)
                time.sleep(10)
            if ad.serial in list_fastboot_devices():
                ad.log.info("Wipe in fastboot")
                ad.fastboot._w(timeout=300, ignore_status=True)
                time.sleep(30)

                # Factory Silent Mode Test
                ad.log.info("Factory Offline in fastboot")
                ad.fastboot.oem("continue-factory")
                time.sleep(30)
                ad.wait_for_boot_completion()
                ad.root_adb()
                ad.log.info("Re-install sl4a")
                ad.adb.shell("settings put global verifier_verify_adb_installs"
                             " 0")
                ad.adb.install("-r /tmp/base.apk")
                time.sleep(10)
                try:
                    ad.start_adb_logcat()
                except:
                    ad.log.error("Failed to start adb logcat!")
                bring_up_sl4a(ad)
                radio_state = ad.droid.telephonyIsRadioOn()
                ad.log.info("Radio State is %s", radio_state)
                if radio_state:
                    ad.log.error("Radio state is ON in Factory Mode")
                    return False
                toggle_airplane_mode(self.log, ad, True)
                time.sleep(5)
                toggle_airplane_mode(self.log, ad, False)
                radio_state = ad.droid.telephonyIsRadioOn()
                ad.log.info("Radio State is %s", radio_state)
                if ad.droid.telephonyIsRadioOn():
                    ad.log.error("Radio state is ON after Airplane Toggle")
                    return False
                ad.log.info("Rebooting and verifying back in service")

                # Bring it back to Online Mode
                ad.stop_services()
                ad.adb.reboot()
                ad.wait_for_boot_completion()
                ad.root_adb()
                bring_up_sl4a(ad)
                radio_state = ad.droid.telephonyIsRadioOn()
                ad.log.info("Radio State is %s", radio_state)
                if not radio_state:
                    ad.log.error("Radio state is OFF in Online Mode")
                    return False
                return True
        except Exception as e:
            ad.log.error(e)
            return False
        finally:
            ad.exit_setup_wizard()
            bring_up_sl4a(ad)

    @test_tracker_info(uuid="e681e6e2-a33c-4cbd-9ec4-8faa003cde6b")
    @TelephonyBaseTest.tel_test_wrap
    def test_carrier_config_version_after_fdr(self):
        """Carrier Config Version Test after FDR

        1. Disable Verity, remount, push carriersettings apk
        2. WiFi is connected
        3. Perform FDR, and re-connect WiFi
        4. Wait for 45 mins and keep checking for version match

        """
        try:
            cc_version_mapping = {
                'vzw': VZW_CARRIER_CONFIG_VERSION,
                'Verizon': VZW_CARRIER_CONFIG_VERSION,
                'att': ATT_CARRIER_CONFIG_VERSION,
            }
            result_flag = False
            time_var = 1
            ad = self.android_devices[0]
            skip_setup_wizard=True

            # CarrierSettingsApk
            carriersettingsapk = self.user_params["carriersettingsapk"]
            if isinstance(carriersettingsapk, list):
                carriersettingsapk = carriersettingsapk[0]
            ad.log.info("Using file path %s", carriersettingsapk)

            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("connect WiFi failed")
                return False

            # Setup Steps
            adb_disable_verity(ad)
            install_carriersettings_apk(ad, carriersettingsapk)

            # FDR
            ad.log.info("Performing FDR")
            fastboot_wipe(ad)
            ad.log.info("FDR Complete")
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("Connect WiFi failed")

            # Wait for 45 mins for CC version upgrade
            while(time_var < WAIT_TIME_FOR_CARRIERCONFIG_CHANGE):
                current_version = get_carrier_config_version(ad)
                if current_version == cc_version_mapping[self.dut_operator]:
                    ad.log.info("Carrier Config Version Match %s in %s mins",
                                current_version, time_var)
                    result_flag = True
                    break
                else:
                    ad.log.debug("Carrier Config Version Not Match")
                time.sleep(60)
                time_var += 1
            if not result_flag:
                ad.log.info("Carrier Config Failed to Update in %s mins",
                             WAIT_TIME_FOR_CARRIERCONFIG_CHANGE)
            return result_flag
        except Exception as e:
            ad.log.error(e)
            return False


    @test_tracker_info(uuid="41e6f2d3-76c9-4d3d-97b3-7075ad98bd41")
    @TelephonyBaseTest.tel_test_wrap
    def test_carrier_id_update_wifi_connected(self):
        """Carrier Id Version Test after WiFi Connected

        1. WiFi is connected
        2. Perform setup steps to cleanup shared_prefs
        3. Send P/H flag update to configUpdater
        4. Wait for 5 mins and keep checking for version match

        """
        try:
            result_flag = False
            time_var = 1
            ad = self.android_devices[0]
            if ad.adb.getprop("ro.build.version.release")[0] in ("9", "P"):
                CARRIER_ID_VERSION = CARRIER_ID_VERSION_P
                CARRIER_ID_METADATA_URL = CARRIER_ID_METADATA_URL_P
                CARRIER_ID_CONTENT_URL = CARRIER_ID_CONTENT_URL_P

            ad.log.info("Before - CarrierId is %s", get_carrier_id_version(ad))
            # Setup Steps
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("connect WiFi failed")
                return False
            cleanup_configupdater(ad)
            time.sleep(5)

            # Trigger Config Update
            ad.log.info("Triggering Config Update")
            ad.log.info("%s", CARRIER_ID_METADATA_URL)
            exe_cmd("adb -s %s shell %s" % (ad.serial,CARRIER_ID_METADATA_URL))
            ad.log.info("%s", CARRIER_ID_CONTENT_URL)
            exe_cmd("adb -s %s shell %s" % (ad.serial,CARRIER_ID_CONTENT_URL))

            # Wait for 5 mins for Carrier Id version upgrade
            while(time_var < WAIT_TIME_FOR_CARRIERID_CHANGE):
                current_version = get_carrier_id_version(ad)
                if current_version == CARRIER_ID_VERSION:
                    ad.log.info("After CarrierId is %s in %s mins",
                                current_version, time_var)
                    result_flag = True
                    break
                else:
                    ad.log.debug("Carrier Id Version Not Match")
                time.sleep(60)
                time_var += 1

            if not result_flag:
                ad.log.info("Carrier Id Failed to Update in %s mins",
                             WAIT_TIME_FOR_CARRIERID_CHANGE)

            # pb file check
            out = ad.adb.shell("ls -l data/misc/carrierid/carrier_list.pb")
            if "No such" in out:
                ad.log.error("carrier_list.pb file is missing")
                result_flag = False
            else:
                ad.log.info("carrier_list.pb file is present")
            return result_flag
        except Exception as e:
            ad.log.error(e)
            return False
        finally:
            carrier_id_path = os.path.join(self.log_path, self.test_name,
                                           "CarrierId_%s" % ad.serial)
            pull_carrier_id_files(ad, carrier_id_path)


    @test_tracker_info(uuid="836d3963-f56d-438e-a35c-0706ac385153")
    @TelephonyBaseTest.tel_test_wrap
    def test_carrier_id_update_wifi_disconnected(self):
        """Carrier Id Version Test with WiFi disconnected

        1. WiFi is connected
        2. Perform setup steps to cleanup shared_prefs
        3. Send P/H flag update to configUpdater
        4. Wait for 5 mins and keep checking for version match

        """
        try:
            result_flag = False
            time_var = 1
            ad = self.android_devices[0]
            ad.log.info("Before - CarrierId is %s", get_carrier_id_version(ad))

            # Wifi Disconnect
            cleanup_configupdater(ad)
            wifi_toggle_state(ad.log, ad, False)

            # Trigger Config Update
            ad.log.info("Triggering Config Update")
            ad.log.info("%s", CARRIER_ID_METADATA_URL)
            exe_cmd("adb -s %s shell %s" % (ad.serial,CARRIER_ID_METADATA_URL))
            ad.log.info("%s", CARRIER_ID_CONTENT_URL)
            exe_cmd("adb -s %s shell %s" % (ad.serial,CARRIER_ID_CONTENT_URL))

            # Wait for 5 mins for Carrier Id version upgrade
            while(time_var < WAIT_TIME_FOR_CARRIERID_CHANGE):
                current_version = get_carrier_id_version(ad)
                if current_version == CARRIER_ID_VERSION:
                    ad.log.info("After CarrierId is %s in %s mins",
                                current_version, time_var)
                    return False
                else:
                    ad.log.debug("Carrier Id Version Not Match")
                time.sleep(60)
                time_var += 1
            time_var = 1
            ad.log.info("Success - CarrierId not upgraded during WiFi OFF")

            # WiFi Connect
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("connect WiFi failed")
                return False

            # Wait for 5 mins for Carrier Id version upgrade
            while(time_var < WAIT_TIME_FOR_CARRIERID_CHANGE):
                current_version = get_carrier_id_version(ad)
                if current_version == CARRIER_ID_VERSION:
                    ad.log.info("After CarrierId is %s in %s mins",
                                current_version, time_var)
                    result_flag = True
                    break
                else:
                    ad.log.debug("Carrier Id Version Not Match")
                time.sleep(60)
                time_var += 1

            if not result_flag:
                ad.log.info("Carrier Id Failed to Update in %s mins",
                             WAIT_TIME_FOR_CARRIERID_CHANGE)
            # pb file check
            out = ad.adb.shell("ls -l data/misc/carrierid/carrier_list.pb")
            if not out or "No such" in out:
                ad.log.error("carrier_list.pb file is missing")
                result_flag = False
            else:
                ad.log.info("carrier_list.pb file is present")
            return result_flag
        except Exception as e:
            ad.log.error(e)
            return False
        finally:
            carrier_id_path = os.path.join(self.log_path, self.test_name,
                                           "CarrierId_%s" % ad.serial)
            pull_carrier_id_files(ad, carrier_id_path)
""" Tests End """
