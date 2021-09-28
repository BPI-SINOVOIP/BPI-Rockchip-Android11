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

import logging
import struct
import time

import numpy as np

from acts.controllers.monsoon_lib.api.lvpm_stock.monsoon_proxy import MonsoonProxy
from acts.controllers.monsoon_lib.sampling.common import UncalibratedSampleChunk
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferList
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ProcessAssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ThreadAssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationError
from acts.controllers.monsoon_lib.sampling.engine.transformer import ParallelTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SequentialTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SourceTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import Transformer
from acts.controllers.monsoon_lib.sampling.enums import Channel
from acts.controllers.monsoon_lib.sampling.enums import Granularity
from acts.controllers.monsoon_lib.sampling.enums import Origin
from acts.controllers.monsoon_lib.sampling.lvpm_stock.calibrations import LvpmCalibrationData
from acts.controllers.monsoon_lib.sampling.lvpm_stock.calibrations import LvpmCalibrationSnapshot
from acts.controllers.monsoon_lib.sampling.lvpm_stock.packet import Packet
from acts.controllers.monsoon_lib.sampling.lvpm_stock.packet import SampleType


class StockLvpmSampler(Transformer):
    """Gathers samples from the Monsoon and brings them back to the caller."""

    def __init__(self, monsoon_serial, duration):
        super().__init__()
        self.monsoon_serial = monsoon_serial
        self.duration = duration

    def _transform(self, input_stream):
        # yapf: disable. Yapf doesn't handle fluent interfaces well.
        (ProcessAssemblyLineBuilder()
         .source(PacketCollector(self.monsoon_serial, self.duration))
         .into(SampleNormalizer())
         .build(output_stream=self.output_stream)
         .run())
        # yapf: enable


class PacketCollector(SourceTransformer):
    """Collects Monsoon packets into a buffer to be sent to another process."""

    def __init__(self, serial=None, sampling_duration=None):
        super().__init__()
        self._monsoon_serial = serial
        self._monsoon_proxy = None
        self.start_time = 0
        self.sampling_duration = sampling_duration

    def _initialize_monsoon(self):
        """Initializes the MonsoonProxy object."""
        self._monsoon_proxy = MonsoonProxy(serialno=self._monsoon_serial)

    def on_begin(self):
        """Begins data collection."""
        self.start_time = time.time()
        self._initialize_monsoon()
        self._monsoon_proxy.start_data_collection()

    def on_end(self):
        """Stops data collection."""
        self._monsoon_proxy.stop_data_collection()
        self._monsoon_proxy.ser.close()

    def _transform_buffer(self, buffer):
        """Fills the given buffer with raw monsoon data at each entry."""
        if (self.sampling_duration
                and self.sampling_duration < time.time() - self.start_time):
            return None

        for index in range(len(buffer)):
            time_before_read = time.time()
            data = self._read_packet()
            if data is None:
                continue
            time_after_read = time.time()
            time_data = struct.pack('dd', time_after_read - self.start_time,
                                    time_after_read - time_before_read)
            buffer[index] = time_data + data

        return buffer

    def _read_packet(self):
        """Reads a single packet from the serial port.

        Packets are sent as Length-Value-Checksum, where the first byte is the
        length, the following bytes are the value and checksum. The checksum is
        the stored in the final byte, and is calculated as the 16 least-
        significant-bits of the sum of all value bytes.

        Returns:
            None if the read failed. Otherwise, the packet data received.
        """
        len_char = self._monsoon_proxy.ser.read(1)
        if not len_char:
            logging.warning('Reading from serial timed out.')
            return None

        data_len = ord(len_char)
        if not data_len:
            logging.warning('Unable to read packet length.')
            return None

        result = self._monsoon_proxy.ser.read(int(data_len))
        result = bytearray(result)
        if len(result) != data_len:
            logging.warning(
                'Length mismatch, expected %d bytes, got %d bytes.', data_len,
                len(result))
            return None
        body = result[:-1]
        checksum = sum(body, data_len) & 0xFF
        if result[-1] != checksum:
            logging.warning(
                'Invalid checksum from serial port! Expected %s, '
                'got %s', hex(checksum), hex(result[-1]))
            return None
        return body


class SampleNormalizer(Transformer):
    """Normalizes the raw packet data into reading values."""

    def _transform(self, input_stream):
        # yapf: disable. Yapf doesn't handle fluent interfaces well.
        (ThreadAssemblyLineBuilder()
         .source(PacketReader(), input_stream=input_stream)
         .into(SampleChunker())
         .into(CalibrationApplier())
         .build(output_stream=self.output_stream)
         .run())
        # yapf: enable

    def _transform_buffer(self, buffer):
        """_transform is overloaded, so this function can be left empty."""
        pass


class PacketReader(ParallelTransformer):
    """Reads the raw packets and converts them into LVPM Packet objects."""

    def _transform_buffer(self, buffer):
        """Converts the raw packets to Packet objects in-place in buffer.

        Args:
            buffer: A list of bytes objects. Will be in-place replaced with
                Packet objects.
        """
        for i, packet in enumerate(buffer):
            time_bytes_size = struct.calcsize('dd')
            # Unpacks the two time.time() values sent by PacketCollector.
            time_since_start, time_of_read = struct.unpack(
                'dd', packet[:time_bytes_size])
            packet = packet[time_bytes_size:]
            # Magic number explanation:
            # LVPM sample packets begin with 4 bytes, have at least one
            # measurement (8 bytes), and have 1 last byte (usually a \x00 byte).
            if len(packet) < 4 + 8 + 1 or packet[0] & 0x20 != 0x20:
                logging.warning(
                    'Tried to collect power sample values, received data of '
                    'type=0x%02x, len=%d instead.', packet[0], len(packet))
                buffer[i] = None
                continue

            buffer[i] = Packet(packet, time_since_start, time_of_read)

        return buffer


