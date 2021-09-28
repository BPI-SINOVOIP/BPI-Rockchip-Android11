#!/usr/bin/env python3
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""AIDEgen metrics functions."""

import logging
import os
import platform
import sys

from aidegen import constant
from aidegen.lib import common_util
from atest import atest_utils

# When combine 3 paths in a single try block, it's hard for the coverage
# counting algorithm to judge the each real path clearly. So, separating them
# into its own try block will increase the coverage.

# Original code as follows,
# try:
#     from asuite.metrics import metrics
#     from asuite.metrics import metrics_base
#     from asuite.metrics import metrics_utils
# except ImportError:
#     logging.debug('Import metrics fail, can\'t send metrics.')
#     metrics = None
#     metrics_base = None
#     metrics_utils = None
try:
    from asuite.metrics import metrics
except ImportError:
    logging.debug('Import metrics fail, can\'t send metrics.')
    metrics = None

try:
    from asuite.metrics import metrics_base
except ImportError:
    logging.debug('Import metrics fail, can\'t send metrics.')
    metrics_base = None

try:
    from asuite.metrics import metrics_utils
except ImportError:
    logging.debug('Import metrics fail, can\'t send metrics.')
    metrics_utils = None


def starts_asuite_metrics(references):
    """Starts to record metrics data.

    Send a metrics data to log server at the same time.

    Args:
        references: a list of reference data, when importing whole Android
                    it contains 'is_android_tree'.
    """
    if not metrics:
        return
    atest_utils.print_data_collection_notice()
    metrics_base.MetricsBase.tool_name = constant.AIDEGEN_TOOL_NAME
    metrics_utils.get_start_time()
    command = ' '.join(sys.argv)
    metrics.AtestStartEvent(
        command_line=command,
        test_references=references,
        cwd=os.getcwd(),
        os=platform.platform())


def ends_asuite_metrics(exit_code, stacktrace='', logs=''):
    """Send the end event to log server.

    Args:
        exit_code: An integer of exit code.
        stacktrace: A string of stacktrace.
        logs: A string of logs.

    Returns:
        Boolean: False if metrics_utils does not exist.
                 True when successfully send metrics.
    """
    if not metrics_utils:
        return False
    metrics_utils.send_exit_event(
        exit_code,
        stacktrace=stacktrace,
        logs=logs)
    return True


def send_exception_metrics(exit_code, stack_trace, log, err_msg):
    """Sends exception metrics.

    For recording the exception metrics, this function is going to be called.
    It is different to ends_asuite_metrics function, which will be called every
    time AIDEGen process is finished.
    The steps you need to do to call this function:
        1. Create an exit code in constants.py.
        2. Generate the stack trace info and the log for debugging.
        3. Show the warning message to users.

    Args:
        exit_code: An integer of exit code.
        stack_trace: A string of stacktrace.
        log: A string of logs.
        err_msg: A string to show warning.
    """
    print('\n{} {}\n'.format(common_util.COLORED_INFO('Warning:'), err_msg))
    stack_trace = common_util.remove_user_home_path(stack_trace)
    log = common_util.remove_user_home_path(log)
    ends_asuite_metrics(exit_code, stack_trace, log)
