# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50WilcoRmaFactoryMode(Cr50Test):
    """Make sure Cr50's factory mode sets GPIOs correctly.

    Cr50 drives the GPIO lines ENTRY_TO_FACT_MODE and SPI_CHROME_SEL, connected
    to the Wilco EC. Normally, ENTRY_TO_FACT_MODE is deasserted, and CHROME_SEL
    is asserted. When Cr50 enters factory mode, they should each switch. When
    Cr50 leaves factory mode, they should go back to the original state.
    """
    version = 1

    # How long to wait while querying H1 GPIO values
    H1_GPIOS_TIMEOUT_SECONDS = 30
    # Masks for bits representing H1 GPIOs in the associated sysfs entry output
    SPI_CHROME_SEL_MASK = 0x02
    ENTRY_TO_FACT_MODE_MASK = 0x01
    # The exit status of a successful shell command
    SUCCESS = 0


    def initialize(self, host, cmdline_args, full_args):
        super(firmware_Cr50WilcoRmaFactoryMode, self).initialize(host,
                cmdline_args, full_args)

        # This test only makes sense with a Wilco EC.
        if self.check_ec_capability():
            raise error.TestNAError('Nothing needs to be tested on this device')
        if not self.cr50.has_command('bpforce'):
            raise error.TestNAError('Cannot run test without bpforce')

        # Switch to dev mode and open CCD, so the test has access to gsctool
        # and bpforce.
        self.fast_open(enable_testlab=True)
        self.switcher.setup_mode('dev')

        # Keep track of whether Cr50 is in factory mode to minimize cleanup.
        self._in_factory_mode = False


    def cleanup(self):
        try:
            self.cr50.set_batt_pres_state(state='follow_batt_pres',
                    atboot=False)
            self.set_factory_mode('disable')
        finally:
            super(firmware_Cr50WilcoRmaFactoryMode, self).cleanup()


    def set_factory_mode(self, state):
        """Enable or disable Cr50's factory mode, using the AP.

        Args:
            state: enable or disable

        Returns:
            True if Cr50 is in the requested state, False otherwise
        """
        if state not in ('enable', 'disable'):
            raise error.TestError('Invalid factory mode state %s', state)
        enable = state == 'enable'

        if self._in_factory_mode == enable:
            logging.debug('Factory mode already in state %s', state)
            return True

        result = cr50_utils.GSCTool(self.host, ('-a', '-F', state),
                ignore_status=True)
        success = result and result.exit_status == self.SUCCESS
        if success and enable:
            self.cr50.wait_for_reboot()
            self.switcher.wait_for_client()

        self._in_factory_mode = enable == success
        return success


    def check_h1_gpios(self, factory_mode):
        """Check that the values of the H1's SPI_CHROME_SEL and
        ENTRY_TO_FACT_MODE GPIOs match the state of Cr50's factory mode.

        Args:
            factory_mode: Whether Cr50 is in factory mode

        Raises:
            TestError if reading the GPIO value from sysfs fails
            TestFail if the GPIOs are set incorrectly for the Cr50 state
        """
        # Reading this value causes the kernel to query the EC and return a
        # value representing the state of the Cr50 GPIOs from the point of view
        # of the EC.
        result = self.host.run('cat /sys/kernel/debug/wilco_ec/h1_gpio',
                timeout=self.H1_GPIOS_TIMEOUT_SECONDS)
        if not result or result.exit_status != self.SUCCESS:
            logging.error(result)
            raise error.TestError('Failed to read H1 GPIOs from EC')

        gpio_value = int(result.stdout, 0)
        spi_chrome_sel = bool(gpio_value & self.SPI_CHROME_SEL_MASK)
        entry_to_fact_mode = bool(gpio_value & self.ENTRY_TO_FACT_MODE_MASK)

        if factory_mode and not (not spi_chrome_sel and entry_to_fact_mode):
            raise error.TestFail('Entered factory mode, but SPI_CHROME_SEL=%s '
                    '(expected False) and ENTRY_TO_FACT_MODE=%s (expected True)'
                    % (spi_chrome_sel, entry_to_fact_mode))
        if not factory_mode and not (spi_chrome_sel and not entry_to_fact_mode):
            raise error.TestFail('Exited factory mode, but SPI_CHROME_SEL=%s '
                    '(expected True) and ENTRY_TO_FACT_MODE=%s (expected False)'
                    % (spi_chrome_sel, entry_to_fact_mode))


    def run_once(self):
        """Run the test."""
        # Try to enter factory mode with battery connected; it should fail.
        in_factory_mode = self.set_factory_mode('enable')
        if in_factory_mode:
            raise error.TestFail(
                    'Able to enter factory mode without disconnecting battery')

        # Try to enter factory mode with battery disconnected; it should succeed
        # and set the H1 GPIOs appropriately.
        self.cr50.set_batt_pres_state(state='disconnect', atboot=False)
        in_factory_mode = self.set_factory_mode('enable')
        if not in_factory_mode:
            raise error.TestFail(
                    'Unable to enter factory mode after disconnecting battery')
        self.check_h1_gpios(factory_mode=True)

        # Exit factory mode; it should succeed and return the H1 GPIOs to their
        # normal state.
        in_factory_mode = not self.set_factory_mode('disable')
        if in_factory_mode:
            raise error.TestFail('Unable to exit factory mode')
        self.check_h1_gpios(factory_mode=False)
