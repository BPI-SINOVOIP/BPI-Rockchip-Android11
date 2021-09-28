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

import logging
import numpy
import os
import scipy.io.wavfile as sciwav

from acts.test_utils.coex.audio_capture_device import AudioCaptureBase
from acts.test_utils.coex.audio_capture_device import CaptureAudioOverAdb
from acts.test_utils.coex.audio_capture_device import CaptureAudioOverLocal
from acts.test_utils.audio_analysis_lib import audio_analysis
from acts.test_utils.audio_analysis_lib.check_quality import quality_analysis

ANOMALY_DETECTION_BLOCK_SIZE = audio_analysis.ANOMALY_DETECTION_BLOCK_SIZE
ANOMALY_GROUPING_TOLERANCE = audio_analysis.ANOMALY_GROUPING_TOLERANCE
PATTERN_MATCHING_THRESHOLD = audio_analysis.PATTERN_MATCHING_THRESHOLD
ANALYSIS_FILE_TEMPLATE = "audio_analysis_%s.txt"
bits_per_sample = 32


def get_audio_capture_device(ad, audio_params):
    """Gets the device object of the audio capture device connected to server.

    The audio capture device returned is specified by the audio_params
    within user_params. audio_params must specify a "type" field, that
    is either "AndroidDevice" or "Local"

    Args:
        ad: Android Device object.
        audio_params: object containing variables to record audio.

    Returns:
        Object of the audio capture device.

    Raises:
        ValueError if audio_params['type'] is not "AndroidDevice" or
            "Local".
    """

    if audio_params['type'] == 'AndroidDevice':
        return CaptureAudioOverAdb(ad, audio_params)

    elif audio_params['type'] == 'Local':
        return CaptureAudioOverLocal(audio_params)

    else:
        raise ValueError('Unrecognized audio capture device '
                         '%s' % audio_params['type'])


class FileNotFound(Exception):
    """Raises Exception if file is not present"""


class AudioCaptureResult(AudioCaptureBase):
    def __init__(self, path, audio_params=None):
        """Initializes Audio Capture Result class.

        Args:
            path: Path of audio capture result.
        """
        super().__init__()
        self.path = path
        self.audio_params = audio_params
        self.analysis_path = os.path.join(self.log_path,
                                          ANALYSIS_FILE_TEMPLATE)
        if self.audio_params:
            self._trim_wave_file()

    def THDN(self, win_size=None, step_size=None, q=1, freq=None):
        """Calculate THD+N value for most recently recorded file.

        Args:
            win_size: analysis window size (must be less than length of
                signal). Used with step size to analyze signal piece by
                piece. If not specified, entire signal will be analyzed.
            step_size: number of samples to move window per-analysis. If not
                specified, entire signal will be analyzed.
            q: quality factor for the notch filter used to remove fundamental
                frequency from signal to isolate noise.
            freq: the fundamental frequency to remove from the signal. If none,
                the fundamental frequency will be determined using FFT.
        Returns:
            channel_results (list): THD+N value for each channel's signal.
                List index corresponds to channel index.
        """
        if not (win_size and step_size):
            return audio_analysis.get_file_THDN(filename=self.path,
                                                q=q,
                                                freq=freq)
        else:
            return audio_analysis.get_file_max_THDN(filename=self.path,
                                                    step_size=step_size,
                                                    window_size=win_size,
                                                    q=q,
                                                    freq=freq)

    def detect_anomalies(self,
                         freq=None,
                         block_size=ANOMALY_DETECTION_BLOCK_SIZE,
                         threshold=PATTERN_MATCHING_THRESHOLD,
                         tolerance=ANOMALY_GROUPING_TOLERANCE):
        """Detect anomalies in most recently recorded file.

        An anomaly is defined as a sample in a recorded sine wave that differs
        by at least the value defined by the threshold parameter from a golden
        generated sine wave of the same amplitude, sample rate, and frequency.

        Args:
            freq (int|float): fundamental frequency of the signal. All other
                frequencies are noise. If None, will be calculated with FFT.
            block_size (int): the number of samples to analyze at a time in the
                anomaly detection algorithm.
            threshold (float): the threshold of the correlation index to
                determine if two sample values match.
            tolerance (float): the sample tolerance for anomaly time values
                to be grouped as the same anomaly
        Returns:
            channel_results (list): anomaly durations for each channel's signal.
                List index corresponds to channel index.
        """
        return audio_analysis.get_file_anomaly_durations(filename=self.path,
                                                         freq=freq,
                                                         block_size=block_size,
                                                         threshold=threshold,
                                                         tolerance=tolerance)

    @property
    def analysis_fileno(self):
        """Returns the file number to dump audio analysis results."""
        counter = 0
        while os.path.exists(self.analysis_path % counter):
            counter += 1
        return counter

    def audio_quality_analysis(self):
        """Measures audio quality based on the audio file given as input.

        Returns:
            analysis_path on success.
        """
        analysis_path = self.analysis_path % self.analysis_fileno
        if not os.path.exists(self.path):
            raise FileNotFound("Recorded file not found")
        try:
            quality_analysis(filename=self.path,
                             output_file=analysis_path,
                             bit_width=bits_per_sample,
                             rate=self.audio_params["sample_rate"],
                             channel=self.audio_params["channel"],
                             spectral_only=False)
        except Exception as err:
            logging.exception("Failed to analyze raw audio: %s" % err)
        return analysis_path

    def _trim_wave_file(self):
        """Trim wave files.

        """
        original_record_file_name = 'original_' + os.path.basename(self.path)
        original_record_file_path = os.path.join(os.path.dirname(self.path),
                                                 original_record_file_name)
        os.rename(self.path, original_record_file_path)
        fs, data = sciwav.read(original_record_file_path)
        trim_start = self.audio_params['trim_start']
        trim_end = self.audio_params['trim_end']
        trim = numpy.array([[trim_start, trim_end]])
        trim = trim * fs
        new_wave_file_list = []
        for elem in trim:
            # To check start and end doesn't exceed raw data dimension
            start_read = min(elem[0], data.shape[0] - 1)
            end_read = min(elem[1], data.shape[0] - 1)
            new_wave_file_list.extend(data[start_read:end_read])
        new_wave_file = numpy.array(new_wave_file_list)

        sciwav.write(self.path, fs, new_wave_file)
