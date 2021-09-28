# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_PrintingEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test effect of PrintingEnabled policy on Chrome OS."""
    version = 1

    POLICY_NAME = 'PrintingEnabled'

    def _print_check(self, case):
        """
        Click the dropdown menu in Chrome, check if the print option is greyed
        out.

        @param case: bool or None, the setting of the PrintingEnabled Policy

        """
        self.ui.doDefault_on_obj('Chrome')
        self.ui.wait_for_ui_obj('/Print/', role='menuItem', isRegex=True)
        print_disabled = self.ui.is_obj_restricted('/Print/',
                                                   role='menuItem',
                                                   isRegex=True)
        if case is not False and print_disabled:
            raise error.TestError('Printing not enabled when it should be')
        elif case is False and not print_disabled:
            raise error.TestError('Printing enabled when it should not be')

    def run_once(self, case):
        """
        Entry point of the test.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={'PrintingEnabled': case},
                        disable_default_apps=False)

        self.ui.start_ui_root(self.cr)
        self._print_check(case)
