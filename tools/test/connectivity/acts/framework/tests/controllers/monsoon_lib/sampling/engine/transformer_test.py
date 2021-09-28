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

import unittest

import mock

from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferList
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferStream
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import IndexedBuffer
from acts.controllers.monsoon_lib.sampling.engine.transformer import ParallelTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SequentialTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import SourceTransformer
from acts.controllers.monsoon_lib.sampling.engine.transformer import Transformer

# The indexes of the arguments returned in Mock's call lists.
ARGS = 0
KWARGS = 1


class TransformerImpl(Transformer):
    """A basic implementation of a Transformer object."""

    def __init__(self):
        super().__init__()
        self.actions = []

    def on_begin(self):
        self.actions.append('begin')

    def on_end(self):
        self.actions.append('end')

    def _transform(self, _):
        self.actions.append('transform')


def raise_exception(tipe=Exception):
    def exception_raiser():
        raise tipe()

    return exception_raiser


class TransformerTest(unittest.TestCase):
    """Tests the Transformer class."""

    def test_transform_calls_functions_in_order(self):
        """Tests transform() calls functions in the correct arrangement."""
        my_transformer = TransformerImpl()

        my_transformer.transform(mock.Mock())

        self.assertEqual(['begin', 'transform', 'end'], my_transformer.actions)

    def test_transform_initializes_input_stream(self):
        """Tests transform() initializes the input_stream before beginning."""
        input_stream = mock.Mock()
        transformer = TransformerImpl()
        # Purposely fail before sending any data
        transformer.on_begin = raise_exception(Exception)

        with self.assertRaises(Exception):
            transformer.transform(input_stream)

        # Asserts initialize was called before on_begin.
        self.assertTrue(input_stream.initialize.called)

    def test_transform_initializes_output_stream(self):
        """Tests transform() initializes the output_stream before beginning."""
        output_stream = mock.Mock()
        transformer = TransformerImpl()
        transformer.set_output_stream(output_stream)
        # Purposely fail before sending any data
        transformer.on_begin = raise_exception(Exception)

        with self.assertRaises(Exception):
            transformer.transform(mock.Mock())

        # Asserts initialize was called before on_begin.
        self.assertTrue(output_stream.initialize.called)


class SourceTransformerTest(unittest.TestCase):
    """Tests the SourceTransformer class."""

    def test_transform_ends_on_buffer_stream_end(self):
        """Tests transformation ends on stream end."""
        source_transformer = SourceTransformer()
        source_transformer.set_output_stream(mock.Mock())
        transform_buffer = mock.Mock(side_effect=[BufferStream.END])
        source_transformer._transform_buffer = transform_buffer

        output_stream = mock.Mock()
        source_transformer.transform(output_stream)

        self.assertFalse(output_stream.add_indexed_buffer.called)

    def test_transform_adds_transformed_index_buffer(self):
        source_transformer = SourceTransformer()
        output_stream = mock.Mock()
        source_transformer.set_output_stream(output_stream)
        expected_buffer = [0, 1, 2]
        transform_buffer = mock.Mock(
            side_effect=[expected_buffer, BufferStream.END])
        source_transformer._transform_buffer = transform_buffer

        source_transformer.transform(mock.Mock())

        self.assertEqual(
            expected_buffer,
            output_stream.add_indexed_buffer.call_args[ARGS][0].buffer)

    def test_transform_increases_buffer_index_each_call(self):
        source_transformer = SourceTransformer()
        output_stream = mock.Mock()
        source_transformer.set_output_stream(output_stream)
        buffer = [0, 1, 2]
        transform_buffer = mock.Mock(
            side_effect=[buffer, buffer, buffer, BufferStream.END])
        source_transformer._transform_buffer = transform_buffer

        source_transformer.transform(mock.Mock())

        self.assertEqual([0, 1, 2], [
            output_stream.add_indexed_buffer.call_args_list[i][ARGS][0].index
            for i in range(output_stream.add_indexed_buffer.call_count)
        ])

    def test_transform_calls_end_stream(self):
        source_transformer = SourceTransformer()
        output_stream = mock.Mock()
        source_transformer.set_output_stream(output_stream)
        transform_buffer = mock.Mock(side_effect=[BufferStream.END])
        source_transformer._transform_buffer = transform_buffer

        source_transformer.transform(mock.Mock())

        self.assertTrue(output_stream.end_stream.called)


