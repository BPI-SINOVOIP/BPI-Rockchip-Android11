#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

from __future__ import print_function

import datetime
import logging
import os
import re

from copy import copy

from acts import tracelogger
from acts.libs.logging import log_stream
from acts.libs.logging.log_stream import LogStyles

log_line_format = "%(asctime)s.%(msecs).03d %(levelname)s %(message)s"
# The micro seconds are added by the format string above,
# so the time format does not include ms.
log_line_time_format = "%Y-%m-%d %H:%M:%S"
log_line_timestamp_len = 23

logline_timestamp_re = re.compile("\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d.\d\d\d")


# yapf: disable
class Style:
    RESET  = '\033[0m'
    BRIGHT = '\033[1m'
    DIM    = '\033[2m'
    NORMAL = '\033[22m'


class Fore:
    BLACK   = '\033[30m'
    RED     = '\033[31m'
    GREEN   = '\033[32m'
    YELLOW  = '\033[33m'
    BLUE    = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN    = '\033[36m'
    WHITE   = '\033[37m'
    RESET   = '\033[39m'


class Back:
    BLACK   = '\033[40m'
    RED     = '\033[41m'
    GREEN   = '\033[42m'
    YELLOW  = '\033[43m'
    BLUE    = '\033[44m'
    MAGENTA = '\033[45m'
    CYAN    = '\033[46m'
    WHITE   = '\033[47m'
    RESET   = '\033[49m'


LOG_LEVELS = {
  'DEBUG':     {'level': 10, 'style': Fore.GREEN + Style.BRIGHT},
  'CASE':      {'level': 11, 'style': Back.BLUE + Fore.WHITE + Style.BRIGHT},
  'SUITE':     {'level': 12, 'style': Back.MAGENTA + Fore.WHITE + Style.BRIGHT},
  'INFO':      {'level': 20, 'style': Style.NORMAL},
  'STEP':      {'level': 15, 'style': Fore.WHITE + Style.BRIGHT},
  'WARNING':   {'level': 30, 'style': Fore.YELLOW + Style.BRIGHT},
  'ERROR':     {'level': 40, 'style': Fore.RED + Style.BRIGHT},
  'EXCEPTION': {'level': 45, 'style': Back.RED + Fore.WHITE + Style.BRIGHT},
  'DEVICE':    {'level': 51, 'style': Fore.CYAN + Style.BRIGHT},
}
# yapf: enable


class ColoredLogFormatter(logging.Formatter):
    def format(self, record):
        colored_record = copy(record)
        level_name = colored_record.levelname
        style = LOG_LEVELS[level_name]['style']
        formatted_level_name = '%s%s%s' % (style, level_name, Style.RESET)
        colored_record.levelname = formatted_level_name
        return super().format(colored_record)


def _parse_logline_timestamp(t):
    """Parses a logline timestamp into a tuple.

    Args:
        t: Timestamp in logline format.

    Returns:
        An iterable of date and time elements in the order of month, day, hour,
        minute, second, microsecond.
    """
    date, time = t.split(' ')
    year, month, day = date.split('-')
    h, m, s = time.split(':')
    s, ms = s.split('.')
    return year, month, day, h, m, s, ms


def is_valid_logline_timestamp(timestamp):
    if len(timestamp) == log_line_timestamp_len:
        if logline_timestamp_re.match(timestamp):
            return True
    return False


def logline_timestamp_comparator(t1, t2):
    """Comparator for timestamps in logline format.

    Args:
        t1: Timestamp in logline format.
        t2: Timestamp in logline format.

    Returns:
        -1 if t1 < t2; 1 if t1 > t2; 0 if t1 == t2.
    """
    dt1 = _parse_logline_timestamp(t1)
    dt2 = _parse_logline_timestamp(t2)
    for u1, u2 in zip(dt1, dt2):
        if u1 < u2:
            return -1
        elif u1 > u2:
            return 1
    return 0


def _get_timestamp(time_format, delta=None):
    t = datetime.datetime.now()
    if delta:
        t = t + datetime.timedelta(seconds=delta)
    return t.strftime(time_format)[:-3]


def epoch_to_log_line_timestamp(epoch_time):
    """Converts an epoch timestamp in ms to log line timestamp format, which
    is readable for humans.

    Args:
        epoch_time: integer, an epoch timestamp in ms.

    Returns:
        A string that is the corresponding timestamp in log line timestamp
        format.
    """
    s, ms = divmod(epoch_time, 1000)
    d = datetime.datetime.fromtimestamp(s)
    return d.strftime("%Y-%m-%d %H:%M:%S.") + str(ms)


