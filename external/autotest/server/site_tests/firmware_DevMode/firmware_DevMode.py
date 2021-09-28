# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.servo import chrome_ec
from autotest_lib.server.cros import vboot_constants as vboot


class firmware_DevMode(FirmwareTest):
    """
    Servo based developer firmware boot test.
    """
    version = 1

    def initialize(self, host, cmdline_args, ec_wp=None):
        super(firmware_DevMode, self).initialize(
                host, cmdline_args, ec_wp=ec_wp)
        self.switcher.setup_mode('normal')
        self.setup_usbkey(usbkey=False)

    def bypass_dev_mode_menu_navigation(self):
        """On tablets/detachables, bypassing the developer warning screen
        using menu option navigation method.
        """
        logging.info("-[bypass_dev_mode]-")
        logging.info("Bypassing developer warning screen using Menu "
                     "navigation method.")
        time.sleep(self.faft_config.firmware_screen)
        self.servo.set_nocheck('volume_up_hold', 100)
        time.sleep(self.faft_config.confirm_screen)
        self.servo.set_nocheck('volume_up_hold', 100)
        time.sleep(self.faft_config.confirm_screen)
        self.servo.set_nocheck('volume_up_hold', 100)
        time.sleep(self.faft_config.confirm_screen)
        logging.info("Selecting power button as enter key to select "
                     "'Developer Options'.")
        self.servo.power_short_press()
        time.sleep(self.faft_config.firmware_screen)
        logging.info("Selecting power button as enter key to select "
                     "'Boot Developer Image'.")
        self.servo.power_short_press()

    def run_once(self):
        """Method which actually runs the test."""
        logging.info("Enable dev mode.")
        self.check_state((self.checkers.crossystem_checker, {
                'devsw_boot': '0',
                'mainfw_type': 'normal',
        }))

        self.switcher._enable_dev_mode_and_reboot()
        # To validate the Menu navigation method to bypass the developer
        # warning screen on tablets/detachables.
        if self.faft_config.mode_switcher_type == 'tablet_detachable_switcher':
            self.bypass_dev_mode_menu_navigation()
        else:
            self.switcher.bypass_dev_mode()
        self.switcher.wait_for_client()

        logging.info("Expected developer mode boot and enable normal mode.")
        self.check_state((self.checkers.crossystem_checker, {
                'devsw_boot': '1',
                'mainfw_type': 'developer',
        }))
        self.switcher.reboot_to_mode(to_mode='normal')

        logging.info("Expected normal mode boot, done.")
        self.check_state((self.checkers.crossystem_checker, {
                'devsw_boot': '0',
                'mainfw_type': 'normal',
        }))

        if self.check_ec_capability() and \
                self.faft_config.mode_switcher_type not in (
                    'keyboard_dev_switcher', 'tablet_detachable_switcher'):
            if self.gbb_flags & vboot.GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC:
                # In order to test that entering dev mode does not work when
                # EC_IN_RW=1, EC software sync must be enabled.  If EC software
                # sync is disabled, then we must skip this portion of the test.
                logging.info("Skipping dev mode transition in EC RW test.")
                return

            logging.info("Rebooting into fake recovery mode (EC still in RW).")
            self.servo.get_power_state_controller().power_off()
            self.ec.set_hostevent(chrome_ec.HOSTEVENT_KEYBOARD_RECOVERY)
            self.servo.power_short_press()

            logging.info("Trying to transition to dev mode with EC_IN_RW=1.")
            self.switcher.trigger_rec_to_dev()
            self.switcher.bypass_dev_mode()
            if not self._client.ping_wait_up(
                    timeout=self.faft_config.delay_reboot_to_ping):
                logging.info("DUT didn't come back up (expected!), rebooting.")
                self.switcher.simple_reboot(sync_before_boot=False)
            self.switcher.wait_for_client()

            logging.info("DUT is back up, should still be in normal mode now.")
            self.check_state((self.checkers.crossystem_checker, {
                    'devsw_boot': '0',
                    'mainfw_type': 'normal',
            }))
