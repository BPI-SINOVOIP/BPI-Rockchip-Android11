# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import urlparse

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.update_engine import update_engine_test as uet

class autoupdate_DisconnectReconnectNetwork(uet.UpdateEngineTest):
    """
    Tests removing network for a couple minutes.

    This test will be used in conjunction with
    autoupdate_ForcedOOBEUpdate.interrupt and autoupdate_Interruptions.

    """
    version = 1

    # Sometimes when network is disabled update_engine progress will move a
    # little. To prevent false positives we fail only for > 1.5% movement.
    _ACCEPTED_MOVEMENT = 0.015

    def _has_progress_stopped(self):
        """Checks that the update_engine progress has stopped moving."""
        before = self._get_update_engine_status()[self._PROGRESS]
        for i in range(0, 10):
            if before != self._get_update_engine_status()[self._PROGRESS]:
                return False
            time.sleep(1)
        return True


    def run_once(self, update_url, time_without_network=120):
        if self._is_update_finished_downloading():
            raise error.TestFail('The update has already finished before we '
                                 'can disconnect network.')
        self._update_server = urlparse.urlparse(update_url).hostname
        self._disable_internet()

        # Check that we are offline.
        result = utils.ping(self._update_server, deadline=5, timeout=5)
        if result != 2:
            raise error.TestFail('Ping succeeded even though we were offline.')

        # We are seeing update_engine progress move a very tiny amount
        # after disconnecting network so wait for it to stop moving.
        utils.poll_for_condition(lambda: self._has_progress_stopped,
                                 desc='Waiting for update progress to stop.')

        # Get the update progress as the network is down
        progress_before = float(self._get_update_engine_status()[
            self._PROGRESS])

        seconds = 1
        while seconds < time_without_network:
            logging.info(self._get_update_engine_status())
            time.sleep(1)
            seconds += 1

        progress_after = float(self._get_update_engine_status()[
            self._PROGRESS])

        if progress_before != progress_after:
            if progress_before < progress_after:
                if progress_after - progress_before > self._ACCEPTED_MOVEMENT:
                    raise error.TestFail('The update continued while the '
                                         'network was supposedly disabled. '
                                         'Before: %f, After: %f' % (
                                         progress_before, progress_after))
                else:
                    logging.warning('The update progress moved slightly while '
                                    'network was off.')
            elif self._is_update_finished_downloading():
                raise error.TestFail('The update finished while the network '
                                     'was disabled. Before: %f, After: %f' %
                                     (progress_before, progress_after))
            else:
                raise error.TestFail('The update appears to have restarted. '
                                     'Before: %f, After: %f' % (progress_before,
                                                                progress_after))