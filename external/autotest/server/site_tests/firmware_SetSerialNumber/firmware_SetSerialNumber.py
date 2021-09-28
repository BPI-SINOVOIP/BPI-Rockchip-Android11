# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import random
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest

# Serial number cannot contain vowels after the first character, so don't allow
# any vowels in this test.
VALID_CHARS = "BCDFGHJKLMNPQRSTVWXYZ0123456789"

class firmware_SetSerialNumber(FirmwareTest):
    """
    Servo based test to set serial number in firmware.

    Test that setting the serial number in firmware during an onsite RMA works.
    Mainboards for onsite RMA will have WP# asserted, flash WP bit cleared,
    the serial_number in VPD set to all spaces, and be in dev mode. After the
    serial number is set in firmware flash write protect will be enabled and
    the device will be in normal mode.
    """
    version = 1

    def start_serial_number_prompt(self):
        """Repeatedly press ctrl-s to bring up serial number prompt."""
        logging.info("Pressing Ctrl-S.")
        timeout = time.time() + (self.faft_config.firmware_screen)
        while time.time() < timeout:
            self.servo.set_nocheck('ctrl_s', 'tab')
            time.sleep(1)

    def generate_serial_number_string(self):
        """Generate a valid serial number based on config"""
        return ''.join(random.choice(VALID_CHARS)
                       for _ in range(self.faft_config.serial_number_length))

    def send_string(self, keys):
        """Send a string over a servo"""
        for key in keys.lower():
            self.servo.set_nocheck('arb_key_config', key)
            self.servo.set_nocheck('arb_key', 'tab')

    def initialize(self, host, cmdline_args):
        super(firmware_SetSerialNumber, self).initialize(host, cmdline_args)

        if self.faft_config.serial_number_length == 0:
            raise error.TestNAError("This test is only valid on devices where "
                                    "serial number can be set in firmware.")

        # This test is run on developer mode only.
        self.switcher.setup_mode('dev')

        # Disable fw wp to clear VPD value
        self.set_hardware_write_protect(False)

        self.faft_client.system.run_shell_command_get_output(
            'flashrom -p host --wp-disable')

        self.faft_client.system.run_shell_command_get_output(
            'vpd -i RO_VPD -s serial_number="%s"' %
            (' ' * self.faft_config.serial_number_length)
        )

        self.set_hardware_write_protect(True)

    def run_once(self):
        """Run the test"""
        serial_number = self.generate_serial_number_string()
        logging.info("Setting serial number to '%s'.", serial_number)

        self.switcher.simple_reboot()
        self.start_serial_number_prompt()
        self.send_string(serial_number)
        # Press enter on set
        self.servo.enter_key()
        # Press enter on confirm
        self.servo.enter_key()
        self.switcher.wait_for_client()

        # Check that device is no longer in dev mode
        self.checkers.mode_checker('normal')

        # Check that serial_number is correctly set
        result = self.faft_client.system.run_shell_command_get_output(
            'vpd -i RO_VPD -g serial_number')[0]

        if result != serial_number:
            raise error.TestFail("Expected serial number '%s' but got '%s'" % (
                serial_number, result))

        # Check that write protect is correctly set
        result = self.faft_client.system.run_shell_command_get_output(
            'flashrom -p host --wp-status')

        result = '\n'.join(result)

        if ('is disabled' in result or
                'start=0x00000000' in result or
                'len=0x00000000' in result):
           raise error.TestFail('Expected write protection to be enabled '
                                'but output was:\n\n%s' % result)

    def cleanup(self):
        self.servo.set_nocheck('fw_wp_state', 'reset')
        super(firmware_SetSerialNumber, self).cleanup()
