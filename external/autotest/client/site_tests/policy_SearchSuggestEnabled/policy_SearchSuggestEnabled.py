# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard


class policy_SearchSuggestEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test effect of SearchSuggestEnabled policy on Chrome OS."""
    version = 1

    POLICY_NAME = 'SearchSuggestEnabled'

    def _search_bar_check(self, policy_value):
        """
        Checks if the search suggestion options appeared or not.

        @param policy_value: bool or None, the setting of the Policy.

        """
        # Open new tab.
        self.keyboard.press_key('ctrl+t')

        # Input random characters into the omnibox. This will trigger the
        # search suggestions dropdown (or not).
        self.keyboard.press_key('f')
        self.keyboard.press_key('s')
        self.keyboard.press_key('w')

        search_sugg = self.ui.item_present(name='/search suggestion/',
                                           isRegex=True)

        if policy_value is False and search_sugg:
            raise error.TestError('Search Suggestions are enabled')
        elif policy_value is not False and not search_sugg:
            raise error.TestError('Search Suggestions are not enabled.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={self.POLICY_NAME: case})
        self.ui.start_ui_root(self.cr)
        self.keyboard = keyboard.Keyboard()
        self._search_bar_check(case)
        self.keyboard.close()
