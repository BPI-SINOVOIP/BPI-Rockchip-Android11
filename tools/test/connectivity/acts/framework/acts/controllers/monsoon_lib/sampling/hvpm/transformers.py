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

import array
import logging
import struct
import time

import numpy as np
from Monsoon import HVPM

from acts.controllers.monsoon_lib.sampling.common import UncalibratedSampleChunk
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferList
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ProcessAssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ThreadAssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationError
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationSnapshot
from acts.controllers.monsoon_lib.sampling.engine.transformer import ParallelTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SequentialTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SourceTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import Transformer
from acts.controllers.monsoon_lib.sampling.enums import Channel
from acts.controllers.monsoon_lib.sampling.enums import Granularity
from acts.controllers.monsoon_lib.sampling.enums import Origin
from acts.controllers.monsoon_lib.sampling.enums import Reading
from acts.controllers.monsoon_lib.sampling.hvpm.calibrations import HvpmCalibrationConstants
from acts.controllers.monsoon_lib.sampling.hvpm.calibrations import HvpmCalibrationData
from acts.controllers.monsoon_lib.sampling.hvpm.packet import HvpmMeasurement
from acts.controllers.monsoon_lib.sampling.hvpm.packet import Packet
from acts.controllers.monsoon_lib.sampling.hvpm.packet import SampleType


class HvpmTransformer(Transformer):
    """Gathers samples from the Monsoon and brings them back to the caller."""

    def __init__(self, monsoon_serial, duration):
        super().__init__()
        self.monsoon_serial = monsoon_serial
        self.duration = duration

    def _transform(self, input_stream):
        # We need to gather the status packet before sampling so we can use the
        # static calibration during sample normalization.
        monsoon = HVPM.Monsoon()
        monsoon.setup_usb(self.monsoon_serial)
        monsoon.fillStatusPacket()
        monsoon_status_packet = monsoon.statusPacket()
        monsoon.closeDevice()

        # yapf: disable. Yapf doesn't handle fluent interfaces well.
        (ProcessAssemblyLineBuilder()
         .source(PacketCollector(self.monsoon_serial, self.duration))
         .into(SampleNormalizer(monsoon_status_packet=monsoon_status_packet))
         .build(output_stream=self.output_stream).run())
        # yapf: enable


class PacketCollector(SourceTransformer):
    """Collects Monsoon packets into a buffer to be sent to another transformer.

    Ideally, the other transformer will be in a separate process to prevent the
    GIL from slowing down packet collection.

    Attributes:
        _monsoon_id: The id of the monsoon.
        _monsoon: The monsoon instance. This is left unset until
                  _initialize_monsoon() is called.
    """

    def __init__(self, monsoon_id, sampling_duration=None):
        super().__init__()
        self._monsoon_id = monsoon_id
        self._monsoon = None
        self.start_time = None
        self.array = array.array('B', b'\x00' * Packet.MAX_PACKET_SIZE)
        self.sampling_duration = sampling_duration

    def _initialize_monsoon(self):
        """Initializes the monsoon object.

        Note that this must be done after the Transformer has started.
        Otherwise, this transformer will have c-like objects, preventing
        the transformer from being used with the multiprocess libraries.
        """
        self._monsoon = HVPM.Monsoon()
        self._monsoon.setup_usb(self._monsoon_id)
        self._monsoon.stopSampling()
        self._monsoon.fillStatusPacket()
        self._monsoon.StartSampling()

    def on_begin(self):
        if __debug__:
            logging.warning(
                'Debug mode is enabled. Expect a higher frequency of dropped '
                'packets. To reduce packet drop, disable your python debugger.'
            )

        self.start_time = time.time()
        self._initialize_monsoon()

    def __del__(self):
        if self._monsoon:
            self.on_end()

    def on_end(self):
        self._monsoon.stopSampling()
        self._monsoon.closeDevice()

    def _transform_buffer(self, buffer):
        """Fills the buffer with packets until time has been reached.

        Returns:
            A BufferList of a single buffer if collection is not yet finished.
            None if sampling is complete.
        """
        index = 0
        if (self.sampling_duration
                and self.sampling_duration < time.time() - self.start_time):
            return None

        for index in range(len(buffer)):
            time_before_read = time.time()
            try:
                data = self._monsoon.Protocol.DEVICE.read(
                    # Magic value for USB bulk reads.
                    0x81,
                    Packet.MAX_PACKET_SIZE,
                    # In milliseconds.
                    timeout=1000)
            except Exception as e:
                logging.warning(e)
                continue
            time_after_read = time.time()
            time_data = struct.pack('dd', time_after_read - self.start_time,
                                    time_after_read - time_before_read)
            buffer[index] = time_data + data.tobytes()

        return buffer


