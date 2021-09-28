# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Stress test Servo's charging functionalities. """

import collections
import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server.cros.power import servo_charger


TOTAL_LOOPS = 100
SLEEP = 5
COMMAND_FAIL = -1

class power_ServoChargeStress(test.test):
    """Stress test Servo's charging functionalities.

    This test runs loops to change Servo's PD role.
    """
    version = 1

    def run_once(self, host, total_loops=TOTAL_LOOPS, sleep=SLEEP):
        """Run loops to change Servo's PD role.

        @param host: CrosHost object representing the DUT.
        @param total_loops: total loops of Servo role change.
        @param sleep: seconds to sleep between Servo role change command.
        """
        self._charge_manager = servo_charger.ServoV4ChargeManager(host,
                                                                  host.servo)
        pd_roles = ['snk', 'src']
        total_fail = collections.defaultdict(int)
        total_success_with_recovery = collections.defaultdict(int)
        keyval = dict()

        for loop in range(total_loops):
            logging.info('') # Add newline to make logs easier to read.
            logging.info('Starting loop %d...', loop)

            for role in pd_roles:
                time.sleep(sleep)
                try:
                    # result = -1   failed;
                    # result = 0    successful;
                    # result > 0    successful with 'result' # of recoveries.
                    if role is 'snk':
                        result = self._charge_manager.stop_charging()
                    elif role is 'src':
                        result = self._charge_manager.start_charging()
                    if result > 0: # Recoveries triggered.
                        total_success_with_recovery[role] += 1
                except error.TestBaseException as e:
                    logging.error('Loop %d: %s', loop, str(e))
                    result = COMMAND_FAIL
                    total_fail[role] += 1
                keyval['loop%04d_%s' % (loop, role)] = result

            logging.info('End of loop %d.', loop)

        logging.info('Restore original Servo PD setting...')
        self._charge_manager.restore_original_setting()
        logging.info('End of test.')

        for role in pd_roles:
            keyval['Total_%s_fail' % role] = total_fail[role]
            keyval['Total_%s_success_with_recovery' % role] = \
                    total_success_with_recovery[role]
        keyval['Total_loops'] = total_loops
        self.write_perf_keyval(keyval)
