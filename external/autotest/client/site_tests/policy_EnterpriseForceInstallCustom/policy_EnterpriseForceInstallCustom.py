# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils

from autotest_lib.client.common_lib.cros import arc
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_EnterpriseForceInstallCustom(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_AlternateErrorPages policy on Chrome OS.

    """
    version = 1

    POLICY_NAME = 'ArcPolicy'
    PACKAGE1 = "com.coffeebeanventures.easyvoicerecorder"

    def _get_actual_policy_apps(self):
        """
        Return the apps listed in the ArcPolicy value with the value of
        "FORCE_INSTALLED" or "BLOCKED".

        @raises error.TestError if no apps are found.

        @returns: App & policies that are set to force_install, or blocked.

        """
        policy_value = self._get_policy_value_from_new_tab(self.POLICY_NAME)
        if not policy_value:
            raise error.TestError('No value for ArcPolicy found!')

        check_apps = []
        checklist = set(['FORCE_INSTALLED', 'BLOCKED'])
        app_settings = 'applications'
        if app_settings not in policy_value:
            raise error.TestError('ArcPolicy has no application settings!')

        for app in policy_value[app_settings]:
            if app['installType'] in checklist:
                check_apps.append(app)
        return check_apps

    def _verify_force_apps_list(self):
        """
        Verify that the expected force-installed apps match the policy value.

        """
        controlled_apps = self._get_actual_policy_apps()
        for pkg in controlled_apps:
            self._verify_package_status(pkg['packageName'], pkg['installType'])

    def _verify_package_status(self, pkg, installType):
        """
        Test that the package is installed/not installed as expected

        @param pkg: Name of the package to check.
        @param installType: Policy Setting of the app.

        @raises error.TestError if any expected apps are not found.

        """
        type_lookup = {'FORCE_INSTALLED': True,
                       'BLOCKED': False}
        status = type_lookup[installType]

        utils.poll_for_condition(
            lambda: self._check_pkg(pkg, status),
            exception=error.TestFail('Package {} not installed!'.format(pkg)),
            timeout=240,
            sleep_interval=1,
            desc='Polling for Package to be installed')

    def _check_pkg(self, pkg, status):
        """
        Checks to see if the package state is proper.

        @param pkg: Name of the package to check.
        @param status: If the package should be installed or not.

        @returns: True if the status is correct, else False.

        """
        if status:
            return arc._is_in_installed_packages_list(pkg)
        else:
            return not arc._is_in_installed_packages_list(pkg)

    def run_once(self):
        """
        Verify that the FORCE_INSTALLED and BLOCKED app settings work properly.

        Test will itterate twice. First will run the test with the app
        installed, and verify it is loaded. The second will block the app, and
        verify that the app is un-installed.

        """
        cases = ['FORCE_INSTALLED', 'BLOCKED']
        for case in cases:

            pol = self.policy_creator(case)
            self.setup_case(user_policies=pol,
                            arc_mode='enabled',
                            use_clouddpc_test=True)

            self._verify_force_apps_list()

    def policy_creator(self, case):
        pol = {'ArcEnabled': True,
               'ArcPolicy':
                   {"installUnknownSourcesDisabled": False,
                    "applications":
                        [{"packageName": self.PACKAGE1,
                          "defaultPermissionPolicy": "GRANT",
                          "installType": case}]
                    }
               }
        return pol
