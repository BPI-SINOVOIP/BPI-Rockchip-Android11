#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import errno
import logging
import os
import signal

from functools import wraps
from vts.runners.host import errors


class TimeoutError(errors.VtsError):
    """Timeout exception class to throw when a function times out."""


def timeout(seconds, message=os.strerror(errno.ETIME), no_exception=False):
    """Timeout decorator for functions.

    Args:
        seconds: int, number of seconds before timing out the decorated function.
        message: string, error message when the decorated function times out.
        no_exception: bool, whether to raise exception when decorated function times out.
                      If set to False, the function will stop execution and return None.

    Returns:
        Decorated function returns if no timeout, or None if timeout but no_exception is True.

    Raises:
        TimeoutError if decorated function times out.
        TypeError if seconds is not integer
    """
    def _handler_timeout(signum, frame):
       raise TimeoutError(message)

    def decorator(func):
        def wrapper(*args, **kwargs):
            if seconds > 0:
                signal.signal(signal.SIGALRM, _handler_timeout)
                signal.alarm(seconds)

            try:
                result = func(*args, **kwargs)
            except TimeoutError as e:
                if no_exception:
                    logging.error(message)
                    return None
                else:
                    raise e
            finally:
                signal.alarm(0)

            return result

        return wraps(func)(wrapper)

    return decorator
