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

from acts.event import event_bus
from acts.event.decorators import register_instance_subscriptions
from acts.event.decorators import register_static_subscriptions
from acts.event.decorators import subscribe
from acts.event.decorators import subscribe_static
from acts.event.event import Event
from acts.event.subscription_handle import SubscriptionHandle
from mock import Mock


class DecoratorsTest(TestCase):
    """Tests the decorators found in acts.event.decorators."""

    def test_subscribe_static_return_type(self):
        """Tests that the subscribe_static is the correct type."""
        mock = Mock()

        @subscribe_static(type)
        def test(_):
            return mock

        self.assertTrue(isinstance(test, SubscriptionHandle))

    def test_subscribe_static_calling_the_function_returns_normally(self):
        """Tests that functions decorated by subscribe_static can be called."""
        static_mock = Mock()

        @subscribe_static(type)
        def test(_):
            return static_mock

        self.assertEqual(test(Mock()), static_mock)

    class DummyClass(object):
        def __init__(self):
            self.mock = Mock()

        @subscribe(type)
        def test(self, _):
            return self.mock

    def test_subscribe_return_type(self):
        """Tests that subscribe returns the correct type."""
        dummy_class = DecoratorsTest.DummyClass()
        self.assertTrue(isinstance(dummy_class.test, SubscriptionHandle))

    def test_subscribe_calling_the_function_returns_normally(self):
        """tests that functions decorated by subscribe can be called."""
        dummy_class = DecoratorsTest.DummyClass()
        self.assertEqual(dummy_class.test(''), dummy_class.mock)


class DummyEvent(Event):
    """A dummy event used for testing registered functions."""


class RegisterStaticSubscriptionsTest(TestCase):

    def test_register_static_subscriptions_returns_passed_in_object(self):
        obj = Mock()
        returned_value = register_static_subscriptions(obj)
        self.assertEqual(obj, returned_value,
                         'register_static_subscriptions returned a value other'
                         'than the object passed in.')

    def test_register_static_subscriptions_registers_properly(self):
        @register_static_subscriptions
        class RegisterStaticSubscriptionsClass(object):
            captured_event = None

            @staticmethod
            @subscribe_static(DummyEvent)
            def on_static_event(evt):
                RegisterStaticSubscriptionsClass.captured_event = evt

        event = DummyEvent()
        event_bus.post(event)

        self.assertEqual(event, RegisterStaticSubscriptionsClass.captured_event,
                         'register_static_subscriptions did not subscribe '
                         'RegisterStaticSubscriptionsClass.on_static_event.')


class RegisterInstanceSubscriptionsTest(TestCase):

    def test_register_instance_subscriptions_returns_passed_in_object(self):
        class SomeClass(object):
            pass

        returned_value = register_instance_subscriptions(SomeClass)
        self.assertEqual(SomeClass, returned_value,
                         'register_instance_subscriptions returned a value '
                         'other than the object passed in.')

    def test_register_instance_subscriptions_registers_properly(self):
        @register_instance_subscriptions
        class RegisterInstanceSubscriptionsClass(object):
            def __init__(self):
                self.captured_event = None

            @subscribe(DummyEvent)
            def on_instance_event(self, evt):
                self.captured_event = evt

        instance = RegisterInstanceSubscriptionsClass()
        event = DummyEvent()
        event_bus.post(event)

        self.assertEqual(
            event, instance.captured_event,
            'register_instance_subscriptions did not subscribe the instance '
            'function RegisterInstanceSubscriptionsClass.on_instance_event.')


if __name__ == '__main__':
    unittest.main()
