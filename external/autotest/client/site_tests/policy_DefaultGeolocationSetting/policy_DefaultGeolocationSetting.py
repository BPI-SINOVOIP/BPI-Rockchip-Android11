# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DefaultGeolocationSetting(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of DefaultGeolocationSetting policy on Chrome OS.

    This test will set the policy, then will check the geolocation setting
    to determine if the policy worked or not.

    The policy sets the result to variable so its easier to call/return
    via the EvaluateJavaScript function.

    """
    version = 1

    POLICY_NAME = 'DefaultGeolocationSetting'
    policy_setting = {'granted': 1,
                      'denied': 2,
                      'prompt': 3,
                      'not_set': None}

    def _notification_check(self, case):
        """
        Navigates to a page, and query the geolocation status.

        @param case: the setting of the DefaultGeolocationSetting Policy

        """
        tab = self.navigate_to_url('chrome://policy')
        f = """new Promise(function(resolve, reject) {
                navigator.permissions.query({name:"geolocation"})
                    .then(function(geoloc) {
                        resolve(geoloc.state)});
            })"""

        content = tab.EvaluateJavaScript(f, promise=True)

        # prompt is the default setting
        if case == 'not_set':
            case = 'prompt'
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
