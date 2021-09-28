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

import queue
from concurrent.futures import ThreadPoolExecutor
import multiprocessing


class AssemblyLine(object):
    """A class for passing data through a chain of threads or processes,
    assembly-line style.

    Attributes:
        nodes: A list of AssemblyLine.Nodes that pass data from one node to the
            next.
    """

    class Node(object):
        """A Node in an AssemblyLine.

        Each node is composed of the following:

         input_stream                    output_stream
        ==============> [ transformer ] ===============>

        Attributes:
            transformer: The Transformer that takes input from the input
                stream, transforms the data, and sends it to the output stream.
            input_stream: The stream of data to be taken in as input to this
                transformer. This stream is the stream to be registered as the
                previous node's output stream.

        Properties:
            output_stream: The stream of data to be passed to the next node.
        """

        def __init__(self, transformer=None, input_stream=None):
            self.transformer = transformer
            self.input_stream = input_stream

        @property
        def output_stream(self):
            return self.transformer.output_stream

        @output_stream.setter
        def output_stream(self, value):
            self.transformer.output_stream = value

    def __init__(self, nodes):
        """Initializes an AssemblyLine class.

        nodes:
            A list of AssemblyLine.Node objects.
        """
        self.nodes = nodes

    def run(self):
        """Runs the AssemblyLine, passing the data between each work node."""
        raise NotImplementedError()


class ProcessAssemblyLine(AssemblyLine):
    """An AssemblyLine that uses processes to schedule work on nodes."""

    def run(self):
        """Runs the AssemblyLine within a process pool."""
        if not self.nodes:
            # If self.nodes is empty, it will create a multiprocessing.Pool of
            # 0 nodes, which raises a ValueError.
            return

        process_pool = multiprocessing.Pool(processes=len(self.nodes))
        for node in self.nodes:
            process_pool.apply_async(node.transformer.transform,
                                     [node.input_stream])
        process_pool.close()
        process_pool.join()


class ThreadAssemblyLine(AssemblyLine):
    """An AssemblyLine that uses threading to schedule work on nodes."""

    def run(self):
        """Runs the AssemblyLine within a thread pool."""
        with ThreadPoolExecutor(max_workers=len(self.nodes)) as thread_pool:
            for node in self.nodes:
                thread_pool.submit(node.transformer.transform,
                                   node.input_stream)


class AssemblyLineBuilder(object):
    """An abstract class that builds an AssemblyLine object.

    Attributes:
    _assembly_line_generator: The callable that creates the AssemblyLine.
        Should be in the form of:

            Args:
                A list of AssemblyLine.Node objects.

            Returns:
                An AssemblyLine object.

    _queue_generator: The callable that creates new queues to be used for
        BufferStreams. Should be in the form of:

            Args:
                None.

            Returns:
                A Queue object.
    """

    def __init__(self, queue_generator, assembly_line_generator):
        """Creates an AssemblyLineBuilder.

        Args:
            queue_generator: A callable of type lambda: Queue().
            assembly_line_generator: A callable of type
                lambda list<AssemblyLine.Node>: AssemblyLine.
        """
        super().__init__()
        self._assembly_line_generator = assembly_line_generator
        self._queue_generator = queue_generator

        self.nodes = []
        self._built = False

    @property
    def built(self):
        return self._built

    def __generate_queue(self):
        """Returns a new Queue object for passing information between nodes."""
        return self._queue_generator()

    @property
    def queue_generator(self):
        """Returns the callable used for generating queues."""
        return self._queue_generator

    def source(self, transformer, input_stream=None):
        """Adds a SourceTransformer to the AssemblyLine.

        Must be the first function call on the AssemblyLineBuilder.

        Args:
            transformer: The SourceTransformer that generates data for the
                AssemblyLine to process.
            input_stream: The input stream to use, if necessary.

        Raises:
            ValueError if source is not the first transformer to be added to
                the AssemblyLine, or the AssemblyLine has been built.
        """
        if self.nodes:
            raise ValueError('AssemblyLines can only have a single source.')
        if input_stream is None:
            input_stream = DevNullBufferStream()
        self.nodes.append(AssemblyLine.Node(transformer, input_stream))
        return self

    def into(self, transformer):
        """Adds the given transformer next in the AssemblyLine.

        Args:
            transformer: The transformer next in the AssemblyLine.

        Raises:
            ValueError if no source node is set, or the AssemblyLine has been
                built.
        """
        if not self.nodes:
            raise ValueError('The source transformer must be set first.')
        if self.built:
            raise ValueError('Cannot add additional nodes after the '
                             'AssemblyLine has been built.')
        stream = BufferStream(self.__generate_queue())
        self.nodes[-1].transformer.set_output_stream(stream)
        self.nodes.append(AssemblyLine.Node(transformer, stream))
        return self

    def build(self, output_stream=None):
        """Builds the AssemblyLine object.

        Note that after this function is called this AssemblyLineBuilder cannot
        be used again, as it is already marked as built.
        """
        if self.built:
            raise ValueError('The AssemblyLine is already built.')
        if not self.nodes:
            raise ValueError('Cannot create an empty assembly line.')
        self._built = True
        if output_stream is None:
            output_stream = DevNullBufferStream()
        self.nodes[-1].output_stream = output_stream
        return self._assembly_line_generator(self.nodes)


