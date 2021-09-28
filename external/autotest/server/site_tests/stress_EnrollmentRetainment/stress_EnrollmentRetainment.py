# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.server import autotest
from autotest_lib.server import test


class stress_EnrollmentRetainment(test.test):
    """Used to kick off policy_EnrollmentRetainment test."""
    version = 1


    def run_once(self, client_test, host, loops):
        """
        Starting point of this test.

        @param client_test: the name of the Client test to run.
        @param host: the host machine running the test.
        @param loops: how many times to loop the test.

        """
        self.autotest_client = autotest.Autotest(host)

        for i in xrange(loops):
            logging.info('Starting loop #%d', i)
            self.autotest_client.run_test(
                client_test, check_client_result=True)
            host.reboot()
