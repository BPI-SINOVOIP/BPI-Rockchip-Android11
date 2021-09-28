#!/usr/bin/env python3.4
#
#   Copyright 2018 - The Android Open Source Project
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

import acts.signals as signals

from acts import asserts
from acts.base_test import BaseTestClass
from acts.libs.ota import ota_updater
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_2G

import acts.test_utils.net.net_test_utils as nutils
import acts.test_utils.wifi.wifi_test_utils as wutils


class WifiTethering2GPskOTATest(BaseTestClass):
    """Wifi Tethering 2G Psk OTA tests"""

    def setup_class(self):

        super(WifiTethering2GPskOTATest, self).setup_class()
        ota_updater.initialize(self.user_params, self.android_devices)

        self.hotspot_device = self.android_devices[0]
        self.tethered_device = self.android_devices[1]
        req_params = ("wifi_hotspot_psk", )
        self.unpack_userparams(req_params)

        # verify hotspot device has lte data and supports tethering
        nutils.verify_lte_data_and_tethering_supported(self.hotspot_device)

        # Save a wifi soft ap configuration and verify that it works
        wutils.save_wifi_soft_ap_config(self.hotspot_device,
                                        self.wifi_hotspot_psk,
                                        WIFI_CONFIG_APBAND_2G)
        self._verify_wifi_tethering()

        # Run OTA below, if ota fails then abort all tests.
        try:
          ota_updater.update(self.hotspot_device)
        except Exception as err:
            raise signals.TestAbortClass(
                "Failed up apply OTA update. Aborting tests")

    def on_fail(self, test_name, begin_time):
        for ad in self.android_devices:
            ad.take_bug_report(test_name, begin_time)

    def teardown_class(self):
        wutils.wifi_toggle_state(self.hotspot_device, True)

    """Helper Functions"""

    def _verify_wifi_tethering(self):
        """Verify wifi tethering"""
        wutils.start_wifi_tethering_saved_config(self.hotspot_device)
        wutils.wifi_connect(self.tethered_device, self.wifi_hotspot_psk)
        # (TODO: @gmoturu) Change to stop_wifi_tethering. See b/109876061
        wutils.wifi_toggle_state(self.hotspot_device, True)

    """Tests"""

    @test_tracker_info(uuid="4b1cec63-d1d2-4046-84e9-e806bb08ce41")
    def test_wifi_tethering_ota_2g_psk(self):
        """ Verify wifi hotspot after ota upgrade

        Steps:
          1. Save a wifi hotspot config with 2g band and psk auth
          2. Do a OTA update
          3. Verify that wifi hotspot works with teh saved config
        """
        self._verify_wifi_tethering()
