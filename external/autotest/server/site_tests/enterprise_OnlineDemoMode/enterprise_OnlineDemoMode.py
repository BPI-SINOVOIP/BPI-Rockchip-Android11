# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test

_CLIENT_TEST = 'enterprise_OnlineDemoModeEnrollment'

class enterprise_OnlineDemoMode(test.test):
    """Enrolls to online demo mode."""
    version = 1


    def run_once(self, host):
        """Runs the client test and clears TPM owner on DUT after it's done."""

        client_at = autotest.Autotest(host)
        client_at.run_test(_CLIENT_TEST)
        client_at._check_client_test_result(host, _CLIENT_TEST)

        tpm_utils.ClearTPMOwnerRequest(host)