class SequentialTransformerTest(unittest.TestCase):
    """Unit tests the SequentialTransformer class."""

    def test_send_buffers_updates_next_index_on_buffer_list(self):
        sequential_transformer = SequentialTransformer()
        sequential_transformer._next_index = 10
        expected_next_index = 15

        sequential_transformer._send_buffers(BufferList([[]] * 5))

        self.assertEqual(expected_next_index,
                         sequential_transformer._next_index)

    def test_send_buffers_updates_next_index_on_single_buffer(self):
        sequential_transformer = SequentialTransformer()
        sequential_transformer._next_index = 10
        expected_next_index = 11

        sequential_transformer._send_buffers([])

        self.assertEqual(expected_next_index,
                         sequential_transformer._next_index)

    def test_send_buffers_sends_buffer_list_with_correct_indexes(self):
        buffers_to_send = [
            [1],
            [1, 2],
            [1, 2, 3],
            [1, 2, 3, 4],
            [1, 2, 3, 4, 5],
        ]
        sequential_transformer = SequentialTransformer()
        output_stream = mock.Mock()
        sequential_transformer.set_output_stream(output_stream)
        sequential_transformer._send_buffers(BufferList(buffers_to_send))

        for expected_index, expected_buffer in enumerate(buffers_to_send):
            call = output_stream.add_indexed_buffer.call_args_list[
                expected_index]
            self.assertEqual(expected_index, call[ARGS][0].index)
            self.assertEqual(expected_buffer, call[ARGS][0].buffer)

    def test_transform_breaks_upon_buffer_stream_end_received(self):
        sequential_transformer = SequentialTransformer()
        output_stream = mock.Mock()
        input_stream = mock.Mock()
        sequential_transformer.set_output_stream(output_stream)
        input_stream.remove_indexed_buffer.side_effect = [BufferStream.END]

        sequential_transformer._transform(input_stream)

        self.assertFalse(output_stream.add_indexed_buffer.called)

    def test_transform_closes_output_stream_when_finished(self):
        sequential_transformer = SequentialTransformer()
        output_stream = mock.Mock()
        input_stream = mock.Mock()
        sequential_transformer.set_output_stream(output_stream)
        input_stream.remove_indexed_buffer.side_effect = [BufferStream.END]

        sequential_transformer._transform(input_stream)

        self.assertTrue(output_stream.end_stream.called)


class ParallelTransformerTest(unittest.TestCase):
    """Unit tests the ParallelTransformer class."""

    def test_transform_breaks_upon_buffer_stream_end_received(self):
        parallel_transformer = ParallelTransformer()
        output_stream = mock.Mock()
        input_stream = mock.Mock()
        parallel_transformer.set_output_stream(output_stream)
        input_stream.remove_indexed_buffer.side_effect = [BufferStream.END]

        parallel_transformer._transform(input_stream)

        self.assertFalse(output_stream.add_indexed_buffer.called)

    def test_transform_closes_output_stream_when_finished(self):
        parallel_transformer = ParallelTransformer()
        output_stream = mock.Mock()
        input_stream = mock.Mock()
        parallel_transformer.set_output_stream(output_stream)
        input_stream.remove_indexed_buffer.side_effect = [BufferStream.END]

        parallel_transformer._transform(input_stream)

        self.assertTrue(output_stream.end_stream.called)

    def test_transform_passes_indexed_buffer_with_updated_buffer(self):
        expected_buffer = [0, 1, 2, 3, 4]
        expected_index = 12345
        parallel_transformer = ParallelTransformer()
        output_stream = mock.Mock()
        input_stream = mock.Mock()
        parallel_transformer.set_output_stream(output_stream)
        input_stream.remove_indexed_buffer.side_effect = [
            IndexedBuffer(expected_index, []), BufferStream.END
        ]
        parallel_transformer._transform_buffer = lambda _: expected_buffer

        parallel_transformer._transform(input_stream)

        self.assertEqual(
            expected_buffer,
            output_stream.add_indexed_buffer.call_args_list[0][ARGS][0].buffer)
        self.assertEqual(
            expected_index,
            output_stream.add_indexed_buffer.call_args_list[0][ARGS][0].index)


if __name__ == '__main__':
    unittest.main()
