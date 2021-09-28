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

from acts.test_utils.instrumentation.power import instrumentation_power_test
from acts.test_utils.instrumentation.device.apps.dismiss_dialogs import \
    DialogDismissalUtil


class ImageCaptureTest(instrumentation_power_test.InstrumentationPowerTest):
    """
    Test class for running instrumentation test CameraTests#testImageCapture.
    """

    def _prepare_device(self):
        super()._prepare_device()
        self.mode_airplane()
        self.base_device_configuration()
        self._dialog_util = DialogDismissalUtil(
            self.ad_dut,
            self.get_file_from_config('dismiss_dialogs_apk')
        )
        self._dialog_util.dismiss_dialogs('GoogleCamera')

    def _cleanup_device(self):
        self._dialog_util.close()
        super()._cleanup_device()

    def test_capture_photos(self):
        """Measures power during photo capture."""
        self.run_and_measure(
            'com.google.android.platform.powertests.CameraTests',
            'testImageCapture', req_params=['hdr_mode'])
        self.validate_power_results()
