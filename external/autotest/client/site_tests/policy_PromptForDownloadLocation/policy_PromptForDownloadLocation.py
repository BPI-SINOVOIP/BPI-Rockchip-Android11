# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base

from telemetry.core import exceptions


class policy_PromptForDownloadLocation(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_PromptForDownloadLocation policy on Chrome OS.

    This test will set the policy, navigate to a test download link, then
    check if the file is downloaded or not.

    """
    version = 1

    POLICY_NAME = 'PromptForDownloadLocation'
    _DOWNLOAD_BASE = ('http://commondatastorage.googleapis.com/'
                      'chromiumos-test-assets-public/audio_power/')

    def run_once(self, case=None):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """

        self.setup_case(user_policies={self.POLICY_NAME: case})

        try:
            self.navigate_to_url(self._DOWNLOAD_BASE)

        #  The url is a test URL and doesn't actually load anything, causing
        #  a Timeout. This is expected and OK.
        except exceptions.TimeoutException:
            pass

        #  Check the downloads directory for the file.
        files = utils.system_output('ls /home/chronos/user/Downloads/')

        if case:
            if 'download' in files:
                raise error.TestError(
                    'Download did not prompt for location when it should have.')
        else:
            if 'download' not in files:
                raise error.TestError(
                    'Download prompted for location when it should not have.')
