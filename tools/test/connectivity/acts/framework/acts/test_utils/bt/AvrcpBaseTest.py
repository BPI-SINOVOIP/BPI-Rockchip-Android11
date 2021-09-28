#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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
"""Perform base Avrcp command from headset to dut"""
import time
import os
import queue

from acts import asserts
from acts.test_utils.abstract_devices.bluetooth_handsfree_abstract_device import BluetoothHandsfreeAbstractDeviceFactory as Factory
from acts.test_utils.bt.simulated_carkit_device import SimulatedCarkitDevice
from acts.test_utils.bt.bt_test_utils import connect_phone_to_headset
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.car.car_media_utils import EVENT_PLAY_RECEIVED
from acts.test_utils.car.car_media_utils import EVENT_PAUSE_RECEIVED
from acts.test_utils.car.car_media_utils import EVENT_SKIP_NEXT_RECEIVED
from acts.test_utils.car.car_media_utils import EVENT_SKIP_PREV_RECEIVED
from acts.test_utils.car.car_media_utils import CMD_MEDIA_PLAY
from acts.test_utils.car.car_media_utils import CMD_MEDIA_PAUSE

ADB_FILE_EXISTS = 'test -e %s && echo True'
DEFAULT_TIMEOUT = 5
EVENT_TIMEOUT = 1


class AvrcpBaseTest(BluetoothBaseTest):
    def __init__(self, configs):
        super(AvrcpBaseTest, self).__init__(configs)
        self.dut = self.android_devices[0]
        serial = self.user_params['simulated_carkit_device']
        controller = SimulatedCarkitDevice(serial)
        self.controller = Factory().generate(controller)

        self.phone_music_files = []
        self.host_music_files = []
        for music_file in self.user_params['music_file_names']:
            self.phone_music_files.append(os.path.join(
                self.user_params['phone_music_file_dir'], music_file))
            self.host_music_files.append(os.path.join(
                self.user_params['host_music_file_dir'], music_file))

        self.ensure_phone_has_music_file()

    def setup_class(self):
        super().setup_class()
        self.controller.power_on()
        time.sleep(DEFAULT_TIMEOUT)

    def teardown_class(self):
        super().teardown_class()
        self.dut.droid.mediaPlayStop()
        self.controller.destroy()

    def setup_test(self):
        self.dut.droid.bluetoothMediaPhoneSL4AMBSStart()
        time.sleep(DEFAULT_TIMEOUT)

        self.dut.droid.bluetoothStartPairingHelper(True)
        if not connect_phone_to_headset(self.dut, self.controller, 600):
            asserts.fail('Not able to connect to hands-free device')

        #make sure SL4AMBS is active MediaSession
        self.dut.droid.bluetoothMediaHandleMediaCommandOnPhone(CMD_MEDIA_PLAY)
        time.sleep(0.5)
        self.dut.droid.bluetoothMediaHandleMediaCommandOnPhone(CMD_MEDIA_PAUSE)

    def teardown_test(self):
        self.dut.droid.bluetoothMediaPhoneSL4AMBSStop()

    def ensure_phone_has_music_file(self):
        """Make sure music file (based on config values) is on the phone."""
        for host_file, phone_file in zip(self.host_music_files,
                                         self.phone_music_files):
            if self.dut.adb.shell(ADB_FILE_EXISTS % phone_file):
                self.log.info(
                    'Music file {} already on phone. Skipping file transfer.'
                    .format(host_file))
            else:
                self.dut.adb.push(host_file, phone_file)
                has_file = self.dut.adb.shell(
                        ADB_FILE_EXISTS % phone_file)
                if not has_file:
                    self.log.error(
                        'Audio file {} not pushed to phone.'.format(host_file))
                self.log.info('Music file successfully pushed to phone.')

    def play_from_controller(self):
        self.dut.ed.clear_all_events()
        self.controller.play()
        try:
            self.dut.ed.pop_event(EVENT_PLAY_RECEIVED, EVENT_TIMEOUT)
        except queue.Empty as e:
            asserts.fail('{} Event Not received'.format(EVENT_PLAY_RECEIVED))
        self.log.info('Event Received : {}'.format(EVENT_PLAY_RECEIVED))

    def pause_from_controller(self):
        self.dut.ed.clear_all_events()
        self.controller.pause()
        try:
            self.dut.ed.pop_event(EVENT_PAUSE_RECEIVED, EVENT_TIMEOUT)
        except queue.Empty as e:
            asserts.fail('{} Event Not received'.format(EVENT_PAUSE_RECEIVED))
        self.log.info('Event Received : {}'.format(EVENT_PAUSE_RECEIVED))

    def skip_next_from_controller(self):
        self.dut.ed.clear_all_events()
        self.controller.next_track()
        try:
            self.dut.ed.pop_event(EVENT_SKIP_NEXT_RECEIVED, EVENT_TIMEOUT)
        except queue.Empty as e:
            asserts.fail('{} Event Not '
                         'received'.format(EVENT_SKIP_NEXT_RECEIVED))
        self.log.info('Event Received : {}'.format(EVENT_SKIP_NEXT_RECEIVED))

    def skip_prev_from_controller(self):
        self.dut.ed.clear_all_events()
        self.controller.previous_track()
        try:
            self.dut.ed.pop_event(EVENT_SKIP_PREV_RECEIVED, EVENT_TIMEOUT)
        except queue.Empty as e:
            asserts.fail('{} Event Not '
                         'received'.format(EVENT_SKIP_PREV_RECEIVED))
        self.log.info('Event Received : {}'.format(EVENT_SKIP_PREV_RECEIVED))
