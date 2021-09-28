# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_SecondaryGoogleAccountSigninAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the SecondaryGoogleAccountSigninAllowed policy in Chrome OS.
    If the policy is set to True/Not Set then users can sign in to Chrome
    with multiple accounts. If the policy is set to False then users won't
    be given an option to sign in with more than one account.

    """
    version = 1
    ACC_REGEX = '/Google Account/'
    ADD_ACCOUNT_REGEX = '/Add account/'
    VIEW_ACCOUNTS = "View accounts"

    def _check_secondary_login(self, case):
        """
        Open a new tab and try using the omnibox as a search box.

        @param case: policy value.

        """
        self.cr.browser.tabs[0].Navigate('chrome://settings/people')

        utils.poll_for_condition(
            lambda: self.ui.item_present(role='button',
                                         name=self.ACC_REGEX,
                                         isRegex=True),
            exception=error.TestError('Test page is not ready.'),
            timeout=20,
            sleep_interval=1)

        self.ui.doDefault_on_obj(name=self.ACC_REGEX,
                                 isRegex=True)
        try:
            self.ui.wait_for_ui_obj(self.ADD_ACCOUNT_REGEX, isRegex=True)
        except:
            if self.ui.item_present(name=self.VIEW_ACCOUNTS):
                self.ui.doDefault_on_obj(name=self.VIEW_ACCOUNTS)
                self.ui.wait_for_ui_obj(self.ADD_ACCOUNT_REGEX, isRegex=True)

        add_accounts_blocked = self.ui.is_obj_restricted(
            name=self.ADD_ACCOUNT_REGEX,
            isRegex=True,
            role="button")

        if case is False and not add_accounts_blocked:
            raise error.TestError(
                "Should not be able to sign into second account but can.")
        elif case is not False and add_accounts_blocked:
            raise error.TestError(
                "Should be able to sign into second account but cannot.")

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'SecondaryGoogleAccountSigninAllowed': case}
        self.setup_case(user_policies=POLICIES, real_gaia=True)
        self.ui.start_ui_root(self.cr)
        self._check_secondary_login(case)
