#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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
from unittest import mock

from acts.controllers.android_lib import services
from acts.controllers.android_lib.events import AndroidStartServicesEvent
from acts.controllers.android_lib.events import AndroidStopServicesEvent
from acts.event import event_bus


class ServicesTest(unittest.TestCase):
    """Tests acts.controllers.android_lib.services"""

    # AndroidService

    def test_register_adds_both_start_and_stop_methods(self):
        """Test that both the _start and _stop methods are registered to
        their respective events upon calling register().
        """
        event_bus._event_bus = event_bus._EventBus()
        service = services.AndroidService(mock.Mock())
        service.register()
        subscriptions = event_bus._event_bus._subscriptions
        self.assertTrue(
            any(subscription._func == service._start for subscription in
                subscriptions[AndroidStartServicesEvent]))
        self.assertTrue(
            any(subscription._func == service._stop for subscription in
                subscriptions[AndroidStopServicesEvent]))

    @unittest.mock.patch.object(services.AndroidService, '_start')
    def test_event_deliver_only_to_matching_serial(self, start_fn):
        """Test that the service only responds to events that matches its
        device serial.
        """
        event_bus._event_bus = event_bus._EventBus()
        service = services.AndroidService(mock.Mock())
        service.ad.serial = 'right_serial'
        service.register()

        wrong_ad = mock.Mock()
        wrong_ad.serial = 'wrong_serial'
        wrong_event = AndroidStartServicesEvent(wrong_ad)
        event_bus.post(wrong_event)
        start_fn.assert_not_called()

        right_ad = mock.Mock()
        right_ad.serial = 'right_serial'
        right_event = AndroidStartServicesEvent(right_ad)
        event_bus.post(right_event)
        start_fn.assert_called_with(right_event)

    def test_unregister_removes_both_start_and_stop_methods(self):
        """Test that both the _start and _stop methods are unregistered from
        their respective events upon calling unregister().
        """
        event_bus._event_bus = event_bus._EventBus()
        service = services.AndroidService(mock.Mock())
        service.register()
        service.unregister()
        subscriptions = event_bus._event_bus._subscriptions
        self.assertFalse(
            any(subscription._func == service._start for subscription in
                subscriptions[AndroidStartServicesEvent]))
        self.assertFalse(
            any(subscription._func == service._stop for subscription in
                subscriptions[AndroidStopServicesEvent]))


if __name__ == '__main__':
    unittest.main()
