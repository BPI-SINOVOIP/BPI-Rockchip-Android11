# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper test measures DUT power via servod with Servo devices."""

from autotest_lib.server.cros.power import power_base_wrapper
from autotest_lib.server.cros.power import power_telemetry_logger


class power_ServodWrapper(power_base_wrapper.PowerBaseWrapper):
    """Wrapper test around a client test.

    This wrapper test runs 1 client test given by user, and measures DUT power
    via servod with Servo devices.
    """
    version = 1

    def _get_power_telemetry_logger(self, host, config, resultsdir):
        """Return powerlog telemetry logger.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data:
                       {'test': 'test_TestName.tag',
                        'servo_host': host of servod instance,
                        'servo_port: port that the servod instance is on}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        """
        return power_telemetry_logger.ServodTelemetryLogger(config,
                                                            resultsdir,
                                                            host)
