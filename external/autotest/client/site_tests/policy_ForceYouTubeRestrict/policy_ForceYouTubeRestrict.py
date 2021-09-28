# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


VERIFY_VIDEO_NOT_LOADED_CMD = ("document.getElementById"
    "('error-screen').innerText;")
RESTRICTED_ONLY_ON_STRICT = 'https://www.youtube.com/watch?v=Fmwfmee2ZTE'
RESTRICTED_ON_MODERATE = 'https://www.youtube.com/watch?v=yR79oLrI1g4'
SEARCH_QUERY = 'https://www.youtube.com/results?search_query=adult'
RESTRICTED_MODE_TOGGLE_ENABLED = 'aria-disabled="false"'
RESTRICTED_MODE_TOGGLE_DISABLED = 'aria-disabled="true"'
RESTRICTED_MODE_TOGGLE_SCRAPE_CMD = ("document.querySelector('* /deep/ "
    "#restricted-mode').innerHTML;")
BURGER_MENU_CLICK = ("document.querySelector('* /deep/ #masthead-container /de"
    "ep/ #end /deep/ ytd-topbar-menu-button-renderer:last-of-type').click();")
BURGER_MENU = ("document.querySelector('* /deep/ #masthead-container /deep/"
    " #end /deep/ ytd-topbar-menu-button-renderer:last-of-type').innerHTML;")
RESTRICTED_MENU_CLICK = ("document.querySelector('* /deep/ ytd-app /deep/"
    " ytd-account-settings /deep/ #restricted').click();")


class policy_ForceYouTubeRestrict(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the ForceYouTubeRestrict policy in Chrome OS.

    If the policy is set to strict than the user will not be able to view any
    restricted videos on YouTube. If the policy is set to moderate than the
    user will not be able to watch some restricted videos. In both cases
    the user will not be able to toggle restricted settings on the website.

    Note: This test doesn't cover the ARC++ app.

    """
    version = 1

    POLICY_NAME = 'ForceYouTubeRestrict'
    TEST_CASES = {
        'Strict': 2,
        'Moderate': 1,
        'Disabled': 0,
        'NotSet': None}


    def _get_content(self, restriction_type):
        """
        Checks the contents of the watch page.

        @param restriction_type: URL with either strict or moderate content.

        @returns text content of the element with video status.

        """
        active_tab = self.navigate_to_url(restriction_type)
        utils.poll_for_condition(
            lambda: self.check_page_readiness(
                active_tab, VERIFY_VIDEO_NOT_LOADED_CMD),
            exception=error.TestFail('Page is not ready.'),
            timeout=5,
            sleep_interval=1)
        return active_tab.EvaluateJavaScript(VERIFY_VIDEO_NOT_LOADED_CMD)


    def _check_restricted_mode(self, case):
        """
        Checks restricted settings by verifying that user is unable to play
        certain videos as well as toggle restricted settings.

        @param case: policy value expected.

        """
        # Navigates to the search page and verifies the status of the toggle
        # button.
        search_tab = self.navigate_to_url(SEARCH_QUERY)
        utils.poll_for_condition(
            lambda: self.check_page_readiness(search_tab, BURGER_MENU),
            exception=error.TestFail('Page is not ready.'),
            timeout=5,
            sleep_interval=1)
        search_tab.EvaluateJavaScript(BURGER_MENU_CLICK)
        time.sleep(1)
        search_tab.EvaluateJavaScript(RESTRICTED_MENU_CLICK)
        time.sleep(1)
        restricted_mode_toggle_status = search_tab.EvaluateJavaScript(
            RESTRICTED_MODE_TOGGLE_SCRAPE_CMD)

        if case == 'Strict' or case == 'Moderate':
            if case == 'Strict':
                strict_content = self._get_content(RESTRICTED_ONLY_ON_STRICT)
                if 'restricted' not in strict_content:
                    raise error.TestFail("Restricted mode is not on, "
                        "user can view restricted video.")
            elif case == 'Moderate':
                moderate_content = self._get_content(RESTRICTED_ON_MODERATE)
                if 'restricted' not in moderate_content:
                    raise error.TestFail("Restricted mode is not on, "
                        "user can view restricted video.")
            if RESTRICTED_MODE_TOGGLE_ENABLED in restricted_mode_toggle_status:
                raise error.TestFail("User is able to toggle restricted mode.")
        else:
            strict_content = self._get_content(RESTRICTED_ONLY_ON_STRICT)
            moderate_content = self._get_content(RESTRICTED_ON_MODERATE)
            if ('restricted' in strict_content or
                    'restricted' in moderate_content):
                raise error.TestFail("Restricted mode is on and it "
                    "shouldn't be.")
            if (RESTRICTED_MODE_TOGGLE_DISABLED in
                    restricted_mode_toggle_status):
                raise error.TestFail("User is not able to "
                    "toggle restricted mode.")


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.POLICIES = {self.POLICY_NAME: self.TEST_CASES[case]}
        self.setup_case(user_policies=self.POLICIES)
        self._check_restricted_mode(case)