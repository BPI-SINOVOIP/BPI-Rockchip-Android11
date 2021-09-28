# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.server.cros.update_engine import update_engine_test

class autoupdate_OmahaResponse(update_engine_test.UpdateEngineTest):
    """
    This server test is used just to get the URL of the payload to use. It
    will then call into a client side test to test different things in
    the omaha response (e.g switching between two urls, bad hash, bad SHA256).
    """
    version = 1

    def cleanup(self):
        """Cleans up after the test."""
        super(autoupdate_OmahaResponse, self).cleanup()
        self._host.reboot()

    def run_once(self, job_repo_url=None, full_payload=True,
                 running_at_desk=False, switch_urls=False, bad_sha256=False,
                 bad_metadata_size=False, test_backoff=False, backoff=False):
        """
        Runs the Omaha response test. This test can be configured to respond
        to an update client in variaty of ways.

        @param job_repo_url: A url pointing to the devserver where the autotest
            package for this build should be staged.
        @param full_payload: True if the payload should be full.
        @param running_at_desk: True if the test is being run locally.
        @param switch_urls: True if we want to test URL switch capability of
            update_engine.
        @param bad_sha256: True if the response should have invalid SHA256.
        @param bad_metadata_size: True if the response should have invalid
            metadta size.
        @param test_backoff: True if we want to test the backoff functionality.
        @param backoff: Whether the backoff is enabled or not.

        """

        self._job_repo_url = job_repo_url

        # Reboot DUT if a previous test left update_engine not idle.
        status = self._get_update_engine_status()
        if self._UPDATE_STATUS_IDLE != status[self._CURRENT_OP]:
            self._host.reboot()

        # Figure out the payload to use for the current build.
        payload = self._get_payload_url(full_payload=full_payload)
        image_url, _ = self._stage_payload_by_uri(payload)

        if running_at_desk:
            image_url = self._copy_payload_to_public_bucket(payload)
            logging.info('We are running from a workstation. Putting URL on a '
                         'public location: %s', image_url)

        if switch_urls:
            self._run_client_test_and_check_result('autoupdate_UrlSwitch',
                                                   image_url=image_url)

        if bad_sha256 or bad_metadata_size:
            self._run_client_test_and_check_result(
                'autoupdate_BadMetadata',
                image_url=image_url,
                bad_metadata_size=bad_metadata_size,
                bad_sha256=bad_sha256)

        if test_backoff:
            self._run_client_test_and_check_result('autoupdate_Backoff',
                                                   image_url=image_url,
                                                   backoff=backoff)
