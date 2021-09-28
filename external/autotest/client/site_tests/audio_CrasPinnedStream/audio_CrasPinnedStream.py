# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.audio import audio_helper
from autotest_lib.client.cros.audio import audio_spec
from autotest_lib.client.cros.audio import audio_test_data
from autotest_lib.client.cros.audio import cmd_utils
from autotest_lib.client.cros.audio import cras_utils

TEST_DURATION = 1


class audio_CrasPinnedStream(audio_helper.cras_rms_test):
    """Verifies audio capture function on multiple devices."""
    version = 1

    @staticmethod
    def wait_for_active_stream_count(expected_count):
        """Wait until the active stream count is correct"""

        utils.poll_for_condition(
                lambda: cras_utils.get_active_stream_count() == expected_count,
                exception=error.TestError(
                        'Timeout waiting active stream count to become %d' %
                        expected_count))

    @staticmethod
    def contains_all_zero(path):
        """Check the recorded sample contains none zero data"""
        with open(path, 'rb') as f:
            samples = f.read()
            for sample in samples:
                if sample != '\x00':
                    return False
        return True

    def run_once(self):
        """Entry point of this test."""

        # The test requires internal mic as second capture device.
        if not audio_spec.has_internal_microphone(utils.get_board_type()):
            logging.info("No internal mic. Skipping the test.")
            return

        # Generate sine raw file that lasts 5 seconds.
        raw_path = os.path.join(self.bindir, '5SEC.raw')
        data_format = dict(
                file_type='raw', sample_format='S16_LE', channel=2, rate=48000)
        raw_file = audio_test_data.GenerateAudioTestData(
                path=raw_path,
                data_format=data_format,
                duration_secs=5,
                frequencies=[440, 440],
                volume_scale=0.9)

        loopback_recorded_file = os.path.join(self.resultsdir,
                                              'loopback_recorded.raw')
        internal_mic_recorded_file = os.path.join(self.resultsdir,
                                                  'internal_mic_recorded.raw')
        node_type = audio_spec.get_internal_mic_node(utils.get_board(),
                                                     utils.get_platform(),
                                                     utils.get_sku())
        device_id = int(
                cras_utils.get_device_id_from_node_type(node_type, True))

        self.wait_for_active_stream_count(0)
        p = cmd_utils.popen(cras_utils.playback_cmd(raw_file.path))
        try:
            loop_c = cmd_utils.popen(
                    cras_utils.capture_cmd(
                            loopback_recorded_file, duration=TEST_DURATION))
            int_c = cmd_utils.popen(
                    cras_utils.capture_cmd(
                            internal_mic_recorded_file,
                            duration=TEST_DURATION,
                            pin_device=device_id))

            # Make sure the audio is still playing.
            if p.poll() != None:
                raise error.TestError('playback stopped')
            # Make sure the recordings finish.
            time.sleep(2 * TEST_DURATION)
        finally:
            cmd_utils.kill_or_log_returncode(p, int_c, loop_c)
            raw_file.delete()

        rms_value = audio_helper.get_rms(loopback_recorded_file)[0]
        self.write_perf_keyval({'rms_value': rms_value})

        audio_helper.recorded_filesize_check(
                os.path.getsize(internal_mic_recorded_file), TEST_DURATION)
        audio_helper.recorded_filesize_check(
                os.path.getsize(loopback_recorded_file), TEST_DURATION)

        if self.contains_all_zero(internal_mic_recorded_file):
            raise error.TestError('Record all zero from internal mic')
