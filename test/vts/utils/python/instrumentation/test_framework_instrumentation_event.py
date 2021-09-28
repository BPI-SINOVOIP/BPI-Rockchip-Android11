#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import time
import re


# Log prefix to be parsed by performance analysis tools
LOGGING_PREFIX = '[Instrumentation]'
# Log line template in the format of <prefix> <type> @<category> #<name>
# Do not use tab(\t) because sometime they can be printed to log as the string '\t'
LOGGING_TEMPLATE = LOGGING_PREFIX + ' {category}: {name} {status}'
# Characters not allowed in provided event category or name
# In event init method, these characters are joint by '|' as regex. Modifications to
# the replacing logic be required if escape character is needed.
ILLEGAL_CHARS = ':\t\r\n'

# A list of events that have began but not ended.
event_stack = []
# A list of events that have finished
# TODO(yuexima): use a new class to store data
event_data = []


def NormalizeNameCategory(name, category):
    """Replaces illegal characters in name and category.

    Illegal characters defined in ILLEGAL_CHARS will be replaced with '_'.

    Args:
        name: string
        category: string

    Returns:
        a tuple (string, string), name and category
    """
    if set(ILLEGAL_CHARS) & set(category + name):
        category = re.sub('|'.join(ILLEGAL_CHARS), '_', category)
        name = re.sub('|'.join(ILLEGAL_CHARS), '_', name)

    return name, category


