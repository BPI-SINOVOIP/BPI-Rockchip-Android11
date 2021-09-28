# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.servo import pd_device


class firmware_PDConnect(FirmwareTest):
    """
    Servo based USB PD connect/disconnect test. If PDTester is not one
    of the device pair elements, then this test requires that at least
    one of the devices support dual role mode in order to force a disconnect
    to connect sequence. The test does not depend on the DUT acting as source
    or sink, either mode should pass.

    Pass critera is 100%  of connections resulting in successful connections
    """
    version = 1
    CONNECT_ITERATIONS = 10
    def _test_connect(self, port_pair):
        """Tests disconnect/connect sequence

        @param port_pair: list of 2 connected PD devices
        """
        # Delay in seconds between disconnect and connect commands
        RECONNECT_DELAY = 2
        for dev in port_pair:
            for attempt in xrange(self.CONNECT_ITERATIONS):
                logging.info('Disconnect/Connect iteration %d', attempt)
                try:
                    if dev.drp_disconnect_connect(RECONNECT_DELAY) == False:
                        raise error.TestFail('Disconnect/Connect Failed')
                except NotImplementedError:
                    logging.warn('Device does not support disconnect/connect')
                    break

    def initialize(self, host, cmdline_args, flip_cc=False):
        super(firmware_PDConnect, self).initialize(host, cmdline_args)
        self.setup_pdtester(flip_cc)
        # Only run in normal mode
        self.switcher.setup_mode('normal')
        self.usbpd.enable_console_channel('usbpd')


    def cleanup(self):
        self.usbpd.send_command('chan 0xffffffff')
        super(firmware_PDConnect, self).cleanup()


    def run_once(self):
        """Exectue disconnect/connect sequence test

        """

        # Create list of available UART consoles
        consoles = [self.usbpd, self.pdtester]
        port_partner = pd_device.PDPortPartner(consoles)
        # Identify a valid test port pair
        port_pair = port_partner.identify_pd_devices()
        if not port_pair:
            raise error.TestFail('No PD connection found!')

        # Test disconnect/connect sequences
        self._test_connect(port_pair)

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
                self._test_connect(port_pair)
            finally:
                # Swap power role again, back to the original
                if not swappable_dev.pr_swap():
                    logging.error('Failed to swap power role to the original')
        else:
            logging.warn('Device pair could not perform power role swap, '
                         'ending test')
