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
import bisect
import logging
import inspect
from threading import RLock

from acts.event.event_subscription import EventSubscription
from acts.event.subscription_handle import SubscriptionHandle


class _EventBus(object):
    """
    Attributes:
        _subscriptions: A dictionary of {EventType: list<EventSubscription>}.
        _registration_id_map: A dictionary of
                             {RegistrationID: EventSubscription}
        _subscription_lock: The lock to prevent concurrent removal or addition
                            to events.
    """

    def __init__(self):
        self._subscriptions = {}
        self._registration_id_map = {}
        self._subscription_lock = RLock()

    def register(self, event_type, func, filter_fn=None, order=0):
        """Subscribes the given function to the event type given.

        Args:
            event_type: The type of the event to subscribe to.
            func: The function to call when the event is posted.
            filter_fn: An option function to be called before calling the
                       subscribed func. If this function returns falsy, then the
                       function will not be invoked.
            order: The order the the subscription should run in. Lower values
                   run first, with the default value set to 0. In the case of a
                   tie between two subscriptions of the same event type, the
                   subscriber added first executes first. In the case of a tie
                   between two subscribers of a different type, the type of the
                   subscription that is more specific goes first (i.e.
                   BaseEventType will execute after ChildEventType if they share
                   the same order).

        Returns:
            A registration ID.
        """
        subscription = EventSubscription(event_type, func,
                                         event_filter=filter_fn,
                                         order=order)
        return self.register_subscription(subscription)

    def register_subscriptions(self, subscriptions):
        """Registers all subscriptions to the event bus.

        Args:
            subscriptions: an iterable that returns EventSubscriptions

        Returns:
            The list of registration IDs.
        """
        registration_ids = []
        for subscription in subscriptions:
            registration_ids.append(self.register_subscription(subscription))

        return registration_ids

    def register_subscription(self, subscription):
        """Registers the given subscription to the event bus.

        Args:
            subscription: An EventSubscription object

        Returns:
            A registration ID.
        """
        with self._subscription_lock:
            if subscription.event_type in self._subscriptions.keys():
                subscription_list = self._subscriptions[subscription.event_type]
                subscription_list.append(subscription)
                subscription_list.sort(key=lambda x: x.order)
            else:
                subscription_list = list()
                bisect.insort(subscription_list, subscription)
                self._subscriptions[subscription.event_type] = subscription_list

            registration_id = id(subscription)
            self._registration_id_map[registration_id] = subscription

        return registration_id

    def post(self, event, ignore_errors=False):
        """Posts an event to its subscribers.

        Args:
            event: The event object to send to the subscribers.
            ignore_errors: Deliver to all subscribers, ignoring any errors.
        """
        listening_subscriptions = []
        for current_type in inspect.getmro(type(event)):
            if current_type not in self._subscriptions.keys():
                continue
            for subscription in self._subscriptions[current_type]:
                listening_subscriptions.append(subscription)

        # The subscriptions will be collected in sorted runs of sorted order.
        # Running timsort here is the optimal way to sort this list.
        listening_subscriptions.sort(key=lambda x: x.order)
        for subscription in listening_subscriptions:
            try:
                subscription.deliver(event)
            except Exception:
                if ignore_errors:
                    logging.exception('An exception occurred while handling '
                                      'an event.')
                    continue
                raise

    def unregister(self, registration_id):
        """Unregisters an EventSubscription.

        Args:
            registration_id: the Subscription or registration_id to unsubscribe.
        """
        if type(registration_id) is SubscriptionHandle:
            subscription = registration_id.subscription
            registration_id = id(registration_id.subscription)
        elif type(registration_id) is EventSubscription:
            subscription = registration_id
            registration_id = id(registration_id)
        elif registration_id in self._registration_id_map.keys():
            subscription = self._registration_id_map[registration_id]
        elif type(registration_id) is not int:
            raise ValueError(
                'Subscription ID "%s" is not a valid ID. This value'
                'must be an integer ID returned from subscribe().'
                % registration_id)
        else:
            # The value is a "valid" id, but is not subscribed. It's possible
            # another thread has unsubscribed this value.
            logging.warning('Attempted to unsubscribe %s, but the matching '
                            'subscription cannot be found.' % registration_id)
            return False

        event_type = subscription.event_type
        with self._subscription_lock:
            self._registration_id_map.pop(registration_id, None)
            if (event_type in self._subscriptions and
                    subscription in self._subscriptions[event_type]):
                self._subscriptions[event_type].remove(subscription)
        return True

    def unregister_all(self, from_list=None, from_event=None):
        """Removes all event subscriptions.

        Args:
            from_list: Unregisters all events from a given list.
            from_event: Unregisters all events of a given event type.
        """
        if from_list is None:
            from_list = list(self._registration_id_map.values())

        for subscription in from_list:
            if from_event is None or subscription.event_type == from_event:
                self.unregister(subscription)


_event_bus = _EventBus()


def register(event_type, func, filter_fn=None, order=0):
    """Subscribes the given function to the event type given.

    Args:
        event_type: The type of the event to subscribe to.
        func: The function to call when the event is posted.
        filter_fn: An option function to be called before calling the subscribed
                   func. If this function returns falsy, then the function will
                   not be invoked.
        order: The order the the subscription should run in. Lower values run
               first, with the default value set to 0. In the case of a tie
               between two subscriptions of the same event type, the
               subscriber added first executes first. In the case of a tie
               between two subscribers of a different type, the type of the
               subscription that is more specific goes first (i.e. BaseEventType
               will execute after ChildEventType if they share the same order).

    Returns:
        A registration ID.
    """
    return _event_bus.register(event_type, func, filter_fn=filter_fn,
                               order=order)


def register_subscriptions(subscriptions):
    """Registers all subscriptions to the event bus.

    Args:
        subscriptions: an iterable that returns EventSubscriptions

    Returns:
        The list of registration IDs.
    """
    return _event_bus.register_subscriptions(subscriptions)


def register_subscription(subscription):
    """Registers the given subscription to the event bus.

    Args:
        subscription: An EventSubscription object

    Returns:
        A registration ID.
    """
    return _event_bus.register_subscription(subscription)


def post(event, ignore_errors=False):
    """Posts an event to its subscribers.

    Args:
        event: The event object to send to the subscribers.
        ignore_errors: Deliver to all subscribers, ignoring any errors.
    """
    _event_bus.post(event, ignore_errors)


def unregister(registration_id):
    """Unregisters an EventSubscription.

    Args:
        registration_id: the Subscription or registration_id to unsubscribe.
    """
    return _event_bus.unregister(registration_id)


def unregister_all(from_list=None, from_event=None):
    """Removes all event subscriptions.

    Args:
        from_list: Unregisters all events from a given list.
        from_event: Unregisters all events of a given event type.
    """
    return _event_bus.unregister_all(from_list=from_list, from_event=from_event)


class listen_for(object):
    """A context-manager class (with statement) for listening to an event within
    a given section of code.

    Usage:

    with listen_for(EventType, event_listener):
        func_that_posts_event()  # Will call event_listener

    func_that_posts_event()  # Will not call event_listener

    """

    def __init__(self, event_type, func, filter_fn=None, order=0):
        self.event_type = event_type
        self.func = func
        self.filter_fn = filter_fn
        self.order = order
        self.registration_id = None

    def __enter__(self):
        self.registration_id = _event_bus.register(self.event_type, self.func,
                                                   filter_fn=self.filter_fn,
                                                   order=self.order)

    def __exit__(self, *unused):
        _event_bus.unregister(self.registration_id)
