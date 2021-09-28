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
import threading
import unittest
from unittest import TestCase

from mock import Mock
from mock import patch

from acts.event import event_bus
from acts.event.event import Event
from acts.event.event_subscription import EventSubscription


class EventBusTest(TestCase):
    """Tests the event_bus functions."""

    def setUp(self):
        """Clears all state from the event_bus between test cases."""
        event_bus._event_bus = event_bus._EventBus()

    def get_subscription_argument(self, register_subscription_call):
        """Gets the subscription argument from a register_subscription call."""
        return register_subscription_call[0][0]

    @patch('acts.event.event_bus._event_bus.register_subscription')
    def test_register_registers_a_subscription(self, register_subscription):
        """Tests that register creates and registers a subscription."""
        mock_event = Mock()
        mock_func = Mock()
        order = 43
        event_bus.register(mock_event, mock_func, order=order)

        args, _ = register_subscription.call_args
        subscription = args[0]

        # Instead of writing an equality operator for only testing,
        # check the internals to make sure they are expected values.
        self.assertEqual(subscription._event_type, mock_event)
        self.assertEqual(subscription._func, mock_func)
        self.assertEqual(subscription.order, order)

    @patch('acts.event.event_bus._event_bus.register_subscription')
    def test_register_subscriptions_for_list(self, register_subscription):
        """Tests that register_subscription is called for each subscription."""
        mocks = [Mock(), Mock(), Mock()]
        subscriptions = [
            EventSubscription(mocks[0], lambda _: None),
            EventSubscription(mocks[1], lambda _: None),
            EventSubscription(mocks[2], lambda _: None),
        ]

        event_bus.register_subscriptions(subscriptions)
        received_subscriptions = set()
        for index, call in enumerate(register_subscription.call_args_list):
            received_subscriptions.add(self.get_subscription_argument(call))

        self.assertEqual(register_subscription.call_count, len(subscriptions))
        self.assertSetEqual(received_subscriptions, set(subscriptions))

    def test_register_subscription_new_event_type(self):
        """Tests that the event_bus can register a new event type."""
        mock_type = Mock()
        bus = event_bus._event_bus
        subscription = EventSubscription(mock_type, lambda _: None)

        reg_id = event_bus.register_subscription(subscription)

        self.assertTrue(mock_type in bus._subscriptions.keys())
        self.assertTrue(subscription in bus._subscriptions[mock_type])
        self.assertTrue(reg_id in bus._registration_id_map.keys())

    def test_register_subscription_existing_type(self):
        """Tests that the event_bus can register an existing event type."""
        mock_type = Mock()
        bus = event_bus._event_bus
        bus._subscriptions[mock_type] = [
            EventSubscription(mock_type, lambda _: None)]
        new_subscription = EventSubscription(mock_type, lambda _: True)

        reg_id = event_bus.register_subscription(new_subscription)

        self.assertTrue(new_subscription in bus._subscriptions[mock_type])
        self.assertTrue(reg_id in bus._registration_id_map.keys())

    def test_post_to_unregistered_event_does_not_call_other_funcs(self):
        """Tests posting an unregistered event will not call other funcs."""
        mock_subscription = Mock()
        bus = event_bus._event_bus
        mock_type = Mock()
        mock_subscription.event_type = mock_type
        bus._subscriptions[mock_type] = [mock_subscription]

        event_bus.post(Mock())

        self.assertEqual(mock_subscription.deliver.call_count, 0)

    def test_post_to_registered_event_calls_all_registered_funcs(self):
        """Tests posting to a registered event calls all registered funcs."""
        mock_subscriptions = [Mock(), Mock(), Mock()]
        bus = event_bus._event_bus
        for subscription in mock_subscriptions:
            subscription.order = 0
        mock_event = Mock()
        bus._subscriptions[type(mock_event)] = mock_subscriptions

        event_bus.post(mock_event)

        for subscription in mock_subscriptions:
            subscription.deliver.assert_called_once_with(mock_event)

    def test_post_with_ignore_errors_calls_all_registered_funcs(self):
        """Tests posting with ignore_errors=True calls all registered funcs,
        even if they raise errors.
        """
        def _raise(_):
            raise Exception
        mock_event = Mock()
        mock_subscriptions = [Mock(), Mock(), Mock()]
        mock_subscriptions[0].deliver.side_effect = _raise
        bus = event_bus._event_bus
        for i, subscription in enumerate(mock_subscriptions):
            subscription.order = i
        bus._subscriptions[type(mock_event)] = mock_subscriptions

        event_bus.post(mock_event, ignore_errors=True)

        for subscription in mock_subscriptions:
            subscription.deliver.assert_called_once_with(mock_event)

    @patch('acts.event.event_bus._event_bus.unregister')
    def test_unregister_all_from_list(self, unregister):
        """Tests unregistering from a list unregisters the specified list."""
        unregister_list = [Mock(), Mock()]

        event_bus.unregister_all(from_list=unregister_list)

        self.assertEqual(unregister.call_count, len(unregister_list))
        for args, _ in unregister.call_args_list:
            subscription = args[0]
            self.assertTrue(subscription in unregister_list)

    @patch('acts.event.event_bus._event_bus.unregister')
    def test_unregister_all_from_event(self, unregister):
        """Tests that all subscriptions under the event are unregistered."""
        mock_event = Mock()
        mock_event_2 = Mock()
        bus = event_bus._event_bus
        unregister_list = [Mock(), Mock()]
        bus._subscriptions[type(mock_event_2)] = [Mock(), Mock(), Mock()]
        bus._subscriptions[type(mock_event)] = unregister_list
        for sub_type in bus._subscriptions.keys():
            for subscription in bus._subscriptions[sub_type]:
                subscription.event_type = sub_type
                bus._registration_id_map[id(subscription)] = subscription

        event_bus.unregister_all(from_event=type(mock_event))

        self.assertEqual(unregister.call_count, len(unregister_list))
        for args, _ in unregister.call_args_list:
            subscription = args[0]
            self.assertTrue(subscription in unregister_list)

    @patch('acts.event.event_bus._event_bus.unregister')
    def test_unregister_all_no_args_unregisters_everything(self, unregister):
        """Tests unregister_all without arguments will unregister everything."""
        mock_event_1 = Mock()
        mock_event_2 = Mock()
        bus = event_bus._event_bus
        unregister_list_1 = [Mock(), Mock()]
        unregister_list_2 = [Mock(), Mock(), Mock()]
        bus._subscriptions[type(mock_event_1)] = unregister_list_1
        bus._subscriptions[type(mock_event_2)] = unregister_list_2
        for sub_type in bus._subscriptions.keys():
            for subscription in bus._subscriptions[sub_type]:
                subscription.event_type = sub_type
                bus._registration_id_map[id(subscription)] = subscription

        event_bus.unregister_all()

        self.assertEqual(unregister.call_count,
                         len(unregister_list_1) + len(unregister_list_2))
        for args, _ in unregister.call_args_list:
            subscription = args[0]
            self.assertTrue(subscription in unregister_list_1 or
                            subscription in unregister_list_2)

    def test_unregister_given_an_event_subscription(self):
        """Tests that unregister can unregister a given EventSubscription."""
        mock_event = Mock()
        bus = event_bus._event_bus
        subscription = EventSubscription(type(mock_event), lambda _: None)
        bus._registration_id_map[id(subscription)] = subscription
        bus._subscriptions[type(mock_event)] = [subscription]

        val = event_bus.unregister(subscription)

        self.assertTrue(val)
        self.assertTrue(subscription not in bus._registration_id_map)
        self.assertTrue(
            subscription not in bus._subscriptions[type(mock_event)])

    def test_unregister_given_a_registration_id(self):
        """Tests that unregister can unregister a given EventSubscription."""
        mock_event = Mock()
        bus = event_bus._event_bus
        subscription = EventSubscription(type(mock_event), lambda _: None)
        registration_id = id(subscription)
        bus._registration_id_map[id(subscription)] = subscription
        bus._subscriptions[type(mock_event)] = [subscription]

        val = event_bus.unregister(registration_id)

        self.assertTrue(val)
        self.assertTrue(subscription not in bus._registration_id_map)
        self.assertTrue(
            subscription not in bus._subscriptions[type(mock_event)])

    def test_unregister_given_object_that_is_not_a_subscription(self):
        """Asserts that a ValueError is raised upon invalid arguments."""
        with self.assertRaises(ValueError):
            event_bus.unregister(Mock())

    def test_unregister_given_invalid_registration_id(self):
        """Asserts that a false is returned upon invalid registration_id."""
        val = event_bus.unregister(9)
        self.assertFalse(val)

    def test_listen_for_registers_listener(self):
        """Tests listen_for registers the listener within the with statement."""
        bus = event_bus._event_bus

        def event_listener(_):
            pass

        with event_bus.listen_for(Event, event_listener):
            self.assertEqual(len(bus._registration_id_map), 1)

    def test_listen_for_unregisters_listener(self):
        """Tests listen_for unregisters the listener after the with statement.
        """
        bus = event_bus._event_bus

        def event_listener(_):
            pass

        with event_bus.listen_for(Event, event_listener):
            pass

        self.assertEqual(len(bus._registration_id_map), 0)


if __name__ == '__main__':
    unittest.main()
