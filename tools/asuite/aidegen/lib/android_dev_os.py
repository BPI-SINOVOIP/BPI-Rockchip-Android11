#!/usr/bin/env python3
#
# Copyright 2018 - The Android Open Source Project
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

"""An enum class to provide the running platform OS info.

AndroidDevOS provides 2 functions to get info from current platform type, it
maps strings got from python standard library method 'platform.system()' to 3
types of OS platform, i.e., Linux, Mac, and Windows.

    Class functions:
        get_os_type, which returns a relevant enum object for current platform.
        get_os_name, which returns the platform OS name in upper case.
"""

from enum import Enum, unique

import platform


@unique
class AndroidDevOS(str, Enum):
    """The string enum class identifies the OS for Android development.

    Currently, it can identify 3 types of OS, e.g., Mac, Linux, and Windows.
    And transform the os name into a meaningful platform name.
    """

    MAC = 'Darwin'
    LINUX = 'Linux'
    WINDOWS = 'Windows'

    @classmethod
    def get_os_name(cls):
        """Get the actual OS name.

        Return:
            AndroidDevOS enum, the os type name for current environment.
        """
        return cls.get_os_type().name

    @classmethod
    def get_os_type(cls):
        """get current OS type.

        Return:
            AndroidDevOS enum object, mapped to relevant platform.
        """
        return {
            'Darwin': cls.MAC,
            'Linux': cls.LINUX,
            'Windows': cls.WINDOWS
        }.get(platform.system(), cls.LINUX)
