# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.input_playback import keyboard


class policy_ArcVideoCaptureAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of the ArcVideoCaptureAllowed ChromeOS policy on ARC.

    This test will launch the ARC container via the ArcEnabled policy, then
    will check the behavior of the passthrough policy VideoCaptureAllowed.

    When the policy is set to False, Video Capture is not allowed. To test
    this, we will attemp to launch the ARC Camera, and check the logs to see
    if the Camera was launched or not.

    """
    version = 1

    def _launch_Arc_Cam(self):
        """Grant the Camera location permission, and launch the Camera app."""
        self.ui.click_and_wait_for_item_with_retries(
            'Launcher',
            '/Search your device, apps/',
            isRegex_wait=True)
        self.ui.doDefault_on_obj('/Search your device, apps/', isRegex=True)
        for button in 'cam':
            time.sleep(0.1)
            self.keyboard.press_key(button)
        self.ui.wait_for_ui_obj('/Camera/', isRegex=True)
        self.ui.doDefault_on_obj('/Camera/', isRegex=True)

    def _test_Arc_cam_status(self, expected):
        """
        Test if the Arc Camera has been opened, or not.

        @param case: bool, value of the VideoCaptureAllowed policy.

        """
        #  Once the Camera is open, get the status from logcat.

        if expected is False:
            self.ui.did_obj_not_load('/Switch to take photo/',
                                     isRegex=True,
                                     timeout=10)
        else:
            self.ui.wait_for_ui_obj('/Switch to take photo/',
                                    isRegex=True,
                                    timeout=10)

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        pol = {'ArcEnabled': True,
               'VideoCaptureAllowed': case}

        self.setup_case(user_policies=pol,
                        arc_mode='enabled',
                        use_clouddpc_test=False)

        # Allow the ARC container time to apply the policy...
        self.keyboard = keyboard.Keyboard()
        self.ui.start_ui_root(self.cr)

        self._launch_Arc_Cam()
        self._test_Arc_cam_status(case)
