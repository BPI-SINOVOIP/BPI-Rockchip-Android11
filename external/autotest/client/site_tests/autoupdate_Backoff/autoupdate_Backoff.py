# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_Backoff(update_engine_test.UpdateEngineTest):
    """
    Tests update_engine's backoff mechanism.

    When an update fails, update_engine will not allow another update to the
    same URL for a certain backoff period. The backoff period is stored in
    /var/lib/update_engine/prefs/backoff-expiry-time. It is stored as an
    integer representing the number of microseconds since 1/1/1601.

    By default, backoff is disabled on test images but by creating a
    'no-ignore-backoff' pref we can test it.

    """
    version = 1

    _BACKOFF_DISABLED = 'Resetting backoff expiry time as payload backoff is ' \
                        'disabled'
    _BACKOFF_ENABLED = 'Incrementing the backoff expiry time'
    _BACKOFF_ERROR = 'Updating payload state for error code: 40 (' \
                     'ErrorCode::kOmahaUpdateDeferredForBackoff)'
    _BACKOFF_EXPIRY_TIME_PREF = 'backoff-expiry-time'
    _NO_IGNORE_BACKOFF_PREF = 'no-ignore-backoff'


    def cleanup(self):
        """Cleans up the state and extra files this test created."""
        utils.run('rm %s' % self._no_ignore_backoff, ignore_status=True)
        utils.run('rm %s' % self._backoff_expiry_time, ignore_status=True)
        super(autoupdate_Backoff, self).cleanup()

    def run_once(self, image_url, backoff):
        """
        Tests update_engine can do backoff.

        @param image_url: The payload url.
        @param backoff: True if backoff is enabled.

        """
        self._no_ignore_backoff = os.path.join(
            self._UPDATE_ENGINE_PREFS_DIR, self._NO_IGNORE_BACKOFF_PREF)
        self._backoff_expiry_time = os.path.join(
            self._UPDATE_ENGINE_PREFS_DIR, self._BACKOFF_EXPIRY_TIME_PREF)
        utils.run('touch %s' % self._no_ignore_backoff, ignore_status=True)

        metadata_dir = autotemp.tempdir()
        self._get_payload_properties_file(image_url,
                                          metadata_dir.name)
        base_url = ''.join(image_url.rpartition('/')[0:2])
        with nebraska_wrapper.NebraskaWrapper(
                log_dir=self.resultsdir,
                update_metadata_dir=metadata_dir.name,
                update_payloads_address=base_url) as nebraska:

            # Only set one URL in the Nebraska response so we can test the
            # backoff functionality quicker.
            response_props = {'disable_payload_backoff': not backoff,
                              'failures_per_url': 1,
                              'critical_update': True}

            # Start the update.
            self._check_for_update(port=nebraska.get_port(),
                                   interactive=False,
                                   **response_props)
            self._wait_for_progress(0.2)

            # Disable internet so the update fails.
            self._disable_internet()
            self._wait_for_update_to_fail()
            self._enable_internet()

            if backoff:
                self._check_update_engine_log_for_entry(self._BACKOFF_ENABLED,
                                                        raise_error=True)
                utils.run('cat %s' % self._backoff_expiry_time)
                try:
                    self._check_for_update(port=nebraska.get_port(),
                                           interactive=False,
                                           wait_for_completion=True,
                                           **response_props)
                except error.CmdError as e:
                    logging.info('Update failed as expected.')
                    logging.error(e)
                    self._check_update_engine_log_for_entry(self._BACKOFF_ERROR,
                                                            raise_error=True)
                    return

                raise error.TestFail('Second update attempt succeeded. It was '
                                     'supposed to have failed due to backoff.')
            else:
                self._check_update_engine_log_for_entry(self._BACKOFF_DISABLED,
                                                        raise_error=True)
                self._check_for_update(port=nebraska.get_port(),
                                       interactive=False,
                                       **response_props)
                self._wait_for_update_to_complete()
