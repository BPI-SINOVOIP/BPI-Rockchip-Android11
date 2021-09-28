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

import os
import threading
import time
import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from host_controller import common
from host_controller.utils.ipc import file_lock_semaphore


class FileLockSemaphoreTest(unittest.TestCase):
    """Tests for FileLockSemaphore"""

    def setUp(self):
        self.sem = file_lock_semaphore.FileLockSemaphore("fls_test")

    def tearDown(self):
        os.remove(os.path.join(os.path.expanduser("~"), common._DEVLOCK_DIR, "fls_test"))

    def testFileLockSemaphoreAcquire(self):
        current_sem_value = self.sem.sem_value
        self.sem.Acquire()
        self.assertEqual(current_sem_value - 1, self.sem.sem_value)
        self.sem.Release()

    def testFileLockSemaphoreRelease(self):
        self.sem.Acquire()
        current_sem_value = self.sem.sem_value
        self.sem.Release()
        self.assertEqual(current_sem_value + 1, self.sem.sem_value)

    def testFileLockSemaphoreOverflow(self):
        current_sem_value = self.sem.sem_value
        self.sem.Release()
        self.assertEqual(current_sem_value, self.sem.sem_value)

    def _AcquireRelease(self):
        self.sem.Acquire()
        self.assertEqual(self.sem.sem_value, 0)
        self.sem.Release()

    def testFileLockSemaphoreUnderflow(self):
        for _ in range(common.MAX_ADB_FASTBOOT_PROCESS):
            self.sem.Acquire()

        file_lock_semaphore.MAX_SLEEP_TIME_IN_SECS = 2
        self._thread = threading.Thread(target=self._AcquireRelease)
        self._thread.daemon = True
        self._thread.start()

        time.sleep(1)
        self.sem.Release()
        self._thread.join()


if __name__ == "__main__":
    unittest.main()
