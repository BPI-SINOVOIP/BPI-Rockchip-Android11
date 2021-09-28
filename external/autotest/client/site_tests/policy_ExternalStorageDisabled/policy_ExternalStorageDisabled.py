# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ExternalStorageDisabled(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1

    POLICY_NAME = 'ExternalStorageDisabled'
    TEST_CASES = {
        'True_Block': True,
        'False_Allow': False,
        'NotSet_Allow': None
    }

    def _test_external_storage(self, policy_value):
        """
        Verify the behavior of the ExternalStorageDisabled policy.

        Check the /media/removable directory and verify that it is empty if the
        policy disables access to external storage, or not empty if external
        storage is allowed.

        @param policy_value: policy value for this case.

        @raises error.TestFail: If the contents of the /media/removable
            directory do not match the policy behavior.

        """
        removable_dir = os.listdir(os.path.join(os.sep, 'media', 'removable'))

        if policy_value:
            if removable_dir:
                raise error.TestFail('External storage was disabled but '
                                     'external storage detected')
        elif not removable_dir:
            raise error.TestFail('External storage enabled but external '
                                 'storage not found')


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.setup_case(user_policies={self.POLICY_NAME: case_value})
        self._test_external_storage(case_value)
