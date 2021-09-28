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


class UncalibratedSampleChunk(object):
    """An uncalibrated sample collection stored with its calibration data.

    These objects are created by the SampleChunker Transformer and read by
    the CalibrationApplier Transformer.

    Attributes:
        samples: the uncalibrated samples list
        calibration_data: the data used to calibrate the samples.
    """

    def __init__(self, samples, calibration_data):
        self.samples = samples
        self.calibration_data = calibration_data
