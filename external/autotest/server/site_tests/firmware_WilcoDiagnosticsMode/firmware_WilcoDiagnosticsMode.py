# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import os

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_WilcoDiagnosticsMode(FirmwareTest):
    """Corrupt the Wilco diagnostics image and then reinstall it.

    Wilco supports entry into a diagnostics image from recovery mode. The image
    is stored in the RW_LEGACY firmware section and updated during AP firmware
    updates.  Entry into the image should fail if the image is corrupted.
    Updating the firmware should restore the diagnostics image.
    """
    version = 1

    # The delay between pressing <F12> to enter diagnostics mode and reaching
    # the confirmation screen; typically about 10 seconds; overshoot to be safe.
    DIAGNOSTICS_CONFIRM_SCREEN_DELAY_SECONDS = 15
    # The delay between pressing <Power> to confirm entry to diagnostics mode
    # and rebooting into diagnostics mode.
    DIAGNOSTICS_CONFIRM_REBOOT_DELAY_SECONDS = 8
    # The delay between rebooting to enter diagnostics mode and rebooting again
    # if that fails.
    DIAGNOSTICS_FAIL_REBOOT_DELAY_SECONDS = 8
    # The name of the diagnostics image file in CBFS.
    DIAG_CBFS_NAME = 'altfw/diag'

    def initialize(self, host, cmdline_args):
        super(firmware_WilcoDiagnosticsMode, self).initialize(
                host, cmdline_args)

        if not self.faft_config.has_diagnostics_image:
            raise error.TestNAError('No diagnostics image for this board.')

        self.setup_firmwareupdate_shellball(shellball=None)
        # Make sure that the shellball is retained over subsequent power cycles.
        self.blocking_sync()
        self.switcher.setup_mode('normal')

    def cleanup(self):
        self.servo.get_power_state_controller().reset()

        super(firmware_WilcoDiagnosticsMode, self).cleanup()

    def _corrupt_diagnostics_image(self):
        # Extract the diagnostics image from the firmware image, corrupt the
        # image, and write a new firmware image with that corrupt diagnostics
        # image.
        local_filename = 'diag.bin'
        cbfs_work_dir = self.faft_client.updater.cbfs_setup_work_dir()
        bios_cbfs_path = os.path.join(cbfs_work_dir,
                self.faft_client.updater.get_bios_relative_path())
        diag_cbfs_path = os.path.join(cbfs_work_dir, local_filename)

        logging.info('Extracting diagnostics')
        self.faft_client.updater.cbfs_extract_diagnostics(self.DIAG_CBFS_NAME,
                local_filename)

        logging.info('Corrupting diagnostics')
        self.faft_client.updater.corrupt_diagnostics_image(local_filename)

        logging.info('Replacing diagnostics')
        self.faft_client.updater.cbfs_replace_diagnostics(self.DIAG_CBFS_NAME,
                local_filename)

        logging.info('Writing back BIOS')
        self.faft_client.bios.write_whole(bios_cbfs_path)
        self.switcher.mode_aware_reboot()

    def _press_f12(self):
        self.servo.set_nocheck('arb_key_config', '<f12>')
        self.servo.set_nocheck('arb_key', 'tab')

    def _enter_diagnostics_mode(self):
        # Reboot to the recovery screen, press <F12>, and press power to
        # confirm.
        logging.info('Rebooting to recovery screen')
        self.switcher.enable_rec_mode_and_reboot(usb_state='host')
        time.sleep(self.faft_config.firmware_screen)
        logging.info('Pressing <F12>')
        self._press_f12()
        time.sleep(self.DIAGNOSTICS_CONFIRM_SCREEN_DELAY_SECONDS)
        logging.info('Pressing <Power> to confirm')
        self.servo.power_short_press()
        # At this point, the DUT will try to reboot into diagnostics mode.

    def run_once(self):
        """Run the body of the test."""
        logging.info('Attempting to enter diagnostics mode')
        self._enter_diagnostics_mode()
        # Wait long enough that DUT would have rebooted to normal mode if
        # diagnostics mode failed.
        time.sleep(self.DIAGNOSTICS_CONFIRM_REBOOT_DELAY_SECONDS +
                self.DIAGNOSTICS_FAIL_REBOOT_DELAY_SECONDS +
                self.faft_config.delay_reboot_to_ping)
        self.switcher.wait_for_client_offline(timeout=5)
        logging.info('DUT offline after entering diagnostics mode')
        self.servo.get_power_state_controller().reset()
        self.switcher.wait_for_client()

        # Corrupt the diagnostics image, try to reboot into diagnostics mode,
        # and verify that the DUT ends up in normal mode (indicating failure to
        # enter diagnostics mode).
        self._corrupt_diagnostics_image()
        self._enter_diagnostics_mode()
        self.switcher.wait_for_client()
        self.checkers.mode_checker('normal')

        # Update the firmware to restore the diagnostics image, reboot into
        # diagnostics mode, and verify that the DUT goes down (indicating
        # success).
        logging.info('Updating firmware')
        self.faft_client.updater.run_autoupdate(None)
        logging.info('Rebooting to apply firmware update')
        self.switcher.mode_aware_reboot()

        logging.info('Attempting to enter diagnostics mode')
        self._enter_diagnostics_mode()
        # Wait long enough that DUT would have rebooted if diagnostics mode
        # failed.
        time.sleep(self.DIAGNOSTICS_CONFIRM_REBOOT_DELAY_SECONDS +
                self.DIAGNOSTICS_FAIL_REBOOT_DELAY_SECONDS +
                self.faft_config.delay_reboot_to_ping)
        self.switcher.wait_for_client_offline(timeout=5)
        logging.info('DUT offline after entering diagnostics mode')
