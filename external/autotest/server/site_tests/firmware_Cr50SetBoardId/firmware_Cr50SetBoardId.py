# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros import filesystem_util
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50SetBoardId(Cr50Test):
    """Verify cr50-set-board-id.sh

    Verify cr50-set-board-id sets the correct board id and flags based on the
    given stage.
    """
    version = 1

    BID_SCRIPT = '/usr/share/cros/cr50-set-board-id.sh'

    # the command to get the brand name
    GET_BRAND = 'cros_config / brand-code'

    # Used when the flags were not initialized in the factory.
    UNKNOWN_FLAGS = 0xff00
    # Used for dev, proto, EVT, and DVT phases.
    DEVELOPMENT_FLAGS = 0x7f7f
    # Used for PVT and MP builds.
    RELEASE_FLAGS = 0x7f80
    TEST_MP_FLAGS = 0x10000
    PHASE_FLAGS_DICT = {
        'unknown' : UNKNOWN_FLAGS,

        'dev' : DEVELOPMENT_FLAGS,
        'proto' : DEVELOPMENT_FLAGS,
        'evt' : DEVELOPMENT_FLAGS,
        'dvt' : DEVELOPMENT_FLAGS,

        'mp' : RELEASE_FLAGS,
        'pvt' : RELEASE_FLAGS,
    }

    # The response strings from cr50-set-board-id
    SUCCESS = ["Successfully updated board ID to 'BID' with phase 'PHASE'.",
               0]
    ERROR_UNKNOWN_PHASE = ['Unknown phase (PHASE)', 1]
    ERROR_INVALID_RLZ = ['Invalid RLZ brand code (BID).', 1]
    ERROR_ALREADY_SET = ['Board ID and flag have already been set.', 2]
    ERROR_BID_SET_DIFFERENTLY = ['Board ID has been set differently.', 3]
    ERROR_FLAG_SET_DIFFERENTLY = ['Flag has been set differently.', 3]
    ERROR_BID_MISMATCH = ['Error 5 while setting board id', 1]

    def initialize(self, host, cmdline_args, full_args, bid=''):
        # Restore the original image, rlz code, and board id during cleanup.
        super(firmware_Cr50SetBoardId, self).initialize(host, cmdline_args,
              full_args, restore_cr50_image=True, restore_cr50_board_id=True)
        if self.servo.main_device_is_ccd():
            raise error.TestNAError('Use a flex cable instead of CCD cable.')

        result = self.host.run(self.GET_BRAND, ignore_status=True)
        platform_brand = result.stdout.strip()
        if result.exit_status or not platform_brand:
            raise error.TestNAError('Could not get "cros_config / brand-code"')
        self.platform_brand = platform_brand

        bid = self.get_saved_cr50_original_version()[2]
        self._bid_flags = int(bid.rsplit(':', 1)[-1], 16) if bid else 0
        if self._bid_flags == self.TEST_MP_FLAGS:
            raise error.TestNAError('cr50-set-board-id cannot be used with '
                                    'test mp images.')
        filesystem_util.make_rootfs_writable(self.host)
        self.host.run('rm %s' % cr50_utils.CR50_PREPVT, ignore_status=True)
        self.host.run('rm %s' % cr50_utils.CR50_PROD, ignore_status=True)


    def run_script(self, expected_result, phase, board_id=''):
        """Run the bid script with the given phase and board id

        Args:
            expected_result: a list with the result message and exit status
            phase: The phase string.
            board_id: The board id string.

        Raises:
            TestFail if the expected result message did not match the script
            output
        """
        message, exit_status = expected_result

        # If we expect an error ignore the exit status
        ignore_status = not not exit_status

        # Run the script with the phase and board id
        cmd = '%s %s %s' % (self.BID_SCRIPT, phase, board_id)
        result = self.host.run(cmd, ignore_status=ignore_status)

        # If we don't give the script a board id, it will use the platform
        # brand
        expected_board_id = board_id if board_id else self.platform_brand
        # Replace the placeholders with the expected board id and phase
        message = message.replace('BID', expected_board_id)
        message = message.replace('PHASE', phase)

        logging.info(result.stdout)
        # Compare the expected script output to the actual script result
        if message not in result.stdout or exit_status != result.exit_status:
            logging.debug(result)
            raise error.TestFail('Expected "%s" got "%s"' % (message,
                                 result.stdout))


    def eraseflashinfo(self):
        """Eraseflashinfo if the board id is set."""
        if cr50_utils.GetChipBoardId(self.host) == cr50_utils.ERASED_CHIP_BID:
            return
        # Erase the board id so we can change it.
        self.eraseflashinfo_and_restore_image()


    def run_once(self):
        """Verify cr50-set-board-id.sh"""
        self.eraseflashinfo()
        # 'A' is too short to be a valid rlz code
        self.run_script(self.ERROR_INVALID_RLZ, 'dvt', 'A')
        # dummy_phase is not a valid phase
        self.run_script(self.ERROR_UNKNOWN_PHASE, 'dummy_phase')
        # The rlz code is checked after the phase
        self.run_script(self.ERROR_UNKNOWN_PHASE, 'dummy_phase', 'A')

        self.eraseflashinfo()
        # Set the board id so we can verify cr50-set-board-id has the correct
        # response to the board id already being set.
        self.run_script(self.SUCCESS, 'dvt', 'TEST')
        # mp has different flags than dvt
        self.run_script(self.ERROR_FLAG_SET_DIFFERENTLY, 'mp', 'TEST')
        # try setting the dvt flags with a different board id
        self.run_script(self.ERROR_BID_SET_DIFFERENTLY, 'dvt', 'test')
        # running with the same phase and board id will raise an error that the
        # board id is already set
        self.run_script(self.ERROR_ALREADY_SET, 'dvt', 'TEST')

        # Verify each stage sets the right flags
        for phase, flags in self.PHASE_FLAGS_DICT.iteritems():
            self.eraseflashinfo()

            expected_response = self.SUCCESS
            expected_brand = self.platform_brand
            expected_flags = flags
            if self._bid_flags & flags != self._bid_flags:
                expected_response = self.ERROR_BID_MISMATCH
                expected_brand = cr50_utils.ERASED_BID_INT
                expected_flags = cr50_utils.ERASED_BID_INT
                logging.info('%s phase mismatch with current image', phase)
            # Run the script to set the board id and flags for the given phase.
            self.run_script(expected_response, phase)

            # Check that the board id and flags are actually set.
            cr50_utils.CheckChipBoardId(self.host, expected_brand,
                                        expected_flags)
