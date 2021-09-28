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

import numpy as np

from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferList
from acts.controllers.monsoon_lib.sampling.engine.transformer import ParallelTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SequentialTransformer


class Tee(SequentialTransformer):
    """Outputs main_current values to the specified file.

    Attributes:
        _filename: the name of the file to open.
        _fd: the filestream written to.
    """

    def __init__(self, filename, measure_after_seconds=0):
        """Creates an OutputStream.

        Args:
            filename: the path to the file to write the collected data to.
            measure_after_seconds: the number of seconds to skip before
                logging data as part of the measurement.
        """
        super().__init__()
        self._filename = filename
        self._fd = None
        self.measure_after_seconds = measure_after_seconds

    def on_begin(self):
        self._fd = open(self._filename, 'w+')

    def on_end(self):
        self._fd.close()

    def _transform_buffer(self, buffer):
        """Writes the reading values to a file.

        Args:
            buffer: A list of HvpmReadings.
        """
        for sample in buffer:
            if sample.sample_time < self.measure_after_seconds:
                continue
            self._fd.write('%.9fs %.12f\n' %
                           (sample.sample_time - self.measure_after_seconds,
                            sample.main_current))
        self._fd.flush()
        return BufferList([buffer])


class SampleAggregator(ParallelTransformer):
    """Aggregates the main current value and the number of samples gathered."""

    def __init__(self, start_after_seconds=0):
        """Creates a new SampleAggregator.

        Args:
            start_after_seconds: The number of seconds to wait before gathering
                data. Useful for allowing the device to settle after USB
                disconnect.
        """
        super().__init__()
        self._num_samples = 0
        self._sum_currents = 0
        self.start_after_seconds = start_after_seconds

    def _transform_buffer(self, buffer):
        """Aggregates the sample data.

        Args:
            buffer: A buffer of H/LvpmReadings.
        """
        for sample in buffer:
            if sample.sample_time < self.start_after_seconds:
                continue
            self._num_samples += 1
            self._sum_currents += sample.main_current
        return buffer

    @property
    def num_samples(self):
        """The number of samples read from the device."""
        return self._num_samples

    @property
    def sum_currents(self):
        """The total sum of current values gathered so far."""
        return self._sum_currents


class DownSampler(SequentialTransformer):
    """Takes in sample outputs and returns a downsampled version of that data.

    Note for speed, the downsampling must occur at a perfect integer divisor of
    the Monsoon's sample rate (5000 hz).
    """
    _MONSOON_SAMPLE_RATE = 5000

    def __init__(self, downsample_factor):
        """Creates a DownSampler Transformer.

        Args:
            downsample_factor: The number of samples averaged together for a
                single output sample.
        """
        super().__init__()

        self._mean_width = int(downsample_factor)
        self._leftovers = []

    def _transform_buffer(self, buffer):
        """Returns the buffer downsampled by an integer factor.

        The algorithm splits data points into three categories:

            tail: The remaining samples where not enough were collected to
                  reach the integer factor for downsampling. The tail is stored
                  in self._leftovers between _transform_buffer calls.
            tailless_buffer: The samples excluding the tail that can be
                             downsampled directly.

        Below is a diagram explaining the buffer math:

        input:          input buffer n              input buffer n + 1
                 ╔══════════════════════════╗  ╔══════════════════════════╗
             ... ║ ╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗ ║  ║ ╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗ ║ ...
                 ║ ╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝ ║  ║ ╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝ ║
                 ╚══════════════════════════╝  ╚══════════════════════════╝
                               ▼                             ▼
        alg:     ╔═════════════════════╦════╗  ╔═════════════════════╦════╗
                 ║ ╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗║╔╗╔╗║  ║ ╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗╔╗║╔╗╔╗║
                 ║ ╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝║╚╝╚╝║  ║ ╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝╚╝║╚╝╚╝║
             ... ║   tailless_buffer   ║tail║  ║   tailless_buffer   ║tail║ ...
                 ╚═════════════════════╩════╝  ╚═════════════════════╩════╝
               ──┬───┘ └─┬─┘ ...  └─┬─┘ └────┬─────┘ └─┬─┘ ...  └─┬─┘ └──┬───
                 ╔╗      ╔╗ ╔╗  ╔╗ ╔╗        ╔╗        ╔╗ ╔╗  ╔╗ ╔╗      ╔╗
                 ╚╝      ╚╝ ╚╝  ╚╝ ╚╝        ╚╝        ╚╝ ╚╝  ╚╝ ╚╝      ╚╝
                 └─────────┬────────┘        └──────────┬─────────┘
                           ▼                            ▼
        output:   ╔════════════════╗           ╔════════════════╗
                  ║ ╔╗ ╔╗ ╔╗ ╔╗ ╔╗ ║           ║ ╔╗ ╔╗ ╔╗ ╔╗ ╔╗ ║
                  ║ ╚╝ ╚╝ ╚╝ ╚╝ ╚╝ ║           ║ ╚╝ ╚╝ ╚╝ ╚╝ ╚╝ ║
                  ╚════════════════╝           ╚════════════════╝
                   output buffer n             output buffer n + 1
        """
        tail_length = int(
            (len(buffer) + len(self._leftovers)) % self._mean_width)

        tailless_buffer = np.array(buffer[:len(buffer) - tail_length])

        sample_count = len(tailless_buffer) + len(self._leftovers)

        downsampled_values = np.mean(
            np.resize(
                np.append(self._leftovers, tailless_buffer),
                (sample_count // self._mean_width, self._mean_width)),
            axis=1)

        self._leftovers = buffer[len(buffer) - tail_length:]

        return downsampled_values
