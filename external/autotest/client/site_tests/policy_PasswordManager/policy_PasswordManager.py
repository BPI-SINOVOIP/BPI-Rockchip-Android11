# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_PasswordManager(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of PasswordManager policy on Chrome OS.

    This test will set the policy, then navigate to the password page
    within the chrome settings and verify the setting.

    """

    version = 1
    TEST_OBJ = '/Offer/'
    ROLE = 'toggleButton'
    EXP_RESULTS = {True: {'restrict': True,
                          'value': 'true',
                          'icon': True},
                   False: {'restrict': True,
                           'value': 'false'},
                   None: {'restrict': False,
                          'value': 'true'}}

    def _check_pword_mgr(self, case):
        """
        Check the status of the 'Offer to Save Passwords' toggle button via
        verifing the buttons current setting (ie enabled/disabled), and verify
        the setting is not able to be controlled by the user when the
        policy is set.

        Additionally there will be a check to verify the "controlled by policy"
        icon is present.

        @param case: Value of the policy settings.

        """
        expected_result = self.EXP_RESULTS[case]
        self.cr.browser.tabs[0].Navigate('chrome://settings/passwords')
        self._wait_for_page()
        icon_present = self.ui.item_present(name=self.TEST_OBJ,
                                            isRegex=True,
                                            role='genericContainer')
        button_restricted = self.ui.is_obj_restricted(
            name=self.TEST_OBJ,
            isRegex=True,
            role=self.ROLE)
        button_setting = self.ui.doCommand_on_obj(
            name=self.TEST_OBJ,
            cmd='checked',
            isRegex=True,
            role=self.ROLE)

        if expected_result['restrict'] and (
                not button_restricted or not icon_present):
            raise error.TestError('Password manager controlable by user.')
        elif not expected_result['restrict'] and (
                button_restricted or icon_present):
            raise error.TestError(
                'Password manager controlled by policy when not set.')

        if expected_result['value'] != button_setting:
            raise error.TestError(
                'Password Manager setting is {} when it should be {}'
                .format(button_setting, expected_result['value']))

    def _wait_for_page(self):
        """Wait for the page to load via all the expected elements loading."""
        self.ui.wait_for_ui_obj(name=self.TEST_OBJ, isRegex=True, timeout=18)

    def run_once(self, case):
        """Setup and run the test configured for the specified test case."""
        self.setup_case(real_gaia=True,
                        user_policies={'PasswordManagerEnabled': case})
        self.ui.start_ui_root(self.cr)
        self._check_pword_mgr(case)
