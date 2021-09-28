# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard


class policy_TouchVirtualKeyboardEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the TouchVirtualKeyboardEnabled policy in Chrome OS.
    If the policy is set to True then the user can see the virtual keyboard
    when trying to enter new text. If the policy is off or not set then virtual
    keyboard should not come up.

    """
    version = 1

    def initialize(self, **kwargs):
        super(policy_TouchVirtualKeyboardEnabled, self).initialize(**kwargs)
        self.keyboard = keyboard.Keyboard()

    def _check_virtual_keyboard(self, case):
        """
        Checking the visibilty of the virtual keyboard.

        @param case: policy value.

        """
        # Pressing ctrl+t to get new tab open with the cursor in the url bar.
        self.keyboard.press_key('ctrl+t')
        # Checking for a unique button on the keyboard.
        on_screen_keyboard = self.ui.item_present('switch to emoji')

        if case is True:
            if not on_screen_keyboard:
                raise error.TestFail(
                    'Virtual keyboard is not visible and it should be.')

        else:
            if on_screen_keyboard:
                raise error.TestFail(
                    'Virtual keyboard is visible and it should not be.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        POLICIES = {'TouchVirtualKeyboardEnabled': case}
        self.setup_case(user_policies=POLICIES)
        self.ui.start_ui_root(self.cr)
        self._check_virtual_keyboard(case)
