#!/usr/bin/env python3
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

"""A generic library for audio related test actions"""

import datetime
import time

from acts import utils
from acts.controllers.buds_lib import tako_trace_logger


class AudioUtilsError(Exception):
    """Generic AudioUtils Error."""


class AudioUtils(object):
    """A utility that manages generic audio interactions and actions on one or
    more devices under test.

    To be maintained such that it is compatible with any devices that pair with
    phone.
    """

    def __init__(self):
        self.logger = tako_trace_logger.TakoTraceLogger()

    def play_audio_into_device(self, audio_file_path, audio_player, dut):
        """Open mic on DUT, play audio into DUT, close mic on DUT.

        Args:
            audio_file_path: the path to the audio file to play, relative to the
                           audio_player
            audio_player: the device from which to play the audio file
            dut: the device with the microphone

        Returns:
            bool: result of opening and closing DUT mic
        """

        if not dut.open_mic():
            self.logger.error('DUT open_mic did not return True')
            return False
        audio_player.play(audio_file_path)
        if not dut.close_mic():
            self.logger.error('DUT close_mic did not return True.')
            return False
        return True

    def get_agsa_interpretation_of_audio_file(self, audio_file_path,
                                              target_interpretation,
                                              audio_player, dut,
                                              android_device):
        """Gets AGSA interpretation from playing audio into DUT.

        **IMPORTANT**: AGSA on android device must be connected to DUT and able
        to receive info from DUT mic.

        Args:
          audio_file_path: the path to the audio file to play, relative to the
                           audio_player
          target_interpretation: what agsa interpretation should be
          audio_player: the device from which to play the audio file
          dut: the device with the microphone
          android_device: android device to which dut is connected

        Returns:
          interpretation: agsa interpretation of audio file
          score: similarity score between interpretation and target
                 interpretation
        """

        play_start_time = datetime.datetime.now()
        interpretation, score = '', 0.0
        if self.play_audio_into_device(audio_file_path=audio_file_path,
                                       audio_player=audio_player,
                                       dut=dut):
            time.sleep(1)
            interpretation = android_device.agsa_interpretation(
                cutoff_time=play_start_time,
                target_interpretation=target_interpretation,
                source='bisto')
            score = utils.string_similarity(target_interpretation,
                                            interpretation)

        return interpretation, score
