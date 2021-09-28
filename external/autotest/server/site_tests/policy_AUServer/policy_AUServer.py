# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server.cros.update_engine import update_engine_test


class policy_AUServer(update_engine_test.UpdateEngineTest):
    """
    This server test is used just to get the URL of the payload to use. It
    will then call into a client side test to test different things in
    the Omaha response.
    """
    version = 1


    def cleanup(self):
        """Cleanup for this test."""
        super(policy_AUServer, self).cleanup()
        tpm_utils.ClearTPMIfOwned(self._host)
        self._host.reboot()


    def run_once(self, client_test, case, full_payload=True,
                 job_repo_url=None, running_at_desk=False):
        """
        Starting point of this test.

        Note: base class sets host as self._host.

        @param client_test: the name of the Client test to run.
        @param case: the case to run for the given Client test.
        @param full_payload: whether the update should be full or incremental.
        @param job_repo_url: url provided at runtime (or passed in locally
                             when running at workstation).
        @param running_at_desk: indicates test is run from a workstation.

        """
        self._job_repo_url = job_repo_url

        # Clear TPM to ensure that client test can enroll device.
        tpm_utils.ClearTPMIfOwned(self._host)

        # Figure out the payload to use for the current build.
        payload = self._get_payload_url(full_payload=full_payload)
        image_url, _ = self._stage_payload_by_uri(payload)

        if running_at_desk:
            image_url = self._copy_payload_to_public_bucket(payload)
            logging.info('We are running from a workstation. Putting URL on a '
                         'public location: %s', image_url)

        logging.info('url: %s', image_url)

        self._run_client_test_and_check_result(client_test,
                                               case=case,
                                               image_url=image_url)
