# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import cryptohome


class platform_CryptohomeTpmLiveTest(test.test):
    """Run cryptohome's TPM live tests."""
    version = 1

    def run_once(self):
        cryptohome.take_tpm_ownership(wait_for_ownership=True)

        tpm_owner_password = cryptohome.get_tpm_status()['Password']
        if not tpm_owner_password:
            raise error.TestError('TPM owner password is empty after taking '
                                  'ownership.')

        # Execute the program which runs the actual test cases. When some test
        # cases fail, the program will return with a non-zero exit code,
        # resulting in raising the CmdError exception and failing the autotest.
        utils.system_output('cryptohome-tpm-live-test', retain_output=True,
                            args=['--owner_password=' + tpm_owner_password])
