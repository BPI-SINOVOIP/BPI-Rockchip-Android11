# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class platform_CryptohomeTpmLiveTestServer(test.test):
    """A test that runs platform_CryptohomeTpmLiveTest and clears the TPM as
    necessary."""
    version = 1

    def run_once(self, host=None):
        self.client = host

        # Skip the test if the TPM is unavailable.
        tpm_status = tpm_utils.TPMStatus(self.client)
        if 'Enabled' not in tpm_status:
            raise error.TestError('Error obtaining TPM enabled state. Status '
                                  'returned by cryptohome: ' + str(tpm_status))
        if not tpm_status['Enabled']:
            return

        # Clear the TPM, so that the client test is able to obtain the TPM owner
        # password.
        tpm_utils.ClearTPMOwnerRequest(self.client, wait_for_ready=True)

        # Run the client test which executes the cryptohome's TPM live test.
        autotest.Autotest(self.client).run_test(
                'platform_CryptohomeTpmLiveTest', check_client_result=True)

        # Clean the TPM up, so that the TPM state clobbered by the TPM live
        # tests doesn't affect subsequent tests.
        tpm_utils.ClearTPMOwnerRequest(self.client)
