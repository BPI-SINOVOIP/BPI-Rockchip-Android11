# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_ECBootTime(FirmwareTest):
    """
    Servo based EC boot time test.
    """
    version = 1

    def initialize(self, host, cmdline_args):
        super(firmware_ECBootTime, self).initialize(host, cmdline_args)
        # Don't bother if there is no Chrome EC.
        if not self.check_ec_capability():
            raise error.TestNAError("Nothing needs to be tested on this device")
        # Only run in normal mode
        self.switcher.setup_mode('normal')
        self.host = host

    def check_boot_time(self):
        """Check EC and AP boot times"""
        # Initialize a list of two strings, one printed by the EC when the AP
        # is taken out of reset, and another one printed when the EC observes
        # the AP running. These strings are used as for console output anchors
        # when calculating the AP boot time.
        #
        # This is very approximate, a better long term solution would be to
        # have the EC print the same fixed strings for these two events on all
        # platforms. http://crosbug.com/p/21628 has been opened to track this
        # issue.
        if self._x86:
            boot_anchors = ["\[([0-9\.]+) PB", "\[([0-9\.]+) .*(HC 0x|Port 80|ACPI query)"]
        elif self._arm_legacy:
            boot_anchors = ["\[([0-9\.]+) AP running ...",
                            "\[([0-9\.]+) XPSHOLD seen"]
        else:
            boot_anchors = ["\[([0-9\.]+) power state 1 = S5",
                            "\[([0-9\.]+) power state 3 = S0"]

        # regular expression to say that EC is ready. For systems that
        # run out of ram there is a second boot where the PMIC is
        # asked to power cycle the EC to be 100% sure (I wish) that
        # the code is clean. Looking for the "Inits done" generates a
        # match after the first boot, and introduces a race between
        # the EC booting the second time and the test sending the
        # power_cmd.
        if self._doubleboot:
            ec_ready = ["(?ms)UART.*UART.*?\[([0-9.]+) "]
        else:
            ec_ready = ["([0-9.]+) Inits done"]

        if self.faft_config.ec_has_powerbtn_cmd:
            # powerbtn takes ms while hold_pwr_button_powero is seconds.
            hold_ms = int(1000 * self.faft_config.hold_pwr_button_poweron)
            power_cmd = 'powerbtn %s' % hold_ms
        else:
            power_cmd = 'power on'

        # Try the EC reboot command several times in case the console
        # output is not clean enough for the full string to be found.
        retry = 10
        while retry > 0:
            retry = retry - 1
            try:
                reboot = self.ec.send_command_get_output(
                    "reboot ap-off", ec_ready)
                break
            except error.TestFail:
                logging.info("Unable to parse EC console output, "
                             "%d more attempts", retry)
        if retry == 0:
            raise error.TestFail("Unable to reboot EC cleanly, " +
                                 "Please try removing AC power")
        logging.debug("reboot: %r", reboot)

        # The EC console must be available 1 second after startup
        time.sleep(1)

        version = self.ec.get_version()

        if not version:
            raise error.TestFail("Unable to get EC console.")

        # Wait until the ap enter the G3
        time.sleep(self.faft_config.ec_reboot_to_g3_delay)

        # Switch on the AP
        power_press = self.ec.send_command_get_output(
            power_cmd, boot_anchors)

        # TODO(crbug.com/847289): reboot_time only measures the time spent in
        # EC's main function, which is not a good measure of "EC cold boot time"
        reboot_time = float(reboot[0][1])
        power_press_time = float(power_press[0][1])
        firmware_resp_time = float(power_press[1][1])
        boot_time = firmware_resp_time - power_press_time
        logging.info("EC cold boot time: %f s", reboot_time)
        if reboot_time > 1.0:
            raise error.TestFail("EC cold boot time longer than 1 second.")
        logging.info("EC boot time: %f s", boot_time)
        if boot_time > 1.0:
            raise error.TestFail("Boot time longer than 1 second.")

    def is_arm_legacy_board(self):
        """Detect whether the board is a legacy ARM board.

        This group of boards prints specific strings on the EC console when the
        EC and AP come out of reset.
        """

        arm_legacy = ('Snow', 'Spring', 'Pit', 'Pi', 'Big', 'Blaze', 'Kitty')
        output = self.faft_client.system.get_platform_name()
        return output in arm_legacy

    def run_once(self):
        """Execute the main body of the test.
        """

        self._x86 = ('x86' in self.faft_config.ec_capability)
        self._doubleboot = ('doubleboot' in self.faft_config.ec_capability)
        self._arm_legacy = self.is_arm_legacy_board()
        logging.info("Reboot and check EC cold boot time and host boot time.")
        self.switcher.mode_aware_reboot('custom', self.check_boot_time)

    def cleanup(self):
        try:
            # Restore the ec_uart_regexp to None
            self.ec.set_uart_regexp('None')

            # Reboot the EC and wait for the host to come up so it is ready for
            # the next test.
            self.ec.reboot()
            self.host.wait_up(timeout=30)
        except Exception as e:
            logging.error("Caught exception: %s", str(e))
        super(firmware_ECBootTime, self).cleanup()
