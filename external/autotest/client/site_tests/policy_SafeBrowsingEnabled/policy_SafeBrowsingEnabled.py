# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_SafeBrowsingEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the SafeBrowsingEnabled policy in Chrome OS.

    If the policy is set to True then Safety Browsing will be enabled.
    If the policy is set to False then Safety Browsing will be disabled.
    If the policy is set to None then Safety Browsing will be enabled.

    """
    version = 1
    safety_browsing_test_page = 'chrome://safe-browsing/'
    safety_browsing_enabled = "Enabled: safebrowsing.enabled"
    safety_browsing_disabled = "Disabled: safebrowsing.enabled"


    def _check_safety_browsing_page(self, case):
        """
        Opens a new chrome://safe-browsing/ page and checks the settings for
        the Safety Browsing mode.

        @param case: policy value.

        """
        active_tab = self.navigate_to_url(self.safety_browsing_test_page)
        page_scrape_cmd = (
            'document.getElementById("preferences-list").'
            'children[0].innerText;')
        utils.poll_for_condition(
            lambda: self.check_page_readiness(
                active_tab, page_scrape_cmd),
            exception=error.TestFail('Page is not ready.'),
            timeout=5,
            sleep_interval=1)
        safety_status = active_tab.EvaluateJavaScript(page_scrape_cmd)

        if case == True or case == None:
            if safety_status != self.safety_browsing_enabled:
                raise error.TestFail('Safety Browsing is disabled'
                                     ' but should be enabled.')
        else:
            if safety_status != self.safety_browsing_disabled:
                raise error.TestFail('Safety Browsing is enabled'
                                     ' but should be disabled.')


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'SafeBrowsingEnabled': case}
        self.setup_case(user_policies=POLICIES)
        self._check_safety_browsing_page(case)
