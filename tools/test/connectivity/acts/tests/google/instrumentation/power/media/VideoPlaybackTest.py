#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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

from acts.test_utils.instrumentation.power import instrumentation_power_test

BIG_FILE_PUSH_TIMEOUT = 600


class VideoPlaybackTest(
    instrumentation_power_test.InstrumentationPowerTest):
    """Test class for running instrumentation tests
    VideoPlaybackHighBitRateTest."""

    def _prepare_device(self):
        super()._prepare_device()
        self.base_device_configuration()

    def test_playback_high_bit_rate(self):
        """Measures power when the device is in a rock bottom state."""
        video_location = self.push_to_external_storage(
            self.get_file_from_config('high_bit_rate_video'),
            timeout=BIG_FILE_PUSH_TIMEOUT)
        self.trigger_scan_on_external_storage()

        self.run_and_measure(
            'com.google.android.platform.powertests.PhotosTests',
            'testVideoPlaybackThroughIntent',
            extra_params=[('video_file_path', video_location)])

        self.validate_power_results()
