# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_LoginStartUpdateLogout(update_engine_test.UpdateEngineTest):
    """
    Logs in, starts an update, waits for a while, then logs out.

    This test is used as part of the server test autoupdate_Interruptions.

    """
    version = 1

    def run_once(self, server, port, progress_to_complete):
        # Login as regular user. Start an update. Then Logout
        with chrome.Chrome(logged_in=True):
            self._check_for_update(server=server, port=port)
            self._wait_for_progress(progress_to_complete)
