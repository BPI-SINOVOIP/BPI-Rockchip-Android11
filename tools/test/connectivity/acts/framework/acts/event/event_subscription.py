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


class EventSubscription(object):
    """A class that defines the way a function is subscribed to an event.

    Attributes:
        event_type: The type of the event.
        _func: The subscribed function.
        _event_filter: A lambda that returns True if an event should be passed
                       to the subscribed function.
        order: The order value in which this subscription should be called.
    """
    def __init__(self, event_type, func, event_filter=None, order=0):
        self._event_type = event_type
        self._func = func
        self._event_filter = event_filter
        self.order = order

    @property
    def event_type(self):
        return self._event_type

    def deliver(self, event):
        """Delivers an event to the subscriber.

        This function will not deliver the event if the event filter rejects the
        event.

        Args:
            event: The event to send to the subscriber.
        """
        if self._event_filter and not self._event_filter(event):
            return
        self._func(event)
