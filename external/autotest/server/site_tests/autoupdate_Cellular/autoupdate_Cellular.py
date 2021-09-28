# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.server.cros.update_engine import update_engine_test

class autoupdate_Cellular(update_engine_test.UpdateEngineTest):
    """
    Tests auto updating over cellular.

    Usually with AU tests we use a lab devserver to hold the payload, and to be
    the omaha instance that DUTs ping. However, over cellular they will not be
    able to reach the devserver. So we will need to put the payload on a
    public google storage location. We will setup an omaha instance on the DUT
    (via autoupdate_CannedOmahaUpdate) that points to the payload on GStorage.

    """
    version = 1


    def cleanup(self):
        """Clean up the tests"""
        self._change_cellular_setting_in_update_engine(False)
        self._host.reboot()


    def run_once(self, job_repo_url=None, full_payload=True):
        """
        Runs the autoupdate test over cellular once.

        @param job_repo_url: The URL of the current build.
        @param full_payload: Whether the payload should be full or delta.

        """
        update_url = self.get_update_url_for_test(job_repo_url,
                                                  full_payload=full_payload,
                                                  public=True)

        self._change_cellular_setting_in_update_engine(True)
        self._run_client_test_and_check_result('autoupdate_CannedOmahaUpdate',
                                               image_url=update_url,
                                               use_cellular=True)
        self._check_for_cellular_entries_in_update_log()
