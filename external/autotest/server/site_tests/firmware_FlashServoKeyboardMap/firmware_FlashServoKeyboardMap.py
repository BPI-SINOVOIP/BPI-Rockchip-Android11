# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server.cros import servo_keyboard_utils


class firmware_FlashServoKeyboardMap(test.test):
    """A test to flash the keyboard map on servo."""
    version = 1

    _ATMEGA_RESET_DELAY = 0.2
    _USB_PRESENT_DELAY = 1

    def run_once(self, host=None):
        """Body of the test."""

        servo = host.servo
        if host.run('hash dfu-programmer', ignore_status=True).exit_status:
            raise error.TestError(
                'The image is too old that does not have dfu-programmer.')

        try:
            servo.set_nocheck('init_usb_keyboard', 'on')

            # Check the result of lsusb.
            time.sleep(self._USB_PRESENT_DELAY)
            lsusb_cmd = ('lsusb -d ' +
                         servo_keyboard_utils.ATMEL_USB_VENDOR_ID + ':')
            result = host.run(lsusb_cmd).stdout.strip()
            if ('LUFA Keyboard Demo' in result and
                    servo_keyboard_utils.is_servo_usb_wake_capable(host)):
                logging.info('Already using the new keyboard map.')
                return

             # Boot AVR into DFU mode by enabling the HardWareBoot mode
             # strapping and reset.
            servo.set_get_all(['at_hwb:on',
                               'atmega_rst:on',
                               'sleep:%f' % self._ATMEGA_RESET_DELAY,
                               'atmega_rst:off',
                               'sleep:%f' % self._ATMEGA_RESET_DELAY,
                               'at_hwb:off'])

            result = host.run(lsusb_cmd).stdout.strip()
            if not 'Atmel Corp. atmega32u4 DFU bootloader' in result:
                message = 'Not an expected chip: %s' % result
                logging.error(message)
                raise error.TestFail(message)

            # Update the keyboard map.
            local_path = os.path.join(self.bindir, 'test_data', 'keyboard.hex')
            host.send_file(local_path, '/tmp')
            logging.info('Updating the keyboard map...')
            host.run('dfu-programmer atmega32u4 erase --force')
            host.run('dfu-programmer atmega32u4 flash /tmp/keyboard.hex')

            # Reset the chip.
            servo.set_get_all(['atmega_rst:on',
                               'sleep:%f' % self._ATMEGA_RESET_DELAY,
                               'atmega_rst:off'])

            # Check the result of lsusb.
            time.sleep(self._USB_PRESENT_DELAY)
            result = host.run(lsusb_cmd).stdout.strip()
            if 'LUFA Keyboard Demo' in result:
                logging.info('Update successfully!')
            else:
                message = 'Update failed; got the result: %s' % result
                logging.error(message)
                raise error.TestFail(message)

        finally:
            # Restore the default settings.
            # Select the chip on the USB mux unless using Servo V4
            if 'servo_v4' not in servo.get_servo_version():
                servo.set('usb_mux_sel4', 'on')
