# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard

from telemetry.core import exceptions


class policy_NewTabPageLocation(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the NewTabPageLocation policy in Chrome OS.

    If the NewTabPageLocation policy is set, when a NewTab is opened,
    the page configured page will be directly loaded.

    """
    version = 1

    def _homepage_check(self, case_value):
        """
        Open a new tab and checks the proper page is opened.

        @param case_value: policy value for this case.

        """
        self.keyboard.press_key('ctrl+t')
        new_page_text = '/Chrome Policies/'
        # Check for a chrome policies ui object.
        if case_value:
            self.ui.wait_for_ui_obj(new_page_text, isRegex=True)
        else:
            url = str(self.cr.browser.tabs[-1].GetUrl())
            if not (
                    self.ui.did_obj_not_load(new_page_text, isRegex=True) and
                    (url == 'chrome://newtab/')):
                raise error.TestFail(
                    'NewTab was not "chrome://newtab/" instead got {}'
                    .format(url))

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.keyboard = keyboard.Keyboard()
        TEST_CASES = {'Set': 'chrome://policy',
                      'NotSet': None}

        case_value = TEST_CASES[case]
        policy_setting = {'NewTabPageLocation': case_value}
        self.setup_case(user_policies=policy_setting)
        self.ui.start_ui_root(self.cr)
        self._homepage_check(case_value)
