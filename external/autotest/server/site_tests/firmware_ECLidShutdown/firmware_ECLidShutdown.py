# Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros import vboot_constants as vboot
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_ECLidShutdown(FirmwareTest):
    """
    Exercises the GBB_FLAG_DISABLE_LID_SHUTDOWN flag: confirms that when the
    flag is enabled, a closed lid will not prevent booting.
    """
    version = 1

    # Delay to allow for a power state change after closing or opening the lid.
    # This value is determined experimentally.
    LID_POWER_STATE_DELAY = 5
    POWER_STATE_CHECK_TRIES = 1
    IGNORE_LID_IN_USERSPACE_CMD = 'echo 0 > /var/lib/power_manager/use_lid'
    CHECK_POWER_MANAGER_CFG_DEFAULT = '[ ! -f /var/lib/power_manager/use_lid ]'

    def initialize(self, host, cmdline_args):
        super(firmware_ECLidShutdown, self).initialize(host, cmdline_args)
        self.setup_usbkey(usbkey=False)

    def cleanup(self):
        """Reopens the lid, boots the DUT to normal mode, and clears the GBB
        flag manipulated in this test.
        """
        try:
            self._reset_ec_regexp()
            logging.info('The screen should turn back on now, during cleanup.')
            self.servo.set('lid_open', 'yes')
            time.sleep(self.LID_POWER_STATE_DELAY)
            self.switcher.simple_reboot(sync_before_boot=False)
            self.switcher.wait_for_client()
            self.clear_set_gbb_flags(vboot.GBB_FLAG_DISABLE_LID_SHUTDOWN, 0)
            self.faft_client.system.run_shell_command(
                    self.CHECK_POWER_MANAGER_CFG_DEFAULT)
        except Exception as exc:
            logging.debug(
                    'Exception during cleanup considered non-fatal.  '
                    'Traceback:', exc_info=True)
        super(firmware_ECLidShutdown, self).cleanup()

    def _reset_ec_regexp(self):
        """Resets ec_uart_regexp field.

        Needs to be done for the ec_uart_regexp otherwise
        dut-control command will time out due to no match.
        """
        self.servo.set('ec_uart_regexp', 'None')

    def _check_lid_shutdown(self):
        """
        Checks behavior with GBB_FLAG_DISABLE_LID_SHUTDOWN disabled.

        Clears the flag that disables listening to the lid for shutdown (when
        the lid is closed at boot), boots into recovery mode and confirms that
        the device stays in a mechanical off state.  Reopens the lid and
        restores the device.
        """
        self.clear_set_gbb_flags(vboot.GBB_FLAG_DISABLE_LID_SHUTDOWN, 0)
        # TODO(kmshelton): Simplify to not use recovery mode.
        self.faft_client.system.request_recovery_boot()
        self.servo.set('lid_open', 'no')
        time.sleep(self.LID_POWER_STATE_DELAY)
        self.switcher.simple_reboot(sync_before_boot=False)
        # Some boards may reset the lid_open state when AP reboot,
        # check b/137612865
        if self.servo.get('lid_open') != 'no':
            self.servo.set('lid_open', 'no')
            time.sleep(self.LID_POWER_STATE_DELAY)
        time.sleep(self.faft_config.firmware_screen)
        if not self.wait_power_state('G3', self.POWER_STATE_CHECK_TRIES):
            raise error.TestFail('The device did not stay in a mechanical off '
                                 'state after a lid close and a warm reboot.')
        self.servo.set('lid_open', 'yes')
        time.sleep(self.LID_POWER_STATE_DELAY)
        time.sleep(self.faft_config.firmware_screen)
        self.switcher.simple_reboot(sync_before_boot=False)
        self._reset_ec_regexp()
        self.switcher.wait_for_client()

        return True

    def _check_lid_shutdown_disabled(self):
        """
        Checks behavior with GBB_FLAG_DISABLE_LID_SHUTDOWN enabled.

        Sets the flag that disables listening to the lid for shutdown (when
        the lid is closed at boot), disables the power daemon, closes the lid,
        boots to a firmware screen.
        """
        self.clear_set_gbb_flags(0, vboot.GBB_FLAG_DISABLE_LID_SHUTDOWN)
        self.faft_client.system.request_recovery_boot()
        self.faft_client.system.run_shell_command(
                self.IGNORE_LID_IN_USERSPACE_CMD)
        self.servo.set('lid_open', 'no')
        if not self.wait_power_state('S0', self.POWER_STATE_CHECK_TRIES):
            raise error.TestError(
                    'The device did not stay in S0 when the lid was closed '
                    'with powerd ignoring lid state.  Maybe userspace power '
                    'management behaviors have changed.')
        self.switcher.simple_reboot(sync_before_boot=False)
        if self.servo.get('lid_open') != 'no':
            self.servo.set('lid_open', 'no')
            time.sleep(self.LID_POWER_STATE_DELAY)
        logging.info('The screen will remain black, even though we are at a '
                     'recovery screen.')
        time.sleep(self.faft_config.firmware_screen)
        if not self.wait_power_state('S0', self.POWER_STATE_CHECK_TRIES):
            raise error.TestFail(
                    'The device did get to an active state when warm reset '
                    'with the lid off and GBB_FLAG_DISABLE_LID_SHUTDOWN '
                    'enabled.')

        return True

    def run_once(self):
        """Runs a single iteration of the test."""
        if not self.check_ec_capability(['lid']):
            raise error.TestNAError('This device needs a lid to run this test')

        logging.info('Verify the device does shutdown when verified boot '
                     'starts while the lid is closed and '
                     'GBB_FLAG_DISABLE_LID_SHUTDOWN disabled.')
        self.check_state(self._check_lid_shutdown)

        logging.info('Verify the device does not shutdown when verified boot '
                     'starts while the lid is closed with '
                     'GBB_FLAG_DISABLE_LID_SHUTDOWN enabled.')
        self.check_state(self._check_lid_shutdown_disabled)
