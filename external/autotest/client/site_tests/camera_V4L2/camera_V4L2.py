# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.video import device_capability


class camera_V4L2(test.test):
    version = 1
    preserve_srcdir = True

    def run_once(self, capability=None, test_list=None):
        if capability is not None:
            device_capability.DeviceCapability().ensure_capability(capability)
        # Enable USB camera HW timestamp
        path = "/sys/module/uvcvideo/parameters/hwtimestamps"
        if os.path.exists(path):
            utils.system("echo 1 > %s" % path)

        if test_list is None:
            test_list = "halv3" if self.should_test_halv3() else "default"
        self.test_list = test_list

        self.find_video_capture_devices()

        for device in self.v4l2_devices:
            self.run_v4l2_test(device)

    def should_test_halv3(self):
        has_v3 = os.path.exists('/usr/bin/cros_camera_service')
        has_v1 = os.path.exists('/usr/bin/arc_camera_service')
        return has_v3 and not has_v1

    def find_video_capture_devices(self):
        cmd = ["media_v4l2_test", "--list_usbcam"]
        stdout = utils.system_output(cmd, retain_output=True)
        self.v4l2_devices = stdout.splitlines()
        if not self.v4l2_devices:
            raise error.TestFail("No V4L2 devices found!")

    def run_v4l2_test(self, device):
        cmd = [
                "media_v4l2_test",
                "--device_path=%s" % device,
        ]
        if self.test_list:
            cmd.append("--test_list=%s" % self.test_list)

        logging.info("Running %s", cmd)
        stdout = utils.system_output(cmd, retain_output=True)
