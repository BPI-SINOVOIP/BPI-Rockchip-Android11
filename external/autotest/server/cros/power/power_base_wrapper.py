# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for power measurement tests with telemetry devices."""

import os

from autotest_lib.server import test
from autotest_lib.server.cros.power import wrapper_test_runner


class PowerBaseWrapper(test.test):
    """Base class for a wrapper test around a client test.

    This wrapper test runs 1 client test given by user, and measures DUT power
    with external power measurement tools.
    """
    version = 1

    def run_once(self, host, config):
        """Measure power while running the client side test.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict.
                       required data: {'test': 'test_TestName.tag'}
        """
        test_runner = wrapper_test_runner.WrapperTestRunner(
                config, self.autodir)
        client_test_name = test_runner.get_tagged_test_name()
        ptl = self._get_power_telemetry_logger(host, config, self.resultsdir)
        try:
            ptl.start_measurement()
            test_runner.run_test(host)
        finally:
            client_test_dir = os.path.join(self.outputdir, client_test_name)
            # If client test name is not tagged in its own control file.
            if not os.path.isdir(client_test_dir):
                client_test_name = client_test_name.split('.', 1)[0]
                client_test_dir = os.path.join(self.outputdir, client_test_name)
            ptl.end_measurement(client_test_dir)

        return

    def _get_power_telemetry_logger(self, host, config, resultsdir):
        """Return the corresponding power telemetry logger.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data: {'test': 'test_TestName.tag'}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        """
        raise NotImplementedError('Subclasses must implement '
                '_get_power_telemetry_logger and return the corresponding '
                'power telemetry logger.')
