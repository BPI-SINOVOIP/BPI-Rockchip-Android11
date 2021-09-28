# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_CrashBrowserAfterUpdate(update_engine_test.UpdateEngineTest):
    """Tests creating a new user and crashing the browser during an update."""
    version = 1

    def run_once(self):
        try:
            # Login as a new user.
            with chrome.Chrome(logged_in=True, dont_override_profile=False) \
                as cr:
                self._wait_for_update_to_complete()
                tab = cr.browser.tabs[0]
                tab.Navigate('chrome://inducebrowsercrashforrealz')
        except Exception as e:
            logging.info('Browser crashed as expected.')
            logging.error(e)
            return

        raise error.TestFail('Browser did not crash! Check the logs.')
