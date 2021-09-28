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

import sys
from acts.event import subscription_bundle
from acts.event.decorators import subscribe
from acts.event.decorators import subscribe_static
from acts.event.event import Event
from acts.event.subscription_bundle import SubscriptionBundle
from mock import Mock
from mock import patch


class SubscriptionBundleTest(TestCase):
    """Tests the SubscriptionBundle class."""

    def test_add_calls_add_subscription_properly(self):
        """Tests that the convenience function add() calls add_subscription."""
        event = object()
        func = object()
        event_filter = object()
        order = object()
        package = SubscriptionBundle()
        package.add_subscription = Mock()

        package.add(event, func, event_filter=event_filter, order=order)

        self.assertEqual(package.add_subscription.call_count, 1)
        subscription = package.add_subscription.call_args[0][0]
        self.assertEqual(subscription._event_type, event)
        self.assertEqual(subscription._func, func)
        self.assertEqual(subscription._event_filter, event_filter)
        self.assertEqual(subscription.order, order)

    @patch('acts.event.event_bus.register_subscription')
    def test_add_subscription_registers_sub_if_package_is_registered(
            self, register_subscription):
        """Tests that add_subscription registers the subscription if the
        SubscriptionBundle is already registered."""
        package = SubscriptionBundle()
        package._registered = True
        mock_subscription = Mock()

        package.add_subscription(mock_subscription)

        self.assertEqual(register_subscription.call_count, 1)
        register_subscription.assert_called_with(mock_subscription)

    def test_add_subscription_adds_to_subscriptions(self):
        """Tests add_subscription adds the subscription to subscriptions."""
        mock_subscription = Mock()
        package = SubscriptionBundle()

        package.add_subscription(mock_subscription)

        self.assertTrue(mock_subscription in package.subscriptions.keys())

    def test_remove_subscription_removes_subscription_from_subscriptions(self):
        """Tests remove_subscription removes the given subscription from the
        subscriptions dictionary."""
        mock_subscription = Mock()
        package = SubscriptionBundle()
        package.subscriptions[mock_subscription] = id(mock_subscription)

        package.remove_subscription(mock_subscription)

        self.assertTrue(mock_subscription not in package.subscriptions.keys())

    @patch('acts.event.event_bus.unregister')
    def test_remove_subscription_unregisters_subscription(self, unregister):
        """Tests that removing a subscription will also unregister it if the
        SubscriptionBundle is registered."""
        mock_subscription = Mock()
        package = SubscriptionBundle()
        package._registered = True
        package.subscriptions[mock_subscription] = id(mock_subscription)

        package.remove_subscription(mock_subscription)

        self.assertEqual(unregister.call_count, 1)
        unregistered_obj = unregister.call_args[0][0]
        self.assertTrue(unregistered_obj == id(mock_subscription) or
                        unregistered_obj == mock_subscription)

    @patch('acts.event.event_bus.register_subscription')
    def test_register_registers_all_subscriptions(self, register_subscription):
        """Tests register() registers all subscriptions within the bundle."""
        mock_subscription_list = [Mock(), Mock(), Mock()]
        package = SubscriptionBundle()
        package._registered = False
        for subscription in mock_subscription_list:
            package.subscriptions[subscription] = None

        package.register()

        self.assertEqual(register_subscription.call_count,
                         len(mock_subscription_list))
        args = {args[0] for args, _ in register_subscription.call_args_list}
        for subscription in mock_subscription_list:
            self.assertTrue(subscription in args or id(subscription) in args)

    @patch('acts.event.event_bus.unregister')
    def test_register_registers_all_subscriptions(self, unregister):
        """Tests register() registers all subscriptions within the bundle."""
        mock_subscription_list = [Mock(), Mock(), Mock()]
        package = SubscriptionBundle()
        package._registered = True
        for subscription in mock_subscription_list:
            package.subscriptions[subscription] = id(subscription)

        package.unregister()

        self.assertEqual(unregister.call_count, len(mock_subscription_list))
        args = {args[0] for args, _ in unregister.call_args_list}
        for subscription in mock_subscription_list:
            self.assertTrue(subscription in args or id(subscription) in args)


class SubscriptionBundleStaticFunctions(TestCase):
    """Tests the static functions found in subscription_bundle.py"""

    @staticmethod
    @subscribe_static(Event)
    def static_listener_1():
        pass

    @staticmethod
    @subscribe_static(Event)
    def static_listener_2():
        pass

    @subscribe(Event)
    def instance_listener_1(self):
        pass

    @subscribe(Event)
    def instance_listener_2(self):
        pass

    def test_create_from_static(self):
        """Tests create_from_static gets all StaticSubscriptionHandles."""
        cls = self.__class__
        bundle = subscription_bundle.create_from_static(cls)

        self.assertEqual(len(bundle.subscriptions), 2)
        keys = bundle.subscriptions.keys()
        self.assertIn(cls.static_listener_1.subscription, keys)
        self.assertIn(cls.static_listener_2.subscription, keys)

    def test_create_from_instance(self):
        """Tests create_from_instance gets all InstanceSubscriptionHandles."""
        bundle = subscription_bundle.create_from_instance(self)

        self.assertEqual(len(bundle.subscriptions), 2)
        keys = bundle.subscriptions.keys()
        self.assertIn(self.instance_listener_1.subscription, keys)
        self.assertIn(self.instance_listener_2.subscription, keys)


@subscribe_static(Event)
def static_listener_1():
    pass


class SubscribeStaticModuleLevelTest(TestCase):
    def test_create_from_static(self):
        """Tests create_from_static gets all StaticSubscriptionHandles."""
        bundle = subscription_bundle.create_from_static(
            sys.modules[self.__module__])

        self.assertEqual(len(bundle.subscriptions), 1)
        keys = bundle.subscriptions.keys()
        self.assertIn(static_listener_1.subscription, keys)


if __name__ == '__main__':
    unittest.main()
