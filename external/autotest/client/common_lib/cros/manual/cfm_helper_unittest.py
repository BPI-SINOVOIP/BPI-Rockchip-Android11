# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file
"""Tests for cfm_helper.py."""

import unittest

import cfm_helper
import get_usb_devices

SPEAKERS = 'speakers'
CAMERAS = 'cameras'
DISPLAY_MIMO = 'display_mimo'
CONTROLLER_MIMO = 'controller_mimo'
DEVICE_TYPES = (SPEAKERS, CAMERAS, DISPLAY_MIMO, CONTROLLER_MIMO)


class TestExtractPeripherals(unittest.TestCase):
    """Test cfm_helper.extract_peripherals()"""

    def create_mock_device_getter(self, device_list):
        """Mock a function to take usb_data and return device_list"""

        def mock_device_getter(usb_data):
            """Return the specified device_list, ignoring usb_data."""
            return device_list

        return mock_device_getter

    def setUp(self):
        """
        Mock the various get_devices functions so that each one returns a
        key-value pair.
        In extract_peripherals(), the key represents the pid_vid, and the value
        represents the device_count.
        For these testing purposes, we use the device_type ('cameras', etc.)
        as the key, and a number 1-4 as the device_count.
        (If we used a device_count of 0 it would be ignored; hence, 1-4.)

        """
        self.original_funcs = {}
        for i in range(len(DEVICE_TYPES)):
            device_type = DEVICE_TYPES[i]
            mock = self.create_mock_device_getter({device_type: i + 1})
            setattr(cfm_helper.get_usb_devices, 'get_%s' % device_type, mock)

    def runTest(self):
        """Verify that all 4 peripheral-types are extracted."""
        peripherals = cfm_helper.extract_peripherals_for_cfm(None)
        self.assertEqual(len(peripherals), 4)
        for i in range(len(DEVICE_TYPES)):
            device_type = DEVICE_TYPES[i]
            self.assertEqual(peripherals[device_type], i + 1)

    def tearDown(self):
        """Restore the original functions, for the sake of other tests."""
        for device_type in DEVICE_TYPES:
            original_func = getattr(get_usb_devices, 'get_%s' % device_type)
            setattr(cfm_helper.get_usb_devices, 'get_%s' % device_type,
                    original_func)


if __name__ == '__main__':
    unittest.main()
