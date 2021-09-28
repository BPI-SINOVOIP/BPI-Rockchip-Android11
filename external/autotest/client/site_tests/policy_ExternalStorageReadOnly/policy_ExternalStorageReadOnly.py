# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ExternalStorageReadOnly(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1

    POLICY_NAME = 'ExternalStorageReadOnly'
    TEST_CASES = {
        'True_Block': True,
        'False_Allow': False,
        'NotSet_Allow': None
    }

    TEST_DIR = os.path.join(os.sep, 'media', 'removable', 'STATE')
    TEST_FILE = os.path.join(TEST_DIR, 'test')

    def cleanup(self):
        """Delete the test file, if it was created."""
        try:
            os.remove(self.TEST_FILE)
        except OSError:
            # The remove call fails if the file isn't created, but that's ok.
            pass

        super(policy_ExternalStorageReadOnly, self).cleanup()

    def _test_external_storage(self, policy_value):
        """
        Verify the behavior of the ExternalStorageReadOnly policy.

        Attempt to create TEST_FILE on the external storage. This should fail
        if the policy is set to True and succeed otherwise.

        @param policy_value: policy value for this case.

        @raises error.TestFail: If the permissions of the /media/removable
            directory do not match the policy behavior.

        """
        # Attempt to modify the external storage by creating a file.
        if not os.path.isdir(self.TEST_DIR):
            raise error.TestWarn('USB Missing. Exiting')
        if os.path.isfile(self.TEST_FILE):
            raise error.TestWarn('Test file existed prior to test.')
        utils.run('touch %s' % self.TEST_FILE, ignore_status=True)
        if policy_value and os.path.isfile(self.TEST_FILE):
            raise error.TestFail('External storage set to read-only but '
                                 'was able to write to storage.')
        elif not policy_value and not os.path.isfile(self.TEST_FILE):
            raise error.TestFail('External storage not read-only but '
                                 'unable to write to storage.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.setup_case(user_policies={self.POLICY_NAME: case_value})
        self._test_external_storage(case_value)