class TestFrameworkInstrumentationEvent(object):
    """An object that represents an event.

    Attributes:
        category: string, a category mark represents a high level event
                  category such as preparer setup and test execution.
        error: string, None if no error. Otherwise contains error messages such
               as duplicated Begin or End.
        name: string, a string to mark specific name of an event for human
              reading. Final performance analysis will mostly focus on category
              granularity instead of name granularity.
        status: int, 0 for not started, 1 for started, 2 for ended, 3 for removed.
        timestamp_begin_cpu: float, cpu time of event begin
        timestamp_begin_wall: float, wall time of event begin
        timestamp_end_cpu: float, cpu time of event end
                           Note: on some operating system (such as windows), the difference
                           between start and end cpu time may be measured using
                           wall time.
        timestamp_end_wall: float, wall time of event end
        _enable_logging: bool or None. Whether to put the event in logging.
                         Should be set to False when timing small pieces of code that could take
                         very short time to run.
                         This value should only be set once for log consistency.
                         If is None, global configuration will be used.
        _disable_subevent_logging: bool, whether to disable logging for events created after this
                                   event begins and before this event ends. This will overwrite
                                   subevent's logging setting if set to True.
    """
    category = None
    name = None
    status = 0
    error = None
    _enable_logging = False
    _disable_subevent_logging = False
    # TODO(yuexima): add on/off toggle param for logging.

    timestamp_begin_cpu = -1
    timestamp_begin_wall = -1
    timestamp_end_cpu = -1
    timestamp_end_wall = -1

    def __init__(self, name, category):
        self.name, self.category = NormalizeNameCategory(name, category)

        if (name, category) != (self.name, self.category):
            self.LogW('TestFrameworkInstrumentation: illegal character detected in '
                          'category or name string. Provided name: %s, category: %s. '
                          'Replacing them as "_"', name, category)

    def Match(self, name, category):
        """Checks whether the given category and name matches this event."""
        return category == self.category and name == self.name

    def Begin(self, enable_logging=None, disable_subevent_logging=False):
        """Performs logging action for the beginning of this event.

        Args:
            enable_logging: bool or None. Whether to put the event in logging.
                            Should be set to False when timing small pieces of code that could take
                            very short time to run.
                            If not specified or is None, global configuration will be used.
                            This value can only be set in Begin method to make logging consistent.
            disable_subevent_logging: bool, whether to disable logging for events created after this
                                      event begins and before this event ends. This will overwrite
                                      subevent's logging setting if set to True.
        """
        timestamp_begin_cpu = time.clock()
        timestamp_begin_wall = time.time()
        global event_stack
        if event_stack and event_stack[-1]._disable_subevent_logging:
            self._enable_logging = False
            self._disable_subevent_logging = True
        else:
            if enable_logging is not None:
                self._enable_logging = enable_logging
            self._disable_subevent_logging = disable_subevent_logging

        if self.status == 1:
            self.LogE('TestFrameworkInstrumentation: event %s has already began. '
                      'Skipping Begin.', self)
            self.error = 'Tried to Begin but already began'
            return
        elif self.status == 2:
            self.LogE('TestFrameworkInstrumentation: event %s has already ended. '
                      'Skipping Begin.', self)
            self.error = 'Tried to Begin but already ended'
            return
        elif self.status == 3:
            self.LogE('TestFrameworkInstrumentation: event %s has already been removed. '
                      'Skipping Begin.', self)
            self.error = 'Tried to Begin but already been removed'
            return

        self.LogD(LOGGING_TEMPLATE.format(category=self.category,
                                          name=self.name,
                                          status='BEGIN'))

        self.status = 1
        self.timestamp_begin_cpu = timestamp_begin_cpu
        self.timestamp_begin_wall = timestamp_begin_wall
        event_stack.append(self)

    def End(self):
        """Performs logging action for the end of this event."""
        timestamp_end_cpu = time.clock()
        timestamp_end_wall = time.time()
        if self.status == 0:
            self.LogE('TestFrameworkInstrumentation: event %s has not yet began. '
                      'Skipping End.', self)
            self.error = 'Tried to End but have not began'
            return
        elif self.status == 2:
            self.LogE('TestFrameworkInstrumentation: event %s has already ended. '
                      'Skipping End.', self)
            self.error = 'Tried to End but already ended'
            return
        elif self.status == 3:
            self.LogE('TestFrameworkInstrumentation: event %s has already been removed. '
                      'Skipping End.', self)
            self.error = 'Tried to End but already been removed'
            return

        self.LogD(LOGGING_TEMPLATE.format(category=self.category,
                                          name=self.name,
                                          status='END'))

        self.status = 2
        self.timestamp_end_cpu = timestamp_end_cpu
        self.timestamp_end_wall = timestamp_end_wall
        global event_data
        event_data.append(self)
        global event_stack
        event_stack.remove(self)

    def CheckEnded(self, remove_reason=''):
        """Checks whether this event has ended and remove it if not.

        This method is designed to be used in a try-catch exception block, where it is
        not obvious whether the End() method has ever been called. In such case, if
        End() has not been called, it usually means exception has happened and the event
        should either be removed or automatically ended. Here we choose remove because
        this method could be called in the finally block in a try-catch block, and there
        could be a lot of noise before reaching the finally block.

        This method does not support being called directly in test_framework_instrumentation
        module with category and name lookup, because there can be events with same names.

        Calling this method multiple times on the same Event object is ok.

        Args:
            remove_reason: string, reason to remove to be recorded when the event was
                           not properly ended. Default is empty string.
        """
        if self.status < 2:
            self.Remove(remove_reason)

    def Remove(self, remove_reason=''):
        """Removes this event from reports and record the reason.

        Calling this method multiple times on the same Event object is ok.

        Args:
            remove_reason: string, reason to remove to be recorded when the event was
                           not properly ended. Default is empty string.
        """
        if self.status == 3:
            return

        self.LogD(LOGGING_TEMPLATE.format(category=self.category,
                                          name=self.name,
                                          status='REMOVE') +
                  ' | Reason: %s' % remove_reason)

        self.error = remove_reason
        self.status = 3
        global event_stack
        event_stack.remove(self)

    def LogD(self, *args):
        """Wrapper function for logging.debug"""
        self._Log(logging.debug, *args)

    def LogI(self, *args):
        """Wrapper function for logging.info"""
        self._Log(logging.info, *args)

    def LogW(self, *args):
        """Wrapper function for logging.warn"""
        self._Log(logging.warn, *args)

    def LogE(self, *args):
        """Wrapper function for logging.error"""
        self._Log(logging.error, *args)

    def _Log(self, log_func, *args):
        if self._enable_logging != False:
            log_func(*args)

    def __str__(self):
        return 'Event object: @%s #%s' % (self.category, self.name) + \
            '\n  Begin timestamp(CPU): %s' % self.timestamp_begin_cpu + \
            '\n  End timestamp(CPU): %s' % self.timestamp_end_cpu + \
            '\n  Begin timestamp(wall): %s' % self.timestamp_begin_wall + \
            '\n  End timestamp(wall): %s' % self.timestamp_end_wall + \
            '\n  Status: %s' % self.status + \
            '\n  Duration(CPU): %s' % (self.timestamp_end_cpu - self.timestamp_begin_cpu) + \
            '\n  Duration(wall): %s' % (self.timestamp_end_wall - self.timestamp_begin_wall)
