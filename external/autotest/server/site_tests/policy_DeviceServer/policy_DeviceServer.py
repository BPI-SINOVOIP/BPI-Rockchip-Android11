# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class policy_DeviceServer(test.test):
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

        # Clear TPM to ensure that client test can enroll device.
        self.host = host
        tpm_utils.ClearTPMIfOwned(self.host)

        self.autotest_client = autotest.Autotest(self.host)
        self.autotest_client.run_test(
            client_test, case=case, check_client_result=True)
