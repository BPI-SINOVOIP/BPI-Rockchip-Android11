# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper test measures DUT power with Sweetberry via powerlog tool."""

from autotest_lib.server.cros.power import power_base_wrapper
from autotest_lib.server.cros.power import power_telemetry_logger
from autotest_lib.server.cros.power import servo_charger


class power_PowerlogWrapper(power_base_wrapper.PowerBaseWrapper):
    """Wrapper test around a client test.

    This wrapper test runs 1 client test given by user, and measures DUT power
    with Sweetberry via powerlog tool.
    """
    version = 1

    def warmup(self, host, charge_control=False):
        """Disconnect DUT from AC power.

        Many power autotests require that DUT is on battery, thus disconnect DUT
        from AC power as preparation.
        """
        super(power_PowerlogWrapper, self).warmup(host)
        if charge_control:
            self._charge_manager = servo_charger.ServoV4ChargeManager(
                    host, host.servo)
            self._charge_manager.stop_charging()

    def cleanup(self, charge_control=False):
        """Connect DUT to AC power.

        This allows DUT to charge between tests, and complies with moblab
        requirement.
        """
        if charge_control:
            self._charge_manager.start_charging()
        super(power_PowerlogWrapper, self).cleanup()

    def _get_power_telemetry_logger(self, host, config, resultsdir):
        """Return powerlog telemetry logger.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data: {'test': 'test_TestName.tag'}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        """
        return power_telemetry_logger.PowerlogTelemetryLogger(config,
                                                              resultsdir,
                                                              host)
