# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_BookmarkBarEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of BookmarkBarEnabled policy on Chrome OS.

    This test will set the policy, then attempt to save a bookmark via the
    star button on the browser.

    It will then navigate to a new page (eg google.com), and attempt to click
    the bookmark in the bookmark bar. If the bar is present, chrome should
    navigate to the bookmark, if not it will remain on its current page.

    """
    version = 1
    TEST_URL = 'chrome://policy/'

    def _bookmarkBarCheck(self, case):
        """
        Checks if the bookmark bar is configured per policy.

        @param case: bool or None, the setting of the BookmarkBarEnabled Policy

        """
        self._save_policy_bookmark()

        new_tab = self.navigate_to_url('https://google.com')
        new_tab.WaitForDocumentReadyStateToBeComplete()

        bookmark_present = self.ui.item_present('Policies', role='button')

        if case and not bookmark_present:
            raise error.TestError('Bookmark bar not enabled.')
        elif not case and bookmark_present:
            raise error.TestError('Bookmark bar enabled.')

    def _save_policy_bookmark(self):
        """Saves the current page as a bookmark."""
        tab = self.navigate_to_url(self.TEST_URL)
        tab.WaitForDocumentReadyStateToBeComplete()

        self.ui.doDefault_on_obj(role='button', name='/Bookmark this/',
                                 isRegex=True)

        # Wait for the button dialog to load.
        self.ui.wait_for_ui_obj('Done', role='button')
        self.ui.doDefault_on_obj(role='button', name='Done')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """

        self.setup_case(user_policies={'BookmarkBarEnabled': case})
        self.ui.start_ui_root(self.cr)
        self._bookmarkBarCheck(case)
