# Copyright (c) 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from autotest_lib.client.common_lib import error
from autotest_lib.server import autotest
from autotest_lib.server import test
from autotest_lib.server.cros.crosperf import device_setup_utils

WAIT_TIME_LOG = 'wait_time.log'
RESULTS_CHART_JSON = 'results-chart.json'

class crosperf_Wrapper(test.test):
    """
    Client test wrapper for crosperf.

    This is a class to run client tests under the crosperf script.

    """
    version = 1

    def run_once(self, test_name, test_args, dut_config_str, dut=None):
        """
        Run a single telemetry test.

        @param test_name: Name of the client test.
        @param test_args: Arguments need to be passed to test.
        @param dut_config_str: A string dumped from json representing DUT
                               configurations.
        @param dut: The autotest host object representing DUT.

        @returns A result of this execution.

        """
        if not test_name:
            raise RuntimeError('Missing client test name to run.')

        if dut_config_str:
            dut_config = json.loads(dut_config_str)
            # Setup device with dut_config arguments before running test.
            wait_time = device_setup_utils.setup_device(dut, dut_config)
            # Wait time can be used to accumulate cooldown time in Crosperf.
            with open(os.path.join(self.resultsdir, WAIT_TIME_LOG), 'w') as f:
                f.write(str(wait_time))

        try:
            # Execute the client side test.
            client_at = autotest.Autotest(dut)
            result = client_at.run_test(test_name, args=test_args)
        except (error.TestFail, error.TestWarn):
            logging.debug('Test did not succeed while executing client test.')
            raise
        except:
            logging.debug('Unexpected failure on client test %s.', test_name)
            raise

        return result
