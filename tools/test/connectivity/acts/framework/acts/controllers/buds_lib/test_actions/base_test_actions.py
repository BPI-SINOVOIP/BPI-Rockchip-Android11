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

"""Base test action class, provide a base class for representing a collection of
test actions.
"""

import datetime
import inspect
import time

from acts.controllers.buds_lib import tako_trace_logger
from acts.libs.utils.timer import TimeRecorder

# All methods start with "_" are considered hidden.
DEFAULT_HIDDEN_ACTION_PREFIX = '_'


def timed_action(method):
    """A common decorator for test actions."""

    def timed(self, *args, **kw):
        """Log the enter/exit/time of the action method."""
        func_name = self._convert_default_action_name(method.__name__)
        if not func_name:
            func_name = method.__name__
        self.log_step('%s...' % func_name)
        self.timer.start_timer(func_name, True)
        result = method(self, *args, **kw)
        # TODO: Method run time collected can be used for automatic KPI checks
        self.timer.stop_timer(func_name)
        return result

    return timed


class TestActionNotFoundError(Exception):
    pass


class BaseTestAction(object):
    """Class for organizing a collection of test actions.

    Test actions are just normal python methods, and should perform a specified
    action. @timed_action decorator can log the entry/exit of the test action,
    and the execution time.

    The BaseTestAction class also provides a mapping between human friendly
    names and test action methods in order to support configuration base
    execution. By default, all methods not hidden (not start with "_") is
    exported as human friendly name by replacing "_" with space.

    Test action method can be called directly, or via
    _perform_action(<human friendly name>, <args...>)
    method.
    """

    @classmethod
    def _fill_default_action_map(cls):
        """Parse current class and get all test actions methods."""
        # a <human readable name>:<method name> map.
        cls._action_map = dict()
        for name, _ in inspect.getmembers(cls, inspect.ismethod):
            act_name = cls._convert_default_action_name(name)
            if act_name:
                cls._action_map[act_name] = name

    @classmethod
    def _convert_default_action_name(cls, func_name):
        """Default conversion between method name -> human readable action name.
        """
        if not func_name.startswith(DEFAULT_HIDDEN_ACTION_PREFIX):
            act_name = func_name.lower()
            act_name = act_name.replace('_', ' ')
            act_name = act_name.title()
            return act_name.strip()
        else:
            return ''

    @classmethod
    def _add_action_alias(cls, default_act_name, alias):
        """Add an alias to an existing test action."""
        if default_act_name in cls._action_map:
            cls._action_map[alias] = cls._action_map[default_act_name]
            return True
        else:
            return False

    @classmethod
    def _get_action_names(cls):
        if not hasattr(cls, '_action_map'):
            cls._fill_default_action_map()
        return cls._action_map.keys()

    @classmethod
    def get_current_time_logcat_format(cls):
        return datetime.datetime.now().strftime('%m-%d %H:%M:%S.000')

    @classmethod
    def _action_exists(cls, action_name):
        """Verify if an human friendly action name exists or not."""
        if not hasattr(cls, '_action_map'):
            cls._fill_default_action_map()
        return action_name in cls._action_map

    @classmethod
    def _validate_actions(cls, action_list):
        """Verify if an human friendly action name exists or not.

        Args:
          :param action_list: list of actions to be validated.

        Returns:
          tuple of (is valid, list of invalid/non-existent actions)
        """
        not_found = []
        for action_name in action_list:
            if not cls._action_exists(action_name):
                not_found.append(action_name)
        all_valid = False if not_found else True
        return all_valid, not_found

    def __init__(self, logger=None):
        if logger is None:
            self.logger = tako_trace_logger.TakoTraceLogger()
            self.log_step = self.logger.step
        else:
            self.logger = logger
            self.log_step = self.logger.info
        self.timer = TimeRecorder()
        self._fill_default_action_map()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def _perform_action(self, action_name, *args, **kwargs):
        """Perform the specified human readable action."""
        if action_name not in self._action_map:
            raise TestActionNotFoundError('Action %s not found this class.'
                                          % action_name)

        method = self._action_map[action_name]
        ret = getattr(self, method)(*args, **kwargs)
        return ret

    @timed_action
    def print_actions(self):
        """Example action methods.

        All test action method must:
            1. return a value. False means action failed, any other value means
               pass.
            2. should not start with "_". Methods start with "_" is hidden.
        All test action method may:
            1. have optional arguments. Mutable argument can be used to pass
               value
            2. raise exceptions. Test case class is expected to handle
               exceptions
        """
        num_acts = len(self._action_map)

        self.logger.info('I can do %d action%s:' %
                      (num_acts, 's' if num_acts != 1 else ''))
        for act in self._action_map.keys():
            self.logger.info(' - %s' % act)
        return True

    @timed_action
    def sleep(self, seconds):
        self.logger.info('%s seconds' % seconds)
        time.sleep(seconds)


if __name__ == '__main__':
    acts = BaseTestAction()
    acts.print_actions()
    acts._perform_action('print actions')
    print(acts._get_action_names())
