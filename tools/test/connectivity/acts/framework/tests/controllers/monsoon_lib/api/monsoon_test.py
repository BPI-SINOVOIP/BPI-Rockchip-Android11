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

import unittest

import mock

from acts.controllers.monsoon_lib.api.common import MonsoonError
from acts.controllers.monsoon_lib.api.common import PASSTHROUGH_STATES
from acts.controllers.monsoon_lib.api.common import PassthroughStates
from acts.controllers.monsoon_lib.api.monsoon import BaseMonsoon

# The position in the call tuple that represents the args array.
ARGS = 0

STILL_TIME_LEFT = 0
OUT_OF_TIME = 9001


class MonsoonImpl(BaseMonsoon):
    MIN_VOLTAGE = 1.5
    MAX_VOLTAGE = 3.0

    set_voltage = mock.Mock()
    release_monsoon_connection = mock.Mock()
    establish_monsoon_connection = mock.Mock()

    def _set_usb_passthrough_mode(self, value):
        self.__usb_passthrough_mode = value

    def __init__(self):
        super().__init__()
        self.__usb_passthrough_mode = None

    @property
    def status(self):
        class StatusPacket(object):
            def __init__(self, passthrough_mode):
                self.usbPassthroughMode = (
                    passthrough_mode
                    if passthrough_mode in PASSTHROUGH_STATES.values() else
                    PASSTHROUGH_STATES.get(passthrough_mode, None))

        return StatusPacket(self.__usb_passthrough_mode)


