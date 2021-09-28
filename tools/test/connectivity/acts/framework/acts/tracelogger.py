#!/usr/bin/env python3
#
# Copyright 2016 - The Android Open Source Project
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

import inspect
import os


class TraceLogger(object):
    def __init__(self, logger):
        self._logger = logger

    @staticmethod
    def _get_trace_info(level=1, offset=2):
        # We want the stack frame above this and above the error/warning/info
        inspect_stack = inspect.stack()
        trace_info = ''
        for i in range(level):
            try:
                stack_frames = inspect_stack[offset + i]
                info = inspect.getframeinfo(stack_frames[0])
                trace_info = '%s[%s:%s:%s]' % (trace_info,
                                               os.path.basename(info.filename),
                                               info.function, info.lineno)
            except IndexError:
                break
        return trace_info

    def _log_with(self, logging_lambda, trace_level, msg, *args, **kwargs):
        trace_info = TraceLogger._get_trace_info(level=trace_level, offset=3)
        logging_lambda('%s %s' % (msg, trace_info), *args, **kwargs)

    def exception(self, msg, *args, **kwargs):
        self._log_with(self._logger.exception, 5, msg, *args, **kwargs)

    def debug(self, msg, *args, **kwargs):
        self._log_with(self._logger.debug, 3, msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        self._log_with(self._logger.error, 3, msg, *args, **kwargs)

    def warn(self, msg, *args, **kwargs):
        self._log_with(self._logger.warn, 1, msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        self._log_with(self._logger.warning, 1, msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        self._log_with(self._logger.info, 1, msg, *args, **kwargs)

    def __getattr__(self, name):
        return getattr(self._logger, name)
