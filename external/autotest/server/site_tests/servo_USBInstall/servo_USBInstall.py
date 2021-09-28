# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from autotest_lib.server import test
from autotest_lib.server.hosts import repair_utils


class servo_USBInstall(test.test):
    """Force install cros to a dut from servo"""
    version = 1

    def run_once(self, host):
        """
        Force install image from servo to a dut via USB

        @param host Host object representing DUT to be re-imaged.
        """
        repair_utils.require_servo(host)
        _, update_url = host.stage_image_for_servo()
        host.servo_install(update_url)
