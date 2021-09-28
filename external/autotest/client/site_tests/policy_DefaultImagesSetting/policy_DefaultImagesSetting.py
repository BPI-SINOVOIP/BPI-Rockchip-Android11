# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import utils

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DefaultImagesSetting(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of DefaultImagesSetting policy on Chrome OS.

    If the policy is not set, or set to 1, images should be displayed within
    the browser on ChromeOS.

    If the policy is set to 2, the image should be blocked.

    It will determine if the game is playable via the presence of the
    snackbar class inside the main-frame-error function.

    """
    version = 1

    POLICY_NAME = 'DefaultImagesSetting'
    LOOKUP = {'allow': 1,
              'block': 2,
              'not_set': None}

    TEST_FILE = 'kittens.html'
    MINIMUM_IMAGE_WIDTH = 640

    def _image_check(self, policy_value):
        """
        Navigates to a test URL with an image and checks if the image is loaded
        or not.

        @param policy_value: the setting of the Policy

        """
        TEST_URL = '%s/%s' % (self.WEB_HOST, self.TEST_FILE)
        tab = self.navigate_to_url(TEST_URL)
        self._wait_for_page_ready(tab)

        image_is_blocked = self._is_image_blocked(tab)
        if (policy_value is None or policy_value == 1) and image_is_blocked:
            raise error.TestError('RIP')

        elif policy_value == 2 and not image_is_blocked:
            raise error.TestError('RIP')

        tab.Close()

    def _wait_for_page_ready(self, tab):
        utils.poll_for_condition(
            lambda: tab.EvaluateJavaScript('pageReady'),
            exception=error.TestError('Test page is not ready.'))

    def _is_image_blocked(self, tab):
        image_width = tab.EvaluateJavaScript(
            "document.getElementById('kittens_id').naturalWidth")
        return image_width < self.MINIMUM_IMAGE_WIDTH

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        policy_value = self.LOOKUP[case]
        self.setup_case(user_policies={self.POLICY_NAME: policy_value})
        self.start_webserver()
        self._image_check(case)
