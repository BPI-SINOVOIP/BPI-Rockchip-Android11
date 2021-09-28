# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros import retry

from autotest_lib.client.cros.enterprise import enterprise_policy_base

from py_utils import TimeoutException


class policy_DeviceWilcoDtcAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test for the DeviceWilcoDtcAllowed policy.

    If the policy is set to True then Wilco daemons should be running.
    If the policy is set to False/None then Wilco daemons should not be
    running.

    """
    version = 1
    _POLICY = 'DeviceWilcoDtcAllowed'

    @retry.retry(TimeoutException, timeout_min=5, delay_sec=10)
    def _run_setup_case(self, case):
        self.setup_case(device_policies={self._POLICY: case}, enroll=True,
            extra_chrome_flags=['--user-always-affiliated'])

    def _update_policy_value(self, value):
        self.update_policies(device_policies={'DeviceWilcoDtcAllowed': value})
        self.verify_policy_value('DeviceWilcoDtcAllowed', value)

    def _check_status(self, expected_status):
        result = utils.run('status wilco_dtc')

        if expected_status:
            if 'process' not in result.stdout:
                raise error.TestFail(
                    'Wilco daemons are not running and they should be.')
        else:
            if 'process' in result.stdout:
                raise error.TestFail(
                    'Wilco daemons are running and they should not be.')


    def run_once(self, case):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self._run_setup_case(case)

        if case is True:
            self._check_status(case)
            self._update_policy_value(False)
            # This sleep is needed to give status some time to update.
            time.sleep(2)
            self._check_status(False)

        if case is False:
            self._check_status(case)
            self._update_policy_value(True)
            # This sleep is needed to give status some time to update.
            time.sleep(2)
            self._check_status(True)

        if case is None:
            self._check_status(None)
