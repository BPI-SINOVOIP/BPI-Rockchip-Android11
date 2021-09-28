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

from acts.signals import ControllerError


class MonsoonError(ControllerError):
    """Raised for exceptions encountered when interfacing with a Monsoon device.
    """


class PassthroughStates(object):
    """An enum containing the values for power monitor's passthrough states."""
    # "Off" or 0 means USB always off.
    OFF = 0
    # "On" or 1 means USB always on.
    ON = 1
    # "Auto" or 2 means USB is automatically turned off during sampling, and
    # turned back on after sampling.
    AUTO = 2


PASSTHROUGH_STATES = {
    'off': PassthroughStates.OFF,
    'on': PassthroughStates.ON,
    'auto': PassthroughStates.AUTO
}


class MonsoonDataRecord(object):
    """A data class for Monsoon data points."""
    def __init__(self, time, current):
        """Creates a new MonsoonDataRecord.

        Args:
            time: the string '{time}s', where time is measured in seconds since
                the beginning of the data collection.
            current: The current in Amperes as a string.
        """
        self._time = float(time[:-1])
        self._current = float(current)

    @property
    def time(self):
        """The time the record was fetched."""
        return self._time

    @property
    def current(self):
        """The amount of current in Amperes measured for the given record."""
        return self._current

    @classmethod
    def create_from_record_line(cls, line):
        """Creates a data record from the line passed in from the output file.
        """
        return cls(*line.split(' '))


class MonsoonResult(object):
    """An object that contains aggregated data collected during sampling.

    Attributes:
        _num_samples: The number of samples gathered.
        _sum_currents: The total sum of all current values gathered, in amperes.
        _hz: The frequency sampling is being done at.
        _voltage: The voltage output during sampling.
    """

    # The number of decimal places to round a value to.
    ROUND_TO = 6

    def __init__(self, num_samples, sum_currents, hz, voltage, datafile_path):
        """Creates a new MonsoonResult.

        Args:
            num_samples: the number of samples collected.
            sum_currents: the total summation of every current measurement.
            hz: the number of samples per second.
            voltage: the voltage used during the test.
            datafile_path: the path to the monsoon data file.
        """
        self._num_samples = num_samples
        self._sum_currents = sum_currents
        self._hz = hz
        self._voltage = voltage
        self.tag = datafile_path

    def get_data_points(self):
        """Returns an iterator of MonsoonDataRecords."""
        class MonsoonDataIterator:
            def __init__(self, file):
                self.file = file

            def __iter__(self):
                with open(self.file, 'r') as f:
                    for line in f:
                        # Remove the newline character.
                        line.strip()
                        yield MonsoonDataRecord.create_from_record_line(line)

        return MonsoonDataIterator(self.tag)

    @property
    def num_samples(self):
        """The number of samples recorded during the test."""
        return self._num_samples

    @property
    def average_current(self):
        """Average current in mA."""
        if self.num_samples == 0:
            return 0
        return round(self._sum_currents * 1000 / self.num_samples,
                     self.ROUND_TO)

    @property
    def total_charge(self):
        """Total charged used in the unit of mAh."""
        return round((self._sum_currents / self._hz) * 1000 / 3600,
                     self.ROUND_TO)

    @property
    def total_power(self):
        """Total power used."""
        return round(self.average_current * self._voltage, self.ROUND_TO)

    @property
    def voltage(self):
        """The voltage during the measurement (in Volts)."""
        return self._voltage

    def __str__(self):
        return ('avg current: %s\n'
                'total charge: %s\n'
                'total power: %s\n'
                'total samples: %s' % (self.average_current, self.total_charge,
                                      self.total_power, self._num_samples))
