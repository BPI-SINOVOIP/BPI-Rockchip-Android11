# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ManagedBookmarks(enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of ManagedBookmarks policy on Chrome OS behavior.

    This test verifies the behavior of Chrome OS for a range of valid values
    of the ManagedBookmarks user policy, as defined by three test cases:
    NotSet_NotShown, SingleBookmark_Shown, and MultiBookmarks_Shown.

    When not set, the policy value is undefined. This induces the default
    behavior of not showing the managed bookmarks folder, which is equivalent
    to what is seen by an un-managed user.

    When one or more bookmarks are specified by the policy, then the Managed
    Bookmarks folder is shown, and the specified bookmarks within it.

    """
    version = 1

    POLICY_NAME = 'ManagedBookmarks'
    BOOKMARKS = [{'name': 'Google',
                  'url': 'https://google.com/'},
                 {'name': 'YouTube',
                  'url': 'https://youtube.com/'},
                 {'name': 'Chromium',
                  'url': 'https://chromium.org/'}]

    # Dictionary of test case names and policy values.
    TEST_CASES = {
        'NotSet_NotShown': None,
        'SingleBookmark_Shown': BOOKMARKS[:1],
        'MultipleBookmarks_Shown': BOOKMARKS
    }

    def _get_set(self, arr):
        """Return the set of key names from an array of dicts."""
        return set([item['name'] for item in arr])

    def _get_managed_bookmarks(self):
        """Return a set of screen difference after the bookmark is clicked."""
        prior_ui = set(self.ui.list_screen_items())
        self.ui.doDefault_on_obj('/managedchrome.com bookmarks/', isRegex=True)
        return set(self.ui.list_screen_items()) - prior_ui

    def _test_managed_bookmarks(self, policy_value):
        """
        Verify CrOS enforces ManagedBookmarks policy.

        When ManagedBookmarks is not set, the UI shall not show the managed
        bookmarks folder nor its contents. When set to one or more bookmarks
        the UI shows the folder and its contents.

        @param policy_value: policy value for this case.

        @raises error.TestFail: If displayed managed bookmarks does not match
            the policy value.

        """
        if not policy_value:
            if self.ui.item_present('/managedchrome.com bookmarks/',
                                    isRegex=True):
                raise error.TestError(
                    'Managed bookmarks present when should not be')

        else:
            screen_items = self._get_managed_bookmarks()
            pol_set = self._get_set(policy_value)
            if pol_set:
                if not pol_set.issubset(screen_items):
                    raise error.TestError(
                        'Managed Boomarks not present when should be'
                        'Items on screen {} expected to find {}'
                        .format(screen_items, pol_set))

            full_set = self._get_set(self.BOOKMARKS)
            dont_want = full_set - pol_set
            if dont_want:
                if dont_want.issubset(screen_items):
                    raise error.TestError(
                        'Managed Boomarks present when should not be'
                        'Items on screen {} expected not to find {}'
                        .format(screen_items, dont_want))

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.setup_case(user_policies={self.POLICY_NAME: case_value})
        self.ui.start_ui_root(self.cr)
        self._test_managed_bookmarks(case_value)
