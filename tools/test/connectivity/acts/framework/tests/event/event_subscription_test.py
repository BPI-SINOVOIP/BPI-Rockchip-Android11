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
import unittest
from unittest import TestCase

from mock import Mock

from acts.event.event_subscription import EventSubscription


class EventSubscriptionTest(TestCase):
    """Tests the EventSubscription class."""

    @staticmethod
    def filter_out_event(_):
        return False

    @staticmethod
    def pass_filter(_):
        return True

    def test_event_type_returns_correct_value(self):
        """Tests that event_type returns the correct event type."""
        expected_event_type = Mock()
        subscription = EventSubscription(expected_event_type, lambda _: None)
        self.assertEqual(expected_event_type, subscription.event_type)

    def test_deliver_dont_deliver_if_event_is_filtered(self):
        """Tests deliver does not call func if the event is filtered out."""
        func = Mock()
        subscription = EventSubscription(Mock(), func,
                                         event_filter=self.filter_out_event)

        subscription.deliver(Mock())

        self.assertFalse(func.called)

    def test_deliver_deliver_accepted_event(self):
        """Tests deliver does call func when the event is accepted."""
        func = Mock()
        subscription = EventSubscription(Mock(), func,
                                         event_filter=self.pass_filter)

        subscription.deliver(Mock())
        self.assertTrue(func.called)


if __name__ == '__main__':
    unittest.main()
