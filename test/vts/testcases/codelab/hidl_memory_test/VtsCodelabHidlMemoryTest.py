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

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner
from vts.proto import ComponentSpecificationMessage_pb2 as CompSpecMsg


class VtsCodelabHidlMemoryTest(base_test.BaseTestClass):
    """Test hidl_memory APIs from host side to target side and HAL server.

    This class tests APIs to operate on memory object from host side.
    It also tests sending memory objects (including argument and return values)
    in HAL servers.
    We use a test HAL service IMemoryTest.hal in this script.

    Attributes:
        MAX_RETRY: int, maximum times to check if HAL server has started.
                   If server is still not started after MAX_RETRY times,
                   stop the testcase.
        SERVICE_NAME: string, script to start the memory test HAL server.
        COMMAND_32: string, command to start the HAL server in a 32-bit device.
        COMMAND_64: string, command to start the HAL server in a 64-bit device.
        CHECK_COMMAND: string, command to check if memory HAL service
                       has started.
        MEM_VAL: int, data to fill the memory with.
        MEM_SIZE: int, size of the memory region allocated on the target side.
        _mem_obj: memory mirror object that users can use to read from or
                  write into.
        _tests_memory: HAL server instance that users can directly call API on.
    """
    # Some constants.
    MAX_RETRY = 3
    SERVICE_NAME = "hidl_test_servers"
    COMMAND_32 = "/data/nativetest/" + SERVICE_NAME + "/" + SERVICE_NAME + " &"
    COMMAND_64 = "/data/nativetest64/" + SERVICE_NAME + "/" + SERVICE_NAME + " &"
    CHECK_COMMAND = "lshal --types=b | grep \"android.hardware.tests.memory@1.0\""
    MEM_SIZE = 1024

    def setUpClass(self):
        """Do necessary setup including starting the HAL service.

        This method starts the HAL service and loads it in the target-side
        driver. It also initializes a memory object on the target side, and
        users can use it for reading and writing.
        """
        self.dut = self.android_devices[0]
        # Send command to start the HAL server.
        if int(self.abi_bitness) == 64:
            self.dut.shell.Execute(self.COMMAND_64)
        else:
            self.dut.shell.Execute(self.COMMAND_32)

        # Wait until service is started.
        # Retry at most three times.
        start_hal_success = False
        for _ in range(self.MAX_RETRY):
            result = self.dut.shell.Execute(self.CHECK_COMMAND)
            if result[const.STDOUT][0] != "":
                start_hal_success = True  # setup successful
                break
            time.sleep(1)  # wait one second.
        # memory HAL service is still not started after waiting for
        # self.MAX_RETRY times, stop the testcase.
        if not start_hal_success:
            logging.error("Failed to start hidl_memory HAL service.")
            return False

        # Load a hidl_memory test hal driver.
        self.dut.hal.InitHidlHal(
            target_type="tests_memory",
            target_basepaths=self.dut.libPaths,
            target_version_major=1,
            target_version_minor=0,
            target_package="android.hardware.tests.memory",
            target_component_name="IMemoryTest",
            hw_binder_service_name="memory",
            bits=int(self.abi_bitness),
            is_test_hal=True)
        # Create a shortcut for HAL server.
        self._tests_memory = self.dut.hal.tests_memory

        # Create a memory object on the client side.
        self._mem_obj = self.dut.resource.InitHidlMemory(
            mem_size=self.MEM_SIZE,
            client=self.dut.hal.GetTcpClient("tests_memory"))

        # Do necessary super class setup.
        super(VtsCodelabHidlMemoryTest, self).setUpClass()

    def testGetSize(self):
        """Test getting memory size correctness. """
        asserts.assertEqual(self.MEM_SIZE, self._mem_obj.getSize())

    def testSimpleWriteRead(self):
        """Test writing to the memory and reading the same data back. """
        write_data = "abcdef"
        # Write data into memory.
        self._mem_obj.update()
        self._mem_obj.updateBytes(write_data, len(write_data))
        self._mem_obj.commit()

        # Read data from memory.
        self._mem_obj.read()
        read_data = self._mem_obj.readBytes(len(write_data))
        asserts.assertEqual(write_data, read_data)

    def testLargeWriteRead(self):
        """Test consecutive writes and reads using integers.

        For each of the 5 iterations, write 5 integers into
        different chunks of memory, and read them back.
        """
        for i in range(5):
            # Writes five integers.
            write_data = [random.randint(0, 100) for j in range(10)]
            write_data_str = str(bytearray(write_data))
            # Start writing at offset i * 5.
            self._mem_obj.updateRange(i * 5, len(write_data_str))
            self._mem_obj.updateBytes(write_data_str, len(write_data_str),
                                      i * 5)
            self._mem_obj.commit()

            # Reads data back.
            self._mem_obj.readRange(i * 5, len(write_data_str))
            read_data_str = self._mem_obj.readBytes(len(write_data_str), i * 5)
            read_data = list(bytearray(read_data_str))
            # Check if read data is correct.
            asserts.assertEqual(write_data, read_data)

    def testWriteTwoRegionsInOneBuffer(self):
        """Test writing into different regions in one memory buffer.

        Writer requests the beginning of the first half and
        the beginning of the second half of the buffer.
        It writes to the second half, commits, and reads the data back.
        Then it writes to the first half, commits, and reads the data back.
        """
        write_data1 = "abcdef"
        write_data2 = "ghijklmno"

        # Reserve both regions.
        self._mem_obj.updateRange(0, len(write_data1))
        self._mem_obj.updateRange(self.MEM_SIZE / 2, len(write_data2))
        # Write to the second region.
        self._mem_obj.updateBytes(write_data2, len(write_data2),
                                  self.MEM_SIZE / 2)
        self._mem_obj.commit()

        # Read from the second region.
        self._mem_obj.read()
        read_data2 = self._mem_obj.readBytes(
            len(write_data2), self.MEM_SIZE / 2)
        self._mem_obj.commit()
        # Check if read data is correct.
        asserts.assertEqual(read_data2, write_data2)

        # Write to the first region.
        self._mem_obj.updateBytes(write_data1, len(write_data1))
        self._mem_obj.commit()

        # Read from the first region.
        self._mem_obj.read()
        read_data1 = self._mem_obj.readBytes(len(write_data1))
        self._mem_obj.commit()
        # Check if read data is correct.
        asserts.assertEqual(write_data1, read_data1)

    def testSharedMemory(self):
        """Test HAL server API fillMemory(), which takes in hidl_memory.

        The memory filled out by the HAL server should be seen by the client.
        """
        # Zero out memory.
        self._mem_obj.update()
        self._mem_obj.updateBytes("\0" * self.MEM_SIZE, self.MEM_SIZE)
        self._mem_obj.commit()

        # Tell HAL server to fill the memory, should be seen by the client,
        # because they use shared memory.
        var_msg = self.prepareHidlMemoryArgument(self._mem_obj.memId)
        # Fill the memory with each byte be integer value 42.
        self._tests_memory.fillMemory(var_msg, 42)
        self._mem_obj.read()
        read_data = self._mem_obj.readBytes(self.MEM_SIZE)
        self._mem_obj.commit()

        # Convert read_data to a list of chars, and convert them back to int8_t.
        read_data = list(read_data)
        read_data = map(lambda i: ord(i), read_data)

        # Check read data.
        asserts.assertEqual(read_data, [42] * self.MEM_SIZE)

    def testSameMemoryDifferentId(self):
        """Test HAL server API haveSomeMemory(), which returns hidl_memory.

        haveSomeMemory() returns input memory directly.
        We pass the client memory object to this API, and the return value
        of this API should be registered in our target-side drivers as well.
        The returned memory object is basically identical to the first memory
        object held by the client.
        They have different id, but they share the same memory region.
        """
        # Prepare a hidl_memory argument.
        var_msg = self.prepareHidlMemoryArgument(self._mem_obj.memId)

        # memory_client2 is identical to self._mem_obj because
        # haveSomeMemory() API in the server just returns the input
        # hidl_memory reference directly.
        memory_client2 = self._tests_memory.haveSomeMemory(var_msg)
        asserts.assertNotEqual(memory_client2, None)
        asserts.assertNotEqual(memory_client2.memId, -1)
        # Check these two clients are stored as different objects.
        asserts.assertNotEqual(memory_client2.memId,
                               self._mem_obj.memId)

        # Fill the memory with each byte being integer value 50.
        self._tests_memory.fillMemory(var_msg, 50)

        # Both clients read.
        self._mem_obj.read()
        read_data1 = self._mem_obj.readBytes(self.MEM_SIZE)
        self._mem_obj.commit()
        memory_client2.read()
        read_data2 = memory_client2.readBytes(self.MEM_SIZE)
        memory_client2.commit()

        # Convert read_data to a list of chars, and convert them back to int8_t.
        read_data1 = list(read_data1)
        read_data1 = map(lambda i: ord(i), read_data1)
        read_data2 = list(read_data2)
        read_data2 = map(lambda i: ord(i), read_data2)

        # Check read data.
        asserts.assertEqual(read_data1, [50] * self.MEM_SIZE)
        asserts.assertEqual(read_data2, [50] * self.MEM_SIZE)

    @staticmethod
    def prepareHidlMemoryArgument(mem_id):
        """Prepare VariableSpecificationMessage containing hidl_memory argument.

        Args:
            mem_id: int, identifies the memory object.

        Returns:
            VariableSpecificationMessage, specifies the memory id.
        """
        var_msg = CompSpecMsg.VariableSpecificationMessage()
        var_msg.type = CompSpecMsg.TYPE_HIDL_MEMORY
        var_msg.hidl_memory_value.mem_id = mem_id
        return var_msg


if __name__ == "__main__":
    test_runner.main()
