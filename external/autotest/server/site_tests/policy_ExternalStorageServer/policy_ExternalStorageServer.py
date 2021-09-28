# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server import autotest
from autotest_lib.server import test


class policy_ExternalStorageServer(test.test):
    """
    This test connects the servo repair USB stick then runs a client-side test
    that relies on a USB being connected.

    """
    version = 1


    def _run_client_test(self, name, case):
        """
        Run client test.

        @raises error.TestFail: In the instance were the client test fails.

        """
        logging.info('Performing %s' % case)
        self.autotest_client.run_test(name,
                                      case=case,
                                      check_client_result=True)


    def cleanup(self):
        """Disconnect USB stick."""
        self.host.servo.switch_usbkey('host')


    def run_once(self, host, client_test=''):
        """
        @param host: A host object representing the DUT.
        @param client_test: Name of client test to run.

        """
        self.host = host
        self.autotest_client = autotest.Autotest(self.host)

        # Connect servo repair USB stick
        self.host.servo.switch_usbkey('dut')

        policy_values = ['True_Block', 'NotSet_Allow', 'False_Allow']

        for case in policy_values:
            self._run_client_test(client_test, case)
