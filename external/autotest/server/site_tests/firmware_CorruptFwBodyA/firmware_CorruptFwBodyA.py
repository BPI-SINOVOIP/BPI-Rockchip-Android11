# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.faft.firmware_test import ConnectionError


class firmware_CorruptFwBodyA(FirmwareTest):
    """
    Servo based firmware body A corruption test.

    The RW firmware A corruption will result booting the firmware B.
    """
    version = 1

    def initialize(self, host, cmdline_args, dev_mode=False):
        super(firmware_CorruptFwBodyA, self).initialize(host, cmdline_args)
        self.backup_firmware()
        self.switcher.setup_mode('dev' if dev_mode else 'normal')
        self.setup_usbkey(usbkey=False)

    def cleanup(self):
        try:
            if self.is_firmware_saved():
                self.restore_firmware()
        except ConnectionError:
            logging.error("ERROR: DUT did not come up.  Need to cleanup!")
        super(firmware_CorruptFwBodyA, self).cleanup()

    def run_once(self):
        """Runs a single iteration of the test."""
        logging.info("Corrupt firmware body A.")
        self.check_state((self.checkers.fw_tries_checker, 'A'))
        self.faft_client.bios.corrupt_body('a')
        self.switcher.mode_aware_reboot()

        logging.info("Expected firmware B boot and restore firmware A.")
        self.check_state((self.checkers.fw_tries_checker, ('B', False)))
        self.faft_client.bios.restore_body('a')
        self.switcher.mode_aware_reboot()

        expected_slot = 'B' if self.fw_vboot2 else 'A'
        logging.info("Expected firmware %s boot, done", expected_slot)
        self.check_state((self.checkers.fw_tries_checker, expected_slot))
