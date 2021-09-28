# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.enterprise import enterprise_policy_base

HISTORY_PAGE = "['#history-app', '#history', '#no-results']"


class policy_SavingBrowserHistoryDisabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the SavingBrowserHistoryDisabled policy in Chrome OS.

    If the SavingBrowserHistoryDisabled is enabled then browsing history
    should not be recorded. If SavingBrowserHistoryDisabled is disabled or
    not set then history should be recorded.

    """
    version = 1

    POLICY_NAME = 'SavingBrowserHistoryDisabled'
    TEST_CASES = {
        'Enabled': True,
        'Disabled': False,
        'NotSet': None}


    def _check_browser_history(self, case_value):
        """
        Checks the browser history page to verify if history was saved.

        @case_value: case value for this case.

        """
        # Adding the URL below to browser's history.
        self.navigate_to_url("http://www.google.com")

        active_tab = self.navigate_to_url("chrome://history")
        history_page_content = utils.shadowroot_query(
            HISTORY_PAGE, "outerHTML")
        utils.poll_for_condition(
            lambda: self.check_page_readiness(active_tab, history_page_content),
            exception=error.TestFail('Page is not ready.'),
            timeout=5,
            sleep_interval=1)
        content = active_tab.EvaluateJavaScript(history_page_content)

        if case_value:
            if "hidden" in content:
                raise error.TestFail("History is not empty and it should be.")
        else:
            if "hidden" not in content:
                raise error.TestFail("History is empty and it should not be.")


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.POLICIES = {self.POLICY_NAME: self.TEST_CASES[case]}
        self.setup_case(user_policies=self.POLICIES)
        self._check_browser_history(self.TEST_CASES[case])
