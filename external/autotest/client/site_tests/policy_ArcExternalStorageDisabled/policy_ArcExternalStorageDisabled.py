# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import arc
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ArcExternalStorageDisabled(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1

    POLICY_NAME = 'ExternalStorageDisabled'
    TEST_CASES = {
        'True_Block': True,
        'False_Allow': False,
        'NotSet_Allow': None
    }

    def _test_arc_external_storage(self, policy_value):
        """
        Verify the behavior of the ExternalStorageDisabled policy on ARC.

        Check the /storage directory and verify that it is empty if the
        policy disables access to external storage, or not empty if external
        storage is allowed.

        @param policy_value: policy value for this case.

        @raises error.TestFail: If the contents of the /media/removable
            directory do not match the policy behavior.

        """

        arc_dirs = set(arc.adb_shell('ls /storage').split())

        base_dirs = set(['emulated', 'self', 'MyFiles'])

        usb_parts = arc_dirs - base_dirs
        if policy_value:
            if usb_parts:
                raise error.TestFail('External storage was disabled but '
                                     'external storage detected')
        elif not usb_parts:
            raise error.TestFail('External storage enabled but external '
                                 'storage not found')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        pol = {'ArcEnabled': True,
               'ExternalStorageDisabled': case_value}
        self.setup_case(user_policies=pol,
                        arc_mode='enabled',
                        use_clouddpc_test=False)
        self._test_arc_external_storage(case_value)
