# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard
from telemetry.core import exceptions

class policy_DeveloperToolsAvailability(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the DeveloperToolsAvailable policy in Chrome OS.
    If the policy is set to Available then the user will be able to open
    a new Developer Tools console. If the policy is Disabled then the user
    should not be able to open a new Developer Tools console. Forced is
    not being tested.

    """

    version = 1


    def initialize(self, **kwargs):
        super(policy_DeveloperToolsAvailability, self).initialize(**kwargs)
        self.keyboard = keyboard.Keyboard()
        self.POLICY_NAME = 'DeveloperToolsAvailability'
        self.POLICIES = {}
        self.TEST_CASES = {
            'NotSet': None,
            'DisabledOnExtensions': 0,
            'Available': 1,
            'Disabled': 2}


    def _check_developer_tools_availability(self, case):
        """
        Opens a new chrome://user-actions page and then tries to open Developer
        Tools console. To see if the new window actually opened the test checks
        what was recorded in user actions.

        @param case: policy description.

        """
        page_scrape_cmd = (
            'document.getElementById("user-actions-table").innerText;')
        user_actions_tab = self.navigate_to_url('chrome://user-actions')

        # The below shortcuts can be used to open Developer Tools, though in
        # different tabs. The first one opens the Elements tab, the next two
        # open the last used tab, and the final one opens the Console tab.

        keys = ['ctrl+shift+c', 'ctrl+shift+i', 'f12', 'ctrl+shift+j']

        for key in keys:
            self.keyboard.press_key(key)

        recorded_user_actions = (
            user_actions_tab.EvaluateJavaScript(page_scrape_cmd))

        if (case == 'Available' or case == 'DisabledOnExtensions'
                or case == 'NotSet'):
            if ('DevTools_ToggleWindow' not in recorded_user_actions and
                'DevTools_ToggleConsole' not in recorded_user_actions):
                    raise error.TestFail("Developer Tools didn't open, but"
                        " should be allowed.")
        elif case == 'Disabled':
            if 'DevTools_ToggleWindow' in recorded_user_actions:
                raise error.TestFail("Developer Tools opened and should "
                    "have been disabled.")


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.POLICIES[self.POLICY_NAME] = case_value

        try:
            self.setup_case(user_policies=self.POLICIES)
            self._check_developer_tools_availability(case)
        except exceptions.TimeoutException:
            if case != 'Disabled':
                raise error.TestFail("Unexpected Timeout Exception")

