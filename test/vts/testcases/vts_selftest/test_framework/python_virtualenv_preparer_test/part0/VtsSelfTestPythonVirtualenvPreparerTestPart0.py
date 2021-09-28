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

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import test_runner


class VtsSelfTestPythonVirtualenvPreparerTestPart0(base_test.BaseTestClass):
    '''Tests plan and module level VirtualenvPreparer.'''

    def setUpClass(self):
        # Since we are running the actual test cases, run_as_vts_self_test
        # must be set to False.
        self.run_as_vts_self_test = False

    def testNonExistingModule(self):
        '''Test whether numpy is not installed from default packages.

        This test assumes numpy is not in default package install list.
        If this turned otherwise, test logic here should be updated.
        '''
        try:
            import numpy
            asserts.fail('numpy should not have been not installed yet.')
        except ImportError:
            pass


if __name__ == "__main__":
    test_runner.main()
