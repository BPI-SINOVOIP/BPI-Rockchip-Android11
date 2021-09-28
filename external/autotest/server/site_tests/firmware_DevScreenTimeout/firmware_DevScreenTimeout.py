# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_DevScreenTimeout(FirmwareTest):
    """
    Servo based developer firmware screen timeout test.

    When booting in developer mode, the firmware shows a screen to warn user
    the disk image is not secured. If a user press Ctrl-D or a timeout reaches,
    it will boot to developer mode. This test is to verify the timeout period.

    This test tries to boot the system in developer mode twice.
    The first one will repeatedly press Ctrl-D on booting in order to reduce
    the time on developer warning screen. The second one will do nothing and
    wait the developer screen timeout. The time difference of these two boots
    is close to the developer screen timeout.
    """
    version = 1

    # We accept 5s timeout margin as we need 5s to ensure client is offline.
    # If the margin is too small and firmware initialization is too fast,
    # the test will fail incorrectly.
    TIMEOUT_MARGIN = 5
    RUN_SHELL_READY_TIME_MARGIN = 5

    fw_time_record = {}

    def record_fw_boot_time(self, tag):
        """Record the current firmware boot time with the tag.

        @param tag: A tag about this boot.
        @raise TestError: If the firmware-boot-time file does not exist.
        """
        time.sleep(self.RUN_SHELL_READY_TIME_MARGIN)
        [fw_time] = self.faft_client.system.run_shell_command_get_output(
                'cat /tmp/firmware-boot-time')
        logging.info('Got firmware boot time [%s]: %s', tag, fw_time)
        if fw_time:
            self.fw_time_record[tag] = float(fw_time)
        else:
            raise error.TestError('Failed to get the firmware boot time.')

    def check_timeout_period(self):
        """Check the firmware screen timeout period matches our spec.

        @raise TestFail: If the timeout period does not match our spec.
        """
        # On tablets/detachables, eliminate the time taken for pressing volume
        # down button (HOLD_VOL_DOWN_BUTTON_BYPASS + 0.1) to calculate the
        # firmware boot time of quick dev boot.
        if self.faft_config.mode_switcher_type == 'tablet_detachable_switcher':
            self.fw_time_record['quick_bypass_boot'] -= \
                            (self.switcher.HOLD_VOL_DOWN_BUTTON_BYPASS + 0.1)
            logging.info(
                    "Firmware boot time [quick_bypass_boot] after "
                    "eliminating the volume down button press time "
                    "(%.1f seconds): %s",
                    self.switcher.HOLD_VOL_DOWN_BUTTON_BYPASS + 0.1,
                    self.fw_time_record['quick_bypass_boot'])
        got_timeout = (self.fw_time_record['timeout_boot'] -
                       self.fw_time_record['quick_bypass_boot'])
        logging.info('Estimated developer firmware timeout: %s', got_timeout)

        if (abs(got_timeout - self.faft_config.dev_screen_timeout) >
                    self.TIMEOUT_MARGIN):
            raise error.TestFail(
                    'The developer firmware timeout does not match our spec: '
                    'expected %.2f +/- %.2f but got %.2f.' %
                    (self.faft_config.dev_screen_timeout, self.TIMEOUT_MARGIN,
                     got_timeout))

    def initialize(self, host, cmdline_args):
        super(firmware_DevScreenTimeout, self).initialize(host, cmdline_args)
        # NA error check point for this test
        if self.faft_config.mode_switcher_type not in (
                'keyboard_dev_switcher', 'tablet_detachable_switcher'):
            raise error.TestNAError("This test is only valid on devices with "
                                    "screens.")
        # This test is run on developer mode only.
        self.switcher.setup_mode('dev')
        self.setup_usbkey(usbkey=False)

    def run_once(self):
        """Runs a single iteration of the test."""
        logging.info("Always expected developer mode firmware A boot.")
        self.check_state((self.checkers.crossystem_checker, {
                'devsw_boot': '1',
                'mainfw_act': 'A',
                'mainfw_type': 'developer',
        }))

        # To add an extra reboot before the measurement
        # to avoid TPM reset too long
        self.switcher.simple_reboot()
        self.switcher.wait_for_client()
        logging.info("Reboot and bypass the Developer warning screen "
                     "immediately.")
        self.switcher.simple_reboot()
        self.switcher.bypass_dev_mode()
        self.switcher.wait_for_client()
        logging.info("Record the firmware boot time without waiting for "
                     "firmware screen.")
        self.record_fw_boot_time('quick_bypass_boot')

        logging.info("Reboot the device, do nothing and wait for screen "
                     "timeout. And then record the firmware boot time.")
        self.switcher.simple_reboot()
        self.switcher.wait_for_client()
        # Record the boot time of firmware screen timeout.
        self.record_fw_boot_time('timeout_boot')

        logging.info("Check the firmware screen timeout matches our spec.")
        self.check_timeout_period()
