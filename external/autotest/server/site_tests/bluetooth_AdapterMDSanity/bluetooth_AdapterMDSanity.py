# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Batch of Bluetooth Multiple Devices sanity tests"""

import time
from autotest_lib.server.cros.bluetooth.bluetooth_adapter_quick_tests import \
     BluetoothAdapterQuickTests


class bluetooth_AdapterMDSanity(BluetoothAdapterQuickTests):
    """A Batch of Bluetooth multiple devices sanity tests. This test is written
       as a batch of tests in order to reduce test time, since auto-test ramp up
       time is costy. The batch is using BluetoothAdapterQuickTests wrapper
       methods to start and end a test and a batch of tests.

       This class can be called to run the entire test batch or to run a
       specific test only
    """

    test_wrapper = BluetoothAdapterQuickTests.quick_test_test_decorator
    batch_wrapper = BluetoothAdapterQuickTests.quick_test_batch_decorator


    @test_wrapper('Two connections', devices={'MOUSE':1, 'BLE_MOUSE':1})
    def md_two_connections_test(self):
        """test whether DUT can connect to classic mouse and ble mouse at the
           same time
        """
        devices = [self.devices['MOUSE'][0], self.devices['BLE_MOUSE'][0]]

        for device in devices:
            self.test_discover_device(device.address)
            self.bluetooth_facade.stop_discovery()
            time.sleep(self.TEST_SLEEP_SECS)
            self.test_pairing(device.address, device.pin, trusted=True)
            time.sleep(self.TEST_SLEEP_SECS)
            self.test_connection_by_adapter(device.address)

        #test whether DUT can disconnect and remove pairing to both devices
        for device in devices:
            self.test_disconnection_by_adapter(device.address)
            self.test_remove_pairing(device.address)

    @batch_wrapper('Multiple Devices Sanity')
    def md_sanity_batch_run(self, num_iterations=1, test_name=None):
        """Run the multiple devices sanity test batch or a specific given test.
           The wrapper of this method is implemented in batch_decorator.
           Using the decorator a test batch method can implement the only its
           core tests invocations and let the decorator handle the wrapper,
           which is taking care for whether to run a specific test or the
           batch as a whole, and running the batch in iterations

           @param num_iterations: how many interations to run
           @param test_name: specifc test to run otherwise None to run the
                             whole batch
        """
        self.md_two_connections_test()

    def run_once(self, host, num_iterations=1, test_name=None,
                 flag='Quick Sanity'):
        """Run the batch of Bluetooth stand sanity tests

        @param host: the DUT, usually a chromebook
        @param num_iterations: the number of rounds to execute the test
        """
        # Initialize and run the test batch or the requested specific test
        self.quick_test_init(host, use_chameleon=True, flag=flag)
        self.md_sanity_batch_run(num_iterations, test_name)
        self.quick_test_cleanup()


