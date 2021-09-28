#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging
import random
import time

from vts.proto import ComponentSpecificationMessage_pb2 as CompSpecMsg
from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import test_runner
from vts.runners.host import const
from vts.utils.python.mirror import py2pb


class VtsCodelabFmqTest(base_test.BaseTestClass):
    """Testcases for API calls on FMQ from host side.

    Attributes:
        _audio: HalMirror, audio HAL server instance.
        _queue1_writer: ResourceFmqMirror, writer of queue 1.
        _queue1_reader: ResourceFmqMirror, reader of queue 1.
        _queue2_writer: ResourceFmqMirror, writer of queue 2.
        _queue2_reader: ResourceFmqMirror, reader of queue 2.
        _queue3_writer: ResourceFmqMirror, writer of queue 3.
        _queue3_reader1: ResourceFmqMirror, reader 1 of queue 3.
        _queue3_reader2: ResourceFmqMirror, reader 2 of queue 3.
        _queue4_writer: ResourceFmqMirror, writer of queue 4.
        _queue4_reader: ResourceFmqMirror, reader of queue 4.
        _stream_in_types: HalMirror, contains interface in IStreamIn.hal.
    """

    def setUpClass(self):
        """This class loads audio HAL in our target-side driver.

        It also initializes four queues for testing:
        Queue 1: synchronized queue (without blocking) between
                 one writer and one reader, sending uint16_t type in the queue.
        Queue 2: synchronized queue (with blocking feature) between
                 one writer and one reader, sending uint32_t type in
                 the queue.
        Queue 3: unsynchronized queue (without blocking) between
                 one writer and two readers, sending double_t type in
                 the queue.
        Queue 4: synchronized queue (without blocking) between
                 one writer and one reader, sending predefined
                 ReadParameters type in IStreamIn.hal interface.
        """
        self.dut = self.android_devices[0]
        # Initialize an audio hal driver to start all managers
        # on the target side.
        # We use it to check sending FMQ with primitive types and
        # other predefined types (e.g. ReadParameters) in audio HAL.
        self.dut.hal.InitHidlHal(
            target_type="audio",
            target_basepaths=self.dut.libPaths,
            target_version_major=4,
            target_version_minor=0,
            target_package="android.hardware.audio",
            target_component_name="IDevicesFactory",
            bits=int(self.abi_bitness))
        # Create a shortcut for audio HAL server.
        self._audio = self.dut.hal.audio

        # Initialize a non-blocking, synchronized writer.
        self._queue1_writer = self.dut.resource.InitFmq(
            data_type="uint16_t",
            sync=True,
            queue_size=2048,
            blocking=False,
            client=self.dut.hal.GetTcpClient("audio"))
        queue1_writer_id = self._queue1_writer.queueId
        asserts.assertNotEqual(queue1_writer_id, -1)

        # Initialize a non-blocking, synchronized reader.
        # This reader shares the same queue as self._queue1_writer.
        self._queue1_reader = self.dut.resource.InitFmq(
            existing_queue=self._queue1_writer,
            client=self.dut.hal.GetTcpClient("audio"))
        queue1_reader_id = self._queue1_reader.queueId
        asserts.assertNotEqual(queue1_reader_id, -1)

        # Initialize a blocking, synchronized writer.
        self._queue2_writer = self.dut.resource.InitFmq(
            data_type="uint32_t",
            sync=True,
            queue_size=2048,
            blocking=True,
            client=self.dut.hal.GetTcpClient("audio"))
        queue2_writer_id = self._queue2_writer.queueId
        asserts.assertNotEqual(queue2_writer_id, -1)

        # Initialize a blocking, synchronized reader.
        # This reader shares the same queue as self._queue2_writer.
        self._queue2_reader = self.dut.resource.InitFmq(
            existing_queue=self._queue2_writer,
            client=self.dut.hal.GetTcpClient("audio"))
        queue2_reader_id = self._queue2_reader.queueId
        asserts.assertNotEqual(queue2_reader_id, -1)

        # Initialize a non-blocking, unsynchronized writer.
        self._queue3_writer = self.dut.resource.InitFmq(
            data_type="double_t",
            sync=False,
            queue_size=2048,
            blocking=False,
            client=self.dut.hal.GetTcpClient("audio"))
        queue3_writer_id = self._queue3_writer.queueId
        asserts.assertNotEqual(queue3_writer_id, -1)

        # Initialize a non-blocking, unsynchronized reader 1.
        # This reader shares the same queue as self._queue3_writer.
        self._queue3_reader1 = self.dut.resource.InitFmq(
            existing_queue=self._queue3_writer,
            client=self.dut.hal.GetTcpClient("audio"))
        queue3_reader1_id = self._queue3_reader1.queueId
        asserts.assertNotEqual(queue3_reader1_id, -1)

        # Initialize a non-blocking, unsynchronized reader 2.
        # This reader shares the same queue as self._queue3_writer and self._queue3_reader1.
        self._queue3_reader2 = self.dut.resource.InitFmq(
            existing_queue=self._queue3_writer,
            client=self.dut.hal.GetTcpClient("audio"))
        queue3_reader2_id = self._queue3_reader2.queueId
        asserts.assertNotEqual(queue3_reader2_id, -1)

        # Find the user-defined type in IStreamIn.hal service.
        self._stream_in_types = self._audio.GetHidlTypeInterface("IStreamIn")
        read_param_type = self._stream_in_types.GetAttribute("ReadParameters")
        # Initialize a non-blocking, synchronized writer.
        self._queue4_writer = self.dut.resource.InitFmq(
            # ::android::hardware::audio::V4_0::IStreamIn::ReadParameters
            data_type=read_param_type.name,
            sync=True,
            queue_size=2048,
            blocking=False,
            client=self.dut.hal.GetTcpClient("audio"))
        queue4_writer_id = self._queue4_writer.queueId
        asserts.assertNotEqual(queue4_writer_id, -1)

        # Initialize a non-blocking, synchronized reader.
        # This reader shares the same queue as self._queue4_writer.
        self._queue4_reader = self.dut.resource.InitFmq(
            existing_queue=self._queue4_writer,
            client=self.dut.hal.GetTcpClient("audio"))
        queue4_reader_id = self._queue4_reader.queueId
        asserts.assertNotEqual(queue4_reader_id, -1)

    def testBasic(self):
        """Tests correctness of basic util methods. """
        # Check the correctness on queue 1, which uses primitive type uint32_t.
        asserts.assertEqual(self._queue1_writer.getQuantumSize(), 2)
        asserts.assertEqual(self._queue1_writer.getQuantumCount(), 2048)
        asserts.assertEqual(self._queue1_writer.availableToWrite(), 2048)
        asserts.assertEqual(self._queue1_reader.availableToRead(), 0)
        asserts.assertTrue(self._queue1_writer.isValid(),
                           "Queue 1 writer should be valid.")
        asserts.assertTrue(self._queue1_reader.isValid(),
                           "Queue 1 reader should be valid.")

        # Also check the correctness on queue 4, which uses predefined type
        # in audio HAL service.
        asserts.assertEqual(self._queue4_writer.getQuantumCount(), 2048)
        asserts.assertEqual(self._queue4_writer.availableToWrite(), 2048)
        asserts.assertEqual(self._queue4_reader.availableToRead(), 0)
        asserts.assertTrue(self._queue4_writer.isValid(),
                           "Queue 4 writer should be valid.")
        asserts.assertTrue(self._queue4_reader.isValid(),
                           "Queue 4 reader should be valid.")

    def testSimpleReadWrite(self):
        """Test a simple interaction between a writer and a reader.

        This test operates on queue 1, and tests basic read/write.
        """
        write_data = self.GetRandomIntegers(2048)
        read_data = []
        # Writer writes some data.
        asserts.assertTrue(
            self._queue1_writer.write(write_data, 2048), "Write queue failed.")
        # Check reader reads them back correctly.
        read_success = self._queue1_reader.read(read_data, 2048)
        asserts.assertTrue(read_success, "Read queue failed.")
        asserts.assertEqual(read_data, write_data)

    def testReadEmpty(self):
        """Test reading from an empty queue. """
        read_data = []
        read_success = self._queue1_reader.read(read_data, 5)
        asserts.assertFalse(read_success,
                            "Read should fail because queue is empty.")

    def testWriteFull(self):
        """Test writes fail when queue is full. """
        write_data = self.GetRandomIntegers(2048)

        # This write should succeed.
        asserts.assertTrue(
            self._queue1_writer.write(write_data, 2048),
            "Writer should write successfully.")
        # This write should fail because queue is full.
        asserts.assertFalse(
            self._queue1_writer.write(write_data, 2048),
            "Writer should fail because queue is full.")

    def testWriteTooLarge(self):
        """Test writing more than queue capacity. """
        write_data = self.GetRandomIntegers(2049)
        # Write overflows the capacity of the queue.
        asserts.assertFalse(
            self._queue1_writer.write(write_data, 2049),
            "Writer should fail since there are too many items to write.")

    def testConsecutiveReadWrite(self):
        """Test consecutive interactions between reader and writer.

        This test operates on queue 1, and tests consecutive read/write.
        """
        for i in range(64):
            write_data = self.GetRandomIntegers(2048)
            asserts.assertTrue(
                self._queue1_writer.write(write_data, 2048),
                "Writer should write successfully.")
            read_data = []
            read_success = self._queue1_reader.read(read_data, 2048)
            asserts.assertTrue(read_success,
                               "Reader should read successfully.")
            asserts.assertEqual(write_data, read_data)

        # Reader should have no more available to read.
        asserts.assertEqual(0, self._queue1_reader.availableToRead())

    def testBlockingReadWrite(self):
        """Test blocking read/write.

        This test operates on queue 2, and tests blocking read/write.
        Writer waits 0.05s and writes.
        Reader blocks for at most 0.1s, and should read successfully.
        TODO: support this when reader and writer operates in parallel.
        """
        write_data = self.GetRandomIntegers(2048)
        read_data = []

        # Writer waits for 0.05s and writes.
        time.sleep(0.05)
        asserts.assertTrue(
            self._queue2_writer.writeBlocking(write_data, 2048, 1000000),
            "Writer should write successfully.")

        # Reader reads.
        read_success = self._queue2_reader.readBlocking(
            read_data, 2048, 1000 * 1000000)
        asserts.assertTrue(read_success,
                           "Reader should read successfully after blocking.")
        asserts.assertEqual(write_data, read_data)

    def testBlockingTimeout(self):
        """Test blocking timeout.

        This test operates on queue2, and tests that reader should time out
        because there is not data available.
        """
        read_data = []
        read_success = self._queue2_reader.readBlocking(
            read_data, 5, 100 * 1000000)
        asserts.assertFalse(
            read_success,
            "Reader blocking should time out because there is no data to read."
        )

    def testUnsynchronizedReadWrite(self):
        """Test separate read from two readers.

        This test operates on queue3, and tests that two readers can read back
        what writer writes.
        """
        # Prepare write data.
        write_data = self.GetRandomFloats(2048)
        read_data1 = []
        read_data2 = []
        asserts.assertTrue(
            self._queue3_writer.write(write_data, 2048),
            "Writer should write successfully.")
        read_success1 = self._queue3_reader1.read(read_data1, 2048)
        read_success2 = self._queue3_reader2.read(read_data2, 2048)
        asserts.assertTrue(read_success1, "Reader 1 should read successfully.")
        asserts.assertTrue(read_success2, "Reader 2 should read successfully.")
        asserts.assertEqual(write_data, read_data1)
        asserts.assertEqual(write_data, read_data2)

    def testIllegalBlocking(self):
        """Test blocking is not allowed in unsynchronized queue.

        This test operates on queue 3, and tests that blocking is not allowed
        in unsynchronized queue.
        """
        write_data = self.GetRandomFloats(2048)
        asserts.assertFalse(
            self._queue3_writer.writeBlocking(write_data, 2048, 100 * 1000000),
            "Blocking operation should fail in unsynchronized queue.")

    def testSimpleReadWriteStructType(self):
        """Test read/write on queue with predefined type in HAL service.

        This test operates on queue 4, and tests reader and writer can interact
        in a queue with predefined type ReadParameters defined in IStreamIn.hal.
        """
        write_data = [{
            "command": self._stream_in_types.ReadCommand.READ,
            "params": {
                "read": 100
            }
        }, {
            "command": self._stream_in_types.ReadCommand.READ,
            "params": {
                "read": 1000
            }
        }, {
            "command":
            self._stream_in_types.ReadCommand.GET_CAPTURE_POSITION,
            "params": {}
        }]

        # Convert each item into a VariableSpecificationMessage using Py2Pb library.
        read_param_type = self._stream_in_types.GetAttribute("ReadParameters")
        converted_write_data = map(
            lambda item: py2pb.Convert(read_param_type, item), write_data)
        asserts.assertTrue(
            self._queue4_writer.write(converted_write_data, 3),
            "Writer should write successfully.")

        # Reader reads the data back, result is a list of dict.
        read_data = []
        asserts.assertTrue(
            self._queue4_reader.read(read_data, 3),
            "Reader should read successfully.")
        for i in range(len(write_data)):
            asserts.assertTrue(
                self.VerifyDict(write_data[i], read_data[i]),
                "Dictionary item %d mismatch.", i)

    @staticmethod
    def GetRandomIntegers(data_size):
        """Helper method to generate a list of random integers between 0 and 100.

        Args:
            data_size: int, length of result list.

        Returns:
            int list, list of integers.
        """
        return [random.randint(0, 100) for i in range(data_size)]

    @staticmethod
    def GetRandomFloats(data_size):
        """Helper method to generate a list of random floats between 0.0 and 100.0.

        Args:
            data_size: int, length of result list.

        Returns:
            float list, list of floats.
        """
        return [random.random() * 100 for i in range(data_size)]

    @staticmethod
    def VerifyDict(correct_dict, return_dict):
        """Check if two dictionary values are equal.

        This method loops through keys in the dictionary. Two dictionaries can
        differ in such cases:
        1. A name exists in the correct dictionary, but not in the returned
           dictionary.
        2. The values differ for the same name. If the value is a primitive
           type, such as integer, string, directly compare them.
           If the value is a nested dict, recursively call this function to
           verify the nested dict.

        Args:
            correct_dict: dict, correct dictionary.
            return_dict: dict, dictionary that is actually returned to reader.

        Returns:
            bool, true if two dictionaries match, false otherwise.
        """
        for name in correct_dict:
            if name not in return_dict:
                return False
            correct_val = correct_dict[name]
            if type(correct_val) == dict:
                if not VtsCodelabFmqTest.VerifyDict(correct_val,
                                                    return_dict[name]):
                    return False
            else:
                if correct_val != return_dict[name]:
                    return False
        return True


if __name__ == "__main__":
    test_runner.main()
