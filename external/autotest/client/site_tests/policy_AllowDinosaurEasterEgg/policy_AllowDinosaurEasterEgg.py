# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_AllowDinosaurEasterEgg(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of AllowDinosaurEasterEgg policy on Chrome OS.

    This test will set the policy, then check if the game is playable
    or not, based off this policy.

    The game should not be playable when the policy is not set.

    It will determine if the game is playable via the presence of the
    snackbar class inside the main-frame-error function

    """
    version = 1

    POLICY_NAME = 'AllowDinosaurEasterEgg'

    def _dino_check(self, policy_value):
        """
        Navigates to the chrome://dino page, and will check to see if the game
        is playable or not.

        @param policy_value: bool or None, the setting of the Dinosaur Policy

        """
        tab = self.navigate_to_url('chrome://dino')
        _BLOCKED_JS = '* /deep/ #main-frame-error div.snackbar'
        content = tab.EvaluateJavaScript("document.querySelector('{}')"
                                         .format(_BLOCKED_JS))

        if policy_value:
            if content is not None:
                raise error.TestError('Dino game was not playable,'
                                      ' but should be allowed by policy.')
        else:
            if content is None:
                raise error.TestError('Dino game was playable,'
                                      ' but should not be allowed by policy.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={self.POLICY_NAME: case})
        self._dino_check(case)
