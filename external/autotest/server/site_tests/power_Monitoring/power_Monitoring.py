# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrap test suite with power_ServodWrapper and run in a time-limited loop."""

import logging
import random
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import retry
from autotest_lib.server import test
from autotest_lib.server.cros.dynamic_suite import suite
from autotest_lib.server.cros.power import servo_charger

# Timeout in seconds to attempt to ping the DUT.
DUT_PINGABLE_TIMEOUT_S = 30.0
# Delay in seconds before reattempting to ping the DUT.
VERIFY_DUT_PINGABLE_DELAY_S = 10.0
# Number of times to attempt to ping (and reset on failure) the DUT.
VERIFY_DUT_PINGABLE_TRIES = 3
# Timeout in minutes to supply to retry decorator for dut pingable verifiction.
VERIFY_DUT_PINGABLE_TIMEOUT_MIN = ((DUT_PINGABLE_TIMEOUT_S +
                                    VERIFY_DUT_PINGABLE_DELAY_S) / 60.0 *
                                   (VERIFY_DUT_PINGABLE_TRIES + 1))
# Delay in seconds before reattempting to query DUT EC for battery charge after
# reset.
GET_CHARGE_WITH_RESET_DELAY_S = 3.0
# Number of times to attempt to get the battery charge (and reset on failure)
# the DUT.
GET_CHARGE_WITH_RESET_TRIES = 3
# Timeout in minutes to supply to retry decorator to get charge with hard reset.
GET_CHARGE_WITH_RESET_TIMEOUT_MIN = (GET_CHARGE_WITH_RESET_DELAY_S / 60.0 *
                                     (GET_CHARGE_WITH_RESET_TRIES + 1))
# Delay in seconds before reattempting to query DUT EC for battery charge after
# timeout/error from EC console
GET_CHARGE_DELAY_S = 0.1
# Number of times to attempt to get the battery charge from the DUT.
GET_CHARGE_TRIES = 3
# Timeout in minutes to supply to retry decorator to get charge.
GET_CHARGE_TIMEOUT_MIN = (GET_CHARGE_DELAY_S / 60.0 *
                          (GET_CHARGE_TRIES + 1))
# Delay in seconds before attempting to query EC again after reset.
EC_RESET_DELAY_S = 3.0
# Delay in seconds to allow the system to detect AP shutdown.
AP_SHUTDOWN_DELAY_S = 10.0
# Polling rate in seconds to poll battery charge to determine charging complete.
CHARGE_STATE_POLLING_RATE_S = 60.0

