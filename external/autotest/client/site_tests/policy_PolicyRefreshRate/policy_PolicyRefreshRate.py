# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_PolicyRefreshRate(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_PolicyRefreshRate policy on Chrome OS.

    """
    version = 1

    def check_refresh_rate(self, case):
        """
        Will check the policy refresh rate from chrome://policy, and verify
        that the text returned alligns with the policy configured.

        @param case: Name of the test case to run.

        """
        tab = self.navigate_to_url('chrome://policy')

        js_text_query = "document.querySelector('{}').innerText"
        refresh_interval_js = '#status-box-container div.refresh-interval'

        # Grab the policy refresh as shown at the top of the page, not from
        # the policy table.
        refresh_interval = tab.EvaluateJavaScript(
                               js_text_query.format(refresh_interval_js))
        if case <= 1800000:
            expected_refresh = '30 mins'
        elif case >= 86400000:
            expected_refresh = '1 day'

        if refresh_interval != expected_refresh:
            raise error.TestFail('Policy refresh incorrect. Got {} expected {}'
                                 .format(refresh_interval, expected_refresh))

    def run_once(self, case):
        """
        @param case: Name of the test case to run.

        """
        self.setup_case(user_policies={'PolicyRefreshRate': case})
        self.check_refresh_rate(case)
