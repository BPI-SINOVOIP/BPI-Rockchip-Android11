# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a server side pinned stream audio test using the Chameleon board."""

import logging
import os
import time

from autotest_lib.client.cros.audio import audio_test_data
from autotest_lib.client.cros.chameleon import audio_test_utils
from autotest_lib.client.cros.chameleon import chameleon_audio_ids
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.server.cros.audio import audio_test


class audio_AudioPinnedStream(audio_test.AudioTest):
    """Server side pinned stream audio test.

    This test talks to a Chameleon board and a Cros device to verify
    pinned stream audio function of the Cros device.

    """
    version = 1
    DELAY_BEFORE_RECORD_SECONDS = 0.5
    PLAYBACK_RECORD_SECONDS = 3
    RECORDING_RECORD_SECONDS = 6
    DELAY_AFTER_BINDING = 0.5

    def run_once(self, playback):
        """Running basic pinned stream audio tests.

        @param playback: True if testing for playback path
        """
        # [1330, 1330] sine wave
        golden_file = audio_test_data.SIMPLE_FREQUENCY_TEST_1330_FILE
        # [2000, 1000] sine wave
        usb_golden_file = audio_test_data.FREQUENCY_TEST_FILE

        if playback:
            source = self.widget_factory.create_widget(
                    chameleon_audio_ids.CrosIds.HEADPHONE)
            recorder = self.widget_factory.create_widget(
                    chameleon_audio_ids.ChameleonIds.LINEIN)
            usb_source = self.widget_factory.create_widget(
                    chameleon_audio_ids.CrosIds.USBOUT)
            usb_recorder = self.widget_factory.create_widget(
                    chameleon_audio_ids.ChameleonIds.USBIN)
        else:
            source = self.widget_factory.create_widget(
                    chameleon_audio_ids.ChameleonIds.LINEOUT)
            recorder = self.widget_factory.create_widget(
                    chameleon_audio_ids.CrosIds.EXTERNAL_MIC)
            usb_source = self.widget_factory.create_widget(
                    chameleon_audio_ids.ChameleonIds.USBOUT)
            usb_recorder = self.widget_factory.create_widget(
                    chameleon_audio_ids.CrosIds.USBIN)

        binder = self.widget_factory.create_binder(source, recorder)
        usb_binder = self.widget_factory.create_binder(usb_source,
                                                       usb_recorder)

        with chameleon_audio_helper.bind_widgets(usb_binder):
            with chameleon_audio_helper.bind_widgets(binder):
                time.sleep(self.DELAY_AFTER_BINDING)

                if playback:
                    audio_test_utils.check_and_set_chrome_active_node_types(
                            self.facade, 'HEADPHONE', None)
                else:
                    audio_test_utils.check_and_set_chrome_active_node_types(
                            self.facade, None, 'MIC')

                logging.info('Setting playback data')
                source.set_playback_data(golden_file)
                usb_source.set_playback_data(usb_golden_file)

                logging.info('Start playing %s', golden_file.path)
                source.start_playback()

                logging.info('Start playing %s, pinned:%s',
                             usb_golden_file.path, playback)
                usb_source.start_playback(pinned=playback)

                time.sleep(self.DELAY_BEFORE_RECORD_SECONDS)

                logging.info('Start recording.')
                recorder.start_recording()
                if playback:
                    # Not any two recorders on chameleon can record at the same
                    # time. USB and LineIn can but we would keep them separate
                    # here to keep things simple and change it when needed.
                    # Should still record [2000, 1000] sine wave from USB as
                    # it was set pinned on USB.
                    time.sleep(self.PLAYBACK_RECORD_SECONDS)
                    recorder.stop_recording()
                    usb_recorder.start_recording()
                    time.sleep(self.PLAYBACK_RECORD_SECONDS)
                    usb_recorder.stop_recording()
                else:
                    usb_recorder.start_recording(pinned=True)
                    time.sleep(self.RECORDING_RECORD_SECONDS)
                    recorder.stop_recording()
                    usb_recorder.stop_recording(pinned=True)

                audio_test_utils.dump_cros_audio_logs(self.host, self.facade,
                                                      self.resultsdir,
                                                      'after_recording')

        recorder.read_recorded_binary()
        usb_recorder.read_recorded_binary()

        recorded_file = os.path.join(self.resultsdir, "recorded.raw")
        logging.info('Saving recorded data to %s', recorded_file)
        recorder.save_file(recorded_file)

        usb_recorded_file = os.path.join(self.resultsdir, "usb_recorded.raw")
        logging.info('Saving recorded data to %s', usb_recorded_file)
        usb_recorder.save_file(usb_recorded_file)

        if playback:
            audio_test_utils.check_recorded_frequency(golden_file, recorder)
        else:
            # The signal quality from chameleon will get affected when usb is
            # also connected. We'll just lower the quality bar here, as it is
            # not the main point of this test.
            audio_test_utils.check_recorded_frequency(
                    golden_file, recorder, second_peak_ratio=0.3)
        audio_test_utils.check_recorded_frequency(usb_golden_file,
                                                  usb_recorder)
