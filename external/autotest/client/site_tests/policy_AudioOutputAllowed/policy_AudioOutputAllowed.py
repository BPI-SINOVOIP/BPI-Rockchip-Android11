# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_AudioOutputAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    version = 1
    POLICY_NAME = 'AudioOutputAllowed'
    TEST_CASES = {
        'NotSet_Allow': None,
        'True_Allow': True,
        'False_Block': False
    }
    NOT_MUTED = '/Volume is on/'
    MUTED = '/Volume is muted/'

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.setup_case(user_policies={self.POLICY_NAME: case_value})
        self.ui.start_ui_root(self.cr)
        self.ui.doDefault_on_obj("/Status tray/", isRegex=True)
        if case_value is False:
            self.ui.wait_for_ui_obj(name=self.MUTED, isRegex=True)
            self.ui.did_obj_not_load(name=self.NOT_MUTED, isRegex=True)
        else:
            self.ui.wait_for_ui_obj(name=self.NOT_MUTED, isRegex=True)
            self.ui.did_obj_not_load(name=self.MUTED, isRegex=True)
