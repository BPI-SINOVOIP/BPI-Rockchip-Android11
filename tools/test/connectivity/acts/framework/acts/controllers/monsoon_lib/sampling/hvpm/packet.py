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


class SampleType:
    """An enum-like class that defines the SampleTypes for LVPM data.

    Note that these values differ from the LVPM values.
    """

    # A measurement sample.
    MEASUREMENT = 0x00

    # A zero calibration sample.
    ZERO_CAL = 0x10

    # A reference calibration sample.
    REF_CAL = 0x30

    @staticmethod
    def is_calibration(value):
        """Returns true iff the SampleType is a type of calibration."""
        return bool(value & 0x10)


class HvpmMeasurement(object):
    """An object that represents a single measurement from the HVPM device.

    Attributes:
        _sample_time: The time the sample was taken.
        values: From the Monsoon API doc, the values are as follows:

    Val │  Byte  │  Type  | Monsoon │ Reading │
    Pos │ Offset │ Format │ Channel │  Type   │ Description
    ────┼────────┼────────┼─────────┼─────────┼──────────────────────────────
     0  │    0   │ uint16 │  Main   │ Coarse  │ Calibration/Measurement value
     1  │    2   │ uint16 │  Main   │ Fine    │ Calibration/Measurement value
     2  │    4   │ uint16 │  USB    │ Coarse  │ Calibration/Measurement value
     3  │    6   │ uint16 │  USB    │ Fine    │ Calibration/Measurement value
     4  │    8   │ uint16 │  Aux    │ Coarse  │ Calibration/Measurement value
     5  │   10   │ uint16 │  Aux    │ Fine    │ Calibration/Measurement value
     6  │   12   │ uint16 │  Main   │ Voltage │ Main V measurement, or Aux V
        │        │        │         │         │    if setVoltageChannel == 1
     7  │   14   │ uint16 │  USB    │ Voltage │ USB Voltage
    ╔══════════════════════════════════════════════════════════════════════╗
    ║ Note: The Monsoon API Doc puts the below values in the wrong order.  ║
    ║       The values in this docstring are in the correct order.         ║
    ╚══════════════════════════════════════════════════════════════════════╝
     8  │   16   │ uint8? │  USB    │ Gain    │ Measurement gain control.
        │        │        │         │         │  * Structure Unknown. May be
        │        │        │         │         │    similar to Main Gain.
     9  │   17   │ uint8  │  Main   │ Gain    │ Measurement gain control.
        │        │        │         │         │  * b0-3: Believed to be gain.
        │        │        │         │         │  * b4-5: SampleType.
        │        │        │         │         │  * b6-7: Unknown.

    """

    # The total number of bytes in a measurement. See the table above.
    SIZE = 18

    def __init__(self, raw_data, sample_time):
        self.values = struct.unpack('>8H2B', raw_data)
        self._sample_time = sample_time

    def __getitem__(self, channel_and_reading_granularity):
        """Returns the requested reading for the given channel.

        See HvpmMeasurement.__doc__ for a reference table.

        Args:
            channel_and_reading_granularity: A tuple of (channel,
                reading_or_granularity).
        """
        channel = channel_and_reading_granularity[0]
        reading_or_granularity = channel_and_reading_granularity[1]

        data_index = self.get_index(channel, reading_or_granularity)

        if reading_or_granularity == Reading.GAIN:
            # The format of this value is undocumented by Monsoon Inc.
            # Assume an unsigned 4-bit integer is used.
            return self.values[data_index] & 0x0F
        return self.values[data_index]

    @staticmethod
    def get_index(channel, reading_or_granularity):
        """Returns the values array index that corresponds with the given query.

        See HvpmMeasurement.__doc__ for details on how this is determined.

        Args:
            channel: The channel to read data from.
            reading_or_granularity: The reading or granularity desired.

        Returns:
            An index corresponding to the data's location in self.values
        """
        if reading_or_granularity == Reading.VOLTAGE:
            return 6 + channel
        if reading_or_granularity == Reading.GAIN:
            return 9 - channel
        # reading_or_granularity is a granularity value.
        return channel * 2 + reading_or_granularity

    def get_sample_time(self):
        """Returns the calculated time for the given sample."""
        return self._sample_time

    def get_sample_type(self):
        """Returns a value contained in SampleType."""
        return self.values[9] & 0x30


class Packet(object):
    """A packet collected directly from serial.read() during sample collection.

    Large amounts of documentation here are pulled directly from
    http://msoon.github.io/powermonitor/Python_Implementation/docs/API.pdf

    For convenience, here is the table of values stored:

    Offset │ Format │ Field            │ Description
    ───────┼────────┼──────────────────┼────────────────────────────────────────
       0   │ uint16 │ dropped_count    │ Number of dropped packets
       2   │  bits  │ flags            │ Flag values. see self.flags property
       3   │ uint8  │ num_measurements │ Number of measurements in this packet
       4   │ byte[] │ measurement[0]   │ Measurement. See HvpmMeasurement class
      22   │ byte[] │ measurement[1]   │ Optional Measurement. See above
      44   │ byte[] │ measurement[2]   │ Optional Measurement. See above

    Note that all of values except dropped_count are stored in big-endian
    format.

    Attributes:
        _packet_data: The raw data received from the packet.
        time_since_start: The timestamp (relative to start) this packet was
            collected.
        time_since_last_sample: The differential between this packet's
            time_since_start and the previous packet's. Note that for the first
            packet, this value will be equal to time_since_start.
    """

    FIRST_MEASUREMENT_OFFSET = 8

    # The maximum size of a packet read from USB.
    # Note: each HVPM Packet can hold a maximum of 3 measurements.
    MAX_PACKET_SIZE = FIRST_MEASUREMENT_OFFSET + HvpmMeasurement.SIZE * 3

    def __init__(self, sampled_bytes):
        self._packet_data = sampled_bytes

        num_data_bytes = (len(sampled_bytes) - Packet.FIRST_MEASUREMENT_OFFSET)
        self.num_measurements = num_data_bytes // HvpmMeasurement.SIZE

        struct_string = (
            '<2dhBx' +
            (str(HvpmMeasurement.SIZE) + 's') * self.num_measurements)

        # yapf: disable. Yapf forces these to try to fit one after the other.
        (self.time_since_start,
         self.time_since_last_sample,
         self.dropped_count,
         self.flags,
         *samples) = struct.unpack(struct_string, sampled_bytes)
        # yapf: enable

        self.measurements = [None] * self.num_measurements

        for i, raw_data in enumerate(samples):
            self.measurements[i] = HvpmMeasurement(raw_data,
                                                   self._get_sample_time(i))

    def _get_sample_time(self, index):
        """Returns the time the sample at the given index was received.

        If multiple samples were captured within the same reading, the samples
        are assumed to be uniformly distributed during the time it took to
        sample the values.
        """
        time_per_sample = self.time_since_last_sample / self.num_measurements
        return time_per_sample * (index + 1) + self.time_since_start

    @property
    def packet_counter(self):
        """The 4-bit packet index."""
        return self.flags & 0x0F

    def get_bytes(self):
        return list(self._packet_data)

    def __getitem__(self, index):
        return self.measurements[index]

    def __len__(self):
        return self.num_measurements
