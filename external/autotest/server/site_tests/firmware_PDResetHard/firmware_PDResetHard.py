# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.servo import pd_device


class firmware_PDResetHard(FirmwareTest):
    """
    USB PD hard reset test.

    Hard resets are issued by both ends of the connection. If the DUT
    is dualrole capable, then a power role swap is executed, and the
    test is repeated with the DUT in the opposite power role. Pass
    criteria is that all attempted hard resets are successful.

    """
    version = 1
    RESET_ITERATIONS = 5
    DELAY_BETWEEN_ITERATIONS = 1

    def _test_hard_reset(self, port_pair):
        """Tests hard reset initated by both ends of PD connection

        @param port_pair: list of 2 connected PD devices
        """
        for dev in port_pair:
            for _ in xrange(self.RESET_ITERATIONS):
                try:
                    if dev.hard_reset() == False:
                        raise error.TestFail('Hard Reset Failed')
                    time.sleep(self.DELAY_BETWEEN_ITERATIONS)
                except NotImplementedError:
                    logging.warn('Device cant hard reset ... skipping')
                    break

    def initialize(self, host, cmdline_args, flip_cc=False):
        super(firmware_PDResetHard, self).initialize(host, cmdline_args)
        self.setup_pdtester(flip_cc)
        # Only run in normal mode
        self.switcher.setup_mode('normal')
        # Turn off console prints, except for USBPD.
        self.usbpd.enable_console_channel('usbpd')

    def cleanup(self):
        self.usbpd.send_command('chan 0xffffffff')
        super(firmware_PDResetHard, self).cleanup()

    def run_once(self):
        """Execute Power Role swap test.

        1. Verify that pd console is accessible
        2. Verify that DUT has a valid PD contract
        3. Make sure dualrole mode is enabled on both ends
        4. Test Hard Reset initiated by both ends of connection
        5. Attempt to change power roles
           If power role changed, then retest soft resets
           Else end test.

        """
        # Create list of available UART consoles
        consoles = [self.usbpd, self.pdtester]
        port_partner = pd_device.PDPortPartner(consoles)
        # Identify a valid test port pair
        port_pair = port_partner.identify_pd_devices()
        if not port_pair:
            raise error.TestFail('No PD connection found!')

        # Test hard resets initiated by both ends
        self._test_hard_reset(port_pair)

        # Swap power roles (if possible). Note the pr swap is attempted
        # for both devices in the connection. This ensures that a device
        # such as Plankton, which is dualrole capable, but has this mode
        # disabled by default, won't prevent the device pair from role swapping.
        swappable_dev = None;
        for dev in port_pair:
            try:
                if dev.pr_swap():
                    swappable_dev = dev
                    break
            except NotImplementedError:
                logging.warn('Power role swap not supported on the device')

        if swappable_dev:
            try:
                # Power role has been swapped, retest.
                self._test_hard_reset(port_pair)
            finally:
                # Swap power role again, back to the original
                if not swappable_dev.pr_swap():
                    logging.error('Failed to swap power role to the original')
        else:
            logging.warn('Device pair could not perform power role swap, '
                         'ending test')
