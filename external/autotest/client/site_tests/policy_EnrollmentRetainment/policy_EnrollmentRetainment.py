# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.enterprise import enterprise_policy_base

USERNAME = 'stressenroll@managedchrome.com'
PASSWORD = 'test0000'


class policy_EnrollmentRetainment(
        enterprise_policy_base.EnterprisePolicyTest):
    """Stress tests the enrollment by continiously restarting."""
    version = 1


    def initialize(self, **kwargs):
        self._initialize_enterprise_policy_test(
            set_auto_logout=False,
            env='prod',
            username=USERNAME,
            password=PASSWORD,
            **kwargs)


    def run_once(self):
        """Entry point of this test."""

        with chrome.Chrome(
            clear_enterprise_policy=False,
            expect_policy_fetch=True,
            disable_gaia_services=False,
            gaia_login=True,
            username=USERNAME,
            password=PASSWORD) as self.cr:
            # Policy that is set on cpanel side that is off by default.
            self.verify_policy_value('DeviceAllowBluetooth', False)
