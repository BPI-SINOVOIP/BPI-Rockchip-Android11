# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.cros.power import power_test
from autotest_lib.client.cros.power import power_utils


class power_WaitForCoolDown(power_test.power_Test):
    """class for power_WaitForCoolDown test."""
    version = 1

    def initialize(self, pdash_note='', seconds_period=20.,
                   force_discharge=False):
        power_utils.set_display_power(power_utils.DISPLAY_POWER_ALL_OFF)
        super(power_WaitForCoolDown, self).initialize(
                seconds_period=seconds_period,
                pdash_note=pdash_note,
                force_discharge=force_discharge)

    def run_once(self, target_temp=48, max_runtime=600):
        """"Look at temperature every |seconds_period| seconds for at most
        |max_runtime| seconds until reported temps do not exceed |target_temp|.

        @param target_temp: Target temperature in celsius.
        @param max_runtime: Maximum runtime in seconds.
        """
        loop_secs = max(1, int(self._seconds_period))
        num_loop = int(max_runtime / loop_secs)
        self.start_measurements()

        for i in range(num_loop):
            max_temp = max(self._tlog.refresh())
            if max_temp <= target_temp:
                logging.info('Cooldown at %d seconds, temp: %.1fC',
                             i * self._seconds_period, max_temp)
                return
            self.loop_sleep(i, loop_secs)

        max_temp = max(self._tlog.refresh())
        logging.warn(
            'Fail to cool down after %d seconds, temp: %.1fC, target: %dC',
            num_loop * loop_secs, max_temp, target_temp)

    def cleanup(self):
        power_utils.set_display_power(power_utils.DISPLAY_POWER_ALL_ON)
        super(power_WaitForCoolDown, self).cleanup()
