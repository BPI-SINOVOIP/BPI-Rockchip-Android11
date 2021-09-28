# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Server side bluetooth tests about sending bluetooth HID reports."""

import logging
import time

from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests


class BluetoothAdapterHIDReportTests(
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """Server side bluetooth tests about sending bluetooth HID reports.

    This test tries to send HID reports to a DUT and verifies if the DUT
    could receive the reports correctly. For the time being, only bluetooth
    mouse events are tested. Bluetooth keyboard events will be supported
    later.
    """

    HID_TEST_SLEEP_SECS = 5

    def run_mouse_tests(self, device):
        """Run all bluetooth mouse reports tests.

        @param device: the bluetooth HID device.

        """
        self.test_mouse_left_click(device)
        self.test_mouse_right_click(device)
        self.test_mouse_move_in_x(device, 80)
        self.test_mouse_move_in_y(device, -50)
        self.test_mouse_move_in_xy(device, -60, 100)
        self.test_mouse_scroll_down(device, 70)
        self.test_mouse_scroll_up(device, 40)
        self.test_mouse_click_and_drag(device, 90, 30)


    def run_keyboard_tests(self, device):
        """Run all bluetooth mouse reports tests.

        @param device: the bluetooth HID device.

        """

        self.test_keyboard_input_from_trace(device, "simple_text")


    def run_hid_reports_test(self, device, suspend_resume=False, reboot=False):
        """Running Bluetooth HID reports tests."""
        logging.info("run hid reports test")
        # Reset the adapter and set it pairable.
        self.test_reset_on_adapter()
        self.test_pairable()

        # Let the adapter pair, and connect to the target device.
        time.sleep(self.HID_TEST_SLEEP_SECS)
        self.test_discover_device(device.address)
        time.sleep(self.HID_TEST_SLEEP_SECS)
        self.test_pairing(device.address, device.pin, trusted=True)
        time.sleep(self.HID_TEST_SLEEP_SECS)
        self.test_connection_by_adapter(device.address)

        if suspend_resume:
            self.suspend_resume()

            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_device_is_paired(device.address)

            # After a suspend/resume, we should check if the device is
            # connected.
            # NOTE: After a suspend/resume, the RN42 kit remains connected.
            #       However, this is not expected behavior for all bluetooth
            #       peripherals.
            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_device_is_connected(device.address)

            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_device_name(device.address, device.name)

        if reboot:
            self.reboot()

            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_device_is_paired(device.address)

            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_connection_by_device(device)

            time.sleep(self.HID_TEST_SLEEP_SECS)
            self.test_device_name(device.address, device.name)

        # Run tests about mouse reports.
        if device.device_type.endswith('MOUSE'):
            self.run_mouse_tests(device)

        if device.device_type.endswith('KEYBOARD'):
            self.run_keyboard_tests(device)

        # Disconnect the device, and remove the pairing.
        self.test_disconnection_by_adapter(device.address)
        self.test_remove_pairing(device.address)
