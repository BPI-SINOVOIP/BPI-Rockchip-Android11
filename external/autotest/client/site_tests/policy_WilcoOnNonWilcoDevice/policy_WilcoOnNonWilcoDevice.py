# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_WilcoOnNonWilcoDevice(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test for looping through Wilco policies on a non wilco device.

    Setting Wilco policies on a non Wilco device should not cause a crash.

    """
    version = 1

    def _run_setup_case(self, tests):
        self.setup_case(
            device_policies={
                tests[0]['Policy_Name']: tests[0]['Policy_Value']},
            enroll=True,
            extra_chrome_flags=['--user-always-affiliated'])

    def run_once(self, tests):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self._run_setup_case(tests)
        tests.pop(0)
        for test in tests:
            self.update_policies(
                device_policies={test['Policy_Name']: test['Policy_Value']})
            self.verify_policy_value(test['Policy_Name'], test['Policy_Value'])
