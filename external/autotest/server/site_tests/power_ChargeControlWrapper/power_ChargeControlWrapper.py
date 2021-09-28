# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper test that controls charging / discharging DUT with Servo v4."""

from autotest_lib.server import test
from autotest_lib.server.cros.power import servo_charger
from autotest_lib.server.cros.power import wrapper_test_runner


class power_ChargeControlWrapper(test.test):
    """Base class for a wrapper test around a client test.

    This wrapper test runs 1 client test given by user, and controls charging /
    discharging the DUT with Servo v4.
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
        test_runner.run_test(host)

    def warmup(self, host):
        """Disconnect DUT from AC power.

        Many power autotests require that DUT is on battery, thus disconnect DUT
        from AC power as preparation.
        """
        super(power_ChargeControlWrapper, self).warmup(host)
        self._charge_manager = servo_charger.ServoV4ChargeManager(host,
                                                                  host.servo)
        self._charge_manager.stop_charging()

    def cleanup(self):
        """Connect DUT to AC power.

        This allows DUT to charge between tests, and complies with moblab
        requirement.
        """
        self._charge_manager.start_charging()
        super(power_ChargeControlWrapper, self).cleanup()
