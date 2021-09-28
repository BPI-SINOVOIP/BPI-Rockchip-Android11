# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DeviceAllowBluetooth(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test for the DeviceAllowBluetooth policy.

    If the policy is set to True/None then bluetooth button is available in
    status tray menu. If the policy is set to False then bluetooth button
    is not available.

    """
    version = 1
    _POLICY = 'DeviceAllowBluetooth'

    def _is_bluetooth_button_present(self, ext):
        bt_present = ext.EvaluateJavaScript("""
        var root;
        chrome.automation.getDesktop(r => root = r);
        bt = root.find({attributes: {role: "button", name: /Bluetooth/}});
        bt;
        """)
        if bt_present is None:
            return False
        return True

    def bluetooth_check(self, case):
        # Click the status tray button in bottom right.
        ext = self.cr.autotest_ext
        ext.ExecuteJavaScript("""
        chrome.automation.getDesktop(root => {
            var button_to_click = root.find(
                {attributes: {
                    role: "button", name: /Status tray/}}).doDefault();
        });
        """)
        time.sleep(1)

        bluetooth_button = self._is_bluetooth_button_present(ext)

        if case is False:
            if bluetooth_button:
                raise error.TestFail(
                    'Bluetooth option is available and it should not be')
        else:
            if not bluetooth_button:
                raise error.TestFail(
                    'Bluetooth option should be available but it is not.')

    def run_once(self, case):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """
        self.setup_case(
                device_policies={self._POLICY: case},
                enroll=True)
        self.bluetooth_check(case)
