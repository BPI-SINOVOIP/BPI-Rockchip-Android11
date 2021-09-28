# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import test

class DevicePolicyServerTest(test.test):
    """
    A server test that verifies a device policy, by refreshing the TPM so the
    DUT is not owned, and then kicking off the client test.

    This class takes charge of the TPM initialization and cleanup, but child
    tests should implement their own run_once().
    """
    version = 1

    def warmup(self, host, *args, **kwargs):
        """Clear TPM to ensure that client test can enroll device."""
        super(DevicePolicyServerTest, self).warmup(host, *args, **kwargs)
        self.host = host
        tpm_utils.ClearTPMIfOwned(self.host)

    def cleanup(self):
        """Get the DUT back to a clean state."""
        tpm_utils.ClearTPMIfOwned(self.host)
        super(DevicePolicyServerTest, self).cleanup()