def get_log_line_timestamp(delta=None):
    """Returns a timestamp in the format used by log lines.

    Default is current time. If a delta is set, the return value will be
    the current time offset by delta seconds.

    Args:
        delta: Number of seconds to offset from current time; can be negative.

    Returns:
        A timestamp in log line format with an offset.
    """
    return _get_timestamp("%Y-%m-%d %H:%M:%S.%f", delta)


def get_log_file_timestamp(delta=None):
    """Returns a timestamp in the format used for log file names.

    Default is current time. If a delta is set, the return value will be
    the current time offset by delta seconds.

    Args:
        delta: Number of seconds to offset from current time; can be negative.

    Returns:
        A timestamp in log file name format with an offset.
    """
    return _get_timestamp("%Y-%m-%d_%H-%M-%S-%f", delta)


def _setup_test_logger(log_path, prefix=None):
    """Customizes the root logger for a test run.

    The logger object has a stream handler and a file handler. The stream
    handler logs INFO level to the terminal, the file handler logs DEBUG
    level to files.

    Args:
        log_path: Location of the log file.
        prefix: A prefix for each log line in terminal.
    """
    logging.log_path = log_path
    log_styles = [
        LogStyles.LOG_INFO + LogStyles.TO_STDOUT,
        LogStyles.DEFAULT_LEVELS + LogStyles.TESTCASE_LOG
    ]
    terminal_format = log_line_format
    if prefix:
        terminal_format = "[{}] {}".format(prefix, log_line_format)
    stream_formatter = ColoredLogFormatter(terminal_format,
                                           log_line_time_format)
    file_formatter = logging.Formatter(log_line_format, log_line_time_format)
    log = log_stream.create_logger('test_run',
                                   '',
                                   log_styles=log_styles,
                                   stream_format=stream_formatter,
                                   file_format=file_formatter)
    log.setLevel(logging.DEBUG)
    _enable_additional_log_levels()


def _enable_additional_log_levels():
    """Enables logging levels used for tracing tests and debugging devices."""
    for log_type, log_data in LOG_LEVELS.items():
        logging.addLevelName(log_data['level'], log_type)


def kill_test_logger(logger):
    """Cleans up a test logger object by removing all of its handlers.

    Args:
        logger: The logging object to clean up.
    """
    for h in list(logger.handlers):
        logger.removeHandler(h)
        if isinstance(h, logging.FileHandler):
            h.close()


def create_latest_log_alias(actual_path):
    """Creates a symlink to the latest test run logs.

    Args:
        actual_path: The source directory where the latest test run's logs are.
    """
    link_path = os.path.join(os.path.dirname(actual_path), "latest")
    if os.path.islink(link_path):
        os.remove(link_path)
    os.symlink(actual_path, link_path)


def setup_test_logger(log_path, prefix=None):
    """Customizes the root logger for a test run.

    Args:
        log_path: Location of the report file.
        prefix: A prefix for each log line in terminal.
        filename: Name of the files. The default is the time the objects
            are requested.
    """
    os.makedirs(log_path, exist_ok=True)
    _setup_test_logger(log_path, prefix)
    create_latest_log_alias(log_path)


def normalize_log_line_timestamp(log_line_timestamp):
    """Replace special characters in log line timestamp with normal characters.

    Args:
        log_line_timestamp: A string in the log line timestamp format. Obtained
            with get_log_line_timestamp.

    Returns:
        A string representing the same time as input timestamp, but without
        special characters.
    """
    norm_tp = log_line_timestamp.replace(' ', '_')
    norm_tp = norm_tp.replace(':', '-')
    return norm_tp


class LoggerAdapter(logging.LoggerAdapter):
    """A LoggerAdapter class that takes in a lambda for transforming logs."""
    def __init__(self, logging_lambda):
        self.logging_lambda = logging_lambda
        super(LoggerAdapter, self).__init__(logging.getLogger(), {})

    def process(self, msg, kwargs):
        return self.logging_lambda(msg), kwargs


def create_logger(logging_lambda=lambda message: message):
    """Returns a logger with logging defined by a given lambda.

    Args:
        logging_lambda: A lambda of the form:
            >>> lambda log_message: return 'string'
    """
    return tracelogger.TraceLogger(LoggerAdapter(logging_lambda))


def create_tagged_trace_logger(tag=''):
    """Returns a logger that logs each line with the given prefix.

    Args:
        tag: The tag of the log line, E.g. if tag == tag123, the output
            line would be:

            <TESTBED> <TIME> <LOG_LEVEL> [tag123] logged message
    """
    def logging_lambda(msg):
        return '[%s] %s' % (tag, msg)

    return create_logger(logging_lambda)
