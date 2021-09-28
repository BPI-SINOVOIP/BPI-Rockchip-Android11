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
import time

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner
from vts.proto import ComponentSpecificationMessage_pb2 as CompSpecMsg


class VtsHalTestsMsgqV1_0HostTest(base_test.BaseTestClass):
    """This testcase is converted from FMQ HAL service test on the target side
       (system/libfmq/tests/msgq_test_client.cpp).
       Since current host-side doesn't support long-form blocking or
       zero copy operations, all testcases related to these two features are not
       converted here.
       UnsynchronizedWriteClientMultiProcess class is not converted here
       because the testcase is similar to OverflowNotificationTest in
       UnsynchronizedWriteClient class.
    """
    TEST_HAL_SERVICES = {"android.hardware.tests.msgq@1.0::ITestMsgQ"}
    MAX_NUM_MSG = 1024
    MAX_RETRY = 3
    SERVICE_NAME = "android.hardware.tests.msgq@1.0-service-test"
    COMMAND_32 = "TREBLE_TESTING_OVERRIDE=true /data/nativetest/" + SERVICE_NAME + "/" + SERVICE_NAME + " &"
    COMMAND_64 = "TREBLE_TESTING_OVERRIDE=true /data/nativetest64/" + SERVICE_NAME + "/" + SERVICE_NAME + " &"
    CHECK_COMMAND = "lshal --types=b | grep \"android.hardware.tests.msgq@1.0\""

    def setUpClass(self):
        self.dut = self.android_devices[0]
        if (int(self.abi_bitness) == 64):
            self.dut.shell.Execute(self.COMMAND_64)
        else:
            self.dut.shell.Execute(self.COMMAND_32)

        start_hal_success = False
        # Wait until service is started.
        # Retry at most three times.
        for i in range(self.MAX_RETRY):
            result = self.dut.shell.Execute(self.CHECK_COMMAND)
            if (result[const.STDOUT][0] != ""):
                start_hal_success = True # setup successful
                break
            time.sleep(1)  # wait one second.

        # msgq HAL service is still not started after waiting for
        # self.MAX_RETRY times, stop the testcase.
        if (not start_hal_success):
            logging.error("Failed to start msgq HAL service.")
            return False

        super(VtsHalTestsMsgqV1_0HostTest, self).setUpClass()
        # Load a msgq test hal driver.
        self.dut.hal.InitHidlHal(
            target_type="tests_msgq",
            target_basepaths=self.dut.libPaths,
            target_version_major=1,
            target_version_minor=0,
            target_package="android.hardware.tests.msgq",
            target_component_name="ITestMsgQ",
            bits=int(self.abi_bitness),
            is_test_hal=True)

        # create a shortcut.
        self._tests_msgq = self.dut.hal.tests_msgq

    def setUp(self):
        # Initialize a FMQ on the target-side driver.
        self._sync_client = self.dut.resource.InitFmq(
            data_type="uint16_t",
            sync=True,
            queue_size=self.MAX_NUM_MSG,
            blocking=True,
            client=self.dut.hal.GetTcpClient("tests_msgq"))
        asserts.assertNotEqual(self._sync_client.queueId, -1)

        # Prepare a VariableSpecificationMessage to specify the FMQ that will be
        # passed into the HAL service.
        var_msg = CompSpecMsg.VariableSpecificationMessage()
        var_msg.type = CompSpecMsg.TYPE_FMQ_SYNC
        fmq_val = var_msg.fmq_value.add()
        fmq_val.fmq_id = self._sync_client.queueId
        fmq_val.scalar_type = "uint16_t"
        fmq_val.type = CompSpecMsg.TYPE_SCALAR

        # Call API in the HAL server.
        sync_init_result = self._tests_msgq.configureFmqSyncReadWrite(var_msg)
        asserts.assertTrue(
            sync_init_result,
            "Hal should configure a synchronized queue without error.")

        # Initialize an unsynchronized queue on the server.
        [success,
         self._unsync_client1] = self._tests_msgq.getFmqUnsyncWrite(True)
        asserts.assertTrue(
            success,
            "Hal should configure an unsynchronized queue without error.")
        # An unsynchronized queue is registered successfully on the target driver.
        asserts.assertNotEqual(self._unsync_client1, None)
        asserts.assertNotEqual(self._unsync_client1.queueId, -1)
        # Register another reader.
        self._unsync_client2 = self.dut.resource.InitFmq(
            existing_queue=self._unsync_client1,
            client=self.dut.hal.GetTcpClient("tests_msgq"))
        asserts.assertNotEqual(self._unsync_client2.queueId, -1)

    def testSyncQueueBlockingReadWriteSuccess(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: BlockingReadWrite2.
           Server is blocked because there is no data to read.
           Client writes once, which unblocks the server.
           Another write from client will also succeed.
        """
        # Request a blocking read from the server.
        # This call returns immediately.
        self._tests_msgq.requestBlockingReadDefaultEventFlagBits(
            self.MAX_NUM_MSG)

        # Client writes, unblocks server.
        write_data = generateSequentialData(self.MAX_NUM_MSG)
        asserts.assertTrue(
            self._sync_client.writeBlocking(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")

        # If blocking read was successful from the server, another write of
        # size 1000 will succeed because there is space to write now.
        asserts.assertTrue(
            self._sync_client.writeBlocking(write_data, self.MAX_NUM_MSG,
                                            5000000000),
            "Client should write successfully if server blocking read is successful."
        )
        # Server reads it back again.
        self._tests_msgq.requestBlockingReadDefaultEventFlagBits(
            self.MAX_NUM_MSG)

    def testSyncQueueSmallInputReaderTest1(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, SmallInputReaderTest1.
           Server writes a small number of messages and client reads it back.
        """
        data_len = 16

        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqSync(data_len),
            "Server should write successfully.")
        # Client reads.
        read_data = []
        asserts.assertTrue(
            self._sync_client.read(read_data, data_len),
            "Client should read successfully.")
        asserts.assertEqual(read_data, generateSequentialData(data_len))

    def testSyncQueueSmallInputWriterTest1(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, SmallInputWriterTest1.
           Client writes a small number of messages and server reads it back.
        """
        data_len = 16
        write_data = generateSequentialData(data_len)

        # Client writes.
        asserts.assertTrue(
            self._sync_client.write(write_data, data_len),
            "Server should write successfully.")
        # Server reads.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(data_len),
            "Client should read successfully.")

    def testSyncQueueReadWhenEmpty(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, ReadWhenEmpty.
           Read should fail when queue is empty.
        """
        asserts.assertEqual(self._sync_client.availableToRead(), 0)
        asserts.assertFalse(
            self._sync_client.read([], 2),
            "Client should fail to read because queue is empty.")

    def testSyncQueueWriteWhenFull(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, WriteWhenFull.
           Write should fail when queue is full.
        """
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client writes.
        asserts.assertTrue(
            self._sync_client.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        asserts.assertEqual(self._sync_client.availableToWrite(), 0)
        # Client tries to write more, fails.
        asserts.assertFalse(
            self._sync_client.write([1], 1),
            "Client should fail to write because queue is full.")
        # Server should read data back correctly.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(self.MAX_NUM_MSG),
            "Server should read successfully")

    def testSyncQueueLargeInputTest1(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, LargeInputTest1.
           Server writes to the queue and client reads the data back.
        """
        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqSync(self.MAX_NUM_MSG),
            "Server should write successfully.")

        write_data = generateSequentialData(self.MAX_NUM_MSG)
        read_data = []
        # Client reads.
        asserts.assertEqual(self._sync_client.availableToRead(),
                            self.MAX_NUM_MSG)
        self._sync_client.read(read_data, self.MAX_NUM_MSG)
        asserts.assertEqual(write_data, read_data)

    def testSyncQueueLargeInputTest2(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, LargeInputTest2.
           Server attempts to write more than the queue capacity and fails.
        """
        asserts.assertEqual(0, self._sync_client.availableToRead())
        # Server attempts to write more than the queue capacity.
        asserts.assertFalse(
            self._tests_msgq.requestWriteFmqSync(self.MAX_NUM_MSG * 2),
            "Server should fail because it writes more than queue capacity.")
        # Check there is still no data for client.
        asserts.assertEqual(0, self._sync_client.availableToRead())
        asserts.assertFalse(
            self._sync_client.read([], 1),
            "Client should fail to read because queue is empty.")

    def testSyncQueueLargeInputTest3(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, LargeInputTest3.
           Client writes until the queue is full, attempts to write one more,
           and fails.
        """
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client fills up the queue.
        asserts.assertTrue(
            self._sync_client.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        asserts.assertEqual(0, self._sync_client.availableToWrite())
        # Client attempts to write one more, fails.
        asserts.assertFalse(
            self._sync_client.write([1], 1),
            "Client should fail to write because queue is full.")
        # Server reads back data from client.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(self.MAX_NUM_MSG),
            "Server should read successfully.")

    def testSyncQueueClientMultipleRead(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, MultipleRead.
           Server acts as a writer, and client reads the data back in batches.
        """
        chunk_size = 100
        chunk_num = 5
        num_messages = chunk_num * chunk_size
        write_data = generateSequentialData(num_messages)

        # Client has no data to read yet.
        asserts.assertEqual(self._sync_client.availableToRead(), 0)
        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqSync(num_messages),
            "Server should write successfully.")

        # Client reads it back continuously.
        total_read_data = []
        for i in range(chunk_num):
            read_data = []
            asserts.assertTrue(
                self._sync_client.read(read_data, chunk_size),
                "Client should read successfully.")
            total_read_data.extend(read_data)

        # Check read_data and write_data are equal.
        asserts.assertEqual(write_data, total_read_data)

    def testSyncQueueClientMultipleWrite(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, MultipleWrite.
           Client writes the data in batches, and server reads it back together.
        """
        chunk_size = 100
        chunk_num = 5
        num_messages = chunk_num * chunk_size
        write_data = generateSequentialData(num_messages)

        # Client should see an empty queue.
        asserts.assertEqual(self._sync_client.availableToWrite(),
                            self.MAX_NUM_MSG)
        for i in range(chunk_num):  # Client keeps writing.
            curr_write_data = write_data[i * chunk_size:(i + 1) * chunk_size]
            asserts.assertTrue(
                self._sync_client.write(curr_write_data, chunk_size),
                "Client should write successfully.")

        # Server reads data back correctly.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(num_messages),
            "Server should read successfully.")

    def testSyncQueueReadWriteWrapAround(self):
        """This test operates on the synchronized queue.
           Mirrors testcase: SynchronizedReadWriteClient, ReadWriteWrapAround1.
           Client writes half of the queue and server reads back.
           Client writes the max capacity, which will cause a wrap
           around in the queue, server should still read back correctly.
        """
        num_messages = self.MAX_NUM_MSG / 2
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client writes half of the queue capacity, and server reads it.
        asserts.assertTrue(
            self._sync_client.write(write_data, num_messages),
            "Client should write successfully.")
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(num_messages),
            "Server should read successfully.")
        # Client writes the max queue capacity, causes a wrap around
        asserts.assertTrue(
            self._sync_client.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        # Server reads back data correctly
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqSync(self.MAX_NUM_MSG),
            "Server should read successfully.")

    def testUnsyncQueueSmallInputReaderTest1(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, SmallInputReaderTest1.
           Server writes a small number of messages and client reads it back.
        """
        data_len = 16

        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(data_len),
            "Server should write successfully.")
        # Client reads.
        read_data = []
        asserts.assertTrue(
            self._unsync_client1.read(read_data, data_len),
            "Client should read successfully.")
        asserts.assertEqual(read_data, generateSequentialData(16))

    def testUnsyncQueueSmallInputWriterTest1(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, SmallInputWriterTest1.
           Client writes a small number of messages and server reads it back.
        """
        data_len = 16
        write_data = generateSequentialData(16)

        # Client writes.
        asserts.assertTrue(
            self._unsync_client1.write(write_data, data_len),
            "Server should write successfully.")
        # Server reads.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqUnsync(data_len),
            "Client should read successfully.")

    def testUnsyncQueueReadWhenEmpty(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, ReadWhenEmpty.
           Read should fail when queue is empty.
        """
        asserts.assertEqual(self._unsync_client1.availableToRead(), 0)
        asserts.assertFalse(
            self._unsync_client1.read([], 2),
            "Client should fail to read because queue is empty.")

    def testUnsyncQueueWriteWhenFull(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, WriteWhenFull.
           Write should still succeed because unsynchronized queue
           allows overflow. Subsequent read should fail.
        """
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client writes.
        asserts.assertTrue(
            self._unsync_client1.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        asserts.assertEqual(self._unsync_client1.availableToWrite(), 0)
        # Client tries to write more, still succeeds.
        asserts.assertTrue(
            self._unsync_client1.write([1], 1),
            "Client should write successfully "
            + "even if queue is full for unsynchronized queue.")
        # Server should fail because queue overflows.
        asserts.assertFalse(
            self._tests_msgq.requestReadFmqUnsync(self.MAX_NUM_MSG),
            "Server should fail to read because queue overflows.")

    def testUnsyncQueueLargeInputTest1(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, LargeInputTest1.
           Server writes to the queue and client reads the data back.
        """
        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(self.MAX_NUM_MSG),
            "Server should write successfully.")

        write_data = generateSequentialData(self.MAX_NUM_MSG)
        read_data = []
        # Client reads.
        asserts.assertEqual(self._unsync_client1.availableToRead(),
                            self.MAX_NUM_MSG)
        asserts.assertTrue(
            self._unsync_client1.read(read_data, self.MAX_NUM_MSG),
            "Client should read successfully.")
        asserts.assertEqual(write_data, read_data)

    def testUnsyncQueueLargeInputTest2(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, LargeInputTest2.
           Server attempts to write more than the queue capacity and fails.
        """
        asserts.assertEqual(0, self._unsync_client1.availableToRead())
        # Server attempts to write more than the queue capacity.
        asserts.assertFalse(
            self._tests_msgq.requestWriteFmqUnsync(self.MAX_NUM_MSG + 1),
            "Server should fail because it writes more than queue capacity.")
        # Check there is still no data for client.
        asserts.assertEqual(0, self._unsync_client1.availableToRead())
        asserts.assertFalse(
            self._unsync_client1.read([], 1),
            "Client should fail to read because queue is empty.")

    def testUnsyncQueueLargeInputTest3(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, LargeInputTest3.
           Client writes until the queue is full, and writes one more,
           which overflows the queue. Read should fail after that.
           Client writes again, and server should be able to read again.
        """
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client fills up the queue.
        asserts.assertTrue(
            self._unsync_client1.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        asserts.assertEqual(0, self._unsync_client1.availableToWrite())
        # Client attempts to write one more, still succeeds.
        asserts.assertTrue(
            self._unsync_client1.write([1], 1),
            "Client should write successfully "
            + "even if queue is full for unsynchronized queue.")
        # Server fails to read because queue overflows.
        asserts.assertFalse(
            self._tests_msgq.requestReadFmqUnsync(self.MAX_NUM_MSG),
            "Server should fail to read because queue overflows.")

        # Do another interaction, and both should succeed.
        asserts.assertTrue(
            self._unsync_client1.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqUnsync(self.MAX_NUM_MSG),
            "Server should read successfully.")

    def testUnsyncQueueClientMultipleRead(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, MultipleRead.
           Server acts as a writer, and client reads the data back in batches.
        """
        chunk_size = 100
        chunk_num = 5
        num_messages = chunk_num * chunk_size
        write_data = generateSequentialData(num_messages)

        # Client has no data to read yet.
        asserts.assertEqual(self._unsync_client1.availableToRead(), 0)
        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(num_messages),
            "Server should write successfully.")

        # Client reads it back continuously.
        total_read_data = []
        for i in range(chunk_num):
            read_data = []
            asserts.assertTrue(
                self._unsync_client1.read(read_data, chunk_size),
                "Client should read successfully.")
            total_read_data.extend(read_data)

        # Check read_data and write_data are equal.
        asserts.assertEqual(write_data, total_read_data)

    def testUnsyncQueueClientMultipleWrite(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, MultipleWrite.
           Client writes the data in batches, and server reads it back together.
        """
        chunk_size = 100
        chunk_num = 5
        num_messages = chunk_num * chunk_size
        write_data = generateSequentialData(num_messages)

        # Client should see an empty queue.
        asserts.assertEqual(self._unsync_client1.availableToWrite(),
                            self.MAX_NUM_MSG)
        for i in range(chunk_num):  # Client keeps writing.
            curr_write_data = write_data[i * chunk_size:(i + 1) * chunk_size]
            asserts.assertTrue(
                self._unsync_client1.write(curr_write_data, chunk_size),
                "Client should write successfully.")

        # Server reads data back correctly.
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqUnsync(num_messages),
            "Server should read successfully.")

    def testUnsyncQueueReadWriteWrapAround(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient, ReadWriteWrapAround.
           Client writes half of the queue and server reads back.
           Client writes the max capacity, which will cause a wrap
           around in the queue, server should still read back correctly.
        """
        num_messages = self.MAX_NUM_MSG / 2
        write_data = generateSequentialData(self.MAX_NUM_MSG)

        # Client writes half of the queue capacity, and server reads it.
        asserts.assertTrue(
            self._unsync_client1.write(write_data, num_messages),
            "Client should write successfully.")
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqUnsync(num_messages),
            "Server should read successfully.")
        # Client writes the max queue capacity, causes a wrap around
        asserts.assertTrue(
            self._unsync_client1.write(write_data, self.MAX_NUM_MSG),
            "Client should write successfully.")
        # Server reads back data correctly
        asserts.assertTrue(
            self._tests_msgq.requestReadFmqUnsync(self.MAX_NUM_MSG),
            "Server should read successfully.")

    def testUnsyncQueueSmallInputMultipleReaderTest(self):
        """This test operates on the unsynchronized queue.
           Mirrors testcase: UnsynchronizedWriteClient,
                             SmallInputMultipleReaderTest.
           Server writes once, and two readers read the data back separately.
        """
        data_len = 16
        # Server writes.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(data_len),
            "Server should write successfully.")
        write_data = generateSequentialData(data_len)

        # Client 1 reads back data correctly.
        read_data1 = []
        asserts.assertTrue(
            self._unsync_client1.read(read_data1, data_len),
            "Client 1 should read successfully.")
        asserts.assertEqual(write_data, read_data1)
        # Client 2 reads back data correctly.
        read_data2 = []
        asserts.assertTrue(
            self._unsync_client2.read(read_data2, data_len),
            "Client 2 should read successfully.")
        asserts.assertEqual(write_data, read_data2)

    def testUnsyncQueueOverflowNotificationTest(self):
        """This test operates on the unsynchronized queue.
           Mirror testcase: UnsynchronizedWriteClient, OverflowNotificationTest.
           For unsynchronized queue, multiple readers can recover from
           a write overflow condition.
        """
        # Server writes twice, overflows the queue.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(self.MAX_NUM_MSG),
            "Server should write successfully.")
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(self.MAX_NUM_MSG),
            "Server should write successfully even if queue overflows.")

        # Both clients fail to read.
        read_data1 = []
        read_data2 = []
        asserts.assertFalse(
            self._unsync_client1.read(read_data1, self.MAX_NUM_MSG),
            "Client 1 should fail to read because queue overflows.")
        asserts.assertFalse(
            self._unsync_client2.read(read_data2, self.MAX_NUM_MSG),
            "Client 2 should fail to read because queue overflows.")

        # Server writes the data again.
        asserts.assertTrue(
            self._tests_msgq.requestWriteFmqUnsync(self.MAX_NUM_MSG),
            "Server should write successfully.")
        # Both clients should be able to read.
        asserts.assertTrue(
            self._unsync_client1.read(read_data1, self.MAX_NUM_MSG),
            "Client 1 should read successfully.")
        asserts.assertTrue(
            self._unsync_client2.read(read_data2, self.MAX_NUM_MSG),
            "Client 2 should read successfully.")


def generateSequentialData(data_size):
    """Util method to generate sequential data from 0 up to MAX_NUM_MSG.
       We use this method because in ITestMsgQ hal server, it always assumes
       writing and reading sequential data from 0 up to MAX_NUM_MSG.

    Args:
        MAX_NUM_MSG: int, length of the result list.

    Returns:
        int list, list of integers.
    """
    return [i for i in range(data_size)]


if __name__ == "__main__":
    test_runner.main()
