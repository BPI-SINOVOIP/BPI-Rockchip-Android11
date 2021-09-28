# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50DevMode(Cr50Test):
    """Verify cr50 can tell the state of the dev mode switch."""
    version = 1


    def check_dev_mode(self, dev_mode):
        """Verify the cr50 tpm info matches the devmode state."""
        if self.cr50.in_dev_mode() != dev_mode:
            raise error.TestFail('Cr50 should%s think dev mode is active' %
                    ('' if dev_mode else "n't"))


    def run_once(self):
        """Check cr50 can see dev mode correctly."""
        self.enter_mode_after_checking_tpm_state('normal')
        self.check_dev_mode(False)
        self.enter_mode_after_checking_tpm_state('dev')
        self.check_dev_mode(True)
        self.enter_mode_after_checking_tpm_state('normal')
        self.check_dev_mode(False)
