# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class firmware_Cr50VirtualNVRamServer(test.test):
    """
    A test that runs firmware_Cr50VirtualNVRam, clearing the TPM first as
    necessary.
    """
    version = 1

    def run_once(self, host=None):
        """Runs a single iteration of the test."""
        self.client = host

        # Skip the test if the TPM is unavailable.
        tpm_status = tpm_utils.TPMStatus(self.client)
        if 'Enabled' not in tpm_status:
            raise error.TestError('Error obtaining TPM enabled state. Status '
                                  'returned by cryptohome: ' + str(tpm_status))
        if not tpm_status['Enabled']:
            raise error.TestNAError("TPM is not enabled")

        # Clear the TPM, so that the client test is able to obtain the TPM owner
        # password.
        tpm_utils.ClearTPMOwnerRequest(self.client, wait_for_ready=True)

        # Run the client test which executes the Cr50VirtualNVRam test.
        autotest.Autotest(self.client).run_test(
                'firmware_Cr50VirtualNVRam', check_client_result=True)

        # Clean the TPM up, so that the TPM state set by the firmware
        # tests doesn't affect subsequent tests.
        tpm_utils.ClearTPMOwnerRequest(self.client)
