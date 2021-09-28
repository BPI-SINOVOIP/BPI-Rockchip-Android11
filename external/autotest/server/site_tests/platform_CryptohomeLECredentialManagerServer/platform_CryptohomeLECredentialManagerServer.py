# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import pinweaver_client
from autotest_lib.server import autotest
from autotest_lib.server import test


class platform_CryptohomeLECredentialManagerServer(test.test):
    """Tests the le_credential_manager functionality of cryptohome.
    """

    version = 1

    def run_once(self, host):
        """Runs the platform_CryptohomeLECredentialManager test across a reboot.
        """
        try:
            pinweaver_client.GetLog(host)
        except pinweaver_client.PinWeaverNotAvailableError:
            logging.info('PinWeaver not supported!')
            raise error.TestNAError('PinWeaver is not available')

        autotest.Autotest(host).run_test(
            'platform_CryptohomeLECredentialManager', pre_reboot=True,
            check_client_result=True)

        host.reboot()

        autotest.Autotest(host).run_test(
            'platform_CryptohomeLECredentialManager', pre_reboot=False,
            check_client_result=True)

        logging.info('Tests passed!')
