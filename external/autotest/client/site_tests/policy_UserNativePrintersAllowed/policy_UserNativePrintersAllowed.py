# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error

from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_UserNativePrintersAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of UserNativePrintersAllowed policy on Chrome OS.

    The test will load the printer settings page, and verify if the
    "Add Printer" button is clickable or not.

    """
    version = 1

    POLICY_NAME = 'UserNativePrintersAllowed'

    def _print_check(self, case):
        """
        Navigates to the chrome://os-settings/cupsPrinters page, and will check
        to see if the "Add Printer" button is enabled/blocked.

        @param case: bool or None, the setting of the policy.

        """
        self.navigate_to_url('chrome://os-settings/cupsPrinters')
        self.ui.wait_for_ui_obj(name='Add Printer', role='button')

        res = self.ui.is_obj_restricted(name='Add Printer', role='button')

        if case is False and not res:
            raise error.TestError(
                'User can add printers when disabled by policy.')
        elif case is not False and res:
            raise error.TestError('User cannot add printers.')

    def run_once(self, case):
        """
        Entry point of the test.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={self.POLICY_NAME: case})
        self.ui.start_ui_root(self.cr)
        self._print_check(case)
