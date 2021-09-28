# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.multimedia import display_facade_native
from autotest_lib.client.cros.multimedia import facade_resource


class display_InternalDisplayRotation(test.test):
    """Display test case which rotates the internal display"""
    version = 1
    ROTATIONS = [90, 180, 270, 0]
    STANDARD_ROTATION = 0
    DELAY_BEFORE_ROTATION = DELAY_AFTER_ROTATION = 3

    def cleanup(self):
        """Autotest cleanup method"""
        # If the rotation is not standard then change rotation to standard
        if self.display_facade:
            if self.display_facade.get_display_rotation(
                    self.internal_display_id) != self.STANDARD_ROTATION:
                logging.info("Setting standard rotation")
                self.display_facade.set_display_rotation(
                        self.internal_display_id, self.STANDARD_ROTATION,
                        self.DELAY_BEFORE_ROTATION, self.DELAY_AFTER_ROTATION)

    def run_once(self):
        """Test to rotate internal display"""
        facade = facade_resource.FacadeResource()
        facade.start_default_chrome()
        self.display_facade = display_facade_native.DisplayFacadeNative(facade)
        self.internal_display_id = self.display_facade.get_internal_display_id()
        logging.info("Internal display ID is %s", self.internal_display_id)
        rotation_before_starts = self.display_facade.get_display_rotation(
                self.internal_display_id)
        logging.info("Rotation before test starts is %d",
                     rotation_before_starts)
        for angle in self.ROTATIONS:
            logging.info("Rotation to be set %d", angle)
            self.display_facade.set_display_rotation(self.internal_display_id,
                                                     angle,
                                                     self.DELAY_BEFORE_ROTATION,
                                                     self.DELAY_AFTER_ROTATION)
            rotation = self.display_facade.get_display_rotation(
                    self.internal_display_id)
            logging.info("Internal display rotation is set to %s", rotation)
            if rotation != angle:
                raise error.TestFail('Failed to set %d rotation' % angle)