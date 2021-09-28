#!/usr/bin/env python
#
# Copyright (C) 2019 The Android Open Source Project
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

"""A test to check if dynamic partitions is enabled for new devices in Q."""

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import test_runner

# The property to indicate dynamic partitions are enabled.
_DYNAMIC_PARTITIONS_PROP = "ro.boot.dynamic_partitions"


class VtsKernelDynamicPartitionsTest(base_test.BaseTestClass):
    """A test class to verify dynamic partitions are enabled."""

    def setUpClass(self):
        """Initializes device and shell."""
        self._dut = self.android_devices[0]

    def testDynamicPartitionsSysProp(self):
        """Checks ro.build.dynamic_partitions is 'true'."""
        asserts.assertEqual("true",
                            self._dut.getProp(_DYNAMIC_PARTITIONS_PROP),
                            "%s is not true" % _DYNAMIC_PARTITIONS_PROP)

if __name__ == "__main__":
    test_runner.main()
