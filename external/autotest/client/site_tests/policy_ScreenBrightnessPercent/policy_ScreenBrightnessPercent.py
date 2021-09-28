# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ScreenBrightnessPercent(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of ScreenBrightnessPercent policy on Chrome OS.

    This test will set the policy, then check the current screen brightness
    via the status bar.

    """
    version = 1

    POLICY_NAME = 'ScreenBrightnessPercent'

    def _test_backlight(self, backlight_level):
        """
        Get the backlight value off the status tray slider.

        @param backlight_level: int or float, UI brightness settings.

        """
        self.ui.start_ui_root(self.cr)
        self.ui.click_and_wait_for_item_with_retries(
            item_to_click="/Status tray/",
            isRegex_click=True,
            item_to_wait_for="/Brightness/",
            isRegex_wait=True)
        value = self.ui.doCommand_on_obj("/Brightness/",
                                         cmd="value",
                                         isRegex=True)
        expected = str(backlight_level) + '%'
        if expected != value:
            raise error.TestError('Mismatch {} v {}'.format(expected, value))

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={
            self.POLICY_NAME: {"BrightnessAC": case}})
        self._test_backlight(case)
