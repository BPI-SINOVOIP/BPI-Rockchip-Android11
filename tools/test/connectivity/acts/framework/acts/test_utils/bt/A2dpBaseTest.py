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
"""Stream music through connected device from phone test implementation."""
import os
import shutil
import time

import acts.test_utils.coex.audio_test_utils as atu
import acts.test_utils.bt.bt_test_utils as btutils
from acts import asserts
from acts.test_utils.abstract_devices.bluetooth_handsfree_abstract_device import BluetoothHandsfreeAbstractDeviceFactory as bt_factory
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest

PHONE_MUSIC_FILE_DIRECTORY = '/sdcard/Music'
INIT_ATTEN = 0


class A2dpBaseTest(BluetoothBaseTest):
    """Stream audio file over desired Bluetooth codec configurations.

    Audio file should be a sine wave. Other audio files will not work for the
    test analysis metrics.

    Device under test is Android phone, connected to headset with a controller
    that can generate a BluetoothHandsfreeAbstractDevice from test_utils.
    abstract_devices.bluetooth_handsfree_abstract_device.
    BuetoothHandsfreeAbstractDeviceFactory.
    """
    def setup_class(self):

        super().setup_class()
        self.dut = self.android_devices[0]
        req_params = ['audio_params', 'music_files']
        #'audio_params' is a dict, contains the audio device type, audio streaming
        #settings such as volumn, duration, audio recording parameters such as
        #channel, sampling rate/width, and thdn parameters for audio processing
        self.unpack_userparams(req_params)
        # Find music file and push it to the dut
        music_src = self.music_files[0]
        music_dest = PHONE_MUSIC_FILE_DIRECTORY
        success = self.dut.push_system_file(music_src, music_dest)
        if success:
            self.music_file = os.path.join(PHONE_MUSIC_FILE_DIRECTORY,
                                           os.path.basename(music_src))
        # Initialize media_control class
        self.media = btutils.MediaControlOverSl4a(self.dut, self.music_file)
        # Set attenuator to minimum attenuation
        if hasattr(self, 'attenuators'):
            self.attenuator = self.attenuators[0]
            self.attenuator.set_atten(INIT_ATTEN)
        # Create the BTOE(Bluetooth-Other-End) device object
        bt_devices = self.user_params.get('bt_devices', [])
        if bt_devices:
            attr, idx = bt_devices.split(':')
            self.bt_device_controller = getattr(self, attr)[int(idx)]
            self.bt_device = bt_factory().generate(self.bt_device_controller)
        else:
            self.log.error('No BT devices config is provided!')

    def teardown_class(self):

        super().teardown_class()
        if hasattr(self, 'media'):
            self.media.stop()
        if hasattr(self, 'attenuator'):
            self.attenuator.set_atten(INIT_ATTEN)
        self.dut.droid.bluetoothFactoryReset()
        self.bt_device.reset()
        self.bt_device.power_off()
        btutils.disable_bluetooth(self.dut.droid)

    def setup_test(self):

        super().setup_test()
        # Initialize audio capture devices
        self.audio_device = atu.get_audio_capture_device(
            self.bt_device_controller, self.audio_params)
        # Reset BT to factory defaults
        self.dut.droid.bluetoothFactoryReset()
        self.bt_device.reset()
        self.bt_device.power_on()
        btutils.enable_bluetooth(self.dut.droid, self.dut.ed)
        btutils.connect_phone_to_headset(self.dut, self.bt_device, 60)
        vol = self.dut.droid.getMaxMediaVolume() * self.audio_params['volume']
        self.dut.droid.setMediaVolume(0)
        time.sleep(1)
        self.dut.droid.setMediaVolume(int(vol))

    def teardown_test(self):

        super().teardown_test()
        self.dut.droid.bluetoothFactoryReset()
        self.media.stop()
        # Set Attenuator to the initial attenuation
        if hasattr(self, 'attenuator'):
            self.attenuator.set_atten(INIT_ATTEN)
        self.bt_device.reset()
        self.bt_device.power_off()
        btutils.disable_bluetooth(self.dut.droid)

    def play_and_record_audio(self, duration):
        """Play and record audio for a set duration.

        Args:
            duration: duration in seconds for music playing
        Returns:
            audio_captured: captured audio file path
        """

        self.log.info('Play and record audio for {} second'.format(duration))
        self.media.play()
        self.audio_device.start()
        time.sleep(duration)
        audio_captured = self.audio_device.stop()
        self.media.stop()
        self.log.info('Audio play and record stopped')
        asserts.assert_true(audio_captured, 'Audio not recorded')
        return audio_captured

    def run_thdn_analysis(self, audio_captured, tag):
        """Calculate Total Harmonic Distortion plus Noise for latest recording.

        Store result in self.metrics.

        Args:
            audio_captured: the captured audio file
        Returns:
            thdn: thdn value in a list
        """
        # Calculate Total Harmonic Distortion + Noise
        audio_result = atu.AudioCaptureResult(audio_captured,
                                              self.audio_params)
        thdn = audio_result.THDN(**self.audio_params['thdn_params'])
        file_name = tag + os.path.basename(audio_result.path)
        file_new = os.path.join(os.path.dirname(audio_result.path), file_name)
        shutil.copyfile(audio_result.path, file_new)
        for ch_no, t in enumerate(thdn):
            self.log.info('THD+N for channel %s: %.4f%%' % (ch_no, t * 100))
        return thdn

    def run_anomaly_detection(self, audio_captured):
        """Detect anomalies in latest recording.

        Store result in self.metrics.

        Args:
            audio_captured: the captured audio file
        Returns:
            anom: anom detected in the captured file
        """
        # Detect Anomalies
        audio_result = atu.AudioCaptureResult(audio_captured)
        anom = audio_result.detect_anomalies(
            **self.audio_params['anomaly_params'])
        num_anom = 0
        for ch_no, anomalies in enumerate(anom):
            if anomalies:
                for anomaly in anomalies:
                    num_anom += 1
                    start, end = anomaly
                    self.log.warning(
                        'Anomaly on channel {} at {}:{}. Duration '
                        '{} sec'.format(ch_no, start // 60, start % 60,
                                        end - start))
        else:
            self.log.info('%i anomalies detected.' % num_anom)
        return anom
