# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Server side bluetooth adapter stress tests involving power consumption."""

import logging
import multiprocessing
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests
from autotest_lib.server.cros.multimedia import remote_facade_factory


test_case_log = bluetooth_adapter_tests.test_case_log


class bluetooth_AdapterPowerMeasure(
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """Server side bluetooth adapter power consumption test."""

    # ---------------------------------------------------------------
    # Definitions of all test cases
    # ---------------------------------------------------------------

    @test_case_log
    def test_case_suspend_power_measurement(self, host, max_power_mw,
                                            suspend_time_secs,
                                            resume_network_timeout_secs=60):
        """Test Case: measure the Bluetooth chip power consumption on suspend"""

        def print_debug_count():
            """Print the debug message about count values."""
            logging.debug('count_fail_to_sleep: %d', self.count_fail_to_sleep)
            logging.debug('count_fail_to_resume: %d', self.count_fail_to_resume)
            logging.debug('count_system_resume_prematurely: %d',
                          self.count_system_resume_prematurely)
            logging.debug('count_success: %d', self.count_success)

        def action_suspend():
            """Calls the host method suspend."""
            host.suspend(suspend_time=suspend_time_secs,
                         allow_early_resume=True)

        boot_id = host.get_boot_id()
        proc = multiprocessing.Process(target=action_suspend)
        proc.daemon = True
        start_time = time.time()
        proc.start()

        # Block waiting until the system has suspended.
        try:
            host.test_wait_for_sleep(suspend_time_secs)
        except Exception as e:
            logging.error('host.test_wait_for_sleep failed: %s', e)
            self.count_fail_to_sleep += 1
            print_debug_count()
            # Skip this time since the system failed to suspend.
            proc.join()
            return

        # Test the Bluetooth chip power consumption.
        if self.test_power_consumption(max_power_mw):
            self.count_success += 1

        # Block waiting until the system has resumed.
        try:
            host.test_wait_for_resume(
                    boot_id, suspend_time_secs + resume_network_timeout_secs)
        except Exception as e:
            logging.error('host.test_wait_for_resume failed: %s', e)
            self.count_fail_to_resume += 1

        # If the system resumes prematurely, do not conduct the test in
        # this iteration.
        actual_suspend_time_secs = time.time() - start_time
        if actual_suspend_time_secs < suspend_time_secs:
            logging.error('actual suspension time %f is less than expected %f',
                          actual_suspend_time_secs, suspend_time_secs)
            self.count_system_resume_prematurely += 1

        print_debug_count()
        proc.join()


    def initialize_servod(self):
        """Peform initialize for servod task."""
        self.count_fail_to_sleep = 0
        self.count_fail_to_resume = 0
        self.count_system_resume_prematurely = 0
        self.count_success = 0

        # When the autotest restarts ui, chrome would issue some Bluetooth
        # commands which may prevent the system from suspending properly.
        # Hence, let's stop ui for now.
        self.host.run_short('stop ui')

        board = self.host.get_board().split(':')[1]
        logging.info('board: %s', board)

        # TODO: figure out a way to support other boards.
        if board != 'kukui':
            raise error.TestError('Only kukui is supported for now.')

        # self.device is a pure XMLRPC server running as chameleond
        # on the chameleon host. We need to enable Servod.
        if not self.device.EnableServod(board):
            raise error.TestError('Failed to enable Servod.')

        # Start the Servod process on the chameleon host.
        if not self.device.servod.Start():
            raise error.TestError('Failed to start Servod on chameleon host.')


    def cleanup_servod(self):
        """Peform cleanup for servod."""
        if not self.device.servod.Stop():
            logging.error('Failed to stop Servod on chameleon host.')

        self.host.run_short('start ui')

        logging.info('count_fail_to_sleep: %d', self.count_fail_to_sleep)
        logging.info('count_fail_to_resume: %d', self.count_fail_to_resume)
        logging.info('count_system_resume_prematurely: %d',
                     self.count_system_resume_prematurely)
        logging.info('count_success: %d', self.count_success)


    def run_once(self, host, max_power_mw=3, device_type='BLUETOOTH_BASE',
                 num_iterations=1, suspend_time_secs=30,
                 test_category='suspension'):
        """Running Bluetooth adapter power consumption autotest during system
        suspension.

        @param host: the DUT host.
        @param max_power_mw: max power allowed in milli-watt
        @param device_type: the device type emulated by the chameleon host
        @param num_iterations: number of times to perform the tests.
        @param suspend_time_secs: the system suspension duration in seconds
        @param test_category: the test category

        """

        self.host = host
        factory = remote_facade_factory.RemoteFacadeFactory(host,
                                                            disable_arc=True)
        self.bluetooth_facade = factory.create_bluetooth_hid_facade()

        self.check_chameleon()
        self.device = self.get_device(device_type)
        self.initialize_servod()
        self.test_power_on_adapter()
        self.test_bluetoothd_running()

        for i in xrange(1, num_iterations + 1):
            logging.info('Starting iteration: %d / %d', i, num_iterations)

            if test_category == 'suspension':
                self.test_case_suspend_power_measurement(host, max_power_mw,
                                                         suspend_time_secs)
            else:
                logging.error('Do not support the test category: %s',
                              test_category)

        if device_type == 'BLUETOOTH_BASE':
            self.cleanup_servod()

        if self.count_success == 0:
            raise error.TestError('System failed to suspend/resume.')

        if self.fails:
            raise error.TestFail(self.fails)
