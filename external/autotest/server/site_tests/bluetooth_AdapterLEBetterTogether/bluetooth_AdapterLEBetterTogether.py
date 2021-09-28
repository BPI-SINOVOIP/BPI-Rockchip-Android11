# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Batch of Bluetooth LE tests for Better Together"""

import logging

from autotest_lib.server.cros.bluetooth.bluetooth_adapter_quick_tests import \
     BluetoothAdapterQuickTests
from autotest_lib.server.cros.bluetooth.bluetooth_adapter_pairing_tests import \
     BluetoothAdapterPairingTests

class bluetooth_AdapterLEBetterTogether(BluetoothAdapterQuickTests,
        BluetoothAdapterPairingTests):
    """A Batch of Bluetooth LE tests for Better Together. This test is written
       as a batch of tests in order to reduce test time, since auto-test ramp up
       time is costly. The batch is using BluetoothAdapterQuickTests wrapper
       methods to start and end a test and a batch of tests.

       This class can be called to run the entire test batch or to run a
       specific test only
    """

    BETTER_TOGETHER_SERVICE_UUID = 'b3b7e28e-a000-3e17-bd86-6e97b9e28c11'
    CLIENT_RX_CHARACTERISTIC_UUID = '00000100-0004-1000-8000-001A11000102'
    CLIENT_TX_CHARACTERISTIC_UUID = '00000100-0004-1000-8000-001A11000101'
    CCCD_VALUE_INDICATION = 0x02

    test_wrapper = BluetoothAdapterQuickTests.quick_test_test_decorator
    batch_wrapper = BluetoothAdapterQuickTests.quick_test_batch_decorator

    @test_wrapper('Smart Unlock', devices={'BLE_KEYBOARD':1})
    def smart_unlock_test(self):
        """Simulate the Smart Unlock flow"""

        filter = {'Transport':'le'}
        parameters = {'MinimumConnectionInterval':6,
                      'MaximumConnectionInterval':6}
        device = self.devices['BLE_KEYBOARD'][0]

        if not self.bluetooth_facade.set_discovery_filter(filter):
            logging.error("Failed to set discovery filter")
            return False

        self.test_discover_device(device.address)

        self.test_stop_discovery()

        if not self.bluetooth_facade.set_le_connection_parameters(
                device.address, parameters):
            logging.error("Failed to set LE connection parameters")
            return False

        if not self.bluetooth_facade.pause_discovery():
            logging.error("Failed to pause discovery")
            return False

        self.test_connection_by_adapter(device.address)

        if not self.bluetooth_facade.unpause_discovery():
            logging.error("Failed to unpause discovery")
            return False

        self.test_set_trusted(device.address)

        self.test_service_resolved(device.address)

        self.test_start_notify(device.address,
                               self.CLIENT_RX_CHARACTERISTIC_UUID,
                               self.CCCD_VALUE_INDICATION)

        self.test_stop_notify(device.address,
                              self.CLIENT_RX_CHARACTERISTIC_UUID)

    @batch_wrapper('Better Together')
    def better_together_batch_run(self, num_iterations=1, test_name=None):
        """Run the Bluetooth LE for Better Together test batch or a specific
           given test. The wrapper of this method is implemented in
           batch_decorator. Using the decorator a test batch method can
           implement the only its core tests invocations and let the
           decorator handle the wrapper, which is taking care for whether to
           run a specific test or the batch as a whole, and running the batch
           in iterations

           @param num_iterations: how many iterations to run
           @param test_name: specific test to run otherwise None to run the
                             whole batch
        """
        self.smart_unlock_test()

    def run_once(self, host, num_iterations=1, test_name=None,
                 flag='Quick Sanity'):
        """Run the batch of Bluetooth LE tests for Better Together

        @param host: the DUT, usually a chromebook
        @param num_iterations: the number of rounds to execute the test
        @test_name: the test to run, or None for all tests
        """

        # Initialize and run the test batch or the requested specific test
        self.quick_test_init(host, use_chameleon=True, flag=flag)
        self.better_together_batch_run(num_iterations, test_name)
        self.quick_test_cleanup()
