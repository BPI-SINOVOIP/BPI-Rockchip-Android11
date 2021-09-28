# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard


class policy_DefaultSearchProvider(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Verify effects of the DefaultSearchProviderSearchURL and
    DefaultSearchProviderKeyword policy. When the
    DefaultSearchProviderSearchURL policy is set, the specified search url
    will be used when a value is entered in the omnibox. When the
    DefaultSearchProviderKeyword is set, the value will trigger the shortcut
    used in the omnibox to trigger the search for this provider.

    """
    version = 1

    def _search_check(self, case):
        """
        Open a new tab, use the omnibox as a search box, and check the URL.

        @param case: Value of the test being run.

        """
        self.ui.start_ui_root(self.cr)
        self.keyboard = keyboard.Keyboard()
        self.ui.doDefault_on_obj(name='Address and search bar')
        # The keys to be pressed for the test
        if case == 'Keyword':
            buttons = ['d', 'a', 'd', 'tab', 's', 's', 'enter']
            expected = '{}{}'.format(self.BASE_URL, 'ss')
        else:
            buttons = ['f', 's', 'w', 'enter']
            expected = '{}{}'.format(self.BASE_URL, 'fsw')

        # Enter the buttons
        for button in buttons:
            self.keyboard.press_key(button)

        tabFound = False
        startTime = time.time()
        while time.time() - startTime < 1:
            tabs = set([tab.GetUrl() for tab in self.cr.browser.tabs])
            if expected in tabs:
                tabFound = True
                break

        if not tabFound:
            raise error.TestFail(
                'Search not formated correctly. expected {} got {}'
                .format(expected, tabs))

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.BASE_URL = 'https://fakeurl/search?q='
        POLICIES = {'DefaultSearchProviderEnabled': True,
                    'DefaultSearchProviderSearchURL':
                       '%s{searchTerms}' % (self.BASE_URL)}
        if case == 'Keyword':
            POLICIES['DefaultSearchProviderKeyword'] = 'dad'
        self.setup_case(user_policies=POLICIES)
        self._search_check(case)
