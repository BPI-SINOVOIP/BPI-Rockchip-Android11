#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import json
import os
import threading
import time

from acts.base_test import BaseTestClass
from acts.controllers import android_device
from acts.controllers import relay_device_controller
from acts.test_utils.bt.bt_test_utils import disable_bluetooth
from acts.test_utils.bt.bt_test_utils import enable_bluetooth
from acts.test_utils.bt.bt_test_utils import setup_multiple_devices_for_bt_test
from acts.test_utils.bt.bt_test_utils import take_btsnoop_logs
from acts.test_utils.coex.coex_test_utils import A2dpDumpsysParser
from acts.test_utils.coex.coex_test_utils import (
    collect_bluetooth_manager_dumpsys_logs)
from acts.test_utils.coex.coex_test_utils import configure_and_start_ap
from acts.test_utils.coex.coex_test_utils import iperf_result
from acts.test_utils.coex.coex_test_utils import parse_fping_results
from acts.test_utils.coex.coex_test_utils import wifi_connection_check
from acts.test_utils.wifi import wifi_retail_ap as retail_ap
from acts.test_utils.wifi.wifi_performance_test_utils import get_iperf_arg_string
from acts.test_utils.wifi.wifi_power_test_utils import get_phone_ip
from acts.test_utils.wifi.wifi_test_utils import reset_wifi
from acts.test_utils.wifi.wifi_test_utils import wifi_connect
from acts.test_utils.wifi.wifi_test_utils import wifi_test_device_init
from acts.test_utils.wifi.wifi_test_utils import wifi_toggle_state

AVRCP_WAIT_TIME = 3


