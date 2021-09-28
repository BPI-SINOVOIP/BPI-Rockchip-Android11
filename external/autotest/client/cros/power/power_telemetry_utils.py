# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper class for power autotests requiring telemetry devices."""

import logging
import time

import numpy

CUSTOM_START = 'PowerTelemetryLogger custom start.'
CUSTOM_END = 'PowerTelemetryLogger custom end.'
INTERPOLATION_RESOLUTION = 6


class TelemetryUtilsError(Exception):
    """Error class for issues using these utilities."""


def interpolate_missing_data(data, max_nan_ratio=None, max_sample_gap=None,
                             max_sample_time_gap=None, timeline=None):
    """Interpolate missing power readings in data.

    @param data: array of values
    @min_nan_ratio: optional, float, max acceptable ratio of NaN to real values
    @max_sample_gap: optional, int, max acceptable number of NaN in a row
    @max_sample_time_gap: optional, float, max measurement gap in seconds
                       Note: supplying max_nan_time_gap requires timeline
    @timeline: array of same size as |data| with timeline info for each sample

    @returns: list, array |data| with missing values interpolated.
    @raises: TelemetryUtilsError if
              - the ratio of NaN is higher than |max_nan_ratio| (if supplied)
              - no NaN gap is larger than |max_sample_gap| (if supplied)
              - no NaN gap takes more time in |timeline| than
                |max_sample_time_gap| (if supplied)
              - all values in |data| are NaN.
    """
    if max_sample_time_gap is not None and timeline is None:
        # These are mutually required.
        raise TelemetryUtilsError('Supplying max_sample_time_gap requires a '
                                  'timeline.')
    data = numpy.array(data)
    nan_data = numpy.isnan(data)
    if max_nan_ratio:
        # Validate the ratio if a ratio is supplied.
        nan_ratio = float(sum(nan_data)) / len(data)
        if nan_ratio > max_nan_ratio:
            # There are too many errors in this source.
            # Throw an error so the user has a chance to adjust their power
            # collection setup.
            raise TelemetryUtilsError('NaN ratio of %.02f '
                                      ' - Max is %.02f.' % (nan_ratio,
                                                            max_nan_ratio))
    if max_sample_gap is not None or max_sample_time_gap is not None:
        # Flag to keep track whether the loop is in a measurement gap (NaN).
        consecutive_nan_start = None
        # Add a dummy at the end to make sure the iteration covers all real
        # examples.
        for i, isnan in enumerate(numpy.append(nan_data, False)):
            if isnan and consecutive_nan_start is None:
                consecutive_nan_start = i
            if not isnan and consecutive_nan_start is not None:
                consecutive_nans = i - consecutive_nan_start
                if max_sample_gap and consecutive_nans >= max_sample_gap:
                    # Reject if there are too many consecutive failures.
                    raise TelemetryUtilsError('Too many consecutive NaN samples'
                                              ': %d.' % consecutive_nans)
                if max_sample_time_gap:
                    # Checks whether the first valid timestamp before the
                    # gap exists and whether the first valid timestamp after the
                    # gap exists.
                    if consecutive_nan_start == 0 or i == len(data):
                        # We cannot determine the gap timeline properly here
                        # as the gap either starts or ends with the time.
                        # Ignore for now.
                        continue
                    sample_time_gap = (timeline[i] -
                                       timeline[consecutive_nan_start-1])
                    if sample_time_gap > max_sample_time_gap:
                        raise TelemetryUtilsError('Excessively long sample gap '
                                                  'of %.02fs. Longest '
                                                  'permissible gap is %.02fs.'
                                                  % (sample_time_gap,
                                                     max_sample_time_gap))

                # Reset the flag for the next gap.
                consecutive_nan_start = None
    # At this point the data passed all validations required.
    sample_idx = numpy.arange(len(data))[[~nan_data]]
    sample_vals = data[[~nan_data]]
    if not len(sample_idx):
        raise TelemetryUtilsError('Data has no valid readings. Cannot '
                                  'interpolate.')
    output = numpy.interp(range(len(data)), sample_idx, sample_vals)
    return [round(x, INTERPOLATION_RESOLUTION) for x in output]

def log_event_ts(message=None, timestamp=None, offset=0):
    """Log the event and timestamp for parsing later.

    @param message: description of the event.
    @param timestamp: timestamp to for the event, if not provided, default to
           current time. Local seconds since epoch.
    @param offset: offset in seconds from the provided timestamp, or offset from
           current time if timestamp is not provided. Can be positive or
           negative.
    """
    if not message:
        return
    if timestamp:
        ts = timestamp + offset
    else:
        ts = time.time() + offset
    logging.debug("%s %s", message, ts)

def start_measurement(timestamp=None, offset=0):
    """Mark the start of power telemetry measurement.

    Optional. Use only once in the client side test that is wrapped in the
    power measurement wrapper tests to help pinpoint exactly where power
    telemetry data should start. PowerTelemetryLogger will trim off excess data
    before this point. If not used, power telemetry data will start right before
    the client side test.
    @param timestamp: timestamp for the start of measurement, if not provided,
           default to current time. Local seconds since epoch.
    @param offset: offset in seconds from the provided timestamp, or offset from
           current time if timestamp is not provided. Can be positive or
           negative.
    """
    log_event_ts(CUSTOM_START, timestamp, offset)

def end_measurement(timestamp=None, offset=0):
    """Mark the end of power telemetry measurement.

    Optional. Use only once in the client side test that is wrapped in the
    power measurement wrapper tests to help pinpoint exactly where power
    telemetry data should end. PowerTelemetryLogger will trim off excess data
    after this point. If not used, power telemetry data will end right after the
    client side test.
    @param timestamp: timestamp for the end of measurement, if not provided,
           default to current time. Local seconds since epoch.
    @param offset: offset in seconds from the provided timestamp, or offset from
           current time if timestamp is not provided. Can be positive or
           negative.
    """
    log_event_ts(CUSTOM_END, timestamp, offset)
