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
"""Note: These calibration classes are based on the original reverse-engineered
algorithm for handling calibration values. As a result, LvpmCalibrationConstants
does not exist for the LVPM stock sampling algorithm."""

import itertools
from collections import deque

from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationWindows
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationSnapshot
from acts.controllers.monsoon_lib.sampling.enums import Channel
from acts.controllers.monsoon_lib.sampling.enums import Granularity
from acts.controllers.monsoon_lib.sampling.enums import Origin
from acts.controllers.monsoon_lib.sampling.lvpm_stock.packet import SampleType

# The numerator used for FINE granularity calibration.
_FINE_NUMERATOR = .0332

# The numerator used for COARSE granularity calibration
_COARSE_NUMERATOR = 2.88


class LvpmCalibrationData(CalibrationWindows):
    """An object that holds the Dynamic Calibration values for HVPM Sampling."""

    def __init__(self, calibration_window_size=5):
        super().__init__(calibration_window_size)

        all_variable_sets = [
            Channel.values,
            (Origin.REFERENCE, Origin.ZERO),
            Granularity.values
        ]  # yapf: disable

        for key in itertools.product(*all_variable_sets):
            self._calibrations[key] = deque()

    def add_calibration_sample(self, sample):
        """Adds calibration values from a calibration sample.

        LVPM Calibration Data is stored as:
            [0]: Main Current calibration
            [1]: USB Current calibration
            [2]: Aux Current calibration
            [3]: Main Voltage (unknown if this is actually calibration or a
                               measurement!)

        Note that coarse vs fine is determined by the position within the
        packet. Even indexes are fine values, odd indexes are coarse values.
        """
        sample_type = sample.get_sample_type()
        if sample_type == SampleType.ZERO_CAL:
            origin = Origin.ZERO
        elif sample_type == SampleType.REF_CAL:
            origin = Origin.REFERENCE
        else:
            raise ValueError(
                'Packet of type %s is not a calibration packet.' % sample_type)
        granularity = sample.get_calibration_granularity()
        for channel in Channel.values:
            self.add(channel, origin, granularity, sample[channel])


class LvpmCalibrationSnapshot(CalibrationSnapshot):
    """A class that holds a snapshot of LVPM Calibration Data.

    According to the original reverse-engineered algorithm for obtaining
    samples, the LVPM determines scale from the reference and zero calibration
    values. Here, we calculate those when taking a snapshot."""

    def __init__(self, lvpm_calibration_base):
        super().__init__(lvpm_calibration_base)
        pairs = itertools.product(Channel.values, Granularity.values)

        for channel, granularity in pairs:
            if granularity == Granularity.COARSE:
                numerator = _COARSE_NUMERATOR
            else:
                numerator = _FINE_NUMERATOR

            divisor = (
                self._calibrations[(channel, Origin.REFERENCE, granularity)] -
                self._calibrations[(channel, Origin.ZERO, granularity)])
            # Prevent division by zero.
            if divisor == 0:
                divisor = .0001

            self._calibrations[(channel, Origin.SCALE,
                                granularity)] = (numerator / divisor)
