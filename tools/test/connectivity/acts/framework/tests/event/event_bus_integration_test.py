#!/usr/bin/env python3
#
#   Copyright 2018 - Google, Inc.
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
import shutil
import tempfile
import unittest
from unittest import TestCase

import mobly.config_parser as mobly_config_parser

from acts.base_test import BaseTestClass
from acts.event import event_bus, subscription_bundle
from acts.event.decorators import subscribe, subscribe_static
from acts.event.event import Event
from acts.test_runner import TestRunner


class TestClass(BaseTestClass):
    instance_event_received = []
    static_event_received = []

    def __init__(self, configs):
        import mock
        self.log = mock.Mock()
        super().__init__(configs)

    @subscribe(Event)
    def subscribed_instance_member(self, event):
        TestClass.instance_event_received.append(event)

    @staticmethod
    @subscribe_static(Event)
    def subscribed_static_member(event):
        TestClass.static_event_received.append(event)

    def test_post_event(self):
        event_bus.post(Event())


class EventBusIntegrationTest(TestCase):
    """Tests the EventBus E2E."""
    def setUp(self):
        """Clears the event bus of all state."""
        self.called_event = False
        event_bus._event_bus = event_bus._EventBus()
        TestClass.instance_event_received = []
        TestClass.static_event_received = []

    def test_test_class_subscribed_fn_receives_event(self):
        """Tests that TestClasses have their subscribed functions called."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            test_run_config = mobly_config_parser.TestRunConfig()
            test_run_config.testbed_name = 'SampleTestBed'
            test_run_config.log_path = tmp_dir

            TestRunner(test_run_config, [('TestClass', [])]).run(TestClass)

        self.assertGreaterEqual(len(TestClass.instance_event_received), 1)
        self.assertEqual(len(TestClass.static_event_received), 0)

    def test_subscribe_static_bundles(self):
        """Tests that @subscribe_static bundles register their listeners."""
        bundle = subscription_bundle.create_from_static(TestClass)
        bundle.register()

        event_bus.post(Event())

        self.assertEqual(len(TestClass.instance_event_received), 0)
        self.assertEqual(len(TestClass.static_event_received), 1)

    def test_subscribe_instance_bundles(self):
        """Tests that @subscribe bundles register only instance listeners."""
        test_run_config = mobly_config_parser.TestRunConfig()
        test_run_config.testbed_name = ''
        test_run_config.log_path = ''
        test_object = TestClass(test_run_config)
        bundle = subscription_bundle.create_from_instance(test_object)
        bundle.register()

        event_bus.post(Event())

        self.assertEqual(len(TestClass.instance_event_received), 1)
        self.assertEqual(len(TestClass.static_event_received), 0)

    def test_event_register(self):
        """Tests that event.register()'d functions can receive posted Events."""
        def event_listener(_):
            self.called_event = True

        event_bus.register(Event, event_listener)
        event_bus.post(Event())

        self.assertTrue(self.called_event)

    def test_event_unregister(self):
        """Tests that an event can be registered, and then unregistered."""
        def event_listener(_):
            self.called_event = False

        registration_id = event_bus.register(Event, event_listener)
        event_bus.unregister(registration_id)
        event_bus.post(Event())

        self.assertFalse(self.called_event)


if __name__ == '__main__':
    unittest.main()
