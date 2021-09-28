# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard

DEFAULT_SEARCH_ENGINE_URL = 'google.com'


class policy_DefaultSearchProviderEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the DefaultSearchProviderEnabled policy in Chrome OS.
    If the policy is set to True/Not Set then typing search queries in the
    omnibox will result in searching on google.com.
    If the policy is set to False then typing search queries in the
    omnibox will not result in searching on google.com.

    """
    version = 1

    def _default_search_provider_enabled(self, case):
        """
        Open a new tab and try using the omnibox as a search box.

        @param case: policy value.

        """

        self.keyboard = keyboard.Keyboard()

        # Open new tab.
        self.keyboard.press_key('ctrl+t')

        time.sleep(1)
        # Input random characters into the omnibox and hit Enter. This will
        # either perform the search or not.
        self.keyboard.press_key('f')
        self.keyboard.press_key('s')
        self.keyboard.press_key('w')
        self.keyboard.press_key('enter')

        current_url = self.cr.browser.tabs[1].GetUrl()

        if case is False:
            if DEFAULT_SEARCH_ENGINE_URL in current_url:
                raise error.TestFail(
                    'Search engine is on in the omnibox and it should not be')

        else:
            if DEFAULT_SEARCH_ENGINE_URL not in current_url:
                raise error.TestFail(
                    'Search engine is off in the omnibox and it should be on')


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'DefaultSearchProviderEnabled': case}
        self.setup_case(user_policies=POLICIES)
        self._default_search_provider_enabled(case)