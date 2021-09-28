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
"""Automated tests for testing passthrough commands in Avrcp/A2dp profile"""
import time

from acts.asserts import assert_equal
from acts.test_utils.bt.AvrcpBaseTest import AvrcpBaseTest
from acts.test_utils.car.car_media_utils import PlaybackState


DEFAULT_TIMEOUT = 0.5

class BtAvrcpPassthroughTest(AvrcpBaseTest):
    def test_play_pause(self):
        """
        Test the Play/Pause passthrough commands.

        Step:
        1. Invoke Play/Pause from controller.
        2. Wait to receive corresponding received event from target.
        3. Check current playback state on target.

        Expected Result:
        Passthrough command received and music play then pause.

        Returns:
          Pass if True
          Fail if False
        """
        self.play_from_controller()
        time.sleep(DEFAULT_TIMEOUT)
        state = (self.dut.droid
                 .bluetoothMediaGetCurrentPlaybackState())['state']
        assert_equal(state, PlaybackState.PLAY,
                     'Current playback state is not Play, is {}'.format(state))

        self.pause_from_controller()
        time.sleep(DEFAULT_TIMEOUT)
        state = (self.dut.droid
                 .bluetoothMediaGetCurrentPlaybackState())['state']
        assert_equal(state, PlaybackState.PAUSE,
                     'Current playback state is not Pause, is {}'.format(state))

    def test_skip_next(self):
        """
        Test the Skip Next passthrough command.

        Step:
        1. Invoke Skip Next from controller.
        2. Wait to receive corresponding received event from target.

        Expected Result:
        Passthrough command received and music skip to next.

        Returns:
          Pass if True
          Fail if False
        """
        self.skip_next_from_controller()

    def test_skip_prev(self):
        """
        Test the Skip Previous passthrough command.

        Step:
        1. Invoke Skip Previous from controller.
        2. Wait to receive corresponding received event from target.

        Expected Result:
        Passthrough command received and music skip to previous.

        Returns:
          Pass if True
          Fail if False
        """
        self.skip_prev_from_controller()