class power_Monitoring(test.test):
    """Time limited loop for running power suites with power_ServodWrapper.

    This test runs a given suite of client tests with power_ServodWrapper
    for a given amount of time, ensuring that if the battery level drops
    below a configurable percentage it will be charged back up to a configurable
    percentage.
    """
    version = 1

    # The 'real' server side test to use to wrap power tests.
    WRAPPER_TEST = 'power_ServodWrapper'
    # The default suite of tests to run with monitoring.
    DEFAULT_SUITE = 'power_monitoring'
    # The default runtime in hours for the suite.
    DEFAULT_RUNTIME_HR = 12.0
    # The maximum runtime for monitoring. 6 weeks.
    # Longer command line configurations will be ignored.
    MAX_RUNTIME_HR = 6 * 7 * 24.0
    # The minimum runtime for monitoring.
    # Lower command line configurations will be ignored.
    MIN_RUNTIME_HR = 0.1
    # The default battery charge percent to stop charging the DUT.
    DEFAULT_STOP_CHARGING_LEVEL = 75.0
    # The default battery charge percent to start charging the DUT.
    DEFAULT_START_CHARGING_LEVEL = 25.0
    # The maximum battery charge percent to stop charging the DUT.
    # Higher command line configurations will be ignored.
    MAX_STOP_CHARGING_LEVEL = 95.0
    # The minimum battery charge percent to start charging the DUT.
    # Lower command line configurations will be ignored.
    MIN_START_CHARGING_LEVEL = 4.0
    # Maximum number of consecutive failures allowed before the monitoring
    # is aborted.
    MAX_CONSECUTIVE_FAILURES = 5
    # Max time to charge from 0 to 100%. Plenty of room for slow charging.
    MAX_CHARGING_TIME_HR = 5

    def initialize(self, host, config={}):
        """Setup power monitoring.

        @param host: CrosHost object representing the DUT.
        @param config: the args argument from test_that in a dict.
            -start_charging_level: float, battery level when charging triggers
            -stop_charging_level: float, battery level when charging stops
            -suite: suite to loop over
            -runtime_hr: runtime in hours the monitoring should run
            -pdash_note: note to add to power dashboard upload
        """
        # power_Monitoring is a wrapper around a wrapper - it does not need
        # to collect all the sysinfo information and potentially be stuck
        # there waiting for a dead device to resurface.
        self.job.fast = True
        start = float(config.get('start_charging_level',
                                 self.DEFAULT_START_CHARGING_LEVEL))
        stop = float(config.get('stop_charging_level',
                                self.DEFAULT_STOP_CHARGING_LEVEL))
        self.stop_charging_level = min(stop, self.MAX_STOP_CHARGING_LEVEL)
        self.start_charging_level = max(start, self.MIN_START_CHARGING_LEVEL)
        self._host = host
        servo = host.servo
        self._charger = servo_charger.ServoV4ChargeManager(host, servo)
        self._charger.stop_charging(verify=True)

        # If no suite is defined, run with power_monitoring suite.
        test_suite = config.get('suite', self.DEFAULT_SUITE)
        runtime_hr = float(config.get('runtime_hr', self.DEFAULT_RUNTIME_HR))
        self._runtime_hr = min(max(runtime_hr, self.MIN_RUNTIME_HR),
                               self.MAX_RUNTIME_HR)

        fs_getter = suite.create_fs_getter(self.autodir)
        # Find the wrapper test.
        w_predicate = suite.test_name_equals_predicate(self.WRAPPER_TEST)
        self._wrapper_test = suite.find_and_parse_tests(fs_getter, w_predicate)
        if not self._wrapper_test:
            raise error.TestFail('%r wrapper test not found.' %
                                 self.WRAPPER_TEST)
        # Find the test suite in autotest file system.
        predicate = suite.name_in_tag_predicate(test_suite)
        self._tests = suite.find_and_parse_tests(fs_getter, predicate)
        if not self._tests:
            raise error.TestNAError('%r suite has no tests under it.' %
                                    test_suite)
        # Sort the tests by their name.
        self._tests.sort(key=lambda t: t.name)
        self._pdash_note = config.get('pdash_note', '')

    def run_once(self, host):
        """Measure power while running the client side tests in a loop.

        @param host: CrosHost object representing the DUT.
        """
        # Figure out end timestamp.
        end = time.time() + self._runtime_hr * 60 * 60
        logging.info('Will be looping over the tests: %s. Order will be '
                     'randomized.', ', '.join(t.name for t in self._tests))
        random.shuffle(self._tests)
        wrapper_name = self._wrapper_test[0].name
        logging.debug('Going to run the tests on wrapper %s.', wrapper_name)
        # Keep track of consecutive failures to bail out if it seems that
        # tests are not passing.
        consecutive_failures = test_run = 0
        while time.time() < end:
            # First verify and charge DUT to configured percentage.
            self._verify_and_charge_dut()
            # Use tests as a queue where the first test is always run
            # and inserted in the back again at the end.
            current_test = self._tests.pop(0)
            logging.info('About to run monitoring on %s.', current_test.name)
            wrapper_config = {'test': current_test.name}
            subdir_tag = '%05d' % test_run
            if self._pdash_note:
                wrapper_config['pdash_note'] = self._pdash_note
            try:
                self.job.run_test(wrapper_name, host=host,
                                  config=wrapper_config, subdir_tag=subdir_tag)
                consecutive_failures = 0
            except Exception as e:
                # Broad except as we don't really care about the exception
                # but rather want to make sure that we know how many failures
                # have happened in a row.
                logging.warn('Issue running %s: %s', current_test.name, str(e))
                consecutive_failures += 1
            if consecutive_failures >= self.MAX_CONSECUTIVE_FAILURES:
                raise error.TestFail('%d consecutive failures. Stopping.' %
                                     consecutive_failures)
            test_run += 1
            # Add the test back to the pipe.
            self._tests.append(current_test)

    def cleanup(self):
        """Turn on charging at the end again."""
        self._charger.start_charging(verify=False)

    @retry.retry(error.TestFail, timeout_min=GET_CHARGE_TIMEOUT_MIN,
                 delay_sec=GET_CHARGE_DELAY_S)
    def _get_charge_percent(self):
        """Retrieve battery_charge_percent from the DUT in a retry loop."""
        return float(self._host.servo.get('battery_charge_percent'))

    @retry.retry(error.TestFail, timeout_min=GET_CHARGE_WITH_RESET_TIMEOUT_MIN,
                 delay_sec=GET_CHARGE_WITH_RESET_DELAY_S)
    def _force_get_charge_percent(self):
        """Attempt to get the charge percent through cold reset if necessary."""
        try:
            return self._get_charge_percent()
        except error.TestFail as e:
            logging.warn('Failed to get battery charge levels even ',
                         'after turning on charging. Cold resetting.'
                         'before re-attempting.')
            self._host.servo._power_state.reset()
            # Allow DUT time after cold reset to come back.
            time.sleep(EC_RESET_DELAY_S)
            raise error.TestFail('Failed to get battery charge percent. %s',
                                 str(e))

    def _charge_dut(self):
        """Charge the DUT up."""
        self._charger.start_charging()
        time.sleep(EC_RESET_DELAY_S)
        current_charge = start_charge = self._force_get_charge_percent()
        logging.debug('Charge level of %d%%. Going to charge up.',
                      current_charge)
        charge_time_mltp = max(self.stop_charging_level -
                               current_charge, 0.0)/100
        # Calculate how many seconds to allow at most for charging.
        charge_time = charge_time_mltp * self.MAX_CHARGING_TIME_HR * 60 * 60
        logging.debug('Going to charge for at most %d minutes.',
                      charge_time/60)
        start = time.time()
        end = start + charge_time
        # Shut down the AP to increase charging speed.
        self._host.servo.set_nocheck('ec_uart_cmd', 'apshutdown')
        # Give the EC time to shutdown properly.
        time.sleep(AP_SHUTDOWN_DELAY_S)
        while time.time() < end and (current_charge <
                                     self.stop_charging_level):
            logging.debug('Charge level at %d%%. Continuing to charge '
                          'until %d%%.', current_charge,
                          self.stop_charging_level)
            # Poll once a minute.
            time.sleep(CHARGE_STATE_POLLING_RATE_S)
            current_charge = self._get_charge_percent()
        if current_charge < self.stop_charging_level:
            # Restart the AP again before indicating failure.
            self._host.servo.set_nocheck('ec_uart_cmd', 'powerbtn')
            raise error.TestFail('Dut only charged from %d%% to %d%% in %d '
                                 'minutes.' % (start_charge, current_charge,
                                               int((time.time()-start)/60)))
        else:
            logging.debug('Charging finished. Charge at %d%% after %d '
                          'minutes.', current_charge, (time.time() - start))
        self._charger.stop_charging(verify=True)
        # AP was shutdown. Restart it again.
        self._host.servo.set_nocheck('ec_uart_cmd', 'powerbtn')

    @retry.retry(error.TestFail, timeout_min=VERIFY_DUT_PINGABLE_TIMEOUT_MIN,
                 delay_sec=VERIFY_DUT_PINGABLE_DELAY_S)
    def _verify_dut(self):
      """Verify DUT can be pinged. Reset if DUT not responsive."""
      if not self._host.ping_wait_up(timeout=DUT_PINGABLE_TIMEOUT_S):
          self._host.servo._power_state.reset()
          raise error.TestFail('Dut did not reboot or respond to ping after '
                               'charging.')

    def _verify_and_charge_dut(self):
        """Charge DUT up to |stop_charging| level if below |start_charging|."""
        try:
            if self._get_charge_percent() < self.start_charging_level:
                # Only go through the charging sequence if necessary.
                self._charge_dut()
        except error.TestFail:
            # Failure to initially get the charge might be due to the EC being
            # off. Try charging to see whether that recovers the device.
            self._charge_dut()
        self._verify_dut()
