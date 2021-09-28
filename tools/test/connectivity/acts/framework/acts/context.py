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

import enum
import logging
import os

from acts.event import event_bus
from acts.event.event import Event
from acts.event.event import TestCaseBeginEvent
from acts.event.event import TestCaseEndEvent
from acts.event.event import TestCaseEvent
from acts.event.event import TestClassBeginEvent
from acts.event.event import TestClassEndEvent
from acts.event.event import TestClassEvent


class ContextLevel(enum.IntEnum):
    ROOT = 0
    TESTCLASS = 1
    TESTCASE = 2


def get_current_context(depth=None):
    """Get the current test context at the specified depth.
    Pulls the most recently created context, with a level at or below the given
    depth, from the _contexts stack.

    Args:
        depth: The desired context level. For example, the TESTCLASS level would
            yield the current test class context, even if the test is currently
            within a test case.

    Returns: An instance of TestContext.
    """
    if depth is None:
        return _contexts[-1]
    return _contexts[min(depth, len(_contexts)-1)]


def get_context_for_event(event):
    """Creates and returns a TestContext from the given event.
    A TestClassContext is created for a TestClassEvent, and a TestCaseContext
    is created for a TestCaseEvent.

    Args:
        event: An instance of TestCaseEvent or TestClassEvent.

    Returns: An instance of TestContext corresponding to the event.

    Raises: TypeError if event is neither a TestCaseEvent nor TestClassEvent
    """
    if isinstance(event, TestCaseEvent):
        return _get_context_for_test_case_event(event)
    if isinstance(event, TestClassEvent):
        return _get_context_for_test_class_event(event)
    raise TypeError('Unrecognized event type: %s %s', event, event.__class__)


def _get_context_for_test_case_event(event):
    """Generate a TestCaseContext from the given TestCaseEvent."""
    return TestCaseContext(event.test_class, event.test_case)


def _get_context_for_test_class_event(event):
    """Generate a TestClassContext from the given TestClassEvent."""
    return TestClassContext(event.test_class)


class NewContextEvent(Event):
    """The event posted when a test context has changed."""


class NewTestClassContextEvent(NewContextEvent):
    """The event posted when the test class context has changed."""


class NewTestCaseContextEvent(NewContextEvent):
    """The event posted when the test case context has changed."""


def _update_test_class_context(event):
    """Pushes a new TestClassContext to the _contexts stack upon a
    TestClassBeginEvent. Pops the most recent context off the stack upon a
    TestClassEndEvent. Posts the context change to the event bus.

    Args:
        event: An instance of TestClassBeginEvent or TestClassEndEvent.
    """
    if isinstance(event, TestClassBeginEvent):
        _contexts.append(_get_context_for_test_class_event(event))
    if isinstance(event, TestClassEndEvent):
        if _contexts:
            _contexts.pop()
    event_bus.post(NewTestClassContextEvent())


def _update_test_case_context(event):
    """Pushes a new TestCaseContext to the _contexts stack upon a
    TestCaseBeginEvent. Pops the most recent context off the stack upon a
    TestCaseEndEvent. Posts the context change to the event bus.

    Args:
        event: An instance of TestCaseBeginEvent or TestCaseEndEvent.
    """
    if isinstance(event, TestCaseBeginEvent):
        _contexts.append(_get_context_for_test_case_event(event))
    if isinstance(event, TestCaseEndEvent):
        if _contexts:
            _contexts.pop()
    event_bus.post(NewTestCaseContextEvent())


event_bus.register(TestClassEvent, _update_test_class_context)
event_bus.register(TestCaseBeginEvent, _update_test_case_context, order=-100)
event_bus.register(TestCaseEndEvent, _update_test_case_context, order=100)


