# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_AllowScreenLock(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of AllowScreenLock policy on Chrome OS.

    This test will set the policy, attempt to lock the screen, then check to
    see if the screen is locked.

    """
    version = 1
    POLICY_NAME = 'AllowScreenLock'

    def _test_lock_status(self, case):
        """
        Verify the screen lock status.

        @param case: bool or None, setting of AllowScreenLock policy.

        """
        _keyboard = keyboard.Keyboard()
        lock_state = self.cr.login_status['isScreenLocked']
        if lock_state:
            raise error.TestFail('Screen was locked prior to test')

        # Lock the screen with the screenlock hotkey
        _keyboard.press_key('search+L')
        is_locked = self.cr.login_status['isScreenLocked']

        # Policy is None or True
        if case is not False and not is_locked:
            raise error.TestFail('Screen was NOT locked when should be')
        elif case is False and is_locked:
            raise error.TestFail('Screen was LOCKED when should not be')
        _keyboard.close()

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={self.POLICY_NAME: case})
        self._test_lock_status(case)
