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
import logging
import time

from acts.controllers.monsoon_lib.api import common
from acts.controllers.monsoon_lib.api.common import MonsoonError
from acts.controllers.monsoon_lib.api.common import PassthroughStates


class BaseMonsoon(object):
    """The base class for all Monsoon interface devices.

    Attributes:
        on_reconnect: The function to call when Monsoon has reconnected USB.
            Raises TimeoutError if the device cannot be found.
        on_disconnect: The function to call when Monsoon has disconnected USB.
    """

    # The minimum non-zero supported voltage for the given Monsoon device.
    MIN_VOLTAGE = NotImplemented

    # The maximum practical voltage for the given Monsoon device.
    MAX_VOLTAGE = NotImplemented

    # When ramping voltage, the rate in volts/second to increase the voltage.
    VOLTAGE_RAMP_RATE = 3

    # The time step between voltage increments. This value does not need to be
    # modified.
    VOLTAGE_RAMP_TIME_STEP = .1

    def __init__(self):
        self._log = logging.getLogger()
        self.on_disconnect = lambda: None
        self.on_reconnect = lambda: None

    @classmethod
    def get_closest_valid_voltage(cls, voltage):
        """Returns the nearest valid voltage value."""
        if voltage < cls.MIN_VOLTAGE / 2:
            return 0
        else:
            return max(cls.MIN_VOLTAGE, min(voltage, cls.MAX_VOLTAGE))

    @classmethod
    def is_voltage_valid(cls, voltage):
        """Returns True iff the given voltage can be set on the device.

        Valid voltage values are {x | x ∈ {0} ∪ [MIN_VOLTAGE, MAX_VOLTAGE]}.
        """
        return cls.get_closest_valid_voltage(voltage) == voltage

    @classmethod
    def validate_voltage(cls, voltage):
        """Raises a MonsoonError if the given voltage cannot be set."""
        if not cls.is_voltage_valid(voltage):
            raise MonsoonError('Invalid voltage %s. Voltage must be zero or '
                               'within range [%s, %s].' %
                               (voltage, cls.MIN_VOLTAGE, cls.MAX_VOLTAGE))

    def set_voltage_safe(self, voltage):
        """Sets the output voltage of monsoon to a safe value.

        This function is effectively:
            self.set_voltage(self.get_closest_valid_voltage(voltage)).

        Args:
            voltage: The voltage to set the output to.
        """
        normalized_voltage = self.get_closest_valid_voltage(voltage)
        if voltage != normalized_voltage:
            self._log.debug(
                'Requested voltage %sV is invalid.' % normalized_voltage)
        self.set_voltage(normalized_voltage)

    def ramp_voltage(self, start, end):
        """Ramps up the voltage to the specified end voltage.

        Increments the voltage by fixed intervals of .1 Volts every .1 seconds.

        Args:
            start: The starting voltage
            end: the end voltage. Must be higher than the starting voltage.
        """
        voltage = start

        while voltage < end:
            self.set_voltage(self.get_closest_valid_voltage(voltage))
            voltage += self.VOLTAGE_RAMP_RATE * self.VOLTAGE_RAMP_TIME_STEP
            time.sleep(self.VOLTAGE_RAMP_TIME_STEP)
        self.set_voltage(end)

    def usb(self, state):
        """Sets the monsoon's USB passthrough mode.

        This is specific to the USB port in front of the monsoon box which
        connects to the powered device, NOT the USB that is used to talk to the
        monsoon itself.

        Args:
            state: The state to set the USB passthrough to. Can either be the
                string name of the state or the integer value.

                "Off" or 0 means USB always off.
                "On" or 1 means USB always on.
                "Auto" or 2 means USB is automatically turned off during
                    sampling, and turned back on after sampling.

        Raises:
            ValueError if the state given is invalid.
            TimeoutError if unable to set the passthrough mode within a minute,
                or if the device was not found after setting the state to ON.
        """
        expected_state = None
        states_dict = common.PASSTHROUGH_STATES
        if isinstance(state, str):
            normalized_state = state.lower()
            expected_state = states_dict.get(normalized_state, None)
        elif state in states_dict.values():
            expected_state = state

        if expected_state is None:
            raise ValueError(
                'USB passthrough state %s is not a valid state. '
                'Expected any of %s.' % (repr(state), states_dict))
        if self.status.usbPassthroughMode == expected_state:
            return

        if expected_state in [PassthroughStates.OFF, PassthroughStates.AUTO]:
            self.on_disconnect()

        start_time = time.time()
        time_limit_seconds = 60
        while self.status.usbPassthroughMode != expected_state:
            current_time = time.time()
            if current_time >= start_time + time_limit_seconds:
                raise TimeoutError('Setting USB mode timed out after %s '
                                   'seconds.' % time_limit_seconds)
            self._set_usb_passthrough_mode(expected_state)
            time.sleep(1)
        self._log.info('Monsoon usbPassthroughMode is now "%s"',
                       state)

        if expected_state in [PassthroughStates.ON]:
            self._on_reconnect()

    def attach_device(self, android_device):
        """Deprecated. Use the connection callbacks instead."""

        def on_reconnect():
            # Make sure the device is connected and available for commands.
            android_device.wait_for_boot_completion()
            android_device.start_services()
            # Release wake lock to put device into sleep.
            android_device.droid.goToSleepNow()
            self._log.info('Dut reconnected.')

        def on_disconnect():
            android_device.stop_services()
            time.sleep(1)

        self.on_reconnect = on_reconnect
        self.on_disconnect = on_disconnect

    def set_on_disconnect(self, callback):
        """Sets the callback to be called when Monsoon disconnects USB."""
        self.on_disconnect = callback

    def set_on_reconnect(self, callback):
        """Sets the callback to be called when Monsoon reconnects USB."""
        self.on_reconnect = callback

    def take_samples(self, assembly_line):
        """Runs the sampling procedure based on the given assembly line."""
        # Sampling is always done in a separate process. Release the Monsoon
        # so the child process can sample from the Monsoon.
        self.release_monsoon_connection()

        try:
            assembly_line.run()
        finally:
            self.establish_monsoon_connection()

    def measure_power(self,
                      duration,
                      measure_after_seconds=0,
                      hz=5000,
                      output_path=None,
                      transformers=None):
        """Measure power consumption of the attached device.

        This function is a default implementation of measuring power consumption
        during gathering measurements. For offline methods, use take_samples()
        with a custom AssemblyLine.

        Args:
            duration: Amount of time to measure power for. Note:
                total_duration = duration + measure_after_seconds
            measure_after_seconds: Number of seconds to wait before beginning
                reading measurement.
            hz: The number of samples to collect per second. Must be a factor
                of 5000.
            output_path: The location to write the gathered data to.
            transformers: A list of Transformer objects that receive passed-in
                          samples. Runs in order sent.

        Returns:
            A MonsoonData object with the measured power data.
        """
        raise NotImplementedError()

    def set_voltage(self, voltage):
        """Sets the output voltage of monsoon.

        Args:
            voltage: The voltage to set the output to.
        """
        raise NotImplementedError()

    def set_max_current(self, amperes):
        """Sets monsoon's max output current.

        Args:
            amperes: The max current in A.
        """
        raise NotImplementedError()

    def set_max_initial_current(self, amperes):
        """Sets the max power-up/initial current.

        Args:
            amperes: The max initial current allowed in amperes.
        """
        raise NotImplementedError()

    @property
    def status(self):
        """Gets the status params of monsoon.

        Returns:
            A dictionary of {status param, value} key-value pairs.
        """
        raise NotImplementedError()

    def _on_reconnect(self):
        """Reconnects the DUT over USB.

        Raises:
            TimeoutError upon failure to reconnect over USB.
        """
        self._log.info('Reconnecting dut.')
        # Wait for two seconds to ensure that the device is ready, then
        # attempt to reconnect. If reconnect times out, reset the passthrough
        # state and try again.
        time.sleep(2)
        try:
            self.on_reconnect()
        except TimeoutError as err:
            self._log.info('Toggling USB and trying again. %s' % err)
            self.usb(PassthroughStates.OFF)
            time.sleep(1)
            self.usb(PassthroughStates.ON)
            self.on_reconnect()

    def _set_usb_passthrough_mode(self, mode):
        """Makes the underlying Monsoon call to set passthrough mode."""
        raise NotImplementedError()

    def reconnect_monsoon(self):
        """Reconnects the Monsoon Serial/USB connection."""
        raise NotImplementedError()

    def release_monsoon_connection(self):
        """Releases the underlying monsoon Serial or USB connection.

        Useful for allowing other processes access to the device.
        """
        raise NotImplementedError()

    def establish_monsoon_connection(self):
        """Establishes the underlying monsoon Serial or USB connection."""
        raise NotImplementedError()
