#!/usr/bin/env python3
#
# Copyright 2019 - The Android Open Source Project
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
import logging

from acts import tracelogger


class TakoTraceLogger(tracelogger.TraceLogger):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.d = self.debug
        self.e = self.error
        self.i = self.info
        self.t = self.step
        self.w = self.warning

    def _logger_level(self, level_name):
        level = logging.getLevelName(level_name)
        return lambda *args, **kwargs: self._logger.log(level, *args, **kwargs)

    def step(self, msg, *args, **kwargs):
        """Delegate a step call to the underlying logger."""
        self._log_with(self._logger_level('STEP'), 1, msg, *args, **kwargs)

    def device(self, msg, *args, **kwargs):
        """Delegate a device call to the underlying logger."""
        self._log_with(self._logger_level('DEVICE'), 1, msg, *args, **kwargs)

    def suite(self, msg, *args, **kwargs):
        """Delegate a device call to the underlying logger."""
        self._log_with(self._logger_level('SUITE'), 1, msg, *args, **kwargs)

    def case(self, msg, *args, **kwargs):
        """Delegate a case call to the underlying logger."""
        self._log_with(self._logger_level('CASE'), 1, msg, *args, **kwargs)

    def flush_log(self):
        """This function exists for compatibility with Tako's logserial module.

        Note that flushing the log is handled automatically by python's logging
        module.
        """
        pass
