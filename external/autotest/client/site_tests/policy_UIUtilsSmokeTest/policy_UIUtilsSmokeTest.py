# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_UIUtilsSmokeTest(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Simple test to check that the ui_utils are mostly working. Good to run this
    if major changes are made to the utils file.

    When new features are added, attempt to add them to this test.

    """
    version = 1

    def _smoke_test(self):
        """The test."""
        self.ui.start_ui_root(self.cr)

        # Checks if both this and the list_screen_items functions are working.
        if not self.ui.get_name_role_list():
            raise error.TestError('No items returned from entire screen')

        self.ui.doDefault_on_obj(name='Launcher', role='button')

        # Check the doCommand, wait_for_ui_obj and item_present
        self.ui.doCommand_on_obj(name='Launcher',
                                 role='button',
                                 cmd='showContextMenu()')
        self.ui.wait_for_ui_obj(name='/Autohide/',
                                isRegex=True,
                                role='menuItem')

        if self.ui.is_obj_restricted(name='Launcher', role='button',):
            raise error.TestError('Launcher should not be restricted')
        if len(self.ui.list_screen_items()) == 0:
            raise error.TestError("list_screen_items returned no items")
        self.ui.click_and_wait_for_item_with_retries('/tray/', 'Settings', True)

    def run_once(self):
        """Run the test."""
        self.setup_case()
        self._smoke_test()
