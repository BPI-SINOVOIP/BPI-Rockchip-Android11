# Copyright (c) 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import common
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_FAFTModeTransitions(FirmwareTest):
    """This test checks FAFT mode transitions work."""
    version = 1

    def _checked_reboot(self, to_mode):
        """Reboots DUT to mode and sanity checks that it has done so.

        @param to_mode: mode_switcher mode to reboot into
        @type to_mode: string

        @see: autotest_lib.server.cros.faft.utils.mode_switcher
        """
        self.switcher.reboot_to_mode(to_mode)
        self.check_state((self.checkers.mode_checker, to_mode))

    def run_once(self, mode_seq=[]):
        """Main test logic.

        @param mode_seq: A list of mode_switcher modes to transition through
        @type mode_seq: list of strings

        @see: autotest_lib.server.cros.faft.utils.mode_switcher
        """

        if len(mode_seq) < 2:
            raise ValueError("Not enough transitions to test: %s" % mode_seq)

        logging.info("Testing transition sequence: %s",  " -> ".join(mode_seq))

        m1 = mode_seq[0]

        logging.info("Start in %s mode.", m1)
        self.switcher.setup_mode(m1)

        for m2 in mode_seq[1:]:
            logging.info("Checking mode transition: %s -> %s.", m1, m2)
            self._checked_reboot(m2)
            m1 = m2
