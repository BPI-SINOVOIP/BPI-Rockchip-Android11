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

import multiprocessing
import time

from Monsoon import HVPM
from Monsoon import Operations as op

from acts.controllers.monsoon_lib.api.common import MonsoonResult
from acts.controllers.monsoon_lib.api.monsoon import BaseMonsoon
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import AssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ThreadAssemblyLine
from acts.controllers.monsoon_lib.sampling.engine.transformers import DownSampler
from acts.controllers.monsoon_lib.sampling.engine.transformers import SampleAggregator
from acts.controllers.monsoon_lib.sampling.engine.transformers import Tee
from acts.controllers.monsoon_lib.sampling.hvpm.transformers import HvpmTransformer


class Monsoon(BaseMonsoon):
    """The controller class for interacting with the HVPM Monsoon."""

    # The device doesn't officially support voltages lower than this. Note that
    # 0 is a valid voltage.
    MIN_VOLTAGE = 0.8

    # The Monsoon doesn't support setting higher voltages than this directly
    # without tripping overvoltage.
    # Note that it is possible to increase the voltage above this value by
    # increasing the voltage by small increments over a period of time.
    # The communication protocol supports up to 16V.
    MAX_VOLTAGE = 13.5

    def __init__(self, serial):
        super().__init__()
        self.serial = serial
        self._mon = HVPM.Monsoon()
        self._mon.setup_usb(serial)
        if self._mon.Protocol.DEVICE is None:
            raise ValueError('HVPM Monsoon %s could not be found.' % serial)

    def set_voltage(self, voltage):
        """Sets the output voltage of monsoon.

        Args:
            voltage: The voltage to set the output to.
        """
        self._log.debug('Setting voltage to %sV.' % voltage)
        self._mon.setVout(voltage)

    def set_max_current(self, amperes):
        """Sets monsoon's max output current.

        Args:
            amperes: The max current in A.
        """
        self._mon.setRunTimeCurrentLimit(amperes)

    def set_max_initial_current(self, amperes):
        """Sets the max power-up/initial current.

        Args:
            amperes: The max initial current allowed in amperes.
        """
        self._mon.setPowerUpCurrentLimit(amperes)

    @property
    def status(self):
        """Gets the status params of monsoon.

        Returns:
            A dictionary of {status param, value} key-value pairs.
        """
        self._mon.fillStatusPacket()
        return self._mon.statusPacket

    def _set_usb_passthrough_mode(self, mode):
        """Sends the call to set usb passthrough mode.

        Args:
            mode: The state to set the USB passthrough to. Can either be the
                string name of the state or the integer value.

                "Off" or 0 means USB always off.
                "On" or 1 means USB always on.
                "Auto" or 2 means USB is automatically turned off during
                    sampling, and turned back on after sampling.
        """
        self._mon.setUSBPassthroughMode(mode)

    def _get_main_voltage(self):
        """Returns the value of the voltage on the main channel."""
        # Any getValue call on a setX function will return the value set for X.
        # Using this, we can pull the last setMainVoltage (or its default).
        return (self._mon.Protocol.getValue(op.OpCodes.setMainVoltage, 4) /
                op.Conversion.FLOAT_TO_INT)

    def measure_power(self,
                      duration,
                      measure_after_seconds=0,
                      hz=5000,
                      output_path=None,
                      transformers=None):
        """See parent docstring for details."""
        voltage = self._get_main_voltage()

        aggregator = SampleAggregator(measure_after_seconds)
        manager = multiprocessing.Manager()

        assembly_line_builder = AssemblyLineBuilder(manager.Queue,
                                                    ThreadAssemblyLine)
        assembly_line_builder.source(
            HvpmTransformer(self.serial, duration + measure_after_seconds))
        if hz != 5000:
            assembly_line_builder.into(DownSampler(int(5000 / hz)))
        if output_path:
            assembly_line_builder.into(Tee(output_path, measure_after_seconds))
        assembly_line_builder.into(aggregator)
        if transformers:
            for transformer in transformers:
                assembly_line_builder.into(transformer)

        self.take_samples(assembly_line_builder.build())

        manager.shutdown()

        self._mon.setup_usb(self.serial)
        monsoon_data = MonsoonResult(aggregator.num_samples,
                                     aggregator.sum_currents, hz, voltage,
                                     output_path)
        self._log.info('Measurement summary:\n%s', str(monsoon_data))
        return monsoon_data

    def reconnect_monsoon(self):
        """Reconnect Monsoon to serial port."""
        self.release_monsoon_connection()
        self._log.info('Closed monsoon connection.')
        time.sleep(5)
        self.establish_monsoon_connection()

    def release_monsoon_connection(self):
        self._mon.closeDevice()

    def establish_monsoon_connection(self):
        self._mon.setup_usb(self.serial)
        # Makes sure the Monsoon is in the command-receiving state.
        self._mon.stopSampling()
