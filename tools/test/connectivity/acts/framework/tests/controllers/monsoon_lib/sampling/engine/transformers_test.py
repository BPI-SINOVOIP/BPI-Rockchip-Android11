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
import statistics
import unittest

import mock

from acts.controllers.monsoon_lib.sampling.engine.transformers import DownSampler
from acts.controllers.monsoon_lib.sampling.engine.transformers import SampleAggregator
from acts.controllers.monsoon_lib.sampling.engine.transformers import Tee

ARGS = 0
KWARGS = 1


# TODO: Import HvpmReading directly when it is added to the codebase.
class HvpmReading(object):
    def __init__(self, data, time):
        self.main_current = data[0]
        self.sample_time = time

    def __add__(self, other):
        return HvpmReading([self.main_current + other.main_current],
                           self.sample_time + other.sample_time)

    def __truediv__(self, other):
        return HvpmReading([self.main_current / other],
                           self.sample_time / other)


class TeeTest(unittest.TestCase):
    """Unit tests the transformers.Tee class."""

    @mock.patch('builtins.open')
    def test_begin_opens_file_on_expected_filename(self, open_mock):
        expected_filename = 'foo'

        Tee(expected_filename).on_begin()

        open_mock.assert_called_with(expected_filename, 'w+')

    @mock.patch('builtins.open')
    def test_end_closes_file(self, open_mock):
        tee = Tee('foo')
        tee.on_begin()

        tee.on_end()

        self.assertTrue(open_mock().close.called)

    @mock.patch('builtins.open')
    def test_transform_buffer_outputs_correct_format(self, open_mock):
        tee = Tee('foo')
        tee.on_begin()

        expected_output = [
            '0.010000000s 1.414213562370\n', '0.020000000s 2.718281828460\n',
            '0.030000000s 3.141592653590\n'
        ]

        tee._transform_buffer([
            HvpmReading([1.41421356237, 0, 0, 0, 0], 0.01),
            HvpmReading([2.71828182846, 0, 0, 0, 0], 0.02),
            HvpmReading([3.14159265359, 0, 0, 0, 0], 0.03),
        ])

        for call, out in zip(open_mock().write.call_args_list,
                             expected_output):
            self.assertEqual(call[ARGS][0], out)


class SampleAggregatorTest(unittest.TestCase):
    """Unit tests the transformers.SampleAggregator class."""

    def test_transform_buffer_respects_start_after_seconds_flag(self):
        sample_aggregator = SampleAggregator(start_after_seconds=1)
        sample_aggregator._transform_buffer([
            HvpmReading([1.41421356237, 0, 0, 0, 0], 0.01),
            HvpmReading([2.71828182846, 0, 0, 0, 0], 0.99),
            HvpmReading([3.14159265359, 0, 0, 0, 0], 1.00),
        ])

        self.assertEqual(sample_aggregator.num_samples, 1)
        self.assertEqual(sample_aggregator.sum_currents, 3.14159265359)

    def test_transform_buffer_sums_currents(self):
        sample_aggregator = SampleAggregator()
        sample_aggregator._transform_buffer([
            HvpmReading([1.41421356237, 0, 0, 0, 0], 0.01),
            HvpmReading([2.71828182846, 0, 0, 0, 0], 0.99),
            HvpmReading([3.14159265359, 0, 0, 0, 0], 1.00),
        ])

        self.assertEqual(sample_aggregator.num_samples, 3)
        self.assertAlmostEqual(sample_aggregator.sum_currents, 7.27408804442)


class DownSamplerTest(unittest.TestCase):
    """Unit tests the DownSampler class."""

    def test_transform_buffer_downsamples_without_leftovers(self):
        downsampler = DownSampler(2)
        buffer = [
            HvpmReading([2, 0, 0, 0, 0], .01),
            HvpmReading([4, 0, 0, 0, 0], .03),
            HvpmReading([6, 0, 0, 0, 0], .05),
            HvpmReading([8, 0, 0, 0, 0], .07),
            HvpmReading([10, 0, 0, 0, 0], .09),
            HvpmReading([12, 0, 0, 0, 0], .011),
        ]

        values = downsampler._transform_buffer(buffer)

        self.assertEqual(len(values), len(buffer) / 2)
        for i, down_sample in enumerate(values):
            self.assertAlmostEqual(
                down_sample.main_current,
                ((buffer[2 * i] + buffer[2 * i + 1]) / 2).main_current)

    def test_transform_stores_unused_values_in_leftovers(self):
        downsampler = DownSampler(3)
        buffer = [
            HvpmReading([2, 0, 0, 0, 0], .01),
            HvpmReading([4, 0, 0, 0, 0], .03),
            HvpmReading([6, 0, 0, 0, 0], .05),
            HvpmReading([8, 0, 0, 0, 0], .07),
            HvpmReading([10, 0, 0, 0, 0], .09),
        ]

        downsampler._transform_buffer(buffer)

        self.assertEqual(len(downsampler._leftovers), 2)
        self.assertIn(buffer[-2], downsampler._leftovers)
        self.assertIn(buffer[-1], downsampler._leftovers)

    def test_transform_uses_leftovers_on_next_calculation(self):
        downsampler = DownSampler(3)
        starting_leftovers = [
            HvpmReading([2, 0, 0, 0, 0], .01),
            HvpmReading([4, 0, 0, 0, 0], .03),
        ]
        downsampler._leftovers = starting_leftovers
        buffer = [
            HvpmReading([6, 0, 0, 0, 0], .05),
            HvpmReading([8, 0, 0, 0, 0], .07),
            HvpmReading([10, 0, 0, 0, 0], .09),
            HvpmReading([12, 0, 0, 0, 0], .011)
        ]

        values = downsampler._transform_buffer(buffer)

        self.assertEqual(len(values), 2)
        self.assertNotIn(starting_leftovers[0], downsampler._leftovers)
        self.assertNotIn(starting_leftovers[1], downsampler._leftovers)

        self.assertAlmostEqual(
            values[0].main_current,
            statistics.mean([
                starting_leftovers[0].main_current,
                starting_leftovers[1].main_current,
                buffer[0].main_current,
            ]))
        self.assertAlmostEqual(
            values[1].main_current,
            statistics.mean([
                buffer[1].main_current,
                buffer[2].main_current,
                buffer[3].main_current,
            ]))


if __name__ == '__main__':
    unittest.main()
