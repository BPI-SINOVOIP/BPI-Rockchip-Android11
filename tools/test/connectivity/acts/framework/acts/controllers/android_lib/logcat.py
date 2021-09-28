#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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
import re

from acts.libs.proc.process import Process
from acts.libs.logging import log_stream
from acts.libs.logging.log_stream import LogStyles

TIMESTAMP_REGEX = r'((?:\d+-)?\d+-\d+ \d+:\d+:\d+.\d+)'


class TimestampTracker(object):
    """Stores the last timestamp outputted by the Logcat process."""

    def __init__(self):
        self._last_timestamp = None

    @property
    def last_timestamp(self):
        return self._last_timestamp

    def read_output(self, message):
        """Reads the message and parses all timestamps from it."""
        all_timestamps = re.findall(TIMESTAMP_REGEX, message)
        if len(all_timestamps) > 0:
            self._last_timestamp = all_timestamps[0]


def _get_log_level(message):
    """Returns the log level for the given message."""
    if message.startswith('-') or len(message) < 37:
        return logging.ERROR
    else:
        log_level = message[36]
        if log_level in ('V', 'D'):
            return logging.DEBUG
        elif log_level == 'I':
            return logging.INFO
        elif log_level == 'W':
            return logging.WARNING
        elif log_level == 'E':
            return logging.ERROR
    return logging.NOTSET


def _log_line_func(log, timestamp_tracker):
    """Returns a lambda that logs a message to the given logger."""

    def log_line(message):
        timestamp_tracker.read_output(message)
        log.log(_get_log_level(message), message)

    return log_line


def _on_retry(serial, extra_params, timestamp_tracker):
    def on_retry(_):
        begin_at = '"%s"' % (timestamp_tracker.last_timestamp or 1)
        additional_params = extra_params or ''

        return 'adb -s %s logcat -T %s -v year %s' % (
            serial, begin_at, additional_params)

    return on_retry


def create_logcat_keepalive_process(serial, logcat_dir, extra_params=''):
    """Creates a Logcat Process that automatically attempts to reconnect.

    Args:
        serial: The serial of the device to read the logcat of.
        logcat_dir: The directory used for logcat file output.
        extra_params: Any additional params to be added to the logcat cmdline.

    Returns:
        A acts.libs.proc.process.Process object.
    """
    logger = log_stream.create_logger(
        'adblog_%s' % serial, log_name=serial, subcontext=logcat_dir,
        log_styles=(LogStyles.LOG_DEBUG | LogStyles.TESTCASE_LOG))
    process = Process('adb -s %s logcat -T 1 -v year %s' %
                      (serial, extra_params))
    timestamp_tracker = TimestampTracker()
    process.set_on_output_callback(_log_line_func(logger, timestamp_tracker))
    process.set_on_terminate_callback(
        _on_retry(serial, extra_params, timestamp_tracker))
    return process