class ThreadAssemblyLineBuilder(AssemblyLineBuilder):
    """An AssemblyLineBuilder for generating ThreadAssemblyLines."""

    def __init__(self, queue_generator=queue.Queue):
        super().__init__(queue_generator, ThreadAssemblyLine)


class ProcessAssemblyLineBuilder(AssemblyLineBuilder):
    """An AssemblyLineBuilder for ProcessAssemblyLines.

    Attributes:
        manager: The multiprocessing.Manager used for having queues communicate
            with one another over multiple processes.
    """

    def __init__(self):
        self.manager = multiprocessing.Manager()
        super().__init__(self.manager.Queue, ProcessAssemblyLine)


class IndexedBuffer(object):
    """A buffer indexed with the order it was generated in."""

    def __init__(self, index, size_or_buffer):
        """Creates an IndexedBuffer.

        Args:
            index: The integer index associated with the buffer.
            size_or_buffer:
                either:
                    An integer specifying the number of slots in the buffer OR
                    A list to be used as a buffer.
        """
        self.index = index
        if isinstance(size_or_buffer, int):
            self.buffer = [None] * size_or_buffer
        else:
            self.buffer = size_or_buffer


class BufferList(list):
    """A list of Buffers.

    This type is useful for differentiating when a buffer has been returned
    from a transformer, vs when a list of buffers has been returned from a
    transformer.
    """


class BufferStream(object):
    """An object that acts as a stream between two transformers."""

    # The object passed to the buffer queue to signal the end-of-stream.
    END = None

    def __init__(self, buffer_queue):
        """Creates a new BufferStream.

        Args:
            buffer_queue: A Queue object used to pass data along the
                BufferStream.
        """
        self._buffer_queue = buffer_queue

    def initialize(self):
        """Initializes the stream.

        When running BufferStreams through multiprocessing, initialize must
        only be called on the process using the BufferStream.
        """
        # Here we need to make any call to the stream to initialize it. This
        # makes read and write times for the first buffer faster, preventing
        # the data at the beginning from being dropped.
        self._buffer_queue.qsize()

    def end_stream(self):
        """Closes the stream.

        By convention, a None object is used, mirroring file reads returning
        an empty string when the end of file is reached.
        """
        self._buffer_queue.put(None, block=False)

    def add_indexed_buffer(self, buffer):
        """Adds the given buffer to the buffer stream."""
        self._buffer_queue.put(buffer, block=False)

    def remove_indexed_buffer(self):
        """Removes an indexed buffer from the array.

        This operation blocks until data is received.

        Returns:
            an IndexedBuffer.
        """
        return self._buffer_queue.get()


class DevNullBufferStream(BufferStream):
    """A BufferStream that is always empty."""

    def __init__(self, *_):
        super().__init__(None)

    def initialize(self):
        """Does nothing. Nothing to initialize."""
        pass

    def end_stream(self):
        """Does nothing. The stream always returns end-of-stream when read."""
        pass

    def add_indexed_buffer(self, buffer):
        """Imitating /dev/null, nothing will be written to the stream."""
        pass

    def remove_indexed_buffer(self):
        """Always returns the end-of-stream marker."""
        return None
