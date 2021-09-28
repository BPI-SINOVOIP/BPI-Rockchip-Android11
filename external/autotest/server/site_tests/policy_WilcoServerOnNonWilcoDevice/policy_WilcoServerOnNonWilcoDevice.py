# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class policy_WilcoServerOnNonWilcoDevice(test.test):
    """
    policy_DeviceServer test used to kick off any arbitrary client test.

    """
    version = 1


    def cleanup(self):
        """Cleanup for this test."""
        tpm_utils.ClearTPMIfOwned(self.host)
        self.host.reboot()


    def run_once(self, client_test, host, case=None):
        """
        Starting point of this test.

        Note: base class sets host as self._host.

        @param client_test: the name of the Client test to run.
        @param case: the case to run for the given Client test.

        """
        # Policies to loop through in the client test.
        tests = [{'Policy_Name': 'DeviceBootOnAcEnabled',
                    'Policy_Value': True},
                 {'Policy_Name': 'DevicePowerPeakShiftEnabled',
                    'Policy_Value': True},
                 {'Policy_Name': 'DeviceDockMacAddressSource',
                     'Policy_Value': 1},
                 {'Policy_Name': 'DeviceUsbPowerShareEnabled',
                     'Policy_Value': True},
                 {'Policy_Name': 'DeviceAdvancedBatteryChargeModeEnabled',
                     'Policy_Value': True},
                 {'Policy_Name': 'DeviceBatteryChargeMode',
                     'Policy_Value': 1},
                ]

        self.host = host
        # Clear TPM to ensure that client test can enroll device.
        tpm_utils.ClearTPMIfOwned(self.host)

        self.autotest_client = autotest.Autotest(self.host)
        self.autotest_client.run_test(
            client_test, tests=tests, check_client_result=True)