class SampleChunker(SequentialTransformer):
    """Chunks input packets into lists of samples with identical calibration.

    This step helps to quickly apply calibration across many samples at once.

    Attributes:
        _stored_raw_samples: The queue of raw samples that have yet to be
            split into a new calibration group.
        calibration_data: The calibration window information.
    """

    def __init__(self):
        super().__init__()
        self._stored_raw_samples = []
        self.calibration_data = LvpmCalibrationData()

    def _on_end_of_stream(self, input_stream):
        self._send_buffers(BufferList([self._cut_new_buffer()]))
        super()._on_end_of_stream(input_stream)

    def _transform_buffer(self, buffer):
        """Takes in data from the buffer and splits it based on calibration.

        This transformer is meant to after the PacketReader.

        Args:
            buffer: A list of Packet objects.

        Returns:
            A BufferList containing 0 or more UncalibratedSampleChunk objects.
        """
        buffer_list = BufferList()
        for packet in buffer:
            # If a read packet was not a sample, the PacketReader returns None.
            # Skip over these dud values.
            if packet is None:
                continue

            for sample in packet:
                sample_type = sample.get_sample_type()

                if sample_type == SampleType.MEASUREMENT:
                    self._stored_raw_samples.append(sample)
                elif SampleType.is_calibration(sample_type):
                    if len(self._stored_raw_samples) > 0:
                        buffer_list.append(self._cut_new_buffer())
                    self.calibration_data.add_calibration_sample(sample)
                else:
                    # There's no information on what this packet means within
                    # Monsoon documentation or code.
                    logging.warning('Received unidentifiable packet with '
                                    'SampleType %s: %s' %
                                    (sample_type, packet.get_bytes(0, None)))
        return buffer_list

    def _cut_new_buffer(self):
        """Cuts a new buffer from the input stream data.

        Returns:
            The newly generated UncalibratedSampleChunk.
        """
        calibration_snapshot = LvpmCalibrationSnapshot(self.calibration_data)
        new_chunk = UncalibratedSampleChunk(self._stored_raw_samples,
                                            calibration_snapshot)
        self._stored_raw_samples = []
        return new_chunk


class LvpmReading(object):
    """The result of fully calibrating a sample. Contains all Monsoon readings.

    Attributes:
        _reading_list: The list of values obtained from the Monsoon.
        _time_of_reading: The time since sampling began that the reading was
            collected at.
    """

    def __init__(self, reading_list, time_of_reading):
        """Creates an LvpmReading.

        Args:
            reading_list:
                [0] Main Current
                [1] USB Current
                [2] Aux Current
            time_of_reading: The time the reading was received.
        """
        self._reading_list = reading_list
        self._time_of_reading = time_of_reading

    @property
    def main_current(self):
        return self._reading_list[0]

    @property
    def usb_current(self):
        return self._reading_list[1]

    @property
    def aux_current(self):
        return self._reading_list[2]

    @property
    def sample_time(self):
        return self._time_of_reading

    def __add__(self, other):
        reading_list = [
            self.main_current + other.main_current,
            self.usb_current + other.usb_current,
            self.aux_current + other.aux_current,
        ]
        sample_time = self.sample_time + other.sample_time

        return LvpmReading(reading_list, sample_time)

    def __truediv__(self, other):
        reading_list = [
            self.main_current / other,
            self.usb_current / other,
            self.aux_current / other,
        ]
        sample_time = self.sample_time / other

        return LvpmReading(reading_list, sample_time)


class CalibrationApplier(ParallelTransformer):
    """Applies the calibration formula to the all given samples.

    Designed to come after a SampleChunker Transformer.
    """

    @staticmethod
    def _is_device_calibrated(data):
        """Checks to see if the Monsoon has completed calibration.

        Args:
            data: the calibration data.

        Returns:
            True if the data is calibrated. False otherwise.
        """
        try:
            # If the data is calibrated for any Origin.REFERENCE value, it is
            # calibrated for all Origin.REFERENCE values. The same is true for
            # Origin.ZERO.
            data.get(Channel.MAIN, Origin.REFERENCE, Granularity.COARSE)
            data.get(Channel.MAIN, Origin.ZERO, Granularity.COARSE)
        except CalibrationError:
            return False
        return True

    def _transform_buffer(self, buffer):
        calibration_data = buffer.calibration_data

        if not self._is_device_calibrated(calibration_data):
            return []

        measurements = np.array([sample.values for sample in buffer.samples])
        readings = np.zeros((len(buffer.samples), 5))

        for channel in Channel.values:
            fine_zero = calibration_data.get(channel, Origin.ZERO,
                                             Granularity.FINE)
            fine_scale = calibration_data.get(channel, Origin.SCALE,
                                              Granularity.FINE)
            coarse_zero = calibration_data.get(channel, Origin.ZERO,
                                               Granularity.COARSE)
            coarse_scale = calibration_data.get(channel, Origin.SCALE,
                                                Granularity.COARSE)

            # A set LSB means a coarse measurement. This bit needs to be
            # cleared before setting calibration. Note that the
            # reverse-engineered algorithm does not rightshift the bits after
            # this operation. This explains the mismatch of calibration
            # constants between the reverse-engineered algorithm and the
            # Monsoon.py algorithm.
            readings[:, channel] = np.where(
                measurements[:, channel] & 1,
                ((measurements[:, channel] & ~1) - coarse_zero) * coarse_scale,
                (measurements[:, channel] - fine_zero) * fine_scale)

        for i in range(len(buffer.samples)):
            buffer.samples[i] = LvpmReading(
                list(readings[i]), buffer.samples[i].get_sample_time())

        return buffer.samples
