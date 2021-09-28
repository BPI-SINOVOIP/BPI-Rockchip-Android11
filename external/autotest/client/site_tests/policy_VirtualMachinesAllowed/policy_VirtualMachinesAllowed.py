# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import retry

from autotest_lib.client.cros.enterprise import enterprise_policy_base

from py_utils import TimeoutException


class policy_VirtualMachinesAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test for the VirtualMachinesAllowed policy.

    If the policy is set to True then installing linux is allowed.
    If the policy is set to False/None then installing linux is not allowed.

    """
    version = 1
    _POLICY = 'VirtualMachinesAllowed'

    @retry.retry(TimeoutException, timeout_min=5, delay_sec=10)
    def _run_setup_case(self, case):
        self.setup_case(device_policies={self._POLICY: case}, enroll=True)

    def run_once(self, case):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self._run_setup_case(case)
        self.ui.start_ui_root(self.cr)

        self.cr.autotest_ext.ExecuteJavaScript('''
            chrome.autotestPrivate.runCrostiniInstaller(function() {})
        ''')

        if case:
            self.ui.wait_for_ui_obj(name='/Linux/', isRegex=True)
        else:
            if not self.ui.did_obj_not_load(name='/Linux/', isRegex=True):
                raise error.TestFail(
                    'Linux is installing and it should not be.')