class SampleNormalizer(Transformer):
    """A Transformer that applies calibration to the input's packets."""

    def __init__(self, monsoon_status_packet):
        """Creates a SampleNormalizer.

        Args:
            monsoon_status_packet: The status of the monsoon. Used for gathering
                the constant calibration data from the device.
        """
        super().__init__()
        self.monsoon_status_packet = monsoon_status_packet

    def _transform(self, input_stream):
        # yapf: disable. Yapf doesn't handle fluent interfaces well.
        (ThreadAssemblyLineBuilder()
         .source(PacketReader(), input_stream=input_stream)
         .into(SampleChunker())
         .into(CalibrationApplier(self.monsoon_status_packet))
         .build(output_stream=self.output_stream).run())
        # yapf: enable


class PacketReader(ParallelTransformer):
    """Reads raw HVPM Monsoon data and converts it into Packet objects.

    Attributes:
        rollover_count: The number of times the dropped_count value has rolled
            over it's maximum value (2^16-1).
        previous_dropped_count: The dropped count read from the last packet.
            Used for determining the true number of dropped samples.
    """
    """The number of seconds before considering dropped_count to be meaningful.

    Monsoon devices will often report 2^16-1 as the dropped count when first
    starting the monsoon. This usually goes away within a few milliseconds.
    """
    DROP_COUNT_TIMER_THRESHOLD = 1

    def __init__(self):
        super().__init__()
        self.rollover_count = 0
        self.previous_dropped_count = 0

    def _transform_buffer(self, buffer):
        """Reads raw sample data and converts it into packet objects."""
        for i in range(len(buffer)):
            buffer[i] = Packet(buffer[i])
            if (buffer[i].time_since_start >
                    PacketReader.DROP_COUNT_TIMER_THRESHOLD):
                self._process_dropped_count(buffer[i])

        return buffer

    def _process_dropped_count(self, packet):
        """Processes the dropped count value, updating the internal counters."""
        if packet.dropped_count == self.previous_dropped_count:
            return

        if packet.dropped_count < self.previous_dropped_count:
            self.rollover_count += 1

        self.previous_dropped_count = packet.dropped_count
        log_function = logging.info if __debug__ else logging.warning
        log_function('At %9f, total dropped count: %s' %
                     (packet.time_since_start, self.total_dropped_count))

    @property
    def total_dropped_count(self):
        """Returns the total dropped count, accounting for rollovers."""
        return self.rollover_count * 2**16 + self.previous_dropped_count

    def on_begin(self):
        if __debug__:
            logging.info(
                'The python debugger is enabled. Expect results to '
                'take longer to process after collection is complete.')

    def on_end(self):
        if self.previous_dropped_count > 0:
            if __debug__:
                logging.info(
                    'During collection, a total of %d packets were '
                    'dropped. To reduce this amount, run your test '
                    'without debug mode enabled.' % self.total_dropped_count)
            else:
                logging.warning(
                    'During collection, a total of %d packets were '
                    'dropped.' % self.total_dropped_count)


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
        self.calibration_data = HvpmCalibrationData()

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
                    # the documentation or code Monsoon Inc. provides.
                    logging.warning('Received unidentifiable packet with '
                                    'SampleType %s: %s' % (sample_type,
                                                           packet.get_bytes()))
        return buffer_list

    def _cut_new_buffer(self):
        """Cuts a new buffer from the input stream data.

        Returns:
            The newly generated UncalibratedSampleChunk.
        """
        calibration_snapshot = CalibrationSnapshot(self.calibration_data)
        new_chunk = UncalibratedSampleChunk(self._stored_raw_samples,
                                            calibration_snapshot)
        # Do not clear the list. Instead, create a new one so the old list can
        # be owned solely by the UncalibratedSampleChunk.
        self._stored_raw_samples = []
        return new_chunk


