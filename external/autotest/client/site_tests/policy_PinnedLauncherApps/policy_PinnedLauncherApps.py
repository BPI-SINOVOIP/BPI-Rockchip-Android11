# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_PinnedLauncherApps(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test the PinnedLauncherApps policy by pinning the default Google Photos
    application.

    This test will:
        Set the application to be pinned via the user policy.
        Verify the application is on the launch bar.
        Verify the application cannot be removed from the launch bar.
        Remove the application from the PinnedLauncherApps policy.
        Verify the application can be removed from the launch bar.

    """
    version = 1
    PINNED_TEXT = '/Remove from Chrome/'
    EXT_NAME = 'Google Photos'

    def _remove_pinned_aps_policy(self):
        """Reset the policy, thus removing any pinned apps."""
        self.update_policies()

    def _remove_pinned_app(self):
        """Remove the pinned app after the test is done."""
        self.ui.doCommand_on_obj(self.EXT_NAME, cmd="showContextMenu()")
        self.ui.wait_for_ui_obj('Unpin')
        self.ui.doDefault_on_obj('Unpin')

        self.ui.wait_for_ui_obj(self.EXT_NAME, remove=True)

    def _check_launcher(self):
        """Run the launcher test."""
        self.ui.wait_for_ui_obj(self.EXT_NAME, timeout=30)
        self.ui.doCommand_on_obj(self.EXT_NAME, cmd="showContextMenu()")
        self.ui.wait_for_ui_obj(self.PINNED_TEXT, isRegex=True)
        if not self.ui.did_obj_not_load('Unpin'):
            self._remove_pinned_app()
            raise error.TestError(
                'App can be removed when pinned by policy!')

        self._remove_pinned_aps_policy()
        self._remove_pinned_app()

        if self.ui.item_present(self.EXT_NAME):
            raise error.TestError('Could not removed pinned app')

    def run_once(self):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        pol = {'PinnedLauncherApps': ['hcglmfcclpfgljeaiahehebeoaiicbko']}
        self.setup_case(user_policies=pol, real_gaia=True)
        self.ui.start_ui_root(self.cr)
        self._check_launcher()
