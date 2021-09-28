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

import itertools

from acts.test_utils.abstract_devices.bluetooth_handsfree_abstract_device import BluetoothHandsfreeAbstractDeviceFactory as bf
from acts.test_utils.bt import BtEnum
from acts.test_utils.bt.bt_test_utils import clear_bonded_devices
from acts.test_utils.coex.CoexPerformanceBaseTest import CoexPerformanceBaseTest
from acts.test_utils.coex.coex_test_utils import avrcp_actions
from acts.test_utils.coex.coex_test_utils import music_play_and_check
from acts.test_utils.coex.coex_test_utils import pair_and_connect_headset
from acts.test_utils.coex.coex_test_utils import perform_classic_discovery
from acts.test_utils.coex.coex_test_utils import push_music_to_android_device


class WlanWithA2dpPerformanceTest(CoexPerformanceBaseTest):

    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = ['standalone_params', 'dut', 'music_file']
        self.unpack_userparams(req_params)
        self.tests = self.generate_test_cases([
            'a2dp_streaming_on_bt', 'perform_discovery_with_headset_connected',
            'a2dp_streaming_and_avrcp'])

    def setup_class(self):
        super().setup_class()
        self.music_file_to_play = push_music_to_android_device(
            self.pri_ad, self.music_file)
        attr, idx = self.dut.split(':')
        self.dut_controller = getattr(self, attr)[int(idx)]
        self.bt_device = bf().generate(self.dut_controller)

    def setup_test(self):
        super().setup_test()
        self.bt_device.power_on()
        self.headset_mac_address = self.bt_device.mac_address
        self.bt_device.enter_pairing_mode()
        self.pri_ad.droid.bluetoothStartPairingHelper(True)
        self.pri_ad.droid.bluetoothMakeDiscoverable()
        if not pair_and_connect_headset(
                self.pri_ad, self.headset_mac_address,
                set([BtEnum.BluetoothProfile.A2DP.value])):
            self.log.error('Failed to pair and connect to headset')
            return False

    def teardown_test(self):
        clear_bonded_devices(self.pri_ad)
        super().teardown_test()

    def a2dp_streaming_on_bt(self):
        """Initiate music streaming to headset and start iperf traffic."""
        tasks = [
                 (music_play_and_check,
                  (self.pri_ad, self.headset_mac_address,
                   self.music_file_to_play,
                   self.audio_params["music_play_time"])),
                 (self.run_iperf_and_get_result, ())]
        return self.set_attenuation_and_run_iperf(tasks)

    def perform_discovery_with_headset_connected(self):
        """Starts iperf traffic based on test and perform bluetooth classic
        discovery.
        """
        tasks = [(self.run_iperf_and_get_result, ()),
                 (perform_classic_discovery,
                  (self.pri_ad, self.iperf["duration"], self.json_file,
                   self.dev_list))]
        return self.set_attenuation_and_run_iperf(tasks)

    def a2dp_streaming_and_avrcp(self):
        """Starts iperf traffic based on test and initiate music streaming and
        check for avrcp controls.
        """
        tasks = [(music_play_and_check,
                  (self.pri_ad, self.headset_mac_address,
                   self.music_file_to_play,
                   self.audio_params["music_play_time"])),
                 (self.run_iperf_and_get_result, ()),
                 (avrcp_actions, (self.pri_ad, self.bt_device))]
        return self.set_attenuation_and_run_iperf(tasks)

    def generate_test_cases(self, test_types):
        test_cases = []
        for protocol, stream, test_type in itertools.product(
                self.standalone_params['protocol'],
                self.standalone_params['stream'], test_types):

            test_name = 'test_performance_with_{}_{}_{}'.format(
                test_type, protocol, stream)

            test_function = getattr(self, test_type)
            setattr(self, test_name, test_function)
            test_cases.append(test_name)
        return test_cases
