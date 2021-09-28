# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_TranslateEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of TranslateEnabled policy on Chrome OS.
    """
    version = 1

    POLICY_NAME = 'TranslateEnabled'

    def _translate_check(self, case):
        """
        Navigates to a test html page which is configured in French, then check
        if the translate button appears.

        @param policy_value: bool or None, the setting of the Policy

        """
        self.cr.browser.tabs[0].Navigate(self.TEST_URL)
        if case is False:
            if not self.ui.did_obj_not_load(name='Translate'):
                raise error.TestError('Translate loaded when disabled.')
        else:
            self.ui.wait_for_ui_obj(name='Translate', role='button')

    def _start_dependencies(self):
        self.TEST_FILE = 'translate_page.html'
        self.TEST_URL = '%s/%s' % (self.WEB_HOST, self.TEST_FILE)
        self.start_webserver()
        self.ui.start_ui_root(self.cr)

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={'TranslateEnabled': case})
        self._start_dependencies()
        self._translate_check(case)
