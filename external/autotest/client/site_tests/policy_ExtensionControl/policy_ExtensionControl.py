# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ExtensionControl(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of multiple Extension control policies on Chrome OS.

    Test ExtensionInstallBlacklist prevents configured extensions from being
        installed.
    Test ExtensionInstallWhitelist does not prevent extensions from being
        installed.
    Test ExtensionInstallForcelist automatically installs configured
        extensions.
    Test when the ExtensionInstallBlacklist is set to '*', and an extension is
        on the ExtensionInstallWhitelist, it can be installed, showing the
        Whitelist takes priority over the Blacklist.

    """
    version = 1
    EXTENSION_ID = 'hoppbgdeajkagempifacalpdapphfoai'
    EXTENSION_PAGE = ('chrome://extensions/?id=%s'
                           % EXTENSION_ID)
    BLOCK_MSG = '/{}"\) is blocked/'.format(EXTENSION_ID)
    DOWNLOAD_URL = ('https://chrome.google.com/webstore/detail/platformkeys-test-extensi/{}'
                    .format(EXTENSION_ID))

    def _ext_check(self):
        """Verifies the extension install permissions are correct."""

        self.navigate_to_url(self.DOWNLOAD_URL)

        if 'ExtensionInstallForcelist' in self.pol_setting:
            # If the extension is installed, the Remove button will be present.
            self.ui.wait_for_ui_obj('Remove from Chrome', role='button')
        else:
            # Wait for the "Add to Chrome" button to load and click on it.
            self.ui.wait_for_ui_obj('Add to Chrome', role='button')
            self.ui.doDefault_on_obj('Add to Chrome', role='button')
            if ('ExtensionInstallWhitelist' in self.pol_setting):
                self.ui.wait_for_ui_obj('Add extension', role='button')
                self.ui.did_obj_not_load(self.BLOCK_MSG, isRegex=True)
            else:
                self.ui.wait_for_ui_obj(self.BLOCK_MSG, isRegex=True)

    def _update_policy(self):
        """
        Update the policy Blacklist to '*', and add the autotest extension to
        the Whitelist.

        NOTE: This is being done after the initial setup_case has been run to
        avoid an issue where the autotest extension will not load with the
        blacklist set to '*', even with it on the Whitelist. This caused the
        test to error and fail. Probably a race on the policy loading.

        """

        self.pol_setting['ExtensionInstallBlacklist'] = ['*']
        # Updating the fake DM server, and checking policy page.
        self.fake_dm_server.setup_policy(self._make_json_blob(
            user_policies=self.pol_setting))
        self.reload_policies()
        self.verify_policy_stats(self.pol_setting)

    def _configure_test(self, case):
        """
        Configures the test variables.

        @param case: Name of the test case to run.

        """
        # Used during the test run.
        autotest_id = 'behllobkkfkfnphdnhnkndlbkcpglgmj'

        if 'Whitelist' in case:
            self.pol_setting['ExtensionInstallWhitelist'] = [self.EXTENSION_ID,
                                                             autotest_id]
        if 'Blacklist' in case and 'Whitelist' not in case:
            self.pol_setting['ExtensionInstallBlacklist'] = [self.EXTENSION_ID]
        if 'Force_Install' in case:
            self.pol_setting['ExtensionInstallForcelist'] = [self.EXTENSION_ID]

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.pol_setting = {}

        self._configure_test(case)
        self.setup_case(user_policies=self.pol_setting)
        self.ui.start_ui_root(self.cr)
        # In this case there is both a Whitelist and Blacklist.
        if len(case) > 1:
            self._update_policy()

        self._ext_check()