class CoexBaseTest(BaseTestClass):

    class IperfVariables:

        def __init__(self, current_test_name):
            self.iperf_started = False
            self.bidirectional_client_path = None
            self.bidirectional_server_path = None
            self.iperf_server_path = None
            self.iperf_client_path = None
            self.throughput = []
            self.protocol = current_test_name.split("_")[-2]
            self.stream = current_test_name.split("_")[-1]
            self.is_bidirectional = False
            if self.stream == 'bidirectional':
                self.is_bidirectional = True

    def setup_class(self):
        super().setup_class()
        self.pri_ad = self.android_devices[0]
        if len(self.android_devices) == 2:
            self.sec_ad = self.android_devices[1]
        elif len(self.android_devices) == 3:
            self.third_ad = self.android_devices[2]
        self.ssh_config = None

        self.counter = 0
        self.thread_list = []
        if not setup_multiple_devices_for_bt_test(self.android_devices):
            self.log.error('Failed to setup devices for bluetooth test')
            return False
        req_params = ['network', 'iperf']
        opt_params = [
            'AccessPoint', 'RetailAccessPoints', 'RelayDevice',
            'required_devices'
        ]
        self.unpack_userparams(req_params, opt_params)

        self.iperf_server = self.iperf_servers[0]
        self.iperf_client = self.iperf_clients[0]

        if hasattr(self, 'RelayDevice'):
            self.audio_receiver = self.relay_devices[0]
            self.audio_receiver.power_on()
            self.headset_mac_address = self.audio_receiver.mac_address
        else:
            self.log.warning('Missing Relay config file.')

        if hasattr(self, 'AccessPoint'):
            self.ap = self.access_points[0]
            configure_and_start_ap(self.ap, self.network)
        elif hasattr(self, 'RetailAccessPoints'):
            self.retail_access_points = retail_ap.create(
                self.RetailAccessPoints)
            self.retail_access_point = self.retail_access_points[0]
            band = self.retail_access_point.band_lookup_by_channel(
                self.network['channel'])
            self.retail_access_point.set_channel(band, self.network['channel'])
        else:
            self.log.warning('config file have no access point information')

        wifi_test_device_init(self.pri_ad)
        wifi_connect(self.pri_ad, self.network, num_of_tries=5)

    def setup_test(self):
        self.tag = 0
        self.result = {}
        self.dev_list = {}
        self.iperf_variables = self.IperfVariables(self.current_test_name)
        self.a2dp_dumpsys = A2dpDumpsysParser()
        self.log_path = os.path.join(self.pri_ad.log_path,
                self.current_test_name)
        os.makedirs(self.log_path, exist_ok=True)
        self.json_file = os.path.join(self.log_path, 'test_results.json')
        for a in self.android_devices:
            a.ed.clear_all_events()
        if not wifi_connection_check(self.pri_ad, self.network['SSID']):
            self.log.error('Wifi connection does not exist')
            return False
        if not enable_bluetooth(self.pri_ad.droid, self.pri_ad.ed):
            self.log.error('Failed to enable bluetooth')
            return False
        if hasattr(self, 'required_devices'):
            if ('discovery' in self.current_test_name or
                    'ble' in self.current_test_name):
                self.create_android_relay_object()
        else:
            self.log.warning('required_devices is not given in config file')

    def teardown_test(self):
        self.parsing_results()
        with open(self.json_file, 'a') as results_file:
            json.dump(self.result, results_file, indent=4, sort_keys=True)
        if not disable_bluetooth(self.pri_ad.droid):
            self.log.info('Failed to disable bluetooth')
            return False
        self.destroy_android_and_relay_object()

    def teardown_class(self):
        if hasattr(self, 'AccessPoint'):
            self.ap.close()
        self.reset_wifi_and_store_results()

    def reset_wifi_and_store_results(self):
        """Resets wifi and store test results."""
        reset_wifi(self.pri_ad)
        wifi_toggle_state(self.pri_ad, False)

    def create_android_relay_object(self):
        """Creates android device object and relay device object if required
        devices has android device and relay device."""
        if 'AndroidDevice' in self.required_devices:
            self.inquiry_devices = android_device.create(
                self.required_devices['AndroidDevice'])
            self.dev_list['AndroidDevice'] = self.inquiry_devices
        if 'RelayDevice' in self.required_devices:
            self.relay = relay_device_controller.create(
                self.required_devices['RelayDevice'])
            self.dev_list['RelayDevice'] = self.relay

    def destroy_android_and_relay_object(self):
        """Destroys android device object and relay device object if required
        devices has android device and relay device."""
        if hasattr(self, 'required_devices'):
            if ('discovery' in self.current_test_name or
                    'ble' in self.current_test_name):
                if hasattr(self, 'inquiry_devices'):
                    for device in range(len(self.inquiry_devices)):
                        inquiry_device = self.inquiry_devices[device]
                        if not disable_bluetooth(inquiry_device.droid):
                            self.log.info('Failed to disable bluetooth')
                    android_device.destroy(self.inquiry_devices)
                if hasattr(self, 'relay'):
                    relay_device_controller.destroy(self.relay)

    def parsing_results(self):
        """Result parser for fping results and a2dp packet drops."""
        if 'fping' in self.current_test_name:
            output_path = '{}{}{}'.format(self.pri_ad.log_path, '/Fping/',
                                          'fping_%s.txt' % self.counter)
            self.result['fping_loss%'] = parse_fping_results(
                self.fping_params['fping_drop_tolerance'], output_path)
            self.counter = +1
        if 'a2dp_streaming' in self.current_test_name:
            file_path = collect_bluetooth_manager_dumpsys_logs(
                self.pri_ad, self.current_test_name)
            self.result['a2dp_packet_drop'] = (
                self.a2dp_dumpsys.parse(file_path))
            if self.result['a2dp_packet_drop'] == 0:
                self.result['a2dp_packet_drop'] = None

    def run_iperf_and_get_result(self):
        """Frames iperf command based on test and starts iperf client on
        host machine.

        Returns:
            throughput: Throughput of the run.
        """
        self.iperf_server.start(tag=self.tag)
        if self.iperf_variables.stream == 'ul':
            iperf_args = get_iperf_arg_string(
                duration=self.iperf['duration'],
                reverse_direction=1,
                traffic_type=self.iperf_variables.protocol
            )
        elif self.iperf_variables.stream == 'dl':
            iperf_args = get_iperf_arg_string(
                duration=self.iperf['duration'],
                reverse_direction=0,
                traffic_type=self.iperf_variables.protocol
            )
        ip = get_phone_ip(self.pri_ad)
        self.tag = self.tag + 1
        self.iperf_variables.iperf_client_path = (
                self.iperf_client.start(ip, iperf_args, self.tag))

        self.iperf_server.stop()
        if (self.iperf_variables.protocol == 'udp' and
                self.iperf_variables.stream == 'ul'):
            throughput = iperf_result(
                self.log, self.iperf_variables.protocol,
                self.iperf_variables.iperf_server_path)
        else:
            throughput = iperf_result(self.log,
                                    self.iperf_variables.protocol,
                                    self.iperf_variables.iperf_client_path)

        if not throughput:
            self.log.error('Iperf failed/stopped')
            self.iperf_variables.throughput.append(0)
        else:
            self.iperf_variables.throughput.append(
                str(round(throughput, 2)) + "Mb/s")
            self.log.info("Throughput: {} Mb/s".format(throughput))
        self.result["throughput"] = self.iperf_variables.throughput
        return throughput

    def on_fail(self, test_name, begin_time):
        """A function that is executed upon a test case failure.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """
        self.log.info('Test {} failed, Fetching Btsnoop logs and bugreport'.
                      format(test_name))
        take_btsnoop_logs(self.android_devices, self, test_name)
        self._take_bug_report(test_name, begin_time)

    def run_thread(self, kwargs):
        """Convenience function to start thread.

        Args:
            kwargs: Function object to start in thread.
        """
        for function in kwargs:
            self.thread = threading.Thread(target=function)
            self.thread_list.append(self.thread)
            self.thread.start()

    def teardown_thread(self):
        """Convenience function to join thread."""
        for thread_id in self.thread_list:
            if thread_id.is_alive():
                thread_id.join()

    def get_call_volume(self):
        """Function to get call volume when bluetooth headset connected.

        Returns:
            Call volume.
        """
        return self.pri_ad.adb.shell(
            'settings list system|grep volume_bluetooth_sco_bt_sco_hs')

    def change_volume(self):
        """Changes volume with HFP call.

        Returns: True if successful, otherwise False.
        """
        if 'Volume_up' and 'Volume_down' in (
                self.relay_devices[0].relays.keys()):
            current_volume = self.get_call_volume()
            self.audio_receiver.press_volume_down()
            time.sleep(AVRCP_WAIT_TIME)  # wait till volume_changes
            if current_volume == self.get_call_volume():
                self.log.error('Decrease volume failed')
                return False
            time.sleep(AVRCP_WAIT_TIME)
            current_volume = self.get_call_volume()
            self.audio_receiver.press_volume_up()
            time.sleep(AVRCP_WAIT_TIME)  # wait till volume_changes
            if current_volume == self.get_call_volume():
                self.log.error('Increase volume failed')
                return False
        else:
            self.log.warning(
                'No volume control pins specified in relay config.')
        return True
