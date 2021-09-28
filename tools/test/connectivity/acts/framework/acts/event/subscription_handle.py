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
from acts.event.event_subscription import EventSubscription


class SubscriptionHandle(object):
    """The object created by a method decorated with an event decorator."""

    def __init__(self, event_type, func, event_filter=None, order=0):
        self._event_type = event_type
        self._func = func
        self._event_filter = event_filter
        self._order = order
        self._subscription = None
        self._owner = None

    @property
    def subscription(self):
        if self._subscription:
            return self._subscription
        self._subscription = EventSubscription(self._event_type, self._func,
                                               event_filter=self._event_filter,
                                               order=self._order)
        return self._subscription

    def __get__(self, instance, owner):
        # If our owner has been initialized, or do not have an instance owner,
        # return self.
        if self._owner is not None or instance is None:
            return self

        # Otherwise, we create a new SubscriptionHandle that will only be used
        # for the instance that owns this SubscriptionHandle.
        ret = SubscriptionHandle(self._event_type, self._func,
                                 self._event_filter, self._order)
        ret._owner = instance
        ret._func = ret._wrap_call(ret._func)
        for attr, value in owner.__dict__.items():
            if value is self:
                setattr(instance, attr, ret)
                break
        return ret

    def _wrap_call(self, func):
        def _wrapped_call(*args, **kwargs):
            if self._owner is None:
                return func(*args, **kwargs)
            else:
                return func(self._owner, *args, **kwargs)
        return _wrapped_call

    def __call__(self, *args, **kwargs):
        return self._func(*args, **kwargs)


class InstanceSubscriptionHandle(SubscriptionHandle):
    """A SubscriptionHandle for instance methods."""


class StaticSubscriptionHandle(SubscriptionHandle):
    """A SubscriptionHandle for static methods."""
