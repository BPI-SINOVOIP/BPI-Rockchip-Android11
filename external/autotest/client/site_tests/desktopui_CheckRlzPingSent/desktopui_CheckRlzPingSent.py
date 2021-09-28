# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import time

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros import cryptohome
from autotest_lib.client.cros.input_playback import input_playback


class desktopui_CheckRlzPingSent(test.test):
    """Tests creating a new user, doing a google search, checking RLZ Data."""
    version = 1

    _RLZ_DATA_FILE = "/home/chronos/user/RLZ Data"

    def _verify_rlz_data(self, expect_caf_ping=True):
        """
        Checks the RLZ data file for CAI and CAF ping events.

        @param expect_caf_ping: True if expecting the CAF event to be in the
                                RLZ data file, False if not expecting it.

        """
        def rlz_data_exists():
            """Check rlz data exists."""
            rlz_data = json.loads(utils.run('cat "%s"' %
                                            self._RLZ_DATA_FILE).stdout)
            logging.debug('rlz data: %s', rlz_data)
            if 'stateful_events' in rlz_data:
                cai_present = 'CAI' in rlz_data['stateful_events']['C']['_']
                caf_present = 'CAF' in rlz_data['stateful_events']['C']['_']
                return cai_present and (caf_present == expect_caf_ping)
            return False

        utils.poll_for_condition(rlz_data_exists, timeout=120)


    def _check_url_for_rlz(self, cr):
        """
        Does a Google search and ensures there is an rlz parameter.

        @param cr: Chrome instance.

        """
        timeout_minutes = 2
        timeout = time.time() + 60 * timeout_minutes

        # Setup a keyboard emulator to open new tabs and type a search.
        with input_playback.InputPlayback() as player:
            player.emulate(input_type='keyboard')
            player.find_connected_inputs()

            while True:
                # Open a new tab, search in the omnibox.
                player.blocking_playback_of_default_file(
                    input_type='keyboard', filename='keyboard_ctrl+t')
                player.blocking_playback_of_default_file(
                    input_type='keyboard', filename='keyboard_b+a+d+enter')
                logging.info(cr.browser.tabs[-1].url)
                if 'rlz=' in cr.browser.tabs[-1].url:
                    break
                else:
                    if time.time() > timeout:
                        raise error.TestFail('RLZ ping did not send in %d '
                                             'minutes.' % timeout_minutes)
                    time.sleep(10)


    def _wait_for_rlz_lock(self):
        """Waits for the DUT to get into locked state after login."""
        def get_install_lockbox_finalized_status():
            status = cryptohome.get_tpm_more_status()
            return status.get('install_lockbox_finalized')

        try:
            utils.poll_for_condition(
                lambda: get_install_lockbox_finalized_status(),
                exception=utils.TimeoutError(),
                timeout=120)
        except utils.TimeoutError:
            raise error.TestFail('Timed out trying to lock the device')


    def run_once(self, ping_timeout=30, logged_in=True, expect_caf_ping=True):
        """
        Tests whether or not the RLZ install event (CAI) and first-use event
        (CAF) pings are sent. After the first user login, the CAI ping will
        be sent after a certain delay. This delay is 24 hours by default, but
        can be overridden by specifying the rlz-ping-delay flag in
        /etc/chrome_dev.conf, or by using the --rlz-ping-delay argument to
        Chrome. Then, if two RW_VPD settings have the correct values
        (should_send_rlz_ping == 1, rlz_embargo_end_date has passed OR not
        specified), the CAF ping will be sent as well. Ping status is checked
        in the /home/chronos/user/'RLZ Data' file, which will contain entries
        for CAI and CAF pings in the 'stateful_events' section.

        @param logged_in: True for real login or False for guest mode.
        @param ping_timeout: Delay time (seconds) before any RLZ pings are
                             sent.
        @param expect_caf_ping: True if expecting the first-use event (CAF)
                                ping to be sent, False if not expecting it.
                                The ping would not be expected if the relevant
                                RW_VPD settings do not have the right
                                combination of values.

        """
        # Browser arg to make DUT send rlz ping after a short delay.
        rlz_flag = '--rlz-ping-delay=%d' % ping_timeout

        # If we are testing the ping is sent in guest mode (logged_in=False),
        # we need to first do a real login and wait for the DUT to become
        # 'locked' for rlz. Then logout and enter guest mode.
        if not logged_in:
            with chrome.Chrome(logged_in=True, extra_browser_args=rlz_flag):
                self._wait_for_rlz_lock()

        with chrome.Chrome(logged_in=logged_in,
                           extra_browser_args=rlz_flag) as cr:
            self._check_url_for_rlz(cr)
            self._verify_rlz_data(expect_caf_ping=expect_caf_ping)
