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

import copy
import time
from acts import utils
from acts.controllers.ap_lib import hostapd_constants as hc
from acts.test_decorators import test_tracker_info
from acts.test_utils.power import PowerWiFiBaseTest as PWBT
from acts.test_utils.wifi import wifi_constants as wc
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi import wifi_power_test_utils as wputils

PHONE_BATTERY_VOLTAGE = 4.2


class PowerWiFiroamingTest(PWBT.PowerWiFiBaseTest):
    def setup_class(self):
        super().setup_class()
        self.unpack_userparams(toggle_interval=20, toggle_times=10)

    def teardown_test(self):
        # Delete the brconfigs attributes as this is duplicated with one of the
        # ap's bridge interface config
        delattr(self, 'brconfigs')
        super().teardown_test()

    # Test cases
    @test_tracker_info(uuid='392622d3-0c5c-4767-afa2-abfb2058b0b8')
    def test_screenoff_roaming(self):
        """Test roaming power consumption with screen off.
        Change the attenuation level to trigger roaming between two APs

        """
        # Setup both APs
        network_main = copy.deepcopy(self.main_network)[hc.BAND_2G]
        network_aux = copy.deepcopy(self.aux_network)[hc.BAND_2G]
        self.log.info('Set attenuation to connect device to the aux AP')
        self.set_attenuation(self.atten_level[wc.AP_AUX])
        self.brconfigs_aux = self.setup_ap_connection(network_aux,
                                                      ap=self.access_point_aux)
        self.log.info('Set attenuation to connect device to the main AP')
        self.set_attenuation(self.atten_level[wc.AP_MAIN])
        self.brconfigs_main = self.setup_ap_connection(
            network_main, ap=self.access_point_main)
        self.dut.droid.goToSleepNow()
        time.sleep(5)
        # Set attenuator to trigger roaming
        self.dut.log.info('Trigger roaming now')
        self.set_attenuation(self.atten_level[self.current_test_name])
        self.measure_power_and_validate()

    @test_tracker_info(uuid='a0459b7c-74ce-4adb-8e55-c5365bc625eb')
    def test_screenoff_toggle_between_AP(self):

        # Set attenuator to connect phone to both networks
        network_main = copy.deepcopy(self.main_network)[hc.BAND_2G]
        network_aux = copy.deepcopy(self.aux_network)[hc.BAND_2G]
        # Connect to both APs
        network_main = self.main_network[hc.BAND_2G]
        network_aux = self.aux_network[hc.BAND_2G]
        self.log.info('Set attenuation to connect device to the main AP')
        self.set_attenuation(self.atten_level[wc.AP_MAIN])
        self.brconfigs_main = self.setup_ap_connection(
            network_main, ap=self.access_point_main)
        self.log.info('Set attenuation to connect device to the aux AP')
        self.set_attenuation(self.atten_level[wc.AP_AUX])
        self.brconfigs_aux = self.setup_ap_connection(network_aux,
                                                      ap=self.access_point_aux)
        self.mon_info.duration = self.toggle_interval
        self.dut.droid.goToSleepNow()
        time.sleep(5)
        # Toggle between two networks
        begin_time = utils.get_current_epoch_time()
        results = []
        for i in range(self.toggle_times):
            self.dut.log.info('Connecting to %s' % network_main[wc.SSID])
            self.dut.droid.wifiConnect(network_main)
            results.append(self.monsoon_data_collect_save())
            self.dut.log.info('Connecting to %s' % network_aux[wc.SSID])
            self.dut.droid.wifiConnect(network_aux)
            results.append(self.monsoon_data_collect_save())
        wputils.monsoon_data_plot(self.mon_info, results)

        total_current = 0
        total_samples = 0
        for result in results:
            total_current += result.average_current * result.num_samples
            total_samples += result.num_samples
        average_current = total_current / total_samples

        self.power_result.metric_value = [
            result.total_power for result in results
        ]
        # Take Bugreport
        if self.bug_report:
            self.dut.take_bug_report(self.test_name, begin_time)
        # Path fail check
        self.pass_fail_check(average_current)

    @test_tracker_info(uuid='e5ff95c0-b17e-425c-a903-821ba555a9b9')
    def test_screenon_toggle_between_AP(self):

        # Set attenuator to connect phone to both networks
        network_main = copy.deepcopy(self.main_network)[hc.BAND_2G]
        network_aux = copy.deepcopy(self.aux_network)[hc.BAND_2G]
        # Connect to both APs
        network_main = self.main_network[hc.BAND_2G]
        network_aux = self.aux_network[hc.BAND_2G]
        self.log.info('Set attenuation to connect device to the main AP')
        self.set_attenuation(self.atten_level[wc.AP_MAIN])
        self.brconfigs_main = self.setup_ap_connection(
            network_main, ap=self.access_point_main)
        self.log.info('Set attenuation to connect device to the aux AP')
        self.set_attenuation(self.atten_level[wc.AP_AUX])
        self.brconfigs_aux = self.setup_ap_connection(network_aux,
                                                      ap=self.access_point_aux)
        self.mon_info.duration = self.toggle_interval
        time.sleep(5)
        # Toggle between two networks
        begin_time = utils.get_current_epoch_time()
        results = []
        for i in range(self.toggle_times):
            self.dut.log.info('Connecting to %s' % network_main[wc.SSID])
            self.dut.droid.wifiConnect(network_main)
            results.append(self.monsoon_data_collect_save())
            self.dut.log.info('Connecting to %s' % network_aux[wc.SSID])
            self.dut.droid.wifiConnect(network_aux)
            results.append(self.monsoon_data_collect_save())
        wputils.monsoon_data_plot(self.mon_info, results)

        total_current = 0
        total_samples = 0
        for result in results:
            total_current += result.average_current * result.num_samples
            total_samples += result.num_samples
        average_current = total_current / total_samples

        self.power_result.metric_value = [
            result.total_power for result in results
        ]
        # Take Bugreport
        if self.bug_report:
            self.dut.take_bug_report(self.test_name, begin_time)
        # Path fail check
        self.pass_fail_check(average_current)

    @test_tracker_info(uuid='a16ae337-326f-4d09-990f-42232c3c0dc4')
    def test_screenoff_wifi_wedge(self):

        # Set attenuator to connect phone to both networks
        network_main = copy.deepcopy(self.main_network)[hc.BAND_2G]
        network_aux = copy.deepcopy(self.aux_network)[hc.BAND_2G]
        # Connect to both APs
        network_main = self.main_network[hc.BAND_2G]
        network_aux = self.aux_network[hc.BAND_2G]
        self.log.info('Set attenuation to connect device to the main AP')
        self.set_attenuation(self.atten_level[wc.AP_MAIN])
        self.brconfigs_main = self.setup_ap_connection(
            network_main, ap=self.access_point_main)
        self.log.info('Set attenuation to connect device to the aux AP')
        self.set_attenuation(self.atten_level[wc.AP_AUX])
        self.brconfigs_aux = self.setup_ap_connection(network_aux,
                                                      ap=self.access_point_aux)
        self.log.info('Forget network {}'.format(network_aux[wc.SSID]))
        wutils.wifi_forget_network(self.dut, network_aux[wc.SSID])
        self.log.info('Set attenuation to trigger wedge condition')
        self.set_attenuation(self.atten_level[self.current_test_name])
        self.dut.droid.goToSleepNow()
        self.measure_power_and_validate()
