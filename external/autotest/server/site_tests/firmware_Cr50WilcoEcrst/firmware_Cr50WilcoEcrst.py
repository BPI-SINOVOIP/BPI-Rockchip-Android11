# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50WilcoEcrst(Cr50Test):
    """Make sure Cr50's ecrst command works as intended.

    EC_RST_L needs to be able to hold the EC in reset. This test verifies the
    hardware works as intended.
    """
    version = 1

    # How long to hold 'ecrst on', in seconds
    SHORT_PULSE = 1


    def initialize(self, host, cmdline_args, full_args):
        super(firmware_Cr50WilcoEcrst, self).initialize(host, cmdline_args,
                full_args)
        if not self.faft_config.gsc_can_wake_ec_with_reset:
            raise error.TestNAError("This DUT has a hardware limitation that "
                                    "prevents cr50 from waking the EC with "
                                    "EC_RST_L.")
        # This test only makes sense with a Wilco EC.
        if self.check_ec_capability():
            raise error.TestNAError("Nothing needs to be tested on this device")

        # Open Cr50, so the test has access to ecrst.
        self.fast_open(True)


    def cr50_ecrst(self, state):
        """Set ecrst on Cr50."""
        self.cr50.send_command('ecrst ' + state)


    def make_ec_reset_bring_up_ap(self):
        """Force the AP to come back up after the next EC reset.

        This is not the default behavior on Wilco. The AFTERG3_EN bit in the
        GEN_PMCON_A register is set by default, which causes the AP to remain
        down after an EC reset. Clearing it will cause the AP to come after the
        next EC reset, at which point Coreboot will set it again, making the
        change in behavior temporary.
        """
        GEN_PMCON_A_ADDRESS = 0xfe001020
        AFTERG3_EN_BIT = 0x1
        orig_reg_string = self.faft_client.system.run_shell_command_get_output(
                'iotools mmio_read32 %#x' % (GEN_PMCON_A_ADDRESS))
        logging.info('iotools output: %s', orig_reg_string)
        orig_reg_value = int(orig_reg_string[0], 0)
        if orig_reg_value > 0xffffffff:
            raise error.TestError(
                    'iotools mmio_read32 returned a value larger than 32 bits')
        new_reg_value = orig_reg_value & ~AFTERG3_EN_BIT
        self.faft_client.system.run_shell_command('iotools mmio_write32 %#x %#x'
                % (GEN_PMCON_A_ADDRESS, new_reg_value))


    def check_ecrst_on_off(self):
        """Verify Cr50 can hold the EC in reset."""
        self.make_ec_reset_bring_up_ap()

        self.cr50_ecrst('on')
        # There isn't a good way to directly tell if the EC is in reset, so
        # verify that the AP has gone down.
        self.switcher.wait_for_client_offline()
        time.sleep(self.SHORT_PULSE)

        self.cr50_ecrst('off')
        self.switcher.wait_for_client()


    def check_ecrst_pulse(self):
        """Verify Cr50 can reset the EC with a pulse."""
        self.make_ec_reset_bring_up_ap()

        orig_boot_id = self.get_bootid()
        self.cr50_ecrst('pulse')
        self.switcher.wait_for_client_offline(orig_boot_id=orig_boot_id)
        self.switcher.wait_for_client()


    def run_once(self):
        """Run the test."""
        self.check_ecrst_on_off()
        self.check_ecrst_pulse()
