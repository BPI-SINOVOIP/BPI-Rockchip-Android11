# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.cellular import test_environment
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_CannedOmahaUpdate(update_engine_test.UpdateEngineTest):
    """
    Client-side mechanism to update a DUT with a given image.

    Restarts update_engine and attempts an update from the image
    pointed to by |image_url| of size |image_size| with checksum
    |image_sha256|. The rest of the parameters are optional.

    If the |allow_failure| parameter is True, then the test will
    succeed even if the update failed.

    """
    version = 1


    def run_canned_update(self, allow_failure, port):
        """
        Performs the update.

        @param allow_failure: True if we dont raise an error on failure.
        @param port: The port number we need to connect to on the server.

        """

        try:
            self._check_for_update(port=port, critical_update=True,
                                   wait_for_completion=True)
        except error.CmdError as e:
            if not allow_failure:
                raise error.TestFail('Update attempt failed: %s' %
                                     self._get_last_error_string())
            else:
                logging.info('Ignoring failed update. Failure reason: %s', e)


    def run_once(self, image_url, allow_failure=False, public_key=None,
                 use_cellular=False):
        """
        Runs an update with canned response using Nebraska.

        @param image_url: The payload url.
        @param allow_failure: If true, failing the update is expected.
        @param public_key: The public key to serve to the update client.
        @param use_cellular: True if this test uses cellular.

        """

        metadata_dir = autotemp.tempdir()
        self._get_payload_properties_file(image_url,
                                          metadata_dir.name,
                                          public_key=public_key)
        base_url = ''.join(image_url.rpartition('/')[0:2])
        with nebraska_wrapper.NebraskaWrapper(
                log_dir=self.resultsdir,
                update_metadata_dir=metadata_dir.name,
                update_payloads_address=base_url) as nebraska:

            if not use_cellular:
                self.run_canned_update(allow_failure, nebraska.get_port())
                return

            # Setup DUT so that we have ssh over ethernet but DUT uses
            # cellular as main connection.
            try:
                with test_environment.CellularOTATestEnvironment() as test_env:
                    service = test_env.shill.wait_for_cellular_service_object()
                    if not service:
                        raise error.TestError('No cellular service found.')

                    CONNECT_TIMEOUT = 120
                    test_env.shill.connect_service_synchronous(
                            service, CONNECT_TIMEOUT)
                    self.run_canned_update(allow_failure, nebraska.get_port())
            except error.TestError as e:
                # Raise as test failure so it is propagated to server test
                # failure message.
                logging.error('Failed setting up cellular connection.')
                raise error.TestFail(e)
