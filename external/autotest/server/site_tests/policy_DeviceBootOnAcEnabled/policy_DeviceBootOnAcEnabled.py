# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class policy_DeviceBootOnAcEnabled(test.test):
    """Test that verifies DeviceBootOnAcEnabled policy.

    If this policy is set to true then boot on AC will always be enabled.

    If this policy is set to false, boot on AC will always be disabled.

    If this policy is left unset, boot on AC is disabled.

    This test has to run on a Wilco device with a servo board.
    """
    version = 1


    def cleanup(self):
        """Clean up DUT.

        Make sure device is on.
        Make sure servo board is supplying device with power.
        Clear TPM.
        """
        self._if_device_off_turn_back_on()
        self.host.servo.set_servo_v4_role('src')
        tpm_utils.ClearTPMIfOwned(self.host)


    def _check_power_discharging(self):
        power_status = self.host.run('cat /sys/class/power_supply/BAT0/status')
        power_status = power_status.stdout.lower().rstrip()
        return power_status == 'discharging'


    def _turn_device_on(self):
        """Turns device back on."""
        self.host.servo.pwr_button()
        time.sleep(1)
        self.host.servo.pwr_button('release')


    def _confirm_dut_off(self):
        """Confirms the DUT is off.

        Note: tried using wait_down instead but the test would just hang.
        """
        if self.host.wait_up(timeout=10):
            raise error.TestError('DUT is on, expected off.')


    def _confirm_dut_on(self):
        """Confirms the DUT is on."""
        if not self.host.wait_up(timeout=20):
            raise error.TestError('DUT is off, expected on.')


    def _if_device_off_turn_back_on(self):
        """Verify device is on, if not turn it on."""
        if not self.host.wait_up(timeout=20):
            self._turn_device_on()


    def run_once(self, host, case):
        """Run the test.

        @param case: the case to run for the given Client test.
        """
        self.host = host
        tpm_utils.ClearTPMIfOwned(self.host)

        self.autotest_client = autotest.Autotest(self.host)
        self.autotest_client.run_test(
            'policy_DeviceBootOnAcEnabled',
            case=case)

        # Stop supplying power from servo, emulating unplugging power.
        self.host.servo.set_servo_v4_role('snk')
        # Verify the dut is running on battery.
        utils.poll_for_condition(
            lambda: self._check_power_discharging(),
            exception=error.TestFail(
                'Device should be running on battery but it is not.'),
            timeout=5,
            sleep_interval=1,
            desc='Polling for power status change.')

        # Turns off the device.
        self.host.servo.power_key('long_press')

        self._confirm_dut_off()

        # Begin supplying power from servo, emulating plugging in power.
        self.host.servo.set_servo_v4_role('src')

        if case is True:
            self._confirm_dut_on()
        else:
            self._confirm_dut_off()
            # Bring device back up.
            self._turn_device_on()
            self._confirm_dut_on()
