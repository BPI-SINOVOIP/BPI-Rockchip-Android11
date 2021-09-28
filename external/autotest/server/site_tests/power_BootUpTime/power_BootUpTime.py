# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.server import autotest
from autotest_lib.server import test
from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class power_BootUpTime(FirmwareTest):
    """Checks the device boot up time"""
    version = 1
    _WAIT_TIME_OPEN_LID = _WAIT_TIME_CLOSE_LID = 10

    def initialize(self, host, cmdline_args, ec_wp=None):
        """Autotest initialize method"""
        self.host = host
        super(power_BootUpTime, self).initialize(self.host, cmdline_args,
                                                 ec_wp=ec_wp)
        self.switcher.setup_mode('normal')

    def run_once(self, bootup_time=8, boot_type='reboot'):
        """Checks the boot up time

        @param bootup_time: Expected boot up time
        @param boot_type: Reboot type, Ex: Reboot, lid_close_open etc
        """
        autotest_client = autotest.Autotest(self.host)

        # Assume that running the test case after immediate flash and
        # keeping the DUT at logout screen.
        autotest_client.run_test('login_LoginSuccess', disable_sysinfo=True)
        if boot_type == 'reboot':
            self.host.reboot()
        elif boot_type == 'lid_close_open':
            logging.info("Closing lid")
            self.host.servo.lid_close()
            self.switcher.wait_for_client_offline(
                    timeout=self._WAIT_TIME_CLOSE_LID)
            logging.info('Opening lid')
            self.host.servo.lid_open()
            if not self.faft_config.lid_wake_from_power_off:
                logging.info('Pressing power button')
                self.host.servo.power_normal_press()
            self.switcher.wait_for_client(timeout=self._WAIT_TIME_OPEN_LID)
        else:
            raise error.TestError("Invalid boot_type, check the boot_type"
                                  " in control file")

        autotest_client.run_test('platform_BootPerf', constraints=[
                'seconds_power_on_to_login <= %d' % bootup_time])
