# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard


class policy_HomepageLocation(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the HomepageLocation policy in Chrome OS.

    If the HomepageLocation policy is set, along with the
    HomepageIsNewTabPage policy, then going to the homepage should take
    you directly to the homepage that was set by the HomepageLocation policy.
    The test does not check if the homepage could be modified.

    """
    version = 1

    def initialize(self, **kwargs):
        super(policy_HomepageLocation, self).initialize(**kwargs)
        self.keyboard = keyboard.Keyboard()
        self.POLICY_NAME = 'HomepageLocation'
        self.SUPPORTING_POLICIES = {
            'HomepageIsNewTabPage': False}
        self.TEST_CASES = {
            'Set': 'chrome://version/',
            'NotSet': None}

    def _homepage_check(self, case_value):
        """
        Navigates to the homepage and checks that it's set.

        @param case_value: policy value for this case.

        """
        self.keyboard.press_key('alt+home')
        current_url = self.cr.browser.tabs[0].GetUrl()
        if case_value:
            if current_url != self.TEST_CASES['Set']:
                raise error.TestFail('Homepage Location was not set.')
        else:
            if current_url != 'chrome://newtab/':
                raise error.TestFail(
                    'Homepage was set to %s instead of chrome://newtab/',
                    current_url)

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.SUPPORTING_POLICIES[self.POLICY_NAME] = case_value
        self.setup_case(user_policies=self.SUPPORTING_POLICIES)
        self._homepage_check(case_value)