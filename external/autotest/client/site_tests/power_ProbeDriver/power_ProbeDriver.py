# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error, utils
from autotest_lib.client.cros.power import power_status, power_utils

class power_ProbeDriver(test.test):
    """Confirms that AC driver is loaded and functioning
    unless device is AC only."""
    version = 1

    def run_once(self, test_which='Mains'):
        # This test doesn't apply to systems that run on AC only.
        if not power_utils.has_battery():
            return
        # Gather power supplies
        status = power_status.get_status()
        run_dict = { 'Mains': self.run_ac, 'Battery': self.run_bat }
        run = run_dict.get(test_which)
        if run:
            run(status)
        else:
            raise error.TestNAError('Unknown test type: %s' % test_which)

    def run_ac(self, status):
        """Checks AC driver.

        @param status: power_status.SysStat object
        """
        if not status.linepower:
            raise error.TestFail('No line power devices found')

        if not status.on_ac():
            raise error.TestFail('Line power is not connected')

        if not status.battery_discharging():
            return

        if status.battery_discharge_ok_on_ac():
            logging.info('DUT battery discharging but deemed ok')
            return

        raise error.TestFail('Battery is discharging')

    def run_bat(self, status):
        """ Checks batteries.

        @param status: power_status.SysStat object
        """
        if not status.battery:
            raise error.TestFail('No battery found')

        if not status.battery.present:
            raise error.TestFail('No battery present')

        if not status.battery_discharging():
            raise error.TestFail('No battery discharging')

        if status.on_ac():
            raise error.TestFail('One of ACs is online')
