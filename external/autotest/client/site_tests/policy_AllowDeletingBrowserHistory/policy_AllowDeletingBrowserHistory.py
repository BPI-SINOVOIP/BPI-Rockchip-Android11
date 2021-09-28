# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_AllowDeletingBrowserHistory(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the AllowDeletingBrowserHistory policy in Chrome OS.
    If the policy is set to True/Not Set then the user will be able to delete
    browse and download histories.
    If the policy is set to False then the user will not be able to
    delete browse and download histories.

    """
    version = 1

    def _check_safety_browsing_page(self, case):
        """
        Opens a new chrome://settings/clearBrowserData page and checks
        if the browse and download histories are deletable.

        @param case: policy value.

        """
        self.navigate_to_url("chrome://settings/clearBrowserData")
        self.ui.start_ui_root(self.cr)
        self.ui.wait_for_ui_obj('Advanced')

        self.ui.doDefault_on_obj('Advanced')
        self.ui.wait_for_ui_obj('/Browsing history/',
                                isRegex=True,
                                role='checkBox')

        button_disabled = self.ui.is_obj_restricted(
            '/Browsing history/',
            isRegex=True,
            role='checkBox')
        if case is False and not button_disabled:
            raise error.TestFail('User is able to delete history.')
        elif case is not False and button_disabled:
            raise error.TestFail('User is unable to delete history.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'AllowDeletingBrowserHistory': case}
        self.setup_case(user_policies=POLICIES)
        self._check_safety_browsing_page(case)