class HvpmReading(object):
    """The result of fully calibrating a sample. Contains all Monsoon readings.

    Attributes:
        _reading_list: The list of values obtained from the Monsoon.
        _time_of_reading: The time since sampling began that the reading was
            collected at.
    """

    def __init__(self, reading_list, time_of_reading):
        """
        Args:
            reading_list: A list of reading values in the order of:
                [0] Main Current
                [1] USB Current
                [2] Aux Current
                [3] Main Voltage
                [4] USB Voltage
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
    def main_voltage(self):
        return self._reading_list[3]

    @property
    def usb_voltage(self):
        return self._reading_list[4]

    @property
    def sample_time(self):
        return self._time_of_reading

    def __add__(self, other):
        return HvpmReading([
            self.main_current + other.main_current,
            self.usb_current + other.usb_current,
            self.aux_current + other.aux_current,
            self.main_voltage + other.main_voltage,
            self.usb_voltage + other.usb_voltage,
        ], self.sample_time + other.sample_time)

    def __truediv__(self, other):
        return HvpmReading([
            self.main_current / other,
            self.usb_current / other,
            self.aux_current / other,
            self.main_voltage / other,
            self.usb_voltage / other,
        ], self.sample_time / other)


class CalibrationApplier(ParallelTransformer):
    """Applies the calibration formula to the all given samples."""

    def __init__(self, monsoon_status_packet):
        super().__init__()
        self.cal_constants = HvpmCalibrationConstants(monsoon_status_packet)
        monsoon = HVPM.Monsoon()
        self.fine_threshold = monsoon.fineThreshold
        self._main_voltage_scale = monsoon.mainvoltageScale
        self._usb_voltage_scale = monsoon.usbVoltageScale
        # According to Monsoon.sampleEngine.__ADCRatio, each tick of the ADC
        # represents this much voltage
        self._adc_ratio = 6.25e-5

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
        """Transforms the buffer's information into HvpmReadings.

        Args:
            buffer: An UncalibratedSampleChunk. This buffer is in-place
                transformed into a buffer of HvpmReadings.
        """
        calibration_data = buffer.calibration_data

        if not self._is_device_calibrated(calibration_data):
            buffer.samples.clear()
            return buffer.samples

        readings = np.zeros((len(buffer.samples), 5))

        measurements = np.array([sample.values for sample in buffer.samples])
        calibrated_value = np.zeros((len(buffer.samples), 2))

        for channel in Channel.values:
            for granularity in Granularity.values:
                scale = self.cal_constants.get(channel, Origin.SCALE,
                                               granularity)
                zero_offset = self.cal_constants.get(channel, Origin.ZERO,
                                                     granularity)
                cal_ref = calibration_data.get(channel, Origin.REFERENCE,
                                               granularity)
                cal_zero = calibration_data.get(channel, Origin.ZERO,
                                                granularity)
                zero_offset += cal_zero
                if cal_ref - zero_offset != 0:
                    slope = scale / (cal_ref - zero_offset)
                else:
                    slope = 0
                if granularity == Granularity.FINE:
                    slope /= 1000

                index = HvpmMeasurement.get_index(channel, granularity)
                calibrated_value[:, granularity] = slope * (
                    measurements[:, index] - zero_offset)

            fine_data_position = HvpmMeasurement.get_index(
                channel, Granularity.FINE)
            readings[:, channel] = np.where(
                measurements[:, fine_data_position] < self.fine_threshold,
                calibrated_value[:, Granularity.FINE],
                calibrated_value[:, Granularity.COARSE]) / 1000.0  # to mA

        main_voltage_index = HvpmMeasurement.get_index(Channel.MAIN,
                                                       Reading.VOLTAGE)
        usb_voltage_index = HvpmMeasurement.get_index(Channel.USB,
                                                      Reading.VOLTAGE)
        readings[:, 3] = (measurements[:, main_voltage_index] * self._adc_ratio
                          * self._main_voltage_scale)
        readings[:, 4] = (measurements[:, usb_voltage_index] * self._adc_ratio
                          * self._usb_voltage_scale)

        for i in range(len(buffer.samples)):
            buffer.samples[i] = HvpmReading(
                list(readings[i]), buffer.samples[i].get_sample_time())

        return buffer.samples
