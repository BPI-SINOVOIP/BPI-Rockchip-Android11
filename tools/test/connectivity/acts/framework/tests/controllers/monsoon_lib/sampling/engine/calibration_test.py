#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
import statistics
import unittest
from collections import deque

from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationError
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationScalars
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationSnapshot
from acts.controllers.monsoon_lib.sampling.engine.calibration import CalibrationWindows
from acts.controllers.monsoon_lib.sampling.enums import Channel
from acts.controllers.monsoon_lib.sampling.enums import Granularity
from acts.controllers.monsoon_lib.sampling.enums import Origin

# These values don't really matter.
C = Channel.MAIN
O = Origin.ZERO
G = Granularity.FINE
C2 = Channel.USB
O2 = Origin.REFERENCE
G2 = Granularity.COARSE


class CalibrationWindowsTest(unittest.TestCase):
    """Unit tests the CalibrationWindows class."""

    def setUp(self):
        # Here, we set up CalibrationWindows with a single dict entry so we can
        # add values to the window. Normally, a child class is responsible for
        # setting the keys of the CalibrationWindows object.
        self.calibration_windows = CalibrationWindows(
            calibration_window_size=5)
        self.calibration_windows._calibrations[(C, O, G)] = deque()

    def test_add_adds_new_value_to_end_of_window(self):
        """Tests add() appends the new value to the end of the window."""
        self.calibration_windows.add(C, O, G, 0)
        self.calibration_windows.add(C, O, G, 1)
        self.calibration_windows.add(C, O, G, 2)

        expected_value = 3

        self.calibration_windows.add(C, O, G, expected_value)

        self.assertEqual(expected_value,
                         self.calibration_windows._calibrations[(C, O, G)][-1])

    def test_add_removes_stale_values(self):
        """Tests add() removes values outside of the calibration window."""
        value_to_remove = 0
        new_values = range(1, 6)

        self.calibration_windows.add(C, O, G, value_to_remove)
        for new_value in new_values:
            self.calibration_windows.add(C, O, G, new_value)

        self.assertNotIn(value_to_remove,
                         self.calibration_windows._calibrations[(C, O, G)])

    def test_get_averages_items_within_window(self):
        """tests get() returns the average of all values within the window."""
        values = range(5)
        expected_value = statistics.mean(values)

        for value in values:
            self.calibration_windows.add(C, O, G, value)

        self.assertEqual(self.calibration_windows.get(C, O, G), expected_value)

    def test_get_raises_error_when_calibration_is_not_complete(self):
        """Tests get() raises CalibrationError when the window is not full."""
        values = range(4)
        for value in values:
            self.calibration_windows.add(C, O, G, value)

        with self.assertRaises(CalibrationError):
            self.calibration_windows.get(C, O, G)


class CalibrationScalarsTest(unittest.TestCase):
    """Unit tests the CalibrationScalars class."""

    def setUp(self):
        # Here, we set up CalibrationScalars with a single dict entry so we can
        # add values to the window. Normally, a child class is responsible for
        # setting the keys of the CalibrationScalars object.
        self.calibration_scalars = CalibrationScalars()
        # Use a non-integer value so unit tests will fail when a bug occurs.
        self.calibration_scalars._calibrations[(C, O, G)] = None

    def test_get_returns_last_added_scalar(self):
        """Tests the value added is the value returned from get()."""
        ignored_value = 2.71828
        expected_value = 3.14159

        self.calibration_scalars.add(C, O, G, ignored_value)
        self.calibration_scalars.add(C, O, G, expected_value)

        self.assertEqual(expected_value, self.calibration_scalars.get(C, O, G))


class CalibrationSnapshotTest(unittest.TestCase):
    """Unit tests the CalibrationSnapshot class."""

    def test_all_keys_are_copied_to_snapshot(self):
        """Tests that all keys from passed-in collection are copied."""
        base_calibration = CalibrationScalars()
        base_calibration._calibrations = {
            (C, O, G): 2.71828,
            (C2, O2, G2): 3.14159,
        }

        calibration_snapshot = CalibrationSnapshot(base_calibration)

        self.assertSetEqual(
            set(base_calibration.get_keys()),
            set(calibration_snapshot.get_keys()))

    def test_init_raises_value_error_upon_illegal_arguments(self):
        """Tests __init__() raises ValueError if the argument is invalid."""
        with self.assertRaises(ValueError):
            CalibrationSnapshot({'illegal': 'dictionary'})

    def test_calibration_error_surfaced_on_get(self):
        """Tests get() raises a CalibrationError if the snapshotted collection
        had a CalibrationError.
        """
        base_calibration = CalibrationScalars()
        base_calibration._calibrations = {
            (C, O, G): CalibrationError('raise me')
        }

        calibration_snapshot = CalibrationSnapshot(base_calibration)

        with self.assertRaises(CalibrationError):
            calibration_snapshot.get(C, O, G)

    def test_calibration_copied_upon_snapshot_created(self):
        """Tests the calibration value is snapshotted."""
        expected_value = 5
        unexpected_value = 10
        base_calibration = CalibrationScalars()
        base_calibration._calibrations = {(C, O, G): expected_value}

        calibration_snapshot = CalibrationSnapshot(base_calibration)
        base_calibration.add(C, O, G, unexpected_value)

        self.assertEqual(calibration_snapshot.get(C, O, G), expected_value)


if __name__ == '__main__':
    unittest.main()
