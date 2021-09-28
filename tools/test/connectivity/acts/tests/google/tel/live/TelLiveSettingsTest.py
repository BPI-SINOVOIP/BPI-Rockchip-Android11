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
    Test Script for Telephony Settings
"""

import os
import time

from acts import signals
from acts.keys import Config
from acts.utils import unzip_maintain_permissions
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_FOR_STATE_CHANGE
from acts.test_utils.tel.tel_test_utils import dumpsys_carrier_config
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import flash_radio
from acts.test_utils.tel.tel_test_utils import get_outgoing_voice_sub_id
from acts.test_utils.tel.tel_test_utils import get_slot_index_from_subid
from acts.test_utils.tel.tel_test_utils import is_sim_locked
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_test_utils import power_off_sim
from acts.test_utils.tel.tel_test_utils import power_on_sim
from acts.test_utils.tel.tel_test_utils import print_radio_info
from acts.test_utils.tel.tel_test_utils import revert_default_telephony_setting
from acts.test_utils.tel.tel_test_utils import set_qxdm_logger_command
from acts.test_utils.tel.tel_test_utils import system_file_push
from acts.test_utils.tel.tel_test_utils import unlock_sim
from acts.test_utils.tel.tel_test_utils import verify_default_telephony_setting
from acts.utils import set_mobile_data_always_on


class TelLiveSettingsTest(TelephonyBaseTest):
    def setup_class(self):
        TelephonyBaseTest.setup_class(self)
        self.dut = self.android_devices[0]
        self.number_of_devices = 1
        self.stress_test_number = self.get_stress_test_number()
        self.carrier_configs = dumpsys_carrier_config(self.dut)
        self.dut_subID = get_outgoing_voice_sub_id(self.dut)
        self.dut_capabilities = self.dut.telephony["subscription"][self.dut_subID].get("capabilities", [])

    @test_tracker_info(uuid="c6149bd6-7080-453d-af37-1f9bd350a764")
    @TelephonyBaseTest.tel_test_wrap
    def test_telephony_factory_reset(self):
        """Test VOLTE is enabled WFC is disabled after telephony factory reset.

        Steps:
        1. Setup DUT with various dataroaming, mobiledata, and default_network.
        2. Call telephony factory reset.
        3. Verify DUT back to factory default.

        Expected Results: dataroaming is off, mobiledata is on, network
                          preference is back to default.
        """
        revert_default_telephony_setting(self.dut)
        self.dut.log.info("Call telephony factory reset")
        self.dut.droid.telephonyFactoryReset()
        time.sleep(MAX_WAIT_TIME_FOR_STATE_CHANGE)
        return verify_default_telephony_setting(self.dut)

    @test_tracker_info(uuid="3afa2070-564f-4e6c-b08d-12dd4381abb9")
    @TelephonyBaseTest.tel_test_wrap
    def test_check_carrier_config(self):
        """Check the carrier_config and network setting for different carrier.

        Steps:
        1. Device loaded with different SIM cards.
        2. Check the carrier_configs are expected value.

        """
        pass

    @test_tracker_info(uuid="64deba57-c1c2-422f-b771-639c95edfbc0")
    @TelephonyBaseTest.tel_test_wrap
    def test_disable_mobile_data_always_on(self):
        """Verify mobile_data_always_on can be disabled.

        Steps:
        1. Disable mobile_data_always_on by adb.
        2. Verify the mobile data_always_on state.

        Expected Results: mobile_data_always_on return 0.
        """
        self.dut.log.info("Disable mobile_data_always_on")
        set_mobile_data_always_on(self.dut, False)
        time.sleep(1)
        return self.dut.adb.shell(
            "settings get global mobile_data_always_on") == "0"

    @test_tracker_info(uuid="56ddcd5a-92b0-46c7-9c2b-d743794efb7c")
    @TelephonyBaseTest.tel_test_wrap
    def test_enable_mobile_data_always_on(self):
        """Verify mobile_data_always_on can be enabled.

        Steps:
        1. Enable mobile_data_always_on by adb.
        2. Verify the mobile data_always_on state.

        Expected Results: mobile_data_always_on return 1.
        """
        self.dut.log.info("Enable mobile_data_always_on")
        set_mobile_data_always_on(self.dut, True)
        time.sleep(1)
        return "1" in self.dut.adb.shell(
            "settings get global mobile_data_always_on")

    @test_tracker_info(uuid="c2cc5b66-40af-4ba6-81cb-6c44ae34cbbb")
    @TelephonyBaseTest.tel_test_wrap
    def test_push_new_radio_or_mbn(self):
        """Verify new mdn and radio can be push to device.

        Steps:
        1. If new radio path is given, flash new radio on the device.
        2. Verify the radio version.
        3. If new mbn path is given, push new mbn to device.
        4. Verify the installed mbn version.

        Expected Results:
        radio and mbn can be pushed to device and mbn.ver is available.
        """
        result = True
        paths = {}
        for path_key, dst_name in zip(["radio_image", "mbn_path"],
                                      ["radio.img", "mcfg_sw"]):
            path = self.user_params.get(path_key)
            if not path:
                continue
            elif isinstance(path, list):
                if not path[0]:
                    continue
                path = path[0]
            if "dev/null" in path:
                continue
            if not os.path.exists(path):
                self.log.error("path %s does not exist", path)
                self.log.info(self.user_params)
                path = os.path.join(
                    self.user_params[Config.key_config_path.value], path)
                if not os.path.exists(path):
                    self.log.error("path %s does not exist", path)
                    continue

            self.log.info("%s path = %s", path_key, path)
            if "zip" in path:
                self.log.info("Unzip %s", path)
                file_path, file_name = os.path.split(path)
                dest_path = os.path.join(file_path, dst_name)
                os.system("rm -rf %s" % dest_path)
                unzip_maintain_permissions(path, file_path)
                path = dest_path
            os.system("chmod -R 777 %s" % path)
            paths[path_key] = path
        if not paths:
            self.log.info("No radio_path or mbn_path is provided")
            raise signals.TestSkip("No radio_path or mbn_path is provided")
        self.log.info("paths = %s", paths)
        for ad in self.android_devices:
            if paths.get("radio_image"):
                print_radio_info(ad, "Before flash radio, ")
                flash_radio(ad, paths["radio_image"])
                print_radio_info(ad, "After flash radio, ")
            if not paths.get("mbn_path") or "mbn" not in ad.adb.shell(
                    "ls /vendor"):
                ad.log.info("No need to push mbn files")
                continue
            push_result = True
            try:
                mbn_ver = ad.adb.shell(
                    "cat /vendor/mbn/mcfg/configs/mcfg_sw/mbn.ver")
                if mbn_ver:
                    ad.log.info("Before push mbn, mbn.ver = %s", mbn_ver)
                else:
                    ad.log.info(
                        "There is no mbn.ver before push, unmatching device")
                    continue
            except:
                ad.log.info(
                    "There is no mbn.ver before push, unmatching device")
                continue
            print_radio_info(ad, "Before push mbn, ")
            for i in range(2):
                if not system_file_push(ad, paths["mbn_path"],
                                        "/vendor/mbn/mcfg/configs/"):
                    if i == 1:
                        ad.log.error("Failed to push mbn file")
                        push_result = False
                else:
                    ad.log.info("The mbn file is pushed to device")
                    break
            if not push_result:
                result = False
                continue
            print_radio_info(ad, "After push mbn, ")
            try:
                new_mbn_ver = ad.adb.shell(
                    "cat /vendor/mbn/mcfg/configs/mcfg_sw/mbn.ver")
                if new_mbn_ver:
                    ad.log.info("new mcfg_sw mbn.ver = %s", new_mbn_ver)
                    if new_mbn_ver == mbn_ver:
                        ad.log.error(
                            "mbn.ver is the same before and after push")
                        result = False
                else:
                    ad.log.error("Unable to get new mbn.ver")
                    result = False
            except Exception as e:
                ad.log.error("cat mbn.ver with error %s", e)
                result = False
        return result

    @TelephonyBaseTest.tel_test_wrap
    def test_set_qxdm_log_mask_ims(self):
        """Set the QXDM Log mask to IMS_DS_CNE_LnX_Golden.cfg"""
        tasks = [(set_qxdm_logger_command, [ad, "IMS_DS_CNE_LnX_Golden.cfg"])
                 for ad in self.android_devices]
        return multithread_func(self.log, tasks)

    @TelephonyBaseTest.tel_test_wrap
    def test_set_qxdm_log_mask_qc_default(self):
        """Set the QXDM Log mask to QC_Default.cfg"""
        tasks = [(set_qxdm_logger_command, [ad, " QC_Default.cfg"])
                 for ad in self.android_devices]
        return multithread_func(self.log, tasks)

    @test_tracker_info(uuid="e2734d66-6111-4e76-aa7b-d3b4cbcde4f1")
    @TelephonyBaseTest.tel_test_wrap
    def test_check_carrier_id(self):
        """Verify mobile_data_always_on can be enabled.

        Steps:
        1. Enable mobile_data_always_on by adb.
        2. Verify the mobile data_always_on state.

        Expected Results: mobile_data_always_on return 1.
        """
        result = True
        if self.dut.adb.getprop("ro.build.version.release")[0] in ("8", "O",
                                                                   "7", "N"):
            raise signals.TestSkip("Not supported in this build")
        old_carrier_id = self.dut.droid.telephonyGetSimCarrierId()
        old_carrier_name = self.dut.droid.telephonyGetSimCarrierIdName()
        self.result_detail = "carrier_id = %s, carrier_name = %s" % (
            old_carrier_id, old_carrier_name)
        self.dut.log.info(self.result_detail)
        sub_id = get_outgoing_voice_sub_id(self.dut)
        slot_index = get_slot_index_from_subid(self.log, self.dut, sub_id)

        if self.dut.model in ("angler", "bullhead", "marlin", "sailfish"):
            msg = "Power off SIM slot is not supported"
            self.dut.log.warning("%s, test finished", msg)
            self.result_detail = "%s, %s" % (self.result_detail, msg)
            return result

        if power_off_sim(self.dut, slot_index):
            for i in range(3):
                carrier_id = self.dut.droid.telephonyGetSimCarrierId()
                carrier_name = self.dut.droid.telephonyGetSimCarrierIdName()
                msg = "After SIM power down, carrier_id = %s(expecting -1), " \
                      "carrier_name = %s(expecting None)" % (carrier_id, carrier_name)
                if carrier_id != -1 or carrier_name:
                    if i == 2:
                        self.dut.log.error(msg)
                        self.result_detail = "%s, %s" % (self.result_detail,
                                                         msg)
                        result = False
                    else:
                        time.sleep(5)
                else:
                    self.dut.log.info(msg)
                    break
        else:
            msg = "Power off SIM slot is not working"
            self.dut.log.error(msg)
            result = False
            self.result_detail = "%s, %s" % (self.result_detail, msg)

        if not power_on_sim(self.dut, slot_index):
            self.dut.log.error("Fail to power up SIM")
            result = False
            setattr(self.dut, "reboot_to_recover", True)
        else:
            if is_sim_locked(self.dut):
                self.dut.log.info("Sim is locked")
                carrier_id = self.dut.droid.telephonyGetSimCarrierId()
                carrier_name = self.dut.droid.telephonyGetSimCarrierIdName()
                msg = "In locked SIM, carrier_id = %s(expecting -1), " \
                      "carrier_name = %s(expecting None)" % (carrier_id, carrier_name)
                if carrier_id != -1 or carrier_name:
                    self.dut.log.error(msg)
                    self.result_detail = "%s, %s" % (self.result_detail, msg)
                    result = False
                else:
                    self.dut.log.info(msg)
                unlock_sim(self.dut)
            elif getattr(self.dut, "is_sim_locked", False):
                self.dut.log.error(
                    "After SIM slot power cycle, SIM in not in locked state")
                return False

            if not ensure_phone_subscription(self.log, self.dut):
                self.dut.log.error("Unable to find a valid subscription!")
                result = False
            time.sleep(15)
            new_carrier_id = self.dut.droid.telephonyGetSimCarrierId()
            new_carrier_name = self.dut.droid.telephonyGetSimCarrierIdName()
            msg = "After SIM power up, new_carrier_id = %s, " \
                  "new_carrier_name = %s" % (new_carrier_id, new_carrier_name)
            if old_carrier_id != new_carrier_id or (old_carrier_name !=
                                                    new_carrier_name):
                self.dut.log.error(msg)
                self.result_detail = "%s, %s" % (self.result_detail, msg)
                result = False
            else:
                self.dut.log.info(msg)
        return result
