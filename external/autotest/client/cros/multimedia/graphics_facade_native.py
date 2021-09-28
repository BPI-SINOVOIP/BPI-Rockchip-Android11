# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to access the local graphics facade."""

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.graphics import graphics_utils


class GraphicsFacadeNative(object):
    """Facade to access graphics utilities for catching GPU hangs."""


    def __init__(self):
        """Initializes the graphics facade.

        Graphics state checker is initialized with a dedicated function to
        control timing for the initial set of errors extracted from logs.

        """
        self._graphics_state_checker = None


    def graphics_state_checker_initialize(self):
        """Create and initialize the graphics state checker object.

        This will establish existing errors and take a snapshot of graphics
        kernel memory.

        """
        self._graphics_state_checker = graphics_utils.GraphicsStateChecker()


    def graphics_state_checker_finalize(self):
        """Throw an error on new GPU hang messages in system logs.

        @raises TestError: Finalize was called before initialize.
        """
        if self._graphics_state_checker is None:
             raise error.TestError('Graphics state checker initialize not called.')
        self._graphics_state_checker.finalize()
