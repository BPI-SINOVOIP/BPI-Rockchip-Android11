# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ShowHomeButton(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the ShowHomeButton policy in Chrome OS.

    If the policy is set to True then the user can see the Home button
    in Chrome. If the policy is set to False/Not Set then users won't
    see the Home button in Chrome.

    """
    version = 1

    def _check_home_button(self, case):
        """
        Open a new tab and try using the omnibox as a search box.

        @param case: policy value.

        """
        home_button = self.ui.item_present('Home')

        if case is True:
            if not home_button:
                raise error.TestFail(
                    'Home button is not visible in Chrome and it should be.')

        else:
            if home_button:
                raise error.TestFail(
                    'Home button is visible in Chrome and it should not be.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'ShowHomeButton': case}
        self.setup_case(user_policies=POLICIES)
        self.ui.start_ui_root(self.cr)
        self._check_home_button(case)
