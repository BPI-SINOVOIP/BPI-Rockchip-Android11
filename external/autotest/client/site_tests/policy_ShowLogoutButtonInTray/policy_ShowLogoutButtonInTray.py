# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ShowLogoutButtonInTray(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the ShowLogoutButtonInTray policy in Chrome OS.
    If the policy is set to True then the user can see the Sign Out button
    on the task bar. If the policy is set to False/Not Set then users won't
    see the Sign out button in the task bar.

    """
    version = 1

    def _check_logout_button_in_tray(self, case):
        """
        Check for the sign out button in the task bar.

        @param case: policy value.

        """
        logout_button = self.ui.item_present('Sign out')

        if case is True:
            if not logout_button:
                raise error.TestFail(
                    'Logout button is not present in tray and it should be.')

        else:
            if logout_button:
                raise error.TestFail(
                    'Logout button is present in tray and it should not be.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'ShowLogoutButtonInTray': case}
        self.setup_case(user_policies=POLICIES)
        self.ui.start_ui_root(self.cr)
        self._check_logout_button_in_tray(case)
