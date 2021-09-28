# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Server side test that controls charging the DUT with Servo v4."""

from autotest_lib.server import autotest
from autotest_lib.server import test
from autotest_lib.server.cros.power import servo_charger

# Max number of hours the DUT can charge for.
_MAX_HOURS = 3


class power_BatteryChargeControl(test.test):
    """Server side test that controls charging the DUT.

    This server side test first makes sure that the attached Servo v4 can
    control charging the DUT, then puts the DUT on AC to charge, and finally
    disconnect DUT from AC.
    """
    version = 1

    def run_once(self, host, percent_charge_to_add, percent_target_charge):
        """Running the test.

        @param host: CrosHost object representing the DUT.
        @param percent_charge_to_add: percentage of the charge capacity charge
                to add. The target charge will be capped at the charge capacity.
        @param percent_target_charge: percentage of the charge capacity target
                charge. The target charge will be capped at the charge capacity.
        """
        servo = host.servo
        charge_manager = servo_charger.ServoV4ChargeManager(host, servo)
        charge_manager.start_charging()

        time_limit = _MAX_HOURS * 60 * 60
        autotest_client = autotest.Autotest(host)
        autotest_client.run_test('power_BatteryCharge',
                                 max_run_time=time_limit,
                                 percent_charge_to_add=percent_charge_to_add,
                                 percent_target_charge=percent_target_charge,
                                 use_design_charge_capacity=False)

        charge_manager.restore_original_setting()
