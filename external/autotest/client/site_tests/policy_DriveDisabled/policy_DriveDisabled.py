# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DriveDisabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_DriveDisabled policy on Chrome OS.

    This test will set the policy, then check if the google drive is mounted
    or not.

    """
    version = 1

    POLICY_NAME = 'DriveDisabled'
    case_value_lookup = {'enable': False,
                         'disable': True,
                         'not_set': None}

    def run_once(self, case=None):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case = self.case_value_lookup[case]

        self.setup_case(user_policies={self.POLICY_NAME: case},
                        extra_chrome_flags=['--enable-features=DriveFS'],
                        real_gaia=True)

        self.check_mount(case)

    def check_mount(self, case):
        """
        Poll for the drive setting. If the case is True (ie disabled), wait
        another few seconds to ensure the drive doesn't start with a delay.

        @param case: Value of the DriveDisabled setting.

        """
        if case:
            e_msg = 'Should not have found mountpoint but did!'
        else:
            e_msg = 'Should have found mountpoint but did not!'
        # It may take some time until drivefs is started, so poll for the
        # mountpoint until timeout.
        utils.poll_for_condition(
            lambda: self.is_drive_properly_set(case),
            exception=error.TestFail(e_msg),
            timeout=10,
            sleep_interval=1,
            desc='Polling for page to load.')

        # Due to this being a negative case, and the poll_for would likely
        # return True immediately, we should wait the maximum duration and do
        # a final check for the mount.
        if case:
            time.sleep(10)

        mountpoint = self._find_drivefs_mount()

        if case and mountpoint:
            raise error.TestFail(e_msg)
        if not case and not mountpoint:
            raise error.TestFail(e_msg)

    def is_drive_properly_set(self, case):
        """
        Checks if the drive status is proper vs the policy settings.policy

        @param case: Value of the DriveDisabled setting.

        """
        if case:
            if not self._find_drivefs_mount():
                return True
        else:
            if self._find_drivefs_mount():
                return True
        return False

    def _find_drivefs_mount(self):
        """Return the mount point of the drive if found, else return None."""
        for mount in utils.mounts():
            if mount['type'] == 'fuse.drivefs':
                return mount['dest']
        return None
