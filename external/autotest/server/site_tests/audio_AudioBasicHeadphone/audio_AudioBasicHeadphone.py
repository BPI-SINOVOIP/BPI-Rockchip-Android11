# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a server side headphone audio test using the Chameleon board."""

import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.audio import audio_test_data
from autotest_lib.client.cros.chameleon import audio_test_utils
from autotest_lib.client.cros.chameleon import chameleon_audio_ids
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.server.cros.audio import audio_test


class audio_AudioBasicHeadphone(audio_test.AudioTest):
    """Server side headphone audio test.

    This test talks to a Chameleon board and a Cros device to verify
    headphone audio function of the Cros device.

    """
    version = 1
    DELAY_BEFORE_RECORD_SECONDS = 0.5
    RECORD_SECONDS = 5
    DELAY_AFTER_BINDING = 0.5
    SILENCE_WAIT = 5

    def run_once(self, check_quality=False):
        """Running basic headphone audio tests.

        @param check_quality: flag to check audio quality.
        """
        if not audio_test_utils.has_audio_jack(self.host):
            raise error.TestNAError(
                    'No audio jack for the DUT.'
                    'Please check label of the host and control file.'
                    'Please check the host label and test dependency.')

        golden_file = audio_test_data.FREQUENCY_TEST_FILE

        source = self.widget_factory.create_widget(
                chameleon_audio_ids.CrosIds.HEADPHONE)
        recorder = self.widget_factory.create_widget(
                chameleon_audio_ids.ChameleonIds.LINEIN)
        binder = self.widget_factory.create_binder(source, recorder)

        with chameleon_audio_helper.bind_widgets(binder):
            time.sleep(self.DELAY_AFTER_BINDING)

            audio_test_utils.dump_cros_audio_logs(
                    self.host, self.facade, self.resultsdir, 'after_binding')

            # Selects and checks the node selected by cras is correct.
            audio_test_utils.check_and_set_chrome_active_node_types(
                    self.facade, audio_test_utils.get_headphone_node(self.host),
                    None)

            logging.info('Setting playback data on Cros device')
            source.set_playback_data(golden_file)

            # Starts playing, waits for some time, and then starts recording.
            # This is to avoid artifact caused by codec initialization.
            logging.info('Start playing %s on Cros device', golden_file.path)
            source.start_playback()

            time.sleep(self.DELAY_BEFORE_RECORD_SECONDS)
            logging.info('Start recording from Chameleon.')
            recorder.start_recording()

            time.sleep(self.RECORD_SECONDS)

            if check_quality:
                # Add a silence while recording to detect audio
                # spikes after playback.
                time.sleep(self.SILENCE_WAIT)

            recorder.stop_recording()
            logging.info('Stopped recording from Chameleon.')

            audio_test_utils.dump_cros_audio_logs(
                    self.host, self.facade, self.resultsdir, 'after_recording')

            recorder.read_recorded_binary()
            logging.info('Read recorded binary from Chameleon.')

        recorded_file = os.path.join(self.resultsdir, "recorded.raw")
        logging.info('Saving recorded data to %s', recorded_file)
        recorder.save_file(recorded_file)

        # Removes the beginning of recorded data. This is to avoid artifact
        # caused by Chameleon codec initialization in the beginning of
        # recording.
        recorder.remove_head(0.5)

        recorded_file = os.path.join(self.resultsdir, "recorded_clipped.raw")
        logging.info('Saving clipped data to %s', recorded_file)
        recorder.save_file(recorded_file)

        # Compares data by frequency. Headphone audio signal has gone through
        # analog processing. This suffers from codec artifacts and noise on the
        # path. Comparing data by frequency is more robust than comparing by
        # correlation, which is suitable for fully-digital audio path like USB
        # and HDMI.
        audio_test_utils.check_recorded_frequency(
                golden_file, recorder, check_artifacts=check_quality)
