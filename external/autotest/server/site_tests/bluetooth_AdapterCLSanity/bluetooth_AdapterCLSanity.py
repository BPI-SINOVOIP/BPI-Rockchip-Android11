# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Batch of of Bluetooth Classic sanity tests"""


from autotest_lib.server.cros.bluetooth.bluetooth_adapter_quick_tests import \
     BluetoothAdapterQuickTests
from autotest_lib.server.cros.bluetooth.bluetooth_adapter_pairing_tests import \
     BluetoothAdapterPairingTests
from autotest_lib.server.cros.bluetooth.bluetooth_adapter_hidreports_tests \
     import BluetoothAdapterHIDReportTests


class bluetooth_AdapterCLSanity(BluetoothAdapterQuickTests,
        BluetoothAdapterPairingTests,
        BluetoothAdapterHIDReportTests):
    """A Batch of Bluetooth Classic sanity tests. This test is written as a batch
       of tests in order to reduce test time, since auto-test ramp up time is
       costly. The batch is using BluetoothAdapterQuickTests wrapper methods to
       start and end a test and a batch of tests.

       This class can be called to run the entire test batch or to run a
       specific test only
    """

    test_wrapper = BluetoothAdapterQuickTests.quick_test_test_decorator
    batch_wrapper = BluetoothAdapterQuickTests.quick_test_batch_decorator


    @test_wrapper('Discovery Test', devices={"MOUSE":1})
    def cl_adapter_discovery_test(self):
        """Performs pairing test with mouse peripheral"""
        device = self.devices['MOUSE'][0]

        self.test_discover_device(device.address)
        self.test_device_name(device.address, device.name)


    @test_wrapper('Pairing Test', devices={"MOUSE":1})
    def cl_adapter_pairing_test(self):
        """Performs pairing test with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.pairing_test(device,
                          check_connected_method=\
                          self.test_mouse_right_click)


    @test_wrapper('keyboard Pairing Test', devices={"KEYBOARD":1})
    def cl_adapter_keyboard_pairing_test(self):
        """Performs pairing test with keyboard peripheral"""
        device = self.devices['KEYBOARD'][0]
        self.pairing_test(device,
                          check_connected_method=\
                          self.run_keyboard_tests)


    @test_wrapper('Pairing Suspend Resume Test', devices={"MOUSE":1})
    def cl_adapter_pairing_suspend_resume_test(self):
        """Performs pairing test over resume with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.pairing_test(device,
                          check_connected_method=\
                          self.test_mouse_right_click,
                          suspend_resume=True)


    @test_wrapper('Pairing Twice Test', devices={"MOUSE":1})
    def cl_adapter_pairing_twice_test(self):
        """Performs pairing twice test with  mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.pairing_test(device,
                          check_connected_method=\
                          self.test_mouse_right_click,
                          pairing_twice=True)


    @test_wrapper('HID Reports Test', devices={"MOUSE":1})
    def cl_HID_reports_test(self):
        """Performs HID report test with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.run_hid_reports_test(device)


    @test_wrapper('HID keyboard Reports Test', devices={'KEYBOARD':1})
    def cl_HID_keyboard_reports_test(self):
        """Performs HID report test with keyboard peripheral"""
        device = self.devices['KEYBOARD'][0]
        self.run_hid_reports_test(device)


    @test_wrapper('HID Reports Suspend Resume Test', devices={"MOUSE":1})
    def cl_HID_reports_suspend_resume_test(self):
        """Performs HID report test over resume with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.run_hid_reports_test(device, suspend_resume=True)


    @test_wrapper('HID Reports Reboot Test', devices={"MOUSE":1})
    def cl_HID_reports_reboot_test(self):
        """Performs HID report test over reboot with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.run_hid_reports_test(device, reboot=True)


    @test_wrapper('Connect Disconnect Loop Test', devices={"MOUSE":1})
    def cl_connect_disconnect_loop_test(self):
        """Performs connect/disconnect test with mouse peripheral"""
        device = self.devices['MOUSE'][0]
        self.connect_disconnect_loop(device=device, loops=3)


    @batch_wrapper('Classic Sanity')
    def cl_sanity_batch_run(self, num_iterations=1, test_name=None):
        """Run the Classic sanity test batch or a specific given test.
           The wrapper of this method is implemented in batch_decorator.
           Using the decorator a test batch method can implement the only its
           core tests invocations and let the decorator handle the wrapper,
           which is taking care for whether to run a specific test or the
           batch as a whole, and running the batch in iterations

           @param num_iterations: how many interations to run
           @param test_name: specifc test to run otherwise None to run the
                             whole batch
        """
        self.cl_adapter_pairing_test()
        self.cl_adapter_keyboard_pairing_test()
        self.cl_adapter_pairing_suspend_resume_test()
        self.cl_adapter_pairing_twice_test()
        self.cl_HID_reports_test()
        self.cl_HID_keyboard_reports_test()
        self.cl_HID_reports_suspend_resume_test()
        #self.cl_HID_reports_reboot_test()
        self.cl_connect_disconnect_loop_test()
        self.cl_adapter_discovery_test()


    def run_once(self, host, num_iterations=1, test_name=None,
                 flag='Quick Sanity'):
        """Run the batch of Bluetooth Classic sanity tests

        @param host: the DUT, usually a chromebook
        @param num_iterations: the number of rounds to execute the test
        @test_name: the test to run, or None for all tests
        """

        # Initialize and run the test batch or the requested specific test
        self.quick_test_init(host, use_chameleon=True, flag=flag)
        self.cl_sanity_batch_run(num_iterations, test_name)
        self.quick_test_cleanup()
