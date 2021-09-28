#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import time

from host_controller import common
from host_controller.utils.ipc import file_lock

MIN_SLEEP_TIME_IN_SECS = 1
MAX_SLEEP_TIME_IN_SECS = 60


class FileLockSemaphore(file_lock.FileLock):
    """Class for system-wide semaphores for inter process-group sync.

    Inherited FileLock to make the decrement and increment operation atomic,
    thus any read and write operations to the file must be leaded by Lock()
    and be followed by Unlock() operations, defined in FileLock

    Attributes:
        _name: string, name of the file used as semaphore.
    """

    def __init__(self, name):
        super(FileLockSemaphore, self).__init__(name, "r+")
        self._name = name
        self._InitSemaphore()

    def Acquire(self):
        """P() operation for the FileLockSemaphore.

        To minimize the starvation, the sleep time will decrease
        for the processes that have waited longer than the others.
        """
        sleep_time = MAX_SLEEP_TIME_IN_SECS
        while self.LockDevice(self._name, True, True):
            if self.sem_value > 0:
                break
            self.UnlockDevice(self._name)
            time.sleep(sleep_time)
            sleep_time = max(sleep_time / 2, MIN_SLEEP_TIME_IN_SECS)

        self._Decrement()
        self.UnlockDevice(self._name)

    def Release(self):
        """V() operation for the FileLockSemaphore."""
        self.LockDevice(self._name, True, True)
        self._Increment()
        self.UnlockDevice(self._name)

    def _InitSemaphore(self):
        """Initializes the file content for semaphore operations.

        The value of the counter must remain in the range
        [0, common.MAX_ADB_FASTBOOT_PROCESS] inclusive.
        """
        self.LockDevice(self._name, True, True)

        try:
            diff = common.MAX_ADB_FASTBOOT_PROCESS - self.sem_max_value
            value_to_init = min(
                max(0, self.sem_value + diff), common.MAX_ADB_FASTBOOT_PROCESS)
        except IndexError:
            value_to_init = common.MAX_ADB_FASTBOOT_PROCESS

        self._lock_fd[self._name].seek(0)
        self._lock_fd[self._name].truncate(0)
        self._lock_fd[self._name].write(
            "%s\n%s" % (common.MAX_ADB_FASTBOOT_PROCESS, value_to_init))

        self.UnlockDevice(self._name)

    @property
    def sem_value(self):
        """Reads the current counter value of the semaphore.

        Returns:
            int, the value of the current counter.
        """
        self._lock_fd[self._name].seek(0)
        return int(self._lock_fd[self._name].read().split()[1])

    @property
    def sem_max_value(self):
        """Reads the current maximum counter value of the semaphore.

        Since the maximum counter value may vary at the deployment time,
        the existing HC process group needs to look for the maximum value
        every time it tries to access the semaphore

        Returns:
            int, the value of the maximum counter.
        """
        self._lock_fd[self._name].seek(0)
        return int(self._lock_fd[self._name].read().split()[0])

    def _Decrement(self):
        """Decrements the internal counter of the semaphore."""
        current_value = self.sem_value
        current_max = self.sem_max_value
        self._lock_fd[self._name].seek(len(str(current_max)) + 1)
        self._lock_fd[self._name].write("%s" % max(0, (current_value - 1)))

    def _Increment(self):
        """Increments the internal counter of the semaphore."""
        current_value = self.sem_value
        current_max = self.sem_max_value
        self._lock_fd[self._name].seek(len(str(current_max)) + 1)
        self._lock_fd[self._name].write("%s" % min(current_max,
                                                   (current_value + 1)))
