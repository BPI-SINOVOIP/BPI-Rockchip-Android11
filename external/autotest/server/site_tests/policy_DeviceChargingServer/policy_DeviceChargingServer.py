# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.server import autotest
from autotest_lib.server.cros.enterprise import device_policy_test
from autotest_lib.server.cros.power import servo_charger, utils


class policy_DeviceChargingServer(device_policy_test.DevicePolicyServerTest):
    """
    A variant of DevicePolicyServerTest that verifies charging policy behavior.
    As of this writing, these features are only present on the Wilco platform.

    It requires a Servo v4 USB-C and Servo Micro attached to the DUT.
    """
    version = 1

    # To be in a testable state, the DUT has to have low enough battery that
    # it can charge. Let's give ourselves a buffer for when the battery
    # inevitably charges a bit in the middle of the test.
    MAX_BATTERY_LEVEL = 95

    # Allow 15 minutes for battery to charge or drain to the needed range.
    BATTERY_CHANGE_TIMEOUT = 15 * 60

    def run_once(self, host, client_test, test_cases, min_battery_level,
                 prep_policies):
        """
        Ensures the DUT's battery level is low enough to charge and above the
        specified level, and then runs the specified client test. Assumes any
        TPM stuff is dealt with in the parent class.
        """
        utils.put_host_battery_in_range(host, min_battery_level,
                                        self.MAX_BATTERY_LEVEL,
                                        self.BATTERY_CHANGE_TIMEOUT)
        charger = servo_charger.ServoV4ChargeManager(host, host.servo)
        charger.start_charging()

        autotest_client = autotest.Autotest(host)
        autotest_client.run_test(
                client_test,
                check_client_result=True,
                test_cases=test_cases,
                min_battery_level=min_battery_level,
                prep_policies=prep_policies)
