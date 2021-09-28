# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import random
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.servo import pd_device


class firmware_PDTrySrc(FirmwareTest):
    """
    Servo based USB PD Try.SRC protocol test.

    When a PD device supports Try.SRC mode and it's enabled, it will attempt
    to always connect as a SRC device. This test is therefore only applicable
    if both devices support dualrole and at least one device supports Try.SRC.

    Pass criteria is that when Try.SRC is enabled the device connects > 95% of
    the time in SRC mode. When it is disabled, there must be at least 25%
    variation in connecting as SRC and SNK.
    """
    version = 1

    CONNECT_ITERATIONS = 20
    PD_DISCONNECT_TIME = 1
    PD_CONNECT_DELAY = 4
    SNK = 0
    SRC = 1
    TRYSRC_OFF_THRESHOLD = 15.0
    TRYSRC_ON_THRESHOLD = 96.0

    def _execute_connect_sequence(self, usbpd_dev, pdtester_dev, trysrc):
        """Execute mulitple connections and track power role

        This method will disconnect/connect a TypeC PD port and
        collect the power role statistics of each connection. The time
        delay for reconnect adds a random delay so that test to increase
        randomness for dualrole swaps.

        @param usbpd_dev: PD device object of DUT
        @param pdtester_dev: PD device object of PDTester
        @param trysrc: True to enable TrySrc before disconnect/connect,
                       False to disable TrySrc, None to do nothing.

        @returns list with number of SNK and SRC connections
        """
        stats = [0, 0]
        random.seed()
        # Try N disconnect/connects
        for attempt in xrange(self.CONNECT_ITERATIONS):
            try:
                # Disconnect time from 1 to 2 seconds
                disc_time = self.PD_DISCONNECT_TIME + random.random()
                logging.info('Disconnect time = %.2f seconds', disc_time)
                # Set the TrySrc value on DUT
                if trysrc is not None:
                    usbpd_dev.try_src(trysrc)
                # Force disconnect/connect
                pdtester_dev.cc_disconnect_connect(disc_time)
                # Wait for connection to be reestablished
                time.sleep(self.PD_DISCONNECT_TIME + self.PD_CONNECT_DELAY)
                # Check power role and update connection stats
                if pdtester_dev.is_snk():
                    stats[self.SNK] += 1;
                    logging.info('Power Role = SNK')
                elif pdtester_dev.is_src():
                    stats[self.SRC] += 1;
                    logging.info('Power Role = SRC')
            except NotImplementedError:
                raise error.TestFail('TrySRC disconnect requires PDTester')
        logging.info('SNK = %d: SRC = %d: Total = %d',
                     stats[0], stats[1], self.CONNECT_ITERATIONS)
        return stats

    def initialize(self, host, cmdline_args, flip_cc=False):
        super(firmware_PDTrySrc, self).initialize(host, cmdline_args)
        self.setup_pdtester(flip_cc)
        # Only run in normal mode
        self.switcher.setup_mode('normal')
        # Turn off console prints, except for USBPD.
        self.usbpd.enable_console_channel('usbpd')

    def cleanup(self):
        self.usbpd.send_command('chan 0xffffffff')
        super(firmware_PDTrySrc, self).cleanup()

    def run_once(self):
        """Execute Try.SRC PD protocol test

        1. Verify that DUT <-> PDTester device pair exists
        2. Verify that DUT supports dualrole
        3. Verify that DUT supports Try.SRC mode
        4. Enable Try.SRC mode, execute disc/connect sequences
        5. Disable Try.SRC mode, execute disc/connect sequences
        6. Compute DUT SRC/SNK connect ratios for both modes
        7. Compare SRC connect ratio to threholds to determine pass/fail
        """

        # Create list of available UART consoles
        consoles = [self.usbpd, self.pdtester]
        # Backup the original dualrole settings
        original_drp = [None, None]
        port_partner = pd_device.PDPortPartner(consoles)
        # Identify PDTester <-> DUT PD device pair
        port_pair = port_partner.identify_pd_devices()
        if not port_pair:
            raise error.TestFail('No DUT to PDTester connection found!')

        # TODO Device pair must have PDTester so that the disconnect/connect
        # sequence does not affect the SRC/SNK connection. PDTester provides
        # a 'fakedisconnect' feature which more closely resembles unplugging
        # and replugging a Type C cable.
        for side in xrange(len(port_pair)):
            original_drp[side] = port_pair[side].drp_get()
            if port_pair[side].is_pdtester:
                # Identify PDTester and DUT device
                p_idx = side
                d_idx = side ^ 1

        try:
            # Both devices must support dualrole mode for this test.
            for port in port_pair:
                try:
                    if not port.drp_set('on'):
                        raise error.TestFail('Could not enable DRP')
                except NotImplementedError:
                    raise error.TestFail('Both devices must support DRP')

            # Check to see if DUT supports Try.SRC mode
            try_src_supported = port_pair[d_idx].try_src(True)

            if not try_src_supported:
                logging.warn('DUT does not support Try.SRC feature. '
                             'Skip running Try.SRC-enabled test case.')
            else:
                # Run disconnect/connect sequence with Try.SRC enabled
                stats_on = self._execute_connect_sequence(
                        usbpd_dev=port_pair[d_idx],
                        pdtester_dev=port_pair[p_idx],
                        trysrc=True)

            # Run disconnect/connect sequence with Try.SRC disabled
            stats_off = self._execute_connect_sequence(
                    usbpd_dev=port_pair[d_idx],
                    pdtester_dev=port_pair[p_idx],
                    trysrc=(False if try_src_supported else None))

            # Compute SNK/(SNK+SRC) ratio (percentage) for Try.SRC off case
            total_off = float(stats_off[self.SNK] + stats_off[self.SRC])
            trysrc_off = float(stats_off[self.SNK]) / total_off * 100.0
            logging.info('SNK ratio with Try.SRC disabled = %.1f%%', trysrc_off)

            # When Try.SRC is off, ideally the SNK/SRC ratio will be close to
            # 50%. However, in practice there is a wide range related to the
            # dualrole swap timers in firmware.
            if (trysrc_off < self.TRYSRC_OFF_THRESHOLD or
                trysrc_off > 100 - self.TRYSRC_OFF_THRESHOLD):
                raise error.TestFail('SRC %% = %.1f: Must be > %.1f & < %.1f' %
                                     (trysrc_off, self.TRYSRC_OFF_THRESHOLD,
                                      100 - self.TRYSRC_OFF_THRESHOLD))

            if try_src_supported:
                # Compute SNK/(SNK+SRC) ratio (percentage) for Try.SRC on case
                total_on = float(stats_on[self.SNK] + stats_on[self.SRC])
                trysrc_on = float(stats_on[self.SNK]) / total_on * 100.0
                logging.info('SNK ratio with Try.SRC enabled = %.1f%%',
                             trysrc_on)

                # When Try.SRC is on, the SRC/SNK, the DUT should connect in SRC
                # mode nearly 100% of the time.
                if trysrc_on < self.TRYSRC_ON_THRESHOLD:
                    raise error.TestFail('SRC %% = %.1f: Must be >  %.1f' %
                                         (trysrc_on, self.TRYSRC_ON_THRESHOLD))
        finally:
            # Reenable Try.SRC mode
            port_pair[d_idx].try_src(True)
            # Restore the original dualrole settings
            for side in xrange(len(port_pair)):
                port_pair[side].drp_set(original_drp[side])
