# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50Unlock(Cr50Test):
    """Verify cr50 unlock."""
    version = 1
    PASSWORD = 'Password'


    def run_once(self):
        """Check cr50 can see dev mode open works correctly"""
        # Make sure testlab mode is enabled, so we can guarantee the password
        # can be cleared.
        if not self.faft_config.has_powerbutton:
            raise error.TestNAError('Can not run test without power button')
            return

        self.fast_open(enable_testlab=True)
        self.cr50.send_command('ccd reset')
        # Set the password
        self.set_ccd_password(self.PASSWORD)
        if self.cr50.password_is_reset():
            raise error.TestFail('Failed to set password')

        self.cr50.set_ccd_level('lock')

        # Verify the password can be used to unlock the console
        self.cr50.send_command('ccd unlock ' + self.PASSWORD)
        if self.cr50.get_ccd_level() != 'unlock':
            raise error.TestFail('Could not unlock cr50 with the password')

        self.cr50.set_ccd_level('lock')
        # Try with the lowercase version of the passsword. Make sure it doesn't
        # work.
        self.cr50.send_command('ccd unlock ' + self.PASSWORD.lower())
        if self.cr50.get_ccd_level() == 'unlock':
            raise error.TestFail('Unlocked cr50 with incorrect password')
        # TODO(b/111418310): figure out what's going on. I dont know why, but
        # for some reason you can't immediately unlock cr50 after a failure on
        # the command line. 5 seconds should be enough.
        #
        # State as of 0.4.8
        # incorrect unlock from command line
        # and immediate unlock attempt from AP -> FAILURE
        #
        # incorrect unlock from command line,
        # wait a bit, and unlock attempt from AP -> SUCCESS

        # Try Unlock from the AP
        try:
            self.ccd_unlock_from_ap(self.PASSWORD, expect_error=True)
        except:
            raise error.TestError('Something is different.Check b/111418310. '
                                  'Is it fixed?')
        else:
            logging.info('Unintentional rate limiting is still happening')

        # Show the same thing works with a 2 second wait
        self.cr50.set_ccd_level('lock')
        self.cr50.send_command('ccd unlock ' + self.PASSWORD.lower())
        if self.cr50.get_ccd_level() != 'lock':
            raise error.TestFail('Unlocked cr50 from AP with incorrect '
                    'password')
        # Unintentional limit. I don't know the exact limit. 3 seconds should
        # be enough.
        time.sleep(5)
        self.ccd_unlock_from_ap(self.PASSWORD)

        # Show the rate limit doesn't apply to Unlocking from the AP.
        # Try Unlock from the AP with lower case version. Make sure it fails
        self.cr50.set_ccd_level('lock')
        self.ccd_unlock_from_ap(self.PASSWORD.lower(), expect_error=True)
        if self.cr50.get_ccd_level() != 'lock':
            raise error.TestFail('Unlocked cr50 from AP with incorrect '
                    'password')
        # Test immediate unlock from AP
        self.ccd_unlock_from_ap(self.PASSWORD)
        # Clear the password which has set at the beginning of this test.
        self.set_ccd_password('clear:' + self.PASSWORD)
