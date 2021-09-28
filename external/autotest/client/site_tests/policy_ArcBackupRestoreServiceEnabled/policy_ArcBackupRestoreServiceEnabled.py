# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils

from autotest_lib.client.common_lib.cros import arc
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ArcBackupRestoreServiceEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_ArcBackupRestoreServiceEnabled policy on the
    ARC++ container within ChromeOS.

    """
    version = 1

    def verify_policy(self, case):
        """
        Verify the policy was properly set

        @param case: integer, value of the policy setting

        """
        if case:
            e_msg = 'Backup manager is disabled and should be enabled.'
        else:
            e_msg = 'Backup manager is enabled and should be disabled.'

        # Give the ARC container time to setup and configure its policy.
        utils.poll_for_condition(
            lambda: self.check_bmgr(case),
            exception=error.TestFail(e_msg),
            timeout=45,
            sleep_interval=5,
            desc='Checking bmgr status')

    def check_bmgr(self, case):
        """
        Check if Android backup and recovery is accessible.

        @param case: integer, value of the policy setting

        @Returns True if accessible and set correctly, False otherwise.

        """
        b_and_r_status = arc.adb_shell('bmgr enabled')

        if case:
            if "Backup Manager currently enabled" not in b_and_r_status:
                return False

        else:
            if "Backup Manager currently disabled" not in b_and_r_status:
                return False

        return True

    def run_once(self, case):
        """
        @param case: integer, value of the policy setting

        """
        pol = {'ArcEnabled': True,
               'ArcBackupRestoreServiceEnabled': case}
        self.setup_case(user_policies=pol,
                        arc_mode='enabled',
                        use_clouddpc_test=True)
        self.verify_policy(case)
