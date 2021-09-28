# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.a11y import a11y_test_base
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_AccessibilityTest(
        enterprise_policy_base.EnterprisePolicyTest,
        a11y_test_base.a11y_test_base):
    """
    Test effect of the following accessibility policies on Chrome OS:
    HighContrastEnabled, LargeCursorEnabled, VirtualKeyboardEnabled, and
    ScreenMagnifierType.

    This test will set the policy and value, then call the Accessibility API
    to see if the feature is enabled or not.

    """
    version = 1

    _LOOKUP = {'HighContrastEnabled': 'highContrast',
               'LargeCursorEnabled': 'largeCursor',
               'VirtualKeyboardEnabled': 'virtualKeyboard',
               'ScreenMagnifierType': 'screenMagnifier'}

    def _check_settings(self, policy, case):
        """Call the accessibility API extension and check the policy was set
        correctly.

        @param policy: Name of the policy set.
        @param case: Value of the set policy.

        """
        value_str = 'true' if case else 'false'
        feature = self._LOOKUP[policy]

        cmd = ('window.__result = null;\n'
               'chrome.accessibilityFeatures.%s.get({}, function(d) {'
               'window.__result = d[\'value\']; });' % (feature))
        self._extension.ExecuteJavaScript(cmd)
        poll_cmd = 'window.__result == %s;' % value_str
        pol_status = self._extension.EvaluateJavaScript(poll_cmd)

        if not pol_status:
            raise error.TestError('{} setting incorrect'.format(policy))

    def run_once(self, policy, case):
        """
        Setup and run the test configured for the specified test case.

        @param policy: Name of the policy to set.
        @param case: Value of the policy to set.

        """

        # Get the accessibility API extension path from the ally_test_base
        extension_path = self._get_extension_path()

        self.setup_case(user_policies={policy: case},
                        extension_paths=[extension_path])

        self._extension = self.cr.get_extension(extension_path)
        self._check_settings(policy, case)
