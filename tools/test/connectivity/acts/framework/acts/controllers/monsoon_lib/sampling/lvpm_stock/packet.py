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
import struct

from acts.controllers.monsoon_lib.sampling.enums import Reading
from acts.controllers.monsoon_lib.sampling.enums import Granularity


class SampleType:
    """An enum-like class that defines the SampleTypes for LVPM data.

    Note that these values differ from the HVPM values.
    """

    # A measurement sample.
    MEASUREMENT = 0x00

    # A zero calibration sample.
    ZERO_CAL = 0x01

    # A reference calibration sample.
    REF_CAL = 0x02

    @staticmethod
    def is_calibration(value):
        """Returns true iff the SampleType is a type of calibration."""
        return value == SampleType.ZERO_CAL or value == SampleType.REF_CAL


class LvpmMeasurement(object):
    """An object that tracks an individual measurement within the LvpmPacket.

    Attributes:
        _sample_time: The time the sample was taken.
        _sample_type: The type of sample stored.
        values: From reverse engineering, the values are as follows:


    If the measurement is a calibration measurement:

    Val │  Byte  │  Type  │ Monsoon │ Reading │
    Pos │ Offset │ Format │ Channel │  Type   │ Description
    ────┼────────┼────────┼─────────┼─────────┼──────────────────────────────
     0  │   0    │  int16 │  Main   │ Current │ Calibration value.
     1  │   2    │  int16 │  USB    │ Current │ Calibration value.
     2  │   4    │  int16 │  Aux    │ Current │ Calibration value.
     3  │   6    │ uint16 │  Main   │ Voltage │ Calibration value.

    If the measurement is a power reading:

    Val │  Byte  │  Type  │ Monsoon │ Reading │
    Pos │ Offset │ Format │ Channel │  Type   │ Description
    ────┼────────┼────────┼─────────┼─────────┼──────────────────────────────
     0  │   0    │  int16 │  Main   │ Current │ b0: if 1, Coarse, else Fine
        │        │        │         │         │ b1-7: Measurement value.
     1  │   2    │  int16 │  USB    │ Current │ b0: if 1, Coarse, else Fine
        │        │        │         │         │ b1-7: Measurement value.
     2  │   4    │  int16 │  Aux    │ Current │ b0: if 1, Coarse, else Fine
        │        │        │         │         │ b1-7: Measurement value.
     3  │   6    │ uint16 │  Main   │ Voltage │ Measurement value.

    """

    # The total number of bytes in a measurement. See the table above.
    SIZE = 8

    def __init__(self, raw_data, sample_time, sample_type, entry_index):
        """Creates a new LVPM Measurement.

        Args:
            raw_data: The raw data format of the LvpmMeasurement.
            sample_time: The time the sample was recorded.
            sample_type: The type of sample that was recorded.
            entry_index: The index of the measurement within the packet.
        """
        self.values = struct.unpack('>3hH', raw_data)
        self._sample_time = sample_time
        self._sample_type = sample_type

        if SampleType.is_calibration(self._sample_type):
            # Calibration packets have granularity values determined by whether
            # or not the entry was odd or even within the returned packet.
            if entry_index % 2 == 0:
                self._granularity = Granularity.FINE
            else:
                self._granularity = Granularity.COARSE
        else:
            # If it is not a calibration packet, each individual reading (main
            # current, usb current, etc) determines granularity value by
            # checking the LSB of the measurement value.
            self._granularity = None

    def __getitem__(self, channel_or_reading):
        """Returns the requested reading for the given channel.

        Args:
            channel_or_reading: either a Channel or Reading.Voltage.
        """
        if channel_or_reading == Reading.VOLTAGE:
            return self.values[3]
        else:
            # Must be a channel. If it is not, this line will throw an
            # IndexError, which is what we will want for invalid values.
            return self.values[channel_or_reading]

    def get_sample_time(self):
        """Returns the time (since the start time) this sample was collected."""
        return self._sample_time

    def get_sample_type(self):
        """Returns a value contained in SampleType."""
        return self._sample_type

    def get_calibration_granularity(self):
        """Returns the granularity associated with this packet.

        If the packet is not a calibration packet, None is returned.
        """
        return self._granularity


class Packet(object):
    """A packet collected directly from serial.read() during sample collection.

    Note that the true documentation for this has been lost to time. This class
    and documentation uses knowledge that comes from several reverse-engineering
    projects. Most of this knowledge comes from
    http://wiki/Main/MonsoonProtocol.

    The data table looks approximately like this:

    Offset │ Format  │ Field   │ Description
    ───────┼─────────┼─────────┼────────────────────────────────────────────
       0   │  uint8  │  flags  │ Bits:
           │         │    &    │  * b0-3: Sequence number (0-15). Increments
           │         │   seq   │          each packet
           │         │         │  * b4: 1 means over-current or thermal kill
           │         │         │  * b5: Main Output, 1 == unit is at voltage,
           │         │         │                     0 == output disabled.
           │         │         │  * b6-7: reserved.
       1   │  uint8  │ packet  │ The type of the packet:
           │         │  type   │   * 0: A data packet
           │         │         │   * 1: A zero calibration packet
           │         │         │   * 2: A reference calibration packet
       2   │  uint8  │ unknown │ Always seems to be 0x00
       3   │  uint8  │ unknown │ Always seems to be 0x00 or 0xC4.
       4   │ byte[8] │   data  │ See LvpmMeasurement.
      ...  │ byte[8] │   data  │ Additional LvpmMeasurements.
      -1   │  uint8  │ unknown │ Last byte, unknown values. Has been seen to
           │         │         │ usually be \x00, or \x84.

    Attributes:
        _packet_data: The raw data received from the packet.
        time_since_start: The timestamp (relative to start) this packet was
            collected.
        time_since_last_sample: The differential between this packet's
            time_since_start and the previous packet's. Note that for the first
            packet, this value will be equal to time_since_start.
    """

    # The number of bytes before the first packet.
    FIRST_MEASUREMENT_OFFSET = 4

    def __init__(self, sampled_bytes, time_since_start,
                 time_since_last_sample):
        self._packet_data = sampled_bytes
        self.time_since_start = time_since_start
        self.time_since_last_sample = time_since_last_sample

        num_data_bytes = len(sampled_bytes) - Packet.FIRST_MEASUREMENT_OFFSET
        num_packets = num_data_bytes // LvpmMeasurement.SIZE

        sample_struct_format = (str(LvpmMeasurement.SIZE) + 's') * num_packets
        struct_string = '>2B2x%sx' % sample_struct_format

        self._flag_data, self.packet_type, *samples = struct.unpack(
            struct_string, sampled_bytes)

        self.measurements = [None] * len(samples)

        for index, raw_measurement in enumerate(samples):
            self.measurements[index] = LvpmMeasurement(
                raw_measurement, self._get_sample_time(index),
                self.packet_type, index)

    def _get_sample_time(self, index):
        """Returns the time the sample at the given index was received.

        If multiple samples were captured within the same reading, the samples
        are assumed to be uniformly distributed during the time it took to
        sample the values.

        Args:
            index: the index of the individual reading from within the sample.
        """
        time_per_sample = self.time_since_last_sample / len(self.measurements)
        return time_per_sample * (index + 1) + self.time_since_start

    @property
    def packet_counter(self):
        return self._flag_data & 0x0F

    def get_bytes(self, start, end_exclusive):
        """Returns a bytearray spanning from start to the end (exclusive)."""
        return self._packet_data[start:end_exclusive]

    def __getitem__(self, index):
        return self.measurements[index]

    def __len__(self):
        return len(self.measurements)
