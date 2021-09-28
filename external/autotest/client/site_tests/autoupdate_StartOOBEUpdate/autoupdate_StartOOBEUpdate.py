# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.cellular import test_environment
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_StartOOBEUpdate(update_engine_test.UpdateEngineTest):
    """Starts a forced update at OOBE.

    Chrome OS will restart when the update is complete so this test will just
    start the update. The rest of the processing will be done in a server
    side test.
    """
    version = 1


    def initialize(self):
        """Test setup."""
        super(autoupdate_StartOOBEUpdate, self).initialize()
        self._clear_custom_lsb_release()


    def _skip_to_oobe_update_screen(self):
        """Skips to the OOBE update check screen."""
        self._oobe.WaitForJavaScriptCondition("typeof Oobe == 'function' && "
                                              "Oobe.readyForTesting",
                                              timeout=30)
        self._oobe.ExecuteJavaScript('Oobe.skipToUpdateForTesting()')


    def _start_oobe_update(self, url):
        """
        Jump to the update check screen at OOBE and wait for update to start.

        @param url: The omaha update URL we expect to call.

        """
        self._create_custom_lsb_release(url)
        # Start chrome instance to interact with OOBE.
        self._chrome = chrome.Chrome(auto_login=False)
        self._oobe = self._chrome.browser.oobe
        self._skip_to_oobe_update_screen()

        timeout = 180
        err_str = 'Update did not start within %d seconds.' % timeout
        try:
            utils.poll_for_condition(self._is_update_started,
                                     error.TestFail(err_str),
                                     timeout=timeout)
        except error.TestFail as e:
            if self._critical_update:
                if not self._get_update_requests():
                    raise error.TestFail('%s There were no update requests in'
                                         ' update_engine.log. OOBE update'
                                         ' screen was missed.' % err_str)
                err_code = self._get_last_error_string()
                if err_code is not None:
                    raise error.TestFail('%s %s' % (err_str, err_code))
                else:
                    raise e


    def run_once(self, image_url, cellular=False, critical_update=True):
        """
        Test that will start a forced update at OOBE.

        @param image_url: The omaha URL to call. It contains the payload url
                          for cellular tests.
        @param cellular: True if we should run this test using a sim card.
        @param critical_update: True if we should have deadline:now in omaha
                                response.

        """
        self._critical_update = critical_update

        if cellular:
            try:
                with test_environment.CellularOTATestEnvironment() as test_env:
                    service = test_env.shill.wait_for_cellular_service_object()
                    if not service:
                        raise error.TestError('No cellular service found.')
                    connect_timeout = 120
                    test_env.shill.connect_service_synchronous(
                        service, connect_timeout)

                    metadata_dir = autotemp.tempdir()
                    self._get_payload_properties_file(image_url,
                                                      metadata_dir.name)
                    # Setup a Nebraska instance on the DUT because we can't
                    # reach devservers over cellular.
                    base_url = ''.join(image_url.rpartition('/')[0:2])
                    with nebraska_wrapper.NebraskaWrapper(
                            log_dir=self.resultsdir,
                            update_metadata_dir=metadata_dir.name,
                            update_payloads_address=base_url) as nebraska:

                        self._start_oobe_update(nebraska.get_update_url(
                            critical_update=self._critical_update))

                        # Remove the custom omaha server from lsb release
                        # because after we reboot it will no longer be running.
                        self._clear_custom_lsb_release()

                        # We need to return from the client test before OOBE
                        # reboots or the server side test will hang. But we
                        # cannot return right away when the OOBE update starts
                        # because all of the code from using a cellular
                        # connection is in client side and we will switch back
                        # to ethernet. So we need to wait for the update to get
                        # as close to the end as possible so that we are done
                        # downloading the payload via cellular and don't need to
                        # ping omaha again. When the DUT reboots it will send a
                        # final update ping to production omaha and then move to
                        # the sign in screen.
                        self._wait_for_update_to_complete(finalizing_ok=True)
            except error.TestError as e:
                logging.error('Failure setting up sim card.')
                raise error.TestFail(e)

        else:
            if self._critical_update:
                self._start_oobe_update(image_url)
            else:
                metadata_dir = autotemp.tempdir()
                self._get_payload_properties_file(image_url, metadata_dir.name)
                base_url = ''.join(image_url.rpartition('/')[0:2])
                with nebraska_wrapper.NebraskaWrapper(
                        log_dir=metadata_dir.name,
                        update_metadata_dir=metadata_dir.name,
                        update_payloads_address=base_url) as nebraska:
                    self._start_oobe_update(nebraska.get_update_url())
