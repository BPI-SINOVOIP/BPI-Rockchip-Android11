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
import enum
import logging
import sys

from acts.controllers.android_device import AndroidDevice
from acts.libs import version_selector


class AndroidApi:
    OLDEST = 0
    MINIMUM = 0
    L = 21
    L_MR1 = 22
    M = 23
    N = 24
    N_MR1 = 25
    O = 26
    O_MR1 = 27
    P = 28
    LATEST = sys.maxsize
    MAX = sys.maxsize


def android_api(min_api=AndroidApi.OLDEST,
                max_api=AndroidApi.LATEST):
    """Decorates a function to only be called for the given API range.

    Only gets called if the AndroidDevice in the args is within the specified
    API range. Otherwise, a different function may be called instead. If the
    API level is out of range, and no other function handles that API level, an
    error is raise instead.

    Note: In Python3.5 and below, the order of kwargs is not preserved. If your
          function contains multiple AndroidDevices within the kwargs, and no
          AndroidDevices within args, you are NOT guaranteed the first
          AndroidDevice is the same one chosen each time the function runs. Due
          to this, we do not check for AndroidDevices in kwargs.

    Args:
         min_api: The minimum API level. Can be an int or an AndroidApi value.
         max_api: The maximum API level. Can be an int or an AndroidApi value.
    """
    def get_api_level(*args, **_):
        for arg in args:
            if isinstance(arg, AndroidDevice):
                return arg.sdk_api_level()
        logging.getLogger().error('An AndroidDevice was not found in the given '
                                  'arguments.')
        return None

    return version_selector.set_version(get_api_level, min_api, max_api)
