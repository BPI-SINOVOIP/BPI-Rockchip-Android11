# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time
import utils

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from telemetry.core.exceptions import EvaluateException


class policy_DefaultJavaScriptSetting(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the DefaultJavaScriptSetting policy in Chrome OS.

    If the DefaultJavaScriptSetting policy is set to Allow, going to the
    local test page should display YES. If the policy is set to Do Not Allow
    going to the local test page should display NO.

    """
    version = 1

    def initialize(self, **kwargs):
        """Initialize this test."""
        super(policy_DefaultJavaScriptSetting, self).initialize(**kwargs)
        self.TEST_FILE = 'js_test.html'
        self.TEST_URL = '%s/%s' % (self.WEB_HOST, self.TEST_FILE)
        self.POLICY_NAME = 'DefaultJavaScriptSetting'
        self.POLICIES = {}
        self.TEST_CASES = {
            'Allow': 1,
            'Do Not Allow': 2}
        self.start_webserver()

    def _can_execute_javascript(self):
        """
        Determine whether JavaScript is allowed to run on the given page.

        @param tab: browser tab containing JavaScript to run.

        Note: This test does not use self.navigate_to_url(), because it can
        not depend on methods that evaluate or execute JavaScript.

        """
        tab = self.cr.browser.tabs.New()
        tab.Activate()
        tab.Navigate(self.TEST_URL)
        time.sleep(1)

        utils.poll_for_condition(
            lambda: tab.url == self.TEST_URL,
            exception=error.TestError('Test page is not ready.'))

        try:
            utils.poll_for_condition(
                lambda: tab.EvaluateJavaScript('jsAllowed', timeout=2),
                exception=error.TestError('Test page is not ready.'))
            return True
        except (EvaluateException, utils.TimeoutError):
            return False

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.POLICIES[self.POLICY_NAME] = case_value
        self.setup_case(user_policies=self.POLICIES)

        if case == 'Allow':
            if not self._can_execute_javascript():
                raise error.TestFail('JavaScript disabled, should be enabled.')
        else:
            if self._can_execute_javascript():
                raise error.TestFail('JavaScript enabled, should be disabled.')
