#!/usr/bin/env python3
# Copyright 2019 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A meta class for singleton pattern."""


class Singleton(type):
    """A singleton metaclass.

    Class attributes:
        _instances: A dictionary keeps singletons' information, whose key is
        class and value is an instance of that class.

    Usage:
        from aidegen.lib import singleton

        class AClass(BaseClass, metaclass=singleton.Singleton):
            pass
    """

    _instances = {}

    def __call__(cls, *args, **kwds):
        """Initialize a singleton instance."""
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwds)
        return cls._instances[cls]
