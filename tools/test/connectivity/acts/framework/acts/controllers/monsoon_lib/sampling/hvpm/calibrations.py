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

import itertools
from collections import deque

from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationScalars
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationWindows
from acts.controllers.monsoon_lib.sampling.enums import Channel
from acts.controllers.monsoon_lib.sampling.enums import Granularity
from acts.controllers.monsoon_lib.sampling.enums import Origin
from acts.controllers.monsoon_lib.sampling.hvpm.packet import SampleType


class HvpmCalibrationData(CalibrationWindows):
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

        The packet is formatted the following way:
            [0]: MAIN, COARSE
            [1]: MAIN, FINE
            [2]: USB,  COARSE
            [3]: USB,  FINE
            [4]: AUX,  COARSE
            [5]: AUX,  FINE
            [...]: ?
            [8]: 0x10 == Origin.ZERO
                 0x30 == Origin.REFERENCE
        """
        sample_type = sample.get_sample_type()
        if sample_type == SampleType.ZERO_CAL:
            origin = Origin.ZERO
        elif sample_type == SampleType.REF_CAL:
            origin = Origin.REFERENCE
        else:
            raise ValueError(
                'Packet of type %s is not a calibration packet.' % sample_type)

        for i in range(6):
            # Reads the last bit to get the Granularity value.
            granularity = i & 0x01
            # Divides by 2 to get the Channel value.
            channel = i >> 1
            self.add(channel, origin, granularity,
                     sample[channel, granularity])


class HvpmCalibrationConstants(CalibrationScalars):
    """Tracks the calibration values gathered from the Monsoon status packet."""

    def __init__(self, monsoon_status_packet):
        """Initializes the calibration constants."""
        super().__init__()

        # Invalid combinations:
        #   *,   REFERENCE, *
        #   AUX, ZERO,      *
        all_variable_sets = [
            Channel.values,
            (Origin.SCALE, Origin.ZERO),
            Granularity.values
        ]  # yapf: disable

        for key in itertools.product(*all_variable_sets):
            if key[0] == Channel.AUX and key[1] == Origin.ZERO:
                # Monsoon status packets do not contain AUX, ZERO readings.
                # Monsoon defaults these values to 0:
                self._calibrations[key] = 0
            else:
                self._calibrations[key] = getattr(
                    monsoon_status_packet,
                    build_status_packet_attribute_name(*key))


# TODO(markdr): Potentially find a better home for this function.
def build_status_packet_attribute_name(channel, origin, granularity):
    """Creates the status packet attribute name from the given keys.

    The HVPM Monsoon status packet returns values in the following format:

        <channel><Granularity><Origin>

    Note that the following combinations are invalid:
        <channel><Granularity>Reference
        aux<Granularity>ZeroOffset

    Args:
        channel: the Channel value of the attribute
        origin: the Origin value of the attribute
        granularity: the Granularity value of the attribute

    Returns:
        A string that corresponds to the attribute of the Monsoon status packet.
    """
    if channel == Channel.MAIN:
        channel = 'main'
    elif channel == Channel.USB:
        channel = 'usb'
    elif channel == Channel.AUX:
        channel = 'aux'
    else:
        raise ValueError('Unknown channel "%s".' % channel)

    if granularity == Granularity.COARSE:
        granularity = 'Coarse'
    elif granularity == Granularity.FINE:
        granularity = 'Fine'
    else:
        raise ValueError('Invalid granularity "%s"' % granularity)

    if origin == Origin.SCALE:
        origin = 'Scale'
    elif origin == Origin.ZERO:
        origin = 'ZeroOffset'
    else:
        # Note: Origin.REFERENCE is not valid for monsoon_status_packet
        # attribute names.
        raise ValueError('Invalid origin "%s"' % origin)

    return '%s%s%s' % (channel, granularity, origin)
