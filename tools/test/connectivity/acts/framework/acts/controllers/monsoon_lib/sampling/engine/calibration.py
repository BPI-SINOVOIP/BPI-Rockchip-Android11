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


class CalibrationError(Exception):
    """Raised when a value is requested before it is properly calibrated."""


class CalibrationCollection(object):
    """The interface for keeping track of calibration values.

    This class is an abstract representation of a collection of Calibration
    values. Some CalibrationCollections may simply be a dictionary that returns
    values given to it (see CalibrationScalars). Others may accept multiple
    values and return the average for a set rolling window (see
    CalibrationWindow).

    Whichever the implementation, this interface gives end-users a way of
    setting and querying a collection of calibration data that comes from a
    Monsoon device.
    """

    def add(self, channel, origin, granularity, value):
        """Adds a value to the calibration storage.

        The passed in channel, origin, and granularity arguments will be used
        as a key to handle and store the value passed in.

        Args:
            channel: The channel this value comes from. See
                MonsoonConstants.Channel.
            origin: The origin type for this value. See MonsoonConstants.Origin.
            granularity: The granularity type for this value. See
                MonsoonConstants.Granularity.
            value: The value to set within the collection.
        """
        raise NotImplementedError()

    def get_keys(self):
        """Returns the list of possible keys for obtaining calibration data.

        Not all possible (Channel, Origin, Granularity) combinations may be
        available for all CalibrationCollections. It is also not guaranteed the
        CalibrationCollection's key set is static.
        """
        raise NotImplementedError()

    def get(self, channel, origin, granularity):
        """Returns the calibration value for a given key."""
        raise NotImplementedError()


class CalibrationWindows(CalibrationCollection):
    """A class that holds calibration data in sliding windows.

    After the window size has been filled, a calibration value is removed every
    time a new calibration value is added.
    """

    def __init__(self, calibration_window_size=5):
        """Creates a collection of CalibrationWindows.

        calibration_window_size: The number of entries in the rolling window to
            consider for calibration.
        """
        super().__init__()
        self._calibrations = dict()
        self._calibration_window_size = calibration_window_size

    def add(self, channel, origin, granularity, value):
        """Adds the given value to the given calibration window.

        Args:
            channel: The channel being calibrated.
            origin: The origin value being calibrated.
            granularity: The granularity level being calibrated.
            value: The calibration value.
        """
        window = self._calibrations[(channel, origin, granularity)]
        if len(window) == self._calibration_window_size:
            window.popleft()
        window.append(value)

    def get_keys(self):
        return self._calibrations.keys()

    def get(self, channel, origin, granularity):
        window = self._calibrations[(channel, origin, granularity)]
        if len(window) < self._calibration_window_size:
            raise CalibrationError('%s is not calibrated yet.' % repr(
                (channel, origin, granularity)))
        return sum(window) / self._calibration_window_size


class CalibrationScalars(CalibrationCollection):
    """A collection of calibrations where scalar values are used.

    Reading scalar calibration values are faster than calculating the
    calibration value from rolling windows.
    """

    def __init__(self):
        self._calibrations = dict()

    def get_keys(self):
        return self._calibrations.keys()

    def add(self, channel, origin, granularity, value):
        """Adds a value to the calibration storage.

        Note that if a value is already within the collection, it will be
        overwritten, since CalibrationScalars can only hold a single value.

        Args:
            channel: The channel being calibrated.
            origin: The origin value being calibrated.
            granularity: The granularity level being calibrated.
            value: The calibration value.
        """
        self._calibrations[(channel, origin, granularity)] = value

    def get(self, channel, origin, granularity):
        return self._calibrations[(channel, origin, granularity)]


class CalibrationSnapshot(CalibrationScalars):
    """A collection of calibrations taken from another CalibrationCollection.

    CalibrationSnapshot calculates all of the calibration values of another
    CalibrationCollection and creates a snapshot of those values. This allows
    the CalibrationWindows to continue getting new values while another thread
    processes the calibration on previously gathered values.
    """

    def __init__(self, calibration_collection):
        """Generates a CalibrationSnapshot from another CalibrationCollection.

        Args:
            calibration_collection: The CalibrationCollection to create a
                snapshot of.
        """
        super().__init__()

        if not isinstance(calibration_collection, CalibrationCollection):
            raise ValueError('Argument must inherit from '
                             'CalibrationCollection.')

        for key in calibration_collection.get_keys():
            try:
                # key's type is tuple(Channel, Origin, Granularity)
                value = calibration_collection.get(*key)
            except CalibrationError as calibration_error:
                # If uncalibrated, store the CalibrationError and raise when a
                # user has asked for the value.
                value = calibration_error
            self._calibrations[key] = value

    def get(self, channel, origin, granularity):
        """Returns the calibration value for the given key.

        Raises:
            CalibrationError if the requested key is not calibrated.
        """
        value = self._calibrations[(channel, origin, granularity)]
        if isinstance(value, CalibrationError):
            # The user requested an uncalibrated value. Raise that error.
            raise value
        return value
