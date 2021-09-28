# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import retry

from autotest_lib.client.cros import cryptohome
from autotest_lib.client.cros.enterprise import enterprise_policy_base

from py_utils import TimeoutException

class policy_DeviceEphemeralUsersEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test for the DeviceEphemeralUsersEnabled policy."""
    version = 1
    _POLICY = 'DeviceEphemeralUsersEnabled'

    def verify_permanent_vault(self, case):
        if case and cryptohome.is_permanent_vault_mounted(
            user=enterprise_policy_base.USERNAME):
                raise error.TestFail(
                    'User should not be permanently vaulted in '
                    'Ephemeral mode.')

        if not case:
            cryptohome.is_permanent_vault_mounted(
                user=enterprise_policy_base.USERNAME, allow_fail=True)

    # Prevents client tests that are kicked off via server tests from flaking.
    @retry.retry(TimeoutException, timeout_min=5, delay_sec=10)
    def _run_setup_case(self, case):
        self.setup_case(device_policies={self._POLICY: case}, enroll=True)

    def run_once(self, case):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self._run_setup_case(case)
        self.verify_permanent_vault(case)