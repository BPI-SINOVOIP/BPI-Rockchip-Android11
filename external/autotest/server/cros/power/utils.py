# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Power utils for server tests."""

from autotest_lib.server import autotest
from autotest_lib.server.cros.power import servo_charger

def put_host_battery_in_range(host, min_level, max_level, timeout):
    """
    Charges or drains the host's battery to the specified range within the
    timeout. This uses a servo v4 and either the power_BatteryCharge or the
    power_BatteryDrain client test.

    @param host: DUT to use
    @param min_level: battery percentage
    @param max_level: battery percentage
    @param timeout: in seconds

    @throws: A TestFail error if getting the current battery level, setting the
             servo's charge state, or running either of the client tests fails.
    """
    current_level = host.get_battery_display_percentage()
    if current_level >= min_level and current_level <= max_level:
        return

    autotest_client = autotest.Autotest(host)
    charge_manager = servo_charger.ServoV4ChargeManager(host, host.servo)
    if current_level < min_level:
        charge_manager.start_charging()
        autotest_client.run_test('power_BatteryCharge',
                                 max_run_time=timeout,
                                 percent_target_charge=min_level,
                                 use_design_charge_capacity=False)
    if current_level > max_level:
        charge_manager.stop_charging()
        autotest_client.run_test('power_BatteryDrain',
                                 drain_to_percent=max_level,
                                 drain_timeout=timeout)
