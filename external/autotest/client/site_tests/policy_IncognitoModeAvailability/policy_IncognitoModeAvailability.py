# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_IncognitoModeAvailability(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the IncognitoModeAvailable policy in Chrome OS.

    If the policy is set to Available then the user will be able to open
    a new Incognito window. If the policy is Disabled then the user should not
    be able to open a new Incognito window. Forced is not being tested.

    """
    version = 1

    def initialize(self, **kwargs):
        super(policy_IncognitoModeAvailability, self).initialize(**kwargs)
        self.POLICY_NAME = 'IncognitoModeAvailability'
        self.POLICIES = {}
        self.TEST_CASES = {
            'Available': 0,
            'Disabled': 1}

    def _check_incognito_mode_availability(self, case):
        """
        Opens a new chrome://user-actions page and then tries to open a new
        Incognito window. To see if the new window actually opened the test
        checks the number of tabs opened as well as what was recorded in
        user actions.

        @param case: policy description.

        """
        button_name = '/New incognito window/'
        self.ui.wait_for_ui_obj('Chrome')
        self.ui.doDefault_on_obj('Chrome')
        if case == 'Available' and not self.ui.item_present(button_name,
                                                            isRegex=True):
            raise error.TestFail('Incognito not available')

        elif case == 'Disabled' and self.ui.item_present(button_name,
                                                         isRegex=True):
           raise error.TestFail('Incognito not available.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.POLICIES[self.POLICY_NAME] = case_value
        self.setup_case(user_policies=self.POLICIES)
        self.ui.start_ui_root(self.cr)
        self._check_incognito_mode_availability(case)
