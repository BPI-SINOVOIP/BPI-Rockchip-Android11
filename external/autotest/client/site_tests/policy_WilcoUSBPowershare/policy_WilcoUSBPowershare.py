# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_WilcoUSBPowershare(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test for setting the DeviceUsbPowerShareEnabled policy."""
    version = 1
    _POLICY = 'DeviceUsbPowerShareEnabled'


    def run_once(self, case):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self.setup_case(device_policies={self._POLICY: case}, enroll=True)
