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

from acts.controllers.monsoon_lib.sampling.engine.assembly_line import AssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import DevNullBufferStream
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import IndexedBuffer
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ProcessAssemblyLine
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ThreadAssemblyLine

ASSEMBLY_LINE_MODULE = (
    'acts.controllers.monsoon_lib.sampling.engine.assembly_line')


def mock_import(full_module_name, import_name):
    return mock.patch('%s.%s' % (full_module_name, import_name))


class ProcessAssemblyLineTest(unittest.TestCase):
    """Tests the basic functionality of ProcessAssemblyLine."""

    @mock.patch('multiprocessing.Pool')
    def test_run_no_nodes(self, pool_mock):
        """Tests run() with no nodes does not spawn a new process."""
        empty_node_list = []
        assembly_line = ProcessAssemblyLine(empty_node_list)

        assembly_line.run()

        self.assertFalse(pool_mock().__enter__().apply_async.called)

    @mock.patch('multiprocessing.Pool')
    def test_run_spawns_new_process_for_each_node(self, pool_mock):
        """Tests run() with a node spawns a new process for each node."""
        node_list = [mock.Mock(), mock.Mock()]
        assembly_line = ProcessAssemblyLine(node_list)

        assembly_line.run()

        apply_async = pool_mock().apply_async
        self.assertEqual(len(node_list), apply_async.call_count)
        for node in node_list:
            apply_async.assert_any_call(node.transformer.transform,
                                        [node.input_stream])


class ThreadAssemblyLineTest(unittest.TestCase):
    """Tests the basic functionality of ThreadAssemblyLine."""

    @mock_import(ASSEMBLY_LINE_MODULE, 'ThreadPoolExecutor')
    def test_run_no_nodes(self, pool_mock):
        """Tests run() with no nodes does not spawn a new thread."""
        empty_node_list = []
        assembly_line = ThreadAssemblyLine(empty_node_list)

        assembly_line.run()

        self.assertFalse(pool_mock().__enter__().submit.called)

    @mock_import(ASSEMBLY_LINE_MODULE, 'ThreadPoolExecutor')
    def test_run_spawns_new_thread_for_each_node(self, pool_mock):
        """Tests run() with a node spawns a new thread for each node."""
        node_list = [mock.Mock(), mock.Mock()]
        assembly_line = ThreadAssemblyLine(node_list)

        assembly_line.run()

        submit = pool_mock().__enter__().submit
        self.assertEqual(len(node_list), submit.call_count)
        for node in node_list:
            submit.assert_any_call(node.transformer.transform,
                                   node.input_stream)


class AssemblyLineBuilderTest(unittest.TestCase):
    """Tests the basic functionality of AssemblyLineBuilder."""

    def test_source_raises_if_nodes_already_in_assembly_line(self):
        """Tests a ValueError is raised if a node already exists."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        first_source = mock.Mock()
        second_source = mock.Mock()
        builder.source(first_source)

        with self.assertRaises(ValueError) as context:
            builder.source(second_source)

        self.assertIn('single source', context.exception.args[0])

    def test_source_sets_input_stream_from_given_stream(self):
        """Tests source() sets input_stream from args."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        input_stream = mock.Mock()
        dummy_source = mock.Mock()

        builder.source(dummy_source, input_stream=input_stream)

        self.assertEqual(input_stream, builder.nodes[-1].input_stream)

    def test_source_creates_a_new_input_stream(self):
        """Tests source() takes in DevNullBufferStream when None is provided."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        dummy_source = mock.Mock()

        builder.source(dummy_source)

        self.assertIsInstance(builder.nodes[-1].input_stream,
                              DevNullBufferStream)

    def test_source_returns_self(self):
        """Tests source() returns the builder."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())

        return_value = builder.source(mock.Mock())

        self.assertEqual(return_value, builder)

    def test_into_raises_value_error_if_source_not_called_yet(self):
        """Tests a ValueError is raised if into() is called before source()."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        dummy_transformer = mock.Mock()

        with self.assertRaises(ValueError) as context:
            builder.into(dummy_transformer)

        self.assertIn('source', context.exception.args[0])

    def test_into_raises_value_error_if_already_built(self):
        """Tests a ValueError is raised into() is called after build()."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        dummy_transformer = mock.Mock()
        # Build before trying to add more nodes.
        builder.source(dummy_transformer).build()

        with self.assertRaises(ValueError) as context:
            builder.into(dummy_transformer)

        self.assertIn('built', context.exception.args[0])

    def test_into_appends_transformer_to_node_list(self):
        """Tests into() appends the transformer to the end of the node list."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        dummy_transformer = mock.Mock()
        dummy_source = mock.Mock()
        builder.source(dummy_source)

        builder.into(dummy_transformer)

        self.assertEqual(dummy_transformer, builder.nodes[-1].transformer)

    def test_into_sets_output_stream_to_newly_created_stream(self):
        """Tests into() sets the input_stream queue to the newly created one."""
        queue_generator = mock.Mock()
        builder = AssemblyLineBuilder(queue_generator, mock.Mock())
        dummy_transformer = mock.Mock()
        dummy_source = mock.Mock()
        builder.source(dummy_source)

        builder.into(dummy_transformer)

        self.assertEqual(queue_generator(),
                         builder.nodes[-1].input_stream._buffer_queue)

    def test_into_returns_self(self):
        """Tests into() returns the builder."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        builder.source(mock.Mock())

        return_value = builder.into(mock.Mock())

        self.assertEqual(return_value, builder)

    def test_build_raises_if_already_built(self):
        """Tests build() raises ValueError if build() was already called."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())
        builder.source(mock.Mock()).build()

        with self.assertRaises(ValueError) as context:
            builder.build()

        self.assertIn('already built', context.exception.args[0])

    def test_build_raises_if_no_source_has_been_set(self):
        """Tests build() raises when there's nothing to build."""
        builder = AssemblyLineBuilder(mock.Mock(), mock.Mock())

        with self.assertRaises(ValueError) as context:
            builder.build()

        self.assertIn('empty', context.exception.args[0])

    def test_build_properly_sets_output_stream(self):
        """Tests build() passes the output_stream to the AssemblyLine."""
        given_output_stream = 1

        assembly_line_generator = mock.Mock()
        builder = AssemblyLineBuilder(mock.Mock(), assembly_line_generator)
        builder.source(mock.Mock())

        builder.build(output_stream=given_output_stream)

        self.assertEqual(
            assembly_line_generator.call_args[0][0][-1].output_stream,
            given_output_stream)

    def test_build_generates_dev_null_stream_by_default(self):
        """Tests build() uses DevNullBufferStream when no output_stream."""
        assembly_line_generator = mock.Mock()
        builder = AssemblyLineBuilder(mock.Mock(), assembly_line_generator)
        builder.source(mock.Mock())

        builder.build()

        self.assertIsInstance(
            assembly_line_generator.call_args[0][0][-1].output_stream,
            DevNullBufferStream)


class IndexedBufferTest(unittest.TestCase):
    """Tests the IndexedBuffer class."""

    def test_create_indexed_buffer_uses_existing_list(self):
        my_list = [0, 1, 2, 3, 4, 5]
        self.assertEqual(IndexedBuffer(0, my_list).buffer, my_list)

    def test_create_indexed_buffer_creates_buffer_when_given_a_size(self):
        buffer_len = 10
        self.assertEqual(len(IndexedBuffer(0, buffer_len).buffer), buffer_len)


if __name__ == '__main__':
    unittest.main()
