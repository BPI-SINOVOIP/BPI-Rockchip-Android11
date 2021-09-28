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


class Event(object):
    """The base class for all event objects."""


# TODO(markdr): Move these into test_runner.py
class TestEvent(Event):
    """The base class for test-related events."""

    def __init__(self):
        pass


class TestCaseEvent(TestEvent):
    """The base class for test-case-related events."""

    def __init__(self, test_class, test_case):
        super().__init__()
        self.test_class = test_class
        self.test_case = test_case

    @property
    def test_case_name(self):
        return self.test_case

    @property
    def test_class_name(self):
        return self.test_class.__class__.__name__


class TestCaseSignalEvent(TestCaseEvent):
    """The base class for test-case-signal-related events."""

    def __init__(self, test_class, test_case, test_signal):
        super().__init__(test_class, test_case)
        self.test_signal = test_signal


class TestCaseBeginEvent(TestCaseEvent):
    """The event posted when a test case has begun."""


class TestCaseEndEvent(TestCaseSignalEvent):
    """The event posted when a test case has ended."""


class TestCaseSkippedEvent(TestCaseSignalEvent):
    """The event posted when a test case has been skipped."""


class TestCaseFailureEvent(TestCaseSignalEvent):
    """The event posted when a test case has failed."""


class TestCasePassedEvent(TestCaseSignalEvent):
    """The event posted when a test case has passed."""


class TestClassEvent(TestEvent):
    """The base class for test-class-related events"""

    def __init__(self, test_class):
        super().__init__()
        self.test_class = test_class


class TestClassBeginEvent(TestClassEvent):
    """The event posted when a test class has begun testing."""


class TestClassEndEvent(TestClassEvent):
    """The event posted when a test class has finished testing."""

    def __init__(self, test_class, result):
        super().__init__(test_class)
        self.result = result