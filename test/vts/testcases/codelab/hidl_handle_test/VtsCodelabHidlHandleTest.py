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

from vts.proto import ComponentSpecificationMessage_pb2 as CompSpecMsg
from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner
from vts.utils.python.file import target_file_utils


class VtsCodelabHidlHandleTest(base_test.BaseTestClass):
    """This class tests hidl_handle API calls from host side.

    Attributes:
        TEST_DIR_PATH: string, directory where the test file will be created.
        TEST_FILE_PATH: string, path to the file that host side uses.
        SERVICE_NAME: string, dumpstate HAL full service name.
        START_COMMAND: string, command to start dumpstate HAL service
                       since it is a lazy HAL.
        CHECK_COMMAND: string, command to check if dumpstate HAL service
                       is started.
        MAX_RETRY: int, number of times to check if dumpstate HAL service
                   is started.
        _created: bool, whether we created TEST_DIR_PATH.
        _writer: ResourceHidlHandleMirror, writer who writes to TEST_FILE_PATH.
        _reader: ResourceHidlHandleMirror, reader who reads from TEST_FILE_PATH.
        _dumpstate: dumpstate HAL server instance.
        _permission: SELinux permission, either enforcing or permissive.
                     This needs to set to permissive on the target side
                     when HAL server writes to user-defined destination.
                     This class will restore the permission level after
                     the test finishes.
    """

    TEST_DIR_PATH = "/data/local/tmp/vts_codelab_tmp/"
    TEST_FILE_PATH = TEST_DIR_PATH + "test.txt"
    SERVICE_NAME = "android.hardware.dumpstate@1.0::IDumpstateDevice/default"
    START_COMMAND = "setprop ctl.interface_start " + SERVICE_NAME
    CHECK_COMMAND = "lshal --types=b | grep \"" + SERVICE_NAME + "\""
    MAX_RETRY = 3

    def setUpClass(self):
        """Necessary setup for the test environment.

        We need to start dumpstate HAL service manually because it is a lazy
        HAL service, then load it in our target-side driver.
        Create a tmp directory in /data to create test files in it if the tmp
        directory doesn't exist.
        We also need to set SELinux permission to permissive because
        HAL server and host side are communicating via a file.
        We will recover SELinux permission during tearDown of this class.
        """
        self.dut = self.android_devices[0]
        # Execute shell command to start dumpstate HAL service because
        # it is a lazy HAL service.
        self.dut.shell.Execute(self.START_COMMAND)

        start_hal_success = False
        # Wait until service is started.
        # Retry at most three times.
        for _ in range(self.MAX_RETRY):
            result = self.dut.shell.Execute(self.CHECK_COMMAND)
            if result[const.STDOUT][0] != "":
                start_hal_success = True  # setup successful
                break
            time.sleep(1)  # wait one second.
        # Dumpstate HAL service is still not started after waiting for
        # self.MAX_RETRY times, stop the testcase.
        if not start_hal_success:
            logging.error("Failed to start dumpstate HAL service.")
            return False

        # Initialize a hal driver to start all managers on the target side,
        # not used for other purposes.
        self.dut.hal.InitHidlHal(
            target_type="dumpstate",
            target_basepaths=self.dut.libPaths,
            target_version_major=1,
            target_version_minor=0,
            target_package="android.hardware.dumpstate",
            target_component_name="IDumpstateDevice",
            bits=int(self.abi_bitness))
        # Make a shortcut name for the dumpstate HAL server.
        self._dumpstate = self.dut.hal.dumpstate

        # In order for dumpstate service to write to file, need to set
        # SELinux to permissive.
        permission_result = self.dut.shell.Execute("getenforce")
        self._permission = permission_result[const.STDOUT][0].strip()
        if self._permission == "Enforcing":
            self.dut.shell.Execute("setenforce permissive")

        # Check if a tmp directory under /data exists.
        self._created = False
        if not target_file_utils.Exists(self.TEST_DIR_PATH, self.dut.shell):
            # Create a tmp directory under /data.
            self.dut.shell.Execute("mkdir " + self.TEST_DIR_PATH)
            # Verify it succeeds. Stop test if it fails.
            if not target_file_utils.Exists(self.TEST_DIR_PATH,
                                            self.dut.shell):
                logging.error("Failed to create " + self.TEST_DIR_PATH +
                              " directory. Stopping test.")
                return False
            # Successfully created the directory.
            logging.info("Manually created " + self.TEST_DIR_PATH +
                         " for the test.")
            self._created = True

    def setUp(self):
        """Initialize a writer and a reader for each test case.

        We open the file with w+ in every test case, which will create the file
        if it doesn't exist, or truncate the file if it exists, because some
        test case won't fully read the content of the file, causing dependency
        between test cases.
        """
        self._writer = self.dut.resource.InitHidlHandleForSingleFile(
            self.TEST_FILE_PATH,
            "w+",
            client=self.dut.hal.GetTcpClient("dumpstate"))
        self._reader = self.dut.resource.InitHidlHandleForSingleFile(
            self.TEST_FILE_PATH,
            "r",
            client=self.dut.hal.GetTcpClient("dumpstate"))
        asserts.assertTrue(self._writer is not None,
                           "Writer should be initialized successfully.")
        asserts.assertTrue(self._reader is not None,
                           "Reader should be initialized successfully.")
        asserts.assertNotEqual(self._writer.handleId, -1)
        asserts.assertNotEqual(self._reader.handleId, -1)

    def tearDown(self):
        """Cleanup for each test case.

        This method closes all open file descriptors on target side.
        """
        # Close all file descriptors by calling CleanUp().
        self.dut.resource.CleanUp()

    def tearDownClass(self):
        """Cleanup after all tests.

        Remove self.TEST_FILE_DIR directory if we created it at the beginning.
        If SELinux permission level is changed, restore it here.
        """
        # Delete self.TEST_DIR_PATH if we created it.
        if self._created:
            logging.info("Deleting " + self.TEST_DIR_PATH +
                         " directory created for this test.")
            self.dut.shell.Execute("rm -rf " + self.TEST_DIR_PATH)

        # Restore SELinux permission level.
        if self._permission == "Enforcing":
            self.dut.shell.Execute("setenforce enforcing")

    def testInvalidWrite(self):
        """Test writing to a file with no write permission should fail. """
        read_data = self._reader.writeFile("abc", 3)
        asserts.assertTrue(
            read_data is None,
            "Reader should fail because it doesn't have write permission to the file."
        )

    def testOpenNonexistingFile(self):
        """Test opening a nonexisting file with read-only flag should fail.

        This test case first checks if the file exists. If it exists, it skips
        this test case. If it doesn't, it will try to open the non-existing file
        with 'r' flag.
        """
        if not target_file_utils.Exists(self.TEST_DIR_PATH + "abc.txt",
                                        self.dut.shell):
            logging.info("Test opening a non-existing file with 'r' flag.")
            failed_reader = self.dut.resource.InitHidlHandleForSingleFile(
                self.TEST_DIR_PATH + "abc.txt",
                "r",
                client=self.dut.hal.GetTcpClient("dumpstate"))
            asserts.assertTrue(
                failed_reader is None,
                "Open a non-existing file with 'r' flag should fail.")

    def testSimpleReadWrite(self):
        """Test a simple read/write interaction between reader and writer. """
        write_data = "Hello World!"
        asserts.assertEqual(
            len(write_data), self._writer.writeFile(write_data,
                                                    len(write_data)))
        read_data = self._reader.readFile(len(write_data))
        asserts.assertEqual(write_data, read_data)

    def testLargeReadWrite(self):
        """Test consecutive reads/writes between reader and writer. """
        write_data = "Android VTS"

        for i in range(10):
            asserts.assertEqual(
                len(write_data),
                self._writer.writeFile(write_data, len(write_data)))
            curr_read_data = self._reader.readFile(len(write_data))
            asserts.assertEqual(curr_read_data, write_data)

    def testHidlHandleArgument(self):
        """Test calling APIs in dumpstate HAL server.

        Host side specifies a handle object in resource_manager, ans pass
        it to dumpstate HAL server to write debug message into it.
        Host side then reads part of the debug message.
        """
        # Prepare a VariableSpecificationMessage to specify the handle object
        # that will be passed into the HAL service.
        var_msg = CompSpecMsg.VariableSpecificationMessage()
        var_msg.type = CompSpecMsg.TYPE_HANDLE
        var_msg.handle_value.handle_id = self._writer.handleId

        self._dumpstate.dumpstateBoard(var_msg)
        # Read 1000 bytes to retrieve part of debug message.
        debug_msg = self._reader.readFile(1000)
        logging.info("Below are part of result from dumpstate: ")
        logging.info(debug_msg)
        asserts.assertNotEqual(debug_msg, "")


if __name__ == "__main__":
    test_runner.main()
