# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DefaultNotificationsSetting(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of DefaultNotificationsSetting policy on Chrome OS.

    This test will set the policy, then will check the notification.permission
    js call to determine if the policy worked or not.


    """
    version = 1

    POLICY_NAME = 'DefaultNotificationsSetting'
    policy_setting = {'granted': 1,
                      'denied': 2,
                      'default': 3,
                      'not_set': None}

    def _notification_check(self, case):
        """
        Navigates to a page, and check the Notification status.

        @param case: the setting of the DefaultNotificationsSetting Policy

        """
        tab = self.navigate_to_url('chrome://policy')

        content = tab.EvaluateJavaScript('Notification.permission')

        # prompt is the default setting
        if case == 'not_set':
            case = 'default'
        if content != case:
            raise error.TestError('Geolocation Setting did not match setting.'
                                  'Expected {e} but received {r}'
                                  .format(e=case, r=content))

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={
            self.POLICY_NAME: self.policy_setting[case]})
        self._notification_check(case)
