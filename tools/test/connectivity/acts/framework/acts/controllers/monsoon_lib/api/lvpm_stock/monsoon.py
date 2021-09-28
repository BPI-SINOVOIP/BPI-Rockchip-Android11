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

from acts.controllers.monsoon_lib.api.common import MonsoonResult
from acts.controllers.monsoon_lib.api.lvpm_stock.monsoon_proxy import MonsoonProxy
from acts.controllers.monsoon_lib.api.monsoon import BaseMonsoon
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import AssemblyLineBuilder
from acts.controllers.monsoon_lib.sampling.engine.assembly_line import ThreadAssemblyLine
from acts.controllers.monsoon_lib.sampling.engine.transformers import DownSampler
from acts.controllers.monsoon_lib.sampling.engine.transformers import SampleAggregator
from acts.controllers.monsoon_lib.sampling.engine.transformers import Tee
from acts.controllers.monsoon_lib.sampling.lvpm_stock.stock_transformers import StockLvpmSampler


class Monsoon(BaseMonsoon):
    """The controller class for interacting with the LVPM Monsoon."""

    # The device protocol has a floor value for positive voltages. Note that 0
    # is still a valid voltage.
    MIN_VOLTAGE = 2.01

    # The device protocol does not support values above this.
    MAX_VOLTAGE = 4.55

    def __init__(self, serial, device=None):
        super().__init__()
        self._mon = MonsoonProxy(serialno=serial, device=device)
        self.serial = serial

    def set_voltage(self, voltage):
        """Sets the output voltage of monsoon.

        Args:
            voltage: Voltage to set the output to.
        """
        self._log.debug('Setting voltage to %sV.' % voltage)
        self._mon.set_voltage(voltage)

    def set_max_current(self, amperes):
        """Sets monsoon's max output current.

        Args:
            amperes: The max current in A.
        """
        self._mon.set_max_current(amperes)

    def set_max_initial_current(self, amperes):
        """Sets the max power-up/initial current.

        Args:
            amperes: The max initial current allowed in amperes.
        """
        self._mon.set_max_initial_current(amperes)

    @property
    def status(self):
        """Gets the status params of monsoon.

        Returns:
            A dictionary of {status param, value} key-value pairs.
        """
        return self._mon.get_status()

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
        self._mon.set_usb_passthrough(mode)

    def measure_power(self,
                      duration,
                      measure_after_seconds=0,
                      hz=5000,
                      output_path=None,
                      transformers=None):
        """See parent docstring for details."""
        voltage = self._mon.get_voltage()

        aggregator = SampleAggregator(measure_after_seconds)
        manager = multiprocessing.Manager()

        assembly_line_builder = AssemblyLineBuilder(manager.Queue,
                                                    ThreadAssemblyLine)
        assembly_line_builder.source(
            StockLvpmSampler(self.serial, duration + measure_after_seconds))
        if hz != 5000:
            assembly_line_builder.into(DownSampler(int(round(5000 / hz))))
        if output_path is not None:
            assembly_line_builder.into(Tee(output_path, measure_after_seconds))
        assembly_line_builder.into(aggregator)
        if transformers:
            for transformer in transformers:
                assembly_line_builder.into(transformer)

        self.take_samples(assembly_line_builder.build())

        manager.shutdown()

        monsoon_data = MonsoonResult(aggregator.num_samples,
                                     aggregator.sum_currents, hz, voltage,
                                     output_path)
        self._log.info('Measurement summary:\n%s', str(monsoon_data))
        return monsoon_data

    def reconnect_monsoon(self):
        """Reconnect Monsoon to serial port."""
        self._log.debug('Close serial connection')
        self._mon.ser.close()
        self._log.debug('Reset serial port')
        time.sleep(5)
        self._log.debug('Open serial connection')
        self._mon.ser.open()
        self._mon.ser.reset_input_buffer()
        self._mon.ser.reset_output_buffer()

    def release_monsoon_connection(self):
        self._mon.release_dev_port()

    def establish_monsoon_connection(self):
        self._mon.obtain_dev_port()
        # Makes sure the Monsoon is in the command-receiving state.
        self._mon.stop_data_collection()
