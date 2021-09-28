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


class Origin:
    """The origin types of a given measurement or calibration.

    The Monsoon returns calibration packets for three types of origin:

        ZERO: The calibrated zeroing point.
        REFERENCE: The reference point used for the returned samples.
        SCALE: The factor at which to scale the returned samples to get power
               consumption data.
    """
    ZERO = 0
    REFERENCE = 1
    SCALE = 2

    values = [ZERO, REFERENCE, SCALE]


class Granularity:
    """The granularity types.

    Monsoon leverages two different granularities when returning power
    measurements. If the power usage exceeds the threshold of the fine
    measurement region, a coarse measurement will be used instead.

    This also means that there need to be two calibration values: one for coarse
    and one for fine.
    """
    COARSE = 0
    FINE = 1

    values = [COARSE, FINE]


class Reading:
    """The extraneous possible reading types.

    Aside from coarse and fine readings (see Granularity), some Monsoons can
    gather readings on the voltage and gain control.
    """
    VOLTAGE = 0x4
    GAIN = 0x6

    values = [VOLTAGE, GAIN]


class Channel:
    """The possible channel types.

    Monsoons can read power measurements from the following three inputs.
    Calibration and reading values may also be available on these channels.
    """
    MAIN = 0
    USB = 1
    AUX = 2

    values = [MAIN, USB, AUX]