class TestContext(object):
    """An object representing the current context in which a test is executing.

    The context encodes the current state of the test runner with respect to a
    particular scenario in which code is being executed. For example, if some
    code is being executed as part of a test case, then the context should
    encode information about that test case such as its name or enclosing
    class.

    The subcontext specifies a relative path in which certain outputs,
    e.g. logcat, should be kept for the given context.

    The full output path is given by
    <base_output_path>/<context_dir>/<subcontext>.

    Attributes:
        _base_output_paths: a dictionary mapping a logger's name to its base
                            output path
        _subcontexts: a dictionary mapping a logger's name to its
                      subcontext-level output directory
    """

    _base_output_paths = {}
    _subcontexts = {}

    def get_base_output_path(self, log_name=None):
        """Gets the base output path for this logger.

        The base output path is interpreted as the reporting root for the
        entire test runner.

        If a path has been added with add_base_output_path, it is returned.
        Otherwise, a default is determined by _get_default_base_output_path().

        Args:
            log_name: The name of the logger.

        Returns:
            The output path.
        """
        if log_name in self._base_output_paths:
            return self._base_output_paths[log_name]
        return self._get_default_base_output_path()
    
    @classmethod
    def add_base_output_path(cls, log_name, base_output_path):
        """Store the base path for this logger.

        Args:
            log_name: The name of the logger.
            base_output_path: The base path of output files for this logger.
            """
        cls._base_output_paths[log_name] = base_output_path

    def get_subcontext(self, log_name=None):
        """Gets the subcontext for this logger.

        The subcontext is interpreted as the directory, relative to the
        context-level path, where all outputs of the given logger are stored.

        If a path has been added with add_subcontext, it is returned.
        Otherwise, the empty string is returned.

        Args:
            log_name: The name of the logger.

        Returns:
            The output path.
        """
        return self._subcontexts.get(log_name, '')

    @classmethod
    def add_subcontext(cls, log_name, subcontext):
        """Store the subcontext path for this logger.

        Args:
            log_name: The name of the logger.
            subcontext: The relative subcontext path of output files for this
                        logger.
        """
        cls._subcontexts[log_name] = subcontext

    def get_full_output_path(self, log_name=None):
        """Gets the full output path for this context.

        The full path represents the absolute path to the output directory,
        as given by <base_output_path>/<context_dir>/<subcontext>

        Args:
            log_name: The name of the logger. Used to specify the base output
                      path and the subcontext.

        Returns:
            The output path.
        """

        path = os.path.join(self.get_base_output_path(log_name),
                            self._get_default_context_dir(),
                            self.get_subcontext(log_name))
        os.makedirs(path, exist_ok=True)
        return path

    @property
    def identifier(self):
        raise NotImplementedError()

    def _get_default_base_output_path(self):
        """Gets the default base output path.

        This will attempt to use the ACTS logging path set up in the global
        logger.

        Returns:
            The logging path.

        Raises:
            EnvironmentError: If the ACTS logger has not been initialized.
        """
        try:
            return logging.log_path
        except AttributeError as e:
            raise EnvironmentError(
                'The ACTS logger has not been set up and'
                ' "base_output_path" has not been set.') from e

    def _get_default_context_dir(self):
        """Gets the default output directory for this context."""
        raise NotImplementedError()


class RootContext(TestContext):
    """A TestContext that represents a test run."""

    @property
    def identifier(self):
        return 'root'

    def _get_default_context_dir(self):
        """Gets the default output directory for this context.

        Logs at the root level context are placed directly in the base level
        directory, so no context-level path exists."""
        return ''


class TestClassContext(TestContext):
    """A TestContext that represents a test class.

    Attributes:
        test_class: The test class instance that this context represents.
    """

    def __init__(self, test_class):
        """Initializes a TestClassContext for the given test class.

        Args:
            test_class: A test class object. Must be an instance of the test
                        class, not the class object itself.
        """
        self.test_class = test_class

    @property
    def test_class_name(self):
        return self.test_class.__class__.__name__

    @property
    def identifier(self):
        return self.test_class_name

    def _get_default_context_dir(self):
        """Gets the default output directory for this context.

        For TestClassContexts, this will be the name of the test class. This is
        in line with the ACTS logger itself.
        """
        return self.test_class_name


class TestCaseContext(TestContext):
    """A TestContext that represents a test case.

    Attributes:
        test_case: The string name of the test case.
        test_class: The test class instance enclosing the test case.
    """

    def __init__(self, test_class, test_case):
        """Initializes a TestCaseContext for the given test case.

        Args:
            test_class: A test class object. Must be an instance of the test
                        class, not the class object itself.
            test_case: The string name of the test case.
        """
        self.test_class = test_class
        self.test_case = test_case

    @property
    def test_case_name(self):
        return self.test_case

    @property
    def test_class_name(self):
        return self.test_class.__class__.__name__

    @property
    def identifier(self):
        return '%s.%s' % (self.test_class_name, self.test_case_name)

    def _get_default_context_dir(self):
        """Gets the default output directory for this context.

        For TestCaseContexts, this will be the name of the test class followed
        by the name of the test case. This is in line with the ACTS logger
        itself.
        """
        return os.path.join(
            self.test_class_name,
            self.test_case_name)


# stack for keeping track of the current test context
_contexts = [RootContext()]
