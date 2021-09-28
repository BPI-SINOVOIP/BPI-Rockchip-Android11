#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

from acts import asserts

from acts.controllers import android_device
from acts.test_utils.bt.bt_test_utils import bluetooth_enabled_check

# TODO: This class to be deprecated for
# ../acts/test_utils/abstract_devices/bluetooth_handsfree_abstract_device.py


class SimulatedCarkitDevice():
    def __init__(self, serial):
        self.ad = android_device.create(serial)[0]
        if not bluetooth_enabled_check(self.ad):
            asserts.fail("No able to turn on bluetooth")
        self.mac_address = self.ad.droid.bluetoothGetLocalAddress()
        self.ad.droid.bluetoothToggleState(False)
        self.ad.droid.bluetoothMediaConnectToCarMBS()

    def destroy(self):
        self.ad.clean_up()

    def accept_call(self):
        return self.ad.droid.telecomAcceptRingingCall(None)

    def end_call(self):
        return self.ad.droid.telecomEndCall()

    def enter_pairing_mode(self):
        self.ad.droid.bluetoothStartPairingHelper(True)
        return self.ad.droid.bluetoothMakeDiscoverable()

    def next_track(self):
        return self.ad.droid.bluetoothMediaPassthrough("skipNext")

    def pause(self):
        return self.ad.droid.bluetoothMediaPassthrough("pause")

    def play(self):
        return self.ad.droid.bluetoothMediaPassthrough("play")

    def power_off(self):
        return self.ad.droid.bluetoothToggleState(False)

    def power_on(self):
        return self.ad.droid.bluetoothToggleState(True)

    def previous_track(self):
        return self.ad.droid.bluetoothMediaPassthrough("skipPrev")

    def reject_call(self):
        return self.ad.droid.telecomCallDisconnect(
            self.ad.droid.telecomCallGetCallIds()[0])

    def volume_down(self):
        target_step = self.ad.droid.getMediaVolume() - 1
        target_step = max(target_step, 0)
        return self.ad.droid.setMediaVolume(target_step)

    def volume_up(self):
        target_step = self.ad.droid.getMediaVolume() + 1
        max_step = self.ad.droid.getMaxMediaVolume()
        target_step = min(target_step, max_step)
        return self.ad.droid.setMediaVolume(target_step)
