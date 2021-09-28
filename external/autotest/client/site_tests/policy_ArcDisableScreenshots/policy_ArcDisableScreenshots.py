# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils

from autotest_lib.client.common_lib.cros import arc
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ArcDisableScreenshots(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_ArcDisableScreenshots policy on the ARC++ container
    within ChromeOS.

    """
    version = 1

    POLICY_NAME = 'ArcPolicy'

    def verify_policy(self, case):
        """
        Verify the policy was properly set

        @param case: bool, value of the policy setting

        """
        if case:
            e_msg = 'ARC++ Sceenshot Taken when it should not have been'
        else:
            e_msg = 'ARC++ Screenshot was blocked when it should not have been'

        # Give the ARC container time to setup and configure its policy.
        utils.poll_for_condition(
            lambda: self.check_screenshot(case),
            exception=error.TestFail(e_msg),
            timeout=30,
            sleep_interval=1,
            desc='Checking for screenshot file size')

    def check_screenshot(self, case):
        """
        Take a sceenshot and check its size, to see if the policy was set
        correctly.

        @param case: bool, value of the policy setting

        @Returns True if the screenshot setting was correct, False otherwise.

        """
        # Remove any lingering possible screenshots
        arc.adb_shell('rm -f /sdcard/test.png', ignore_status=True)

        # Take a screenshot, then check its size
        arc.adb_shell('screencap > /sdcard/test.png', ignore_status=True)
        screenshotsize = arc.adb_shell('du -s /sdcard/test.png',
                                       ignore_status=True).split()[0]

        # Some devices screenshot may contain metadata that would be up to 8b
        if case and int(screenshotsize) > 8:
            return False
        # No screenshot should be under 100b
        elif not case and int(screenshotsize) < 100:
            return False

        return True

    def run_once(self, case):
        """
        @param case: bool, value of the policy setting

        """
        pol = {'ArcEnabled': True,
               'DisableScreenshots': case}
        self.setup_case(user_policies=pol,
                        arc_mode='enabled',
                        use_clouddpc_test=True)

        self.verify_policy(case)
