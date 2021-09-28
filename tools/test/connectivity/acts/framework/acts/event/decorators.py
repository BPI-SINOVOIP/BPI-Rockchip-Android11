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
from acts.event.subscription_handle import InstanceSubscriptionHandle
from acts.event.subscription_handle import StaticSubscriptionHandle
from acts.event import subscription_bundle


def subscribe_static(event_type, event_filter=None, order=0):
    """A decorator that subscribes a static or module-level function.

    This function must be registered manually.
    """
    class InnerSubscriptionHandle(StaticSubscriptionHandle):
        def __init__(self, func):
            super().__init__(event_type, func,
                             event_filter=event_filter,
                             order=order)

    return InnerSubscriptionHandle


def subscribe(event_type, event_filter=None, order=0):
    """A decorator that subscribes an instance method."""
    class InnerSubscriptionHandle(InstanceSubscriptionHandle):
        def __init__(self, func):
            super().__init__(event_type, func,
                             event_filter=event_filter,
                             order=order)

    return InnerSubscriptionHandle


def register_static_subscriptions(decorated):
    """Registers all static subscriptions in decorated's attributes.

    Args:
        decorated: The object being decorated

    Returns:
        The decorated.
    """
    subscription_bundle.create_from_static(decorated).register()

    return decorated


def register_instance_subscriptions(obj):
    """A decorator that subscribes all instance subscriptions after object init.
    """
    old_init = obj.__init__

    def init_replacement(self, *args, **kwargs):
        old_init(self, *args, **kwargs)
        subscription_bundle.create_from_instance(self).register()

    obj.__init__ = init_replacement
    return obj
