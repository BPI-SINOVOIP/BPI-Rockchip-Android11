#!/usr/bin/env python3.4
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import time
from acts.test_decorators import test_tracker_info
from acts.test_utils.power import PowerWiFiBaseTest as PWBT
from acts.test_utils.wifi import wifi_power_test_utils as wputils


class PowerWiFidtimTest(PWBT.PowerWiFiBaseTest):
    def dtim_test_func(self, dtim_max=10):
        """A reusable function for DTIM test.
        Covering different DTIM value, with screen ON or OFF and 2g/5g network

        Args:
            dtim: the value for DTIM set on the phone
            screen_status: screen on or off
            network: a dict of information for the network to connect
        """
        attrs = ['screen_status', 'wifi_band', 'dtim']
        indices = [2, 4, 6]
        self.decode_test_configs(attrs, indices)
        # Initialize the dut to rock-bottom state
        rebooted = wputils.change_dtim(
            self.dut,
            gEnableModulatedDTIM=int(self.test_configs.dtim),
            gMaxLIModulatedDTIM=dtim_max)
        if rebooted:
            self.dut_rockbottom()
        self.dut.log.info('DTIM value of the phone is now {}'.format(
            self.test_configs.dtim))
        self.setup_ap_connection(
            self.main_network[self.test_configs.wifi_band])
        if self.test_configs.screen_status == 'OFF':
            self.dut.droid.goToSleepNow()
            self.dut.log.info('Screen is OFF')
        time.sleep(5)
        self.measure_power_and_validate()

    # Test cases
    @test_tracker_info(uuid='b6c4114d-984a-4269-9e77-2bec0e4b6e6f')
    def test_screen_OFF_band_2g_dtim_2(self):
        self.dtim_test_func()

    @test_tracker_info(uuid='384d3b0f-4335-4b00-8363-308ec27a150c')
    def test_screen_ON_band_2g_dtim_1(self):
        self.dtim_test_func()

    @test_tracker_info(uuid='017f57c3-e133-461d-80be-d025d1491d8a')
    def test_screen_OFF_band_5g_dtim_2(self):
        self.dtim_test_func()

    @test_tracker_info(uuid='327af44d-d9e7-49e0-9bda-accad6241dc7')
    def test_screen_ON_band_5g_dtim_1(self):
        self.dtim_test_func()
