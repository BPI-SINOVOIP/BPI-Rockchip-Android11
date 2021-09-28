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
r"""Acloud metrics functions."""

import logging

from acloud.internal import constants
_NO_METRICS = "--no-metrics"


logger = logging.getLogger(__name__)


# pylint: disable=broad-except, import-error
def LogUsage(argv):
    """Log acloud start event.

    Log acloud start event and the usage, following are the data we log:
    - tool_name: All asuite tools are storing event in the same database. This
      property is provided to distinguish different tools.
    - command_line: Log all command arguments.
    - test_references: Should be a list, we record the acloud sub-command.
      e.g. create/delete/reconnect/..etc. We could use this property as filter
      criteria to speed up query time.
    - cwd: User's current working directory.
    - os: The platform that users are working at.

    Args:
        argv: A list of system arguments.

    Returns:
        True if start event is sent and need to pair with end event.
    """
    if _NO_METRICS in argv:
        return False

    try:
        from asuite import atest_utils
        from asuite.metrics import metrics_utils
        atest_utils.print_data_collection_notice()
        metrics_utils.send_start_event(tool_name=constants.TOOL_NAME,
                                       command_line=' '.join(argv),
                                       test_references=[argv[0]])
        return True
    except Exception as e:
        logger.debug("Failed to send start event:%s", str(e))

    return False


# pylint: disable=broad-except
def LogExitEvent(exit_code, stacktrace="", logs=""):
    """Log acloud exit event.

    A start event should followed by an exit event to calculate the consuming
    time. This function will be run at the end of acloud main process or
    at the init of the error object.

    Args:
        exit_code: Integer, the exit code of acloud main process.
        stacktrace: A string of stacktrace.
        logs: A string of logs.
    """
    try:
        from asuite.metrics import metrics_utils
        metrics_utils.send_exit_event(exit_code, stacktrace=stacktrace,
                                      logs=logs)
    except Exception as e:
        logger.debug("Failed to send exit event:%s", str(e))
