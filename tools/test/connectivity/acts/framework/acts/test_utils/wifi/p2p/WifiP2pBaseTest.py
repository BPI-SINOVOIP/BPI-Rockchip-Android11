#!/usr/bin/env python3
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

import acts.utils
import os
import re
import time

from acts import asserts
from acts import utils
from acts.base_test import BaseTestClass
from acts.keys import Config
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.p2p import wifi_p2p_const as p2pconsts

WAIT_TIME = 60


class WifiP2pBaseTest(BaseTestClass):
    def __init__(self, controllers):
        if not hasattr(self, 'android_devices'):
            super(WifiP2pBaseTest, self).__init__(controllers)

    def setup_class(self):
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
        required_params = ()
        optional_params = (
                "skip_read_factory_mac", "pixel_models", "cnss_diag_file")
        self.unpack_userparams(required_params,
                               optional_params,
                               skip_read_factory_mac=0)

        self.dut1 = self.android_devices[0]
        self.dut2 = self.android_devices[1]
        if self.skip_read_factory_mac:
            self.dut1_mac = None
            self.dut2_mac = None
        else:
            self.dut1_mac = self.get_p2p_mac_address(self.dut1)
            self.dut2_mac = self.get_p2p_mac_address(self.dut2)

        #init location before init p2p
        acts.utils.set_location_service(self.dut1, True)
        acts.utils.set_location_service(self.dut2, True)

        wutils.wifi_test_device_init(self.dut1)
        utils.sync_device_time(self.dut1)
        self.dut1.droid.wifiP2pInitialize()
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        asserts.assert_true(self.dut1.droid.wifiP2pIsEnabled(),
                            "DUT1's p2p should be initialized but it didn't")
        self.dut1.name = "Android_" + self.dut1.serial
        self.dut1.droid.wifiP2pSetDeviceName(self.dut1.name)
        wutils.wifi_test_device_init(self.dut2)
        utils.sync_device_time(self.dut2)
        self.dut2.droid.wifiP2pInitialize()
        time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
        asserts.assert_true(self.dut2.droid.wifiP2pIsEnabled(),
                            "DUT2's p2p should be initialized but it didn't")
        self.dut2.name = "Android_" + self.dut2.serial
        self.dut2.droid.wifiP2pSetDeviceName(self.dut2.name)

        if len(self.android_devices) > 2:
            self.dut3 = self.android_devices[2]
            acts.utils.set_location_service(self.dut3, True)
            wutils.wifi_test_device_init(self.dut3)
            utils.sync_device_time(self.dut3)
            self.dut3.droid.wifiP2pInitialize()
            time.sleep(p2pconsts.DEFAULT_FUNCTION_SWITCH_TIME)
            asserts.assert_true(
                self.dut3.droid.wifiP2pIsEnabled(),
                "DUT3's p2p should be initialized but it didn't")
            self.dut3.name = "Android_" + self.dut3.serial
            self.dut3.droid.wifiP2pSetDeviceName(self.dut3.name)
        if hasattr(self, "cnss_diag_file"):
            if isinstance(self.cnss_diag_file, list):
                self.cnss_diag_file = self.cnss_diag_file[0]
            if not os.path.isfile(self.cnss_diag_file):
                self.cnss_diag_file = os.path.join(
                    self.user_params[Config.key_config_path.value],
                    self.cnss_diag_file)

    def teardown_class(self):
        self.dut1.droid.wifiP2pClose()
        self.dut2.droid.wifiP2pClose()
        acts.utils.set_location_service(self.dut1, False)
        acts.utils.set_location_service(self.dut2, False)

        if len(self.android_devices) > 2:
            self.dut3.droid.wifiP2pClose()
            acts.utils.set_location_service(self.dut3, False)
        for ad in self.android_devices:
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()

    def setup_test(self):
        if hasattr(self, "cnss_diag_file") and hasattr(self, "pixel_models"):
            wutils.start_cnss_diags(
                self.android_devices, self.cnss_diag_file, self.pixel_models)
        self.tcpdump_proc = []
        if hasattr(self, "android_devices"):
            for ad in self.android_devices:
                proc = nutils.start_tcpdump(ad, self.test_name)
                self.tcpdump_proc.append((ad, proc))

        for ad in self.android_devices:
            ad.ed.clear_all_events()

    def teardown_test(self):
        if hasattr(self, "cnss_diag_file") and hasattr(self, "pixel_models"):
            wutils.stop_cnss_diags(self.android_devices, self.pixel_models)
        for proc in self.tcpdump_proc:
            nutils.stop_tcpdump(
                    proc[0], proc[1], self.test_name, pull_dump=False)
        self.tcpdump_proc = []
        for ad in self.android_devices:
            # Clear p2p group info
            ad.droid.wifiP2pRequestPersistentGroupInfo()
            event = ad.ed.pop_event("WifiP2pOnPersistentGroupInfoAvailable",
                                    p2pconsts.DEFAULT_TIMEOUT)
            for network in event['data']:
                ad.droid.wifiP2pDeletePersistentGroup(network['NetworkId'])
            # Clear p2p local service
            ad.droid.wifiP2pClearLocalServices()

    def on_fail(self, test_name, begin_time):
        for ad in self.android_devices:
            ad.take_bug_report(test_name, begin_time)
            ad.cat_adb_log(test_name, begin_time)
            wutils.get_ssrdumps(ad)
        if hasattr(self, "cnss_diag_file") and hasattr(self, "pixel_models"):
            wutils.stop_cnss_diags(self.android_devices, self.pixel_models)
            for ad in self.android_devices:
                wutils.get_cnss_diag_log(ad)
        for proc in self.tcpdump_proc:
            nutils.stop_tcpdump(proc[0], proc[1], self.test_name)
        self.tcpdump_proc = []

    def get_p2p_mac_address(self, dut):
        """Gets the current MAC address being used for Wi-Fi Direct."""
        dut.reboot()
        time.sleep(WAIT_TIME)
        out = dut.adb.shell("ifconfig p2p0")
        return re.match(".* HWaddr (\S+).*", out, re.S).group(1)
