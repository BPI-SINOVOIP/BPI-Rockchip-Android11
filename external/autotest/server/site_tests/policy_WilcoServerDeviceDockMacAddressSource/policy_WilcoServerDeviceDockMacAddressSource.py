# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class policy_WilcoServerDeviceDockMacAddressSource(test.test):
    """Test that verifies DeviceDockMacAddressSource policy.

    If the policy is set to 1, dock will grab the designated mac address from
    the device.
    If the policy is set to 2, dock mac address will match the device mac.
    If the policy is set to 3, dock will use its own mac address.

    This test has to run on a Wilco device.

    The way the test is currently setup is: ethernet cable is plugged into the
    device and dock is not plugged into the internet directly. This might
    change later on.
    """
    version = 1


    def cleanup(self):
        """Clean up DUT."""
        tpm_utils.ClearTPMIfOwned(self.host)


    def run_once(self, client_test, host, case):
        """Run the test.

        @param client_test: the name of the Client test to run.
        @param case: the case to run for the given Client test.
        """
        self.host = host
        tpm_utils.ClearTPMIfOwned(self.host)

        self.autotest_client = autotest.Autotest(self.host)
        self.autotest_client.run_test(client_test, case=case)

        self.host.reboot()

        self.autotest_client.run_test(
            client_test, case=case, enroll=False, check_mac=True)
