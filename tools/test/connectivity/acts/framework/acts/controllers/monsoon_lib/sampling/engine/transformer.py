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

from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferList
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import BufferStream
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import DevNullBufferStream
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import IndexedBuffer


class Transformer(object):
    """An object that represents how to transform a given buffer into a result.

    Attributes:
        output_stream: The stream to output data to upon transformation.
            Defaults to a DevNullBufferStream.
    """

    def __init__(self):
        self.output_stream = DevNullBufferStream(None)

    def set_output_stream(self, output_stream):
        """Sets the Transformer's output stream to the given output stream."""
        self.output_stream = output_stream

    def transform(self, input_stream):
        """Transforms input_stream data and passes it to self.output_stream.

        Args:
            input_stream: The BufferStream of input data this transformer should
                transform. Note that the type of data stored within BufferStream
                is not guaranteed to be in the format expected, much like STDIN
                is not guaranteed to be the format a process expects. However,
                for performance, users should expect the data to be properly
                formatted anyway.
        """
        input_stream.initialize()
        self.output_stream.initialize()
        class_name = self.__class__.__qualname__
        try:
            logging.debug('%s transformer beginning.', class_name)
            self.on_begin()
            logging.debug('%s transformation started.', class_name)
            self._transform(input_stream)
        except Exception:
            # TODO(markdr): Get multi-process error reporting to play nicer.
            logging.exception('%s ran into an exception.', class_name)
            raise
        finally:
            logging.debug('%s transformation ended.', class_name)
            self.on_end()
            logging.debug('%s finished.', class_name)

    def _transform_buffer(self, buffer):
        """Transforms a given buffer.

        The implementation can either:

        1) Return the transformed buffer. Can be either in-place or a new
           buffer.

        2) Return a BufferList: a list of transformed buffers. This is useful
           for grouping data together for faster operations.

        Args:
            buffer: The buffer to transform

        Returns:
            either a buffer or a BufferList. See detailed documentation.
        """
        raise NotImplementedError()

    def _on_end_of_stream(self, input_stream):
        """To be called when the input stream has sent the end of stream signal.

        This is particularly useful for flushing any stored memory into the
        output stream.

        Args:
            input_stream: the stream that was closed.
        """
        # By default, this function closes the output stream.
        self.output_stream.end_stream()

    def _transform(self, input_stream):
        """Should call _transform_buffer within this function."""
        raise NotImplementedError()

    def on_begin(self):
        """A function called before the transform loop begins."""
        pass

    def on_end(self):
        """A function called after the transform loop has ended."""
        pass


class SourceTransformer(Transformer):
    """The base class for generating data in an AssemblyLine.

    Note that any Transformer will be able to generate data, but this class is
    a generic way to send data.

    Attributes:
        _buffer_size: The buffer size for each IndexedBuffer sent over the
            output stream.
    """

    def __init__(self):
        super().__init__()
        # Defaulted to 64, which is small enough to be passed within the .6ms
        # window, but large enough so that it does not spam the queue.
        self._buffer_size = 64

    def _transform(self, _):
        """Generates data and sends it to the output stream."""
        buffer_index = 0
        while True:
            indexed_buffer = IndexedBuffer(buffer_index, self._buffer_size)
            buffer = self._transform_buffer(indexed_buffer.buffer)
            if buffer is BufferStream.END:
                break
            indexed_buffer.buffer = buffer
            self.output_stream.add_indexed_buffer(indexed_buffer)
            buffer_index += 1

        self.output_stream.end_stream()

    def _transform_buffer(self, buffer):
        """Fills the passed-in buffer with data."""
        raise NotImplementedError()


class SequentialTransformer(Transformer):
    """A transformer that receives input in sequential order.

    Attributes:
        _next_index: The index of the next IndexedBuffer that should be read.
    """

    def __init__(self):
        super().__init__()
        self._next_index = 0

    def _transform(self, input_stream):
        while True:
            indexed_buffer = input_stream.remove_indexed_buffer()
            if indexed_buffer is BufferStream.END:
                break
            buffer_or_buffers = self._transform_buffer(indexed_buffer.buffer)
            if buffer_or_buffers is not None:
                self._send_buffers(buffer_or_buffers)

        self._on_end_of_stream(input_stream)

    def _send_buffers(self, buffer_or_buffer_list):
        """Sends buffers over to the output_stream.

        Args:
            buffer_or_buffer_list: A BufferList or buffer object. Note that if
                buffer is None, it is effectively an end-of-stream signal.
        """
        if not isinstance(buffer_or_buffer_list, BufferList):
            # Assume a single buffer was returned
            buffer_or_buffer_list = BufferList([buffer_or_buffer_list])

        buffer_list = buffer_or_buffer_list
        for buffer in buffer_list:
            new_buffer = IndexedBuffer(self._next_index, buffer)
            self.output_stream.add_indexed_buffer(new_buffer)
            self._next_index += 1

    def _transform_buffer(self, buffer):
        raise NotImplementedError()


class ParallelTransformer(Transformer):
    """A Transformer that is capable of running in parallel.

    Buffers received may be unordered. For ordered input, use
    SequentialTransformer.
    """

    def _transform(self, input_stream):
        while True:
            indexed_buffer = input_stream.remove_indexed_buffer()
            if indexed_buffer is None:
                break
            buffer = self._transform_buffer(indexed_buffer.buffer)
            indexed_buffer.buffer = buffer
            self.output_stream.add_indexed_buffer(indexed_buffer)

        self._on_end_of_stream(input_stream)

    def _transform_buffer(self, buffer):
        """Transforms a given buffer.

        Note that ParallelTransformers can NOT return a BufferList. This is a
        limitation with the current indexing system. If the input buffer is
        replaced with multiple buffers, later transformers will not know what
        the proper order of buffers is.

        Args:
            buffer: The buffer to transform

        Returns:
            either None or a buffer. See detailed documentation.
        """
        raise NotImplementedError()
