#!/usr/bin/env python3.4
#
#   Copyright 2020 - The Android Open Source Project
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
import acts.test_utils.wifi.rpm_controller_utils as rutils
import acts.test_utils.wifi.wifi_test_utils as wutils
from acts import asserts
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

SSID = "DO_NOT_CONNECT"
TIMEOUT = 60
WAIT_TIME = 10

class RttPreFlightTest(WifiBaseTest):
    """Turns on/off 802.11mc AP before and after RTT tests."""

    def setup_class(self):
        super().setup_class()
        self.dut = self.android_devices[0]
        required_params = ["rpm_ip", "rpm_port"]
        self.unpack_userparams(req_param_names=required_params)
        self.rpm_telnet = rutils.create_telnet_session(self.rpm_ip)

    ### Tests ###

    def test_turn_on_80211mc_ap(self):
        self.rpm_telnet.turn_on(self.rpm_port)
        curr_time = time.time()
        while time.time() < curr_time + TIMEOUT:
            time.sleep(WAIT_TIME)
            if wutils.start_wifi_connection_scan_and_check_for_network(
                self.dut, SSID):
                return True
        self.log.error("Failed to turn on AP")
        return False
