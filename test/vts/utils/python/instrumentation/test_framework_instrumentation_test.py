#!/usr/bin/env python
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
#

import unittest

from vts.utils.python.instrumentation import test_framework_instrumentation as tfi
from vts.utils.python.instrumentation import test_framework_instrumentation_event as tfie
from vts.utils.python.instrumentation import test_framework_instrumentation_test_submodule as tfits


class TestFrameworkInstrumentationTest(unittest.TestCase):
    """Unit tests for test_framework_instrumentation module"""

    def setUp(self):
        """Setup tasks"""
        self.category = 'category_default'
        self.name = 'name_default'
        tfie.event_data = []
        tfie.event_stack = []
        tfi.counts = {}

    def testEventName(self):
        """Tests whether illegal characters are being recognized and replaced."""
        for name in tfie.ILLEGAL_CHARS:
            # TODO(yuexima): disable error logging for this test case
            event = tfie.TestFrameworkInstrumentationEvent(name, '')
            self.assertNotEqual(event.name, name, 'name %s should not be accepted.' % name)

    def testEventMatch(self):
        """Tests whether Event object can match with a category and name."""
        category = '1'
        name = '2'
        event = tfie.TestFrameworkInstrumentationEvent(name, category)
        self.assertTrue(event.Match(name, category))
        self.assertFalse(event.Match('3', category))

    def testEndAlreadyEnded(self):
        """Tests End command on already ended event."""
        event = tfi.Begin(self.name, self.category, enable_logging=False)
        event.End()
        self.assertEqual(event.status, 2)
        self.assertIsNone(event.error)
        event.End()
        self.assertEqual(event.status, 2)
        self.assertTrue(event.error)

    def testEndMatch(self):
        """Tests End command with name matching."""
        event = tfi.Begin(self.name, self.category)
        self.assertEqual(event.status, 1)
        tfi.End(self.name, self.category)
        self.assertEqual(event.status, 2)
        self.assertIsNone(event.error)

    def testEndFromOtherModule(self):
        """Tests the use of End command from another module."""
        event = tfi.Begin(self.name, self.category)
        self.assertEqual(event.status, 1)
        tfits.TestFrameworkInstrumentationTestSubmodule().End(self.name, self.category)
        self.assertEqual(event.status, 2)
        self.assertIsNone(event.error)

    def testCategories(self):
        """Tests access to TestFrameworkInstrumentationCategories object"""
        self.assertTrue(tfi.categories.Add(self.name, self.category))
        self.assertFalse(tfi.categories.Add('', self.name))
        self.assertFalse(tfi.categories.Add(None, self.name))
        self.assertFalse(tfi.categories.Add('1a', self.name))

    def testCheckEnded(self):
        """Tests the CheckEnded method of TestFrameworkInstrumentationEvent"""
        event = tfi.Begin(self.name, self.category)

        # Verify initial condition
        self.assertTrue(bool(tfie.event_stack))
        self.assertEqual(event.status, 1)

        event.CheckEnded()
        # Check event status is Remove
        self.assertEqual(event.status, 3)

        # Check event is removed from stack
        self.assertFalse(bool(tfie.event_stack))

        # Check whether duplicate calls doesn't give error
        event.CheckEnded()
        self.assertEqual(event.status, 3)

    def testRemove(self):
        """Tests the Remove method of TestFrameworkInstrumentationEvent"""
        event = tfi.Begin(self.name, self.category)

        # Verify initial condition
        self.assertTrue(bool(tfie.event_stack))
        self.assertEqual(event.status, 1)

        event.Remove()
        # Check event status is Remove
        self.assertEqual(event.status, 3)

        # Check event is removed from stack
        self.assertFalse(bool(tfie.event_stack))

        # Check whether duplicate calls doesn't give error
        event.Remove()
        self.assertEqual(event.status, 3)

    def testEndAlreadyRemoved(self):
        """Tests End command on already ended event."""
        event = tfi.Begin(self.name, self.category, enable_logging=False)
        reason = 'no reason'
        event.Remove(reason)
        self.assertEqual(event.status, 3)
        self.assertEqual(event.error, reason)
        event.End()
        self.assertEqual(event.status, 3)
        self.assertNotEqual(event.error, reason)

    def testEnableLogging(self):
        """Tests the enable_logging option."""
        # Test not specified case
        event = tfi.Begin(self.name, self.category)
        self.assertFalse(event._enable_logging)
        event.End()

        # Test set to True case
        event = tfi.Begin(self.name, self.category, enable_logging=True)
        self.assertTrue(event._enable_logging)
        event.End()

        # Test set to False case
        event = tfi.Begin(self.name, self.category, enable_logging=None)
        self.assertFalse(event._enable_logging)
        event.End()

    def testDisableSubEventLoggingOverwriting(self):
        """Tests the disable_subevent_logging option's overwriting feature.

        Tests whether the top event's disable_subevent_logging overwrite
        subevent's disable_subevent_logging option only when it is set to True
        """
        # Test top event disable_subevent_logging option not specified case
        event = tfi.Begin(self.name, self.category)
        self.assertFalse(event._disable_subevent_logging)
        event_sub = tfi.Begin(self.name, self.category, disable_subevent_logging=True)
        self.assertTrue(event_sub._disable_subevent_logging)
        event_sub.End()
        event.End()

        # Test top event disable_subevent_logging option set to False
        event = tfi.Begin(self.name, self.category, disable_subevent_logging=False)
        self.assertFalse(event._disable_subevent_logging)
        event_sub = tfi.Begin(self.name, self.category, disable_subevent_logging=True)
        self.assertTrue(event_sub._disable_subevent_logging)
        event_sub.End()
        event.End()

        # Test top event disable_subevent_logging option set to True
        event = tfi.Begin(self.name, self.category, disable_subevent_logging=True)
        self.assertTrue(event._disable_subevent_logging)
        event_sub1 = tfi.Begin(self.name, self.category, disable_subevent_logging=False)
        self.assertTrue(event_sub1._disable_subevent_logging)
        event_sub1.End()
        event_sub2 = tfi.Begin(self.name, self.category)
        self.assertTrue(event_sub2._disable_subevent_logging)
        event_sub2.End()
        event.End()

    def testDisableSubEventLoggingNesting(self):
        """Tests the disable_subevent_logging option.

        Tests whether the top event's disable_subevent_logging can overwrite
        subevents of deeper levels when set to True.
        """
        # Test top event disable_subevent_logging option set to True
        event = tfi.Begin(self.name, self.category, disable_subevent_logging=True)
        self.assertTrue(event._disable_subevent_logging)
        event_sub = tfi.Begin(self.name, self.category, disable_subevent_logging=False)
        self.assertTrue(event_sub._disable_subevent_logging)
        event_sub_sub1 = tfi.Begin(self.name, self.category)
        self.assertTrue(event_sub_sub1._disable_subevent_logging)
        event_sub_sub1.End()
        event_sub_sub2 = tfi.Begin(self.name, self.category, disable_subevent_logging=False)
        self.assertTrue(event_sub_sub2._disable_subevent_logging)
        event_sub_sub2.End()
        event_sub.End()
        event.End()

    def testCount(self):
        """Tests the count API."""
        tfi.Count(self.name, self.category)
        tfi.Count(self.name, self.category)
        self.assertEqual(len(tfi.counts), 1)
        self.assertEqual(len(tfi.counts[self.name, self.category]), 2)
        tfi.Count(self.name)
        self.assertEqual(len(tfi.counts), 2)

    def testGenerateTextReport(self):
        """Tests the GenerateTextReport method."""
        event = tfi.Begin('name1', 'cat1', disable_subevent_logging=True)
        event_sub = tfi.Begin('name2', 'cat2', disable_subevent_logging=False)
        event_sub.End()
        event.End()
        res = tfi.GenerateTextReport()

        # Checks result is not empty
        self.assertGreater(len(res), 0)

        # Since the format of the result is subject to change, here we only
        # checks whether the names and categories are mentioned in the result.
        self.assertIn('name1', res)
        self.assertIn('name2', res)
        self.assertIn('cat1', res)
        self.assertIn('cat2', res)


if __name__ == "__main__":
    unittest.main()