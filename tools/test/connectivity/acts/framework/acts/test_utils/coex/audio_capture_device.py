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

import logging
import os
import pyaudio
import wave

from acts import context


WAVE_FILE_TEMPLATE = 'recorded_audio_%s.wav'
ADB_PATH = 'sdcard/Music/'
ADB_FILE = 'rec.pcm'


class AudioCaptureBase(object):
    """Base class for Audio capture."""

    def __init__(self):

        self.wave_file = os.path.join(self.log_path, WAVE_FILE_TEMPLATE)
        self.file_dir = self.log_path

    @property
    def log_path(self):
        """Returns current log path."""
        current_context = context.get_current_context()
        full_out_dir = os.path.join(current_context.get_full_output_path(),
                                    'AudioCapture')

        os.makedirs(full_out_dir, exist_ok=True)
        return full_out_dir

    @property
    def next_fileno(self):
        counter = 0
        while os.path.exists(self.wave_file % counter):
            counter += 1
        return counter

    @property
    def last_fileno(self):
        return self.next_fileno - 1

    @property
    def get_last_record_duration_millis(self):
        """Get duration of most recently recorded file.

        Returns:
            duration (float): duration of recorded file in milliseconds.
        """
        latest_file_path = self.wave_file % self.last_fileno
        print (latest_file_path)
        with wave.open(latest_file_path, 'r') as f:
            frames = f.getnframes()
            rate = f.getframerate()
            duration = (frames / float(rate)) * 1000
        return duration

    def write_record_file(self, audio_params, frames):
        """Writes the recorded audio into the file.

        Args:
            audio_params: A dict with audio configuration.
            frames: Recorded audio frames.

        Returns:
            file_name: wave file name.
        """
        file_name = self.wave_file % self.next_fileno
        logging.debug('writing to %s' % file_name)
        wf = wave.open(file_name, 'wb')
        wf.setnchannels(audio_params['channel'])
        wf.setsampwidth(audio_params['sample_width'])
        wf.setframerate(audio_params['sample_rate'])
        wf.writeframes(frames)
        wf.close()
        return file_name


class CaptureAudioOverAdb(AudioCaptureBase):
    """Class to capture audio over android device which acts as the
    a2dp sink or hfp client. This captures the digital audio and converts
    to analog audio for post processing.
    """

    def __init__(self, ad, audio_params):
        """Initializes CaptureAudioOverAdb.

        Args:
            ad: An android device object.
            audio_params: Dict containing audio record settings.
        """
        super().__init__()
        self._ad = ad
        self.audio_params = audio_params
        self.adb_path = None

    def start(self):
        """Start the audio capture over adb."""
        self.adb_path = os.path.join(ADB_PATH, ADB_FILE)
        cmd = 'ap2f --usage 1 --start --duration {} --target {}'.format(
            self.audio_params['duration'], self.adb_path,
        )
        self._ad.adb.shell_nb(cmd)

    def stop(self):
        """Stops the audio capture and stores it in wave file.

        Returns:
            File name of the recorded file.
        """
        cmd = '{} {}'.format(self.adb_path, self.file_dir)
        self._ad.adb.pull(cmd)
        self._ad.adb.shell('rm {}'.format(self.adb_path))
        return self._convert_pcm_to_wav()

    def _convert_pcm_to_wav(self):
        """Converts raw pcm data into wave file.

        Returns:
            file_path: Returns the file path of the converted file
            (digital to analog).
        """
        file_to_read = os.path.join(self.file_dir, ADB_FILE)
        with open(file_to_read, 'rb') as pcm_file:
            frames = pcm_file.read()
        file_path = self.write_record_file(self.audio_params, frames)
        return file_path


class CaptureAudioOverLocal(AudioCaptureBase):
    """Class to capture audio on local server using the audio input devices
    such as iMic/AudioBox. This class mandates input deivce to be connected to
    the machine.
    """
    def __init__(self, audio_params):
        """Initializes CaptureAudioOverLocal.

        Args:
            audio_params: Dict containing audio record settings.
        """
        super().__init__()
        self.audio_params = audio_params
        self.channels = self.audio_params['channel']
        self.chunk = self.audio_params['chunk']
        self.sample_rate = self.audio_params['sample_rate']
        self.__input_device = None
        self.audio = None
        self.frames = []

    @property
    def name(self):
        return self.__input_device["name"]

    def __get_input_device(self):
        """Checks for the audio capture device."""
        if self.__input_device is None:
            for i in range(self.audio.get_device_count()):
                device_info = self.audio.get_device_info_by_index(i)
                logging.debug('Device Information: {}'.format(device_info))
                if self.audio_params['input_device'] in device_info['name']:
                    self.__input_device = device_info
                    break
            else:
                raise DeviceNotFound(
                    'Audio Capture device {} not found.'.format(
                        self.audio_params['input_device']))
        return self.__input_device

    def start(self, trim_beginning=0, trim_end=0):
        """Starts audio recording on host machine.

        Args:
            trim_beginning: how many seconds to trim from the beginning
            trim_end: how many seconds to trim from the end
        """
        self.audio = pyaudio.PyAudio()
        self.__input_device = self.__get_input_device()
        stream = self.audio.open(
            format=pyaudio.paInt16,
            channels=self.channels,
            rate=self.sample_rate,
            input=True,
            frames_per_buffer=self.chunk,
            input_device_index=self.__input_device['index'])
        b_chunks = trim_beginning * (self.sample_rate // self.chunk)
        e_chunks = trim_end * (self.sample_rate // self.chunk)
        total_chunks = self.sample_rate // self.chunk * self.audio_params[
            'duration']
        for i in range(total_chunks):
            try:
                data = stream.read(self.chunk, exception_on_overflow=False)
            except IOError as ex:
                logging.error('Cannot record audio: {}'.format(ex))
                return False
            if b_chunks <= i < total_chunks - e_chunks:
                self.frames.append(data)

        stream.stop_stream()
        stream.close()

    def stop(self):
        """Terminates the pulse audio instance.

        Returns:
            File name of the recorded audio file.
        """
        self.audio.terminate()
        frames = b''.join(self.frames)
        return self.write_record_file(self.audio_params, frames)


class DeviceNotFound(Exception):
    """Raises exception if audio capture device is not found."""
