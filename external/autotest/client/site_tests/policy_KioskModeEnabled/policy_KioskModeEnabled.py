# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.common_lib import utils


class policy_KioskModeEnabled(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test for verifying that the DUT entered kiosk mode."""
    version = 1

    def run_once(self):
        """Entry point of this test."""

        # ID of the kiosk app to start.
        kId = 'afhcomalholahplbjhnmahkoekoijban'

        self.DEVICE_POLICIES = {
            'DeviceLocalAccounts': [
                {'account_id': kId, 'kiosk_app': {'app_id': kId}, 'type': 1}],
            'DeviceLocalAccountAutoLoginId': kId
        }

        self.setup_case(
            device_policies=self.DEVICE_POLICIES,
            enroll=True,
            kiosk_mode=True,
            auto_login=False)
        self.ui.start_ui_root(self.cr)
        self.ui.wait_for_ui_obj(name='/Kiosk/', isRegex=True, timeout=30)
