# Copyright 2016 The Android Open Source Project
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

"""Signal related functionality."""

import os
import signal
import sys

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path


def relay_signal(handler, signum, frame):
    """Notify a listener returned from getsignal of receipt of a signal.

    Returns:
      True if it was relayed to the target, False otherwise.
      False in particular occurs if the target isn't relayable.
    """
    if handler in (None, signal.SIG_IGN):
        return True
    if handler == signal.SIG_DFL:
        # This scenario is a fairly painful to handle fully, thus we just
        # state we couldn't handle it and leave it to client code.
        return False
    handler(signum, frame)
    return True
