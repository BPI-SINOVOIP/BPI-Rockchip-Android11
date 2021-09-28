# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_Cr50TpmManufactured(FirmwareTest):
    """Check if the TPM is manufactured."""
    version = 1

    NUM_RESETS = 5
    MANUFACTURED_RE = 'tpm_manufactured:( NOT)? manufactured'

    def run_once(self):
        """Check if the TPM is manufactured."""
        # Signing is different in dev signed images. This test doesn't really
        # apply.
        if not self.cr50.using_prod_rw_keys():
            raise error.TestNAError('Running dev signed image')

        # Reset the AP a couple of times in case the console drops any
        # characters.
        for i in range(self.NUM_RESETS):
            self.servo.get_power_state_controller().warm_reset()
            time.sleep(10)
        # Use cr50 uart capture to check the cr50 console messages. We could
        # send sysrst pulse and check the cr50 response, but that would require
        # opening cr50 which may not be possible if the TPM isn't manufactured.
        # We want to make this as accessible as possible and checking the
        # captured uart is the best way to do that.
        self._record_uart_capture()
        with open(self.cr50_uart_file, 'r') as f:
            contents = f.read()
        logging.debug('Cr50 reset logs:\n%s', contents)

        found = re.findall(self.MANUFACTURED_RE, contents)
        # The uart won't be captured if someone has the cr50 console open.
        if not found:
            raise error.TestNAError('Could not find %r. Close cr50 console' %
                                    self.MANUFACTURED_RE)
        logging.debug('Matches for tpm_manufactured: %r', found)
        res = set(found)
        if len(res) > 1:
            raise error.TestError('Found more than one manufacture setting')
        manufactured_state = res.pop()
        logging.info('tpm%s manufactured', manufactured_state)
        if ' NOT' == manufactured_state:
            raise error.TestFail('TPM not manufactured')