class BaseMonsoonTest(unittest.TestCase):
    """Tests acts.controllers.monsoon_lib.api.monsoon.Monsoon."""

    def setUp(self):
        self.sleep_patch = mock.patch('time.sleep')
        self.sleep_patch.start()
        MonsoonImpl.set_voltage = mock.Mock()
        MonsoonImpl.release_monsoon_connection = mock.Mock()
        MonsoonImpl.establish_monsoon_connection = mock.Mock()

    def tearDown(self):
        self.sleep_patch.stop()

    def test_get_closest_valid_voltage_returns_zero_when_low(self):
        voltage_to_round_to_zero = MonsoonImpl.MIN_VOLTAGE / 2 - 0.1
        self.assertEqual(
            MonsoonImpl.get_closest_valid_voltage(voltage_to_round_to_zero), 0)

    def test_get_closest_valid_voltage_snaps_to_min_when_low_but_close(self):
        voltage_to_round_to_min = MonsoonImpl.MIN_VOLTAGE / 2 + 0.1
        self.assertEqual(
            MonsoonImpl.get_closest_valid_voltage(voltage_to_round_to_min),
            MonsoonImpl.MIN_VOLTAGE)

    def test_get_closest_valid_voltage_snaps_to_max_when_high(self):
        voltage_to_round_to_max = MonsoonImpl.MAX_VOLTAGE * 2
        self.assertEqual(
            MonsoonImpl.get_closest_valid_voltage(voltage_to_round_to_max),
            MonsoonImpl.MAX_VOLTAGE)

    def test_get_closest_valid_voltage_to_not_round(self):
        valid_voltage = (MonsoonImpl.MAX_VOLTAGE + MonsoonImpl.MIN_VOLTAGE) / 2

        self.assertEqual(
            MonsoonImpl.get_closest_valid_voltage(valid_voltage),
            valid_voltage)

    def test_is_voltage_valid_voltage_is_valid(self):
        valid_voltage = (MonsoonImpl.MAX_VOLTAGE + MonsoonImpl.MIN_VOLTAGE) / 2

        self.assertTrue(MonsoonImpl.is_voltage_valid(valid_voltage))

    def test_is_voltage_valid_voltage_is_not_valid(self):
        invalid_voltage = MonsoonImpl.MIN_VOLTAGE - 2

        self.assertFalse(MonsoonImpl.is_voltage_valid(invalid_voltage))

    def test_validate_voltage_voltage_is_valid(self):
        valid_voltage = (MonsoonImpl.MAX_VOLTAGE + MonsoonImpl.MIN_VOLTAGE) / 2

        MonsoonImpl.validate_voltage(valid_voltage)

    def test_validate_voltage_voltage_is_not_valid(self):
        invalid_voltage = MonsoonImpl.MIN_VOLTAGE - 2

        with self.assertRaises(MonsoonError):
            MonsoonImpl.validate_voltage(invalid_voltage)

    def test_set_voltage_safe_rounds_unsafe_voltage(self):
        invalid_voltage = MonsoonImpl.MIN_VOLTAGE - .1
        monsoon = MonsoonImpl()

        monsoon.set_voltage_safe(invalid_voltage)

        monsoon.set_voltage.assert_called_once_with(MonsoonImpl.MIN_VOLTAGE)

    def test_set_voltage_safe_does_not_round_safe_voltages(self):
        valid_voltage = (MonsoonImpl.MAX_VOLTAGE + MonsoonImpl.MIN_VOLTAGE) / 2
        monsoon = MonsoonImpl()

        monsoon.set_voltage_safe(valid_voltage)

        monsoon.set_voltage.assert_called_once_with(valid_voltage)

    def test_ramp_voltage_sets_vout_to_final_value(self):
        """Tests the desired end voltage is set."""
        monsoon = MonsoonImpl()
        expected_value = monsoon.MIN_VOLTAGE

        monsoon.ramp_voltage(0, expected_value)

        self.assertEqual(
            MonsoonImpl.set_voltage.call_args_list[-1][ARGS][0],
            expected_value, 'The last call to setVout() was not the expected '
            'final value.')

    def test_ramp_voltage_ramps_voltage_over_time(self):
        """Tests that voltage increases between each call."""
        monsoon = MonsoonImpl()

        difference = (MonsoonImpl.VOLTAGE_RAMP_RATE *
                      MonsoonImpl.VOLTAGE_RAMP_TIME_STEP * 5)
        monsoon.ramp_voltage(MonsoonImpl.MIN_VOLTAGE,
                             MonsoonImpl.MIN_VOLTAGE + difference)

        previous_voltage = 0
        for set_voltage_call in MonsoonImpl.set_voltage.call_args_list:
            self.assertGreaterEqual(
                set_voltage_call[ARGS][0], previous_voltage,
                'ramp_voltage does not always increment voltage.')
            previous_voltage = set_voltage_call[ARGS][0]

    def test_usb_accepts_passthrough_state_sets_with_str(self):
        monsoon = MonsoonImpl()
        state_string = 'on'

        monsoon.usb(state_string)

        self.assertEqual(monsoon.status.usbPassthroughMode,
                         PASSTHROUGH_STATES[state_string])

    def test_usb_accepts_passthrough_state_sets_with_int_value(self):
        monsoon = MonsoonImpl()

        monsoon.usb(1)

        self.assertEqual(monsoon.status.usbPassthroughMode, 1)

    def test_usb_raises_on_invalid_str_value(self):
        monsoon = MonsoonImpl()

        with self.assertRaises(ValueError):
            monsoon.usb('DEADBEEF')

    def test_usb_raises_on_invalid_int_value(self):
        monsoon = MonsoonImpl()

        with self.assertRaises(ValueError):
            monsoon.usb(9001)

    @mock.patch('time.time')
    def test_usb_raises_timeout_error(self, time):
        monsoon = MonsoonImpl()
        time.side_effect = [STILL_TIME_LEFT, OUT_OF_TIME]

        with self.assertRaises(TimeoutError):
            monsoon.usb(1)

    def test_usb_does_not_set_passthrough_mode_if_unchanged(self):
        """Tests that the passthrough mode is not reset if it is unchanged."""
        monsoon = MonsoonImpl()
        existing_state = PassthroughStates.ON
        monsoon._set_usb_passthrough_mode(existing_state)
        monsoon._set_usb_passthrough_mode = mock.Mock()

        monsoon.usb(existing_state)

        self.assertFalse(
            monsoon._set_usb_passthrough_mode.called,
            'usbPassthroughMode should not be called when the '
            'state does not change.')

    def take_samples_always_reestablishes_the_monsoon_connection(self):
        monsoon = MonsoonImpl()
        assembly_line = mock.Mock()
        assembly_line.run.side_effect = Exception('Some Terrible error')

        monsoon.take_samples(assembly_line)

        self.assertTrue(monsoon.establish_monsoon_connection.called)


if __name__ == '__main__':
    unittest.main()
