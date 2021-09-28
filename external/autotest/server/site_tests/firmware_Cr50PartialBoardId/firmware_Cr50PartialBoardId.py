# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50PartialBoardId(Cr50Test):
    """Verify cr50 partial board id.


    Verify the board id flags can be set alone and the brand can be set later.
    """
    version = 1

    # Brand used for testing. It doesn't matter what this is.
    DEFAULT_BRAND = 'ZZAF'

    WHITELABEL_FLAGS = 0x3f80
    OTHER_FLAGS = 0x7f7f

    SUCCESS = ''
    ERR_ALREADY_SET = 'Error 7 while setting board id'
    ERR_BID_MISMATCH = 'Error 5 while setting board id'


    def initialize(self, host, cmdline_args, full_args, bid=''):
        """Generate the test flags and verify the device setup."""
        # Restore the original image, rlz code, and board id during cleanup.
        super(firmware_Cr50PartialBoardId, self).initialize(
            host, cmdline_args, full_args, restore_cr50_image=True,
            restore_cr50_board_id=True)
        if self.servo.main_device_is_ccd():
            raise error.TestNAError('Use a flex cable instead of CCD cable.')

        running_ver = self.get_saved_cr50_original_version()
        logging.info('Cr50 Version: %s', running_ver)
        bid = running_ver[2]
        brand = self.get_device_brand()
        self.test_brand = brand if brand else self.DEFAULT_BRAND
        logging.info('Test Brand: %r', self.test_brand)
        self.image_flags = int(bid.rsplit(':', 1)[-1], 16) if bid else 0
        # The image may have non-zero flags. Use test flags as close to the
        # whitelabel flags as possible, but make sure they can be used with
        # the running image.
        self.test_flags = self.WHITELABEL_FLAGS | self.image_flags
        self.other_flags = self.OTHER_FLAGS | self.image_flags


    def eraseflashinfo(self):
        """Eraseflashinfo if the board id is set."""
        if cr50_utils.GetChipBoardId(self.host) == cr50_utils.ERASED_CHIP_BID:
            return
        # Erase the board id so we can change it.
        self.eraseflashinfo_and_restore_image()


    def set_bid_with_dbg_image(self, bid):
        """Use the console command on the DBG image to set the board id."""
        self.eraseflashinfo_and_restore_image(self.get_saved_dbg_image_path())
        self.cr50.set_board_id(int(bid[0], 16), bid[2])
        self.cr50_update(self.get_saved_cr50_original_path(), rollback=True)


    @staticmethod
    def get_bid_str(bid):
        """Returns a string representation of the board id tuple."""
        bid_str_fields = []
        for field in bid:
            if isinstance(field, str):
                bid_str_fields.append(field)
            elif isinstance(field, int):
                bid_str_fields.append(hex(field))
            else:
                bid_str_fields.append(str(field))
        return ':'.join(bid_str_fields)


    def set_board_id_check_response(self, set_bid, expected_bid, expected_msg):
        """Try to set the board id and verify the response.

        @param set_bid: a tuple of the board id to set (brand, brand_inv,
                        flags). brand_inv is ignored, but it's included to be
                        consistent with expected_bid. The flags should be an
                        integer.
        @param expected_bid: a tuple of the board id cr50 should have (brand,
                             brand_inv, flags). brand_inv is ignored if it's
                             None. The flags should be an integer.
        @param expected_msg: The expected response from setting the board id
                             with gsctool.
        @raises TestFail if the cr50 board id doesn't match expected_bid or
                the gsctool doesn't contain expected_msg.
        """
        logging.info('Set BID %s.', self.get_bid_str(set_bid))
        logging.info('Expect BID %s (%s)', self.get_bid_str(expected_bid),
                     expected_msg)
        board_id_arg = '%s:0x%08x' % (set_bid[0], set_bid[2])

        result = cr50_utils.GSCTool(self.host, ['-a', '-i', board_id_arg],
                                    ignore_status=True)

        stderr = result.stderr.strip()
        result_msg = stderr if stderr else result.stdout.strip()
        logging.info('Response: BID %s %s', board_id_arg,
                     ('(%s)' % result_msg) if result_msg else '')
        if expected_msg and expected_msg not in result_msg:
            err = ('Unexpected response setting %r (%d): got %r expected %r' %
                   (board_id_arg, result.exit_status, result_msg,
                    expected_msg))
            raise error.TestFail(err)
        cr50_utils.CheckChipBoardId(self.host, expected_bid[0], expected_bid[2],
                                    board_id_inv=expected_bid[1])


    def run_once(self):
        """Verify partial board id"""
        self.eraseflashinfo()
        # Basic check. Setting board id fails if it's already been fully set.
        bid = (self.test_brand, None, self.test_flags)
        self.set_board_id_check_response(bid, bid, self.SUCCESS)
        self.set_board_id_check_response(bid, bid, self.ERR_ALREADY_SET)

        self.eraseflashinfo()
        # No special behavior for flags that are 0xffffffff. The flags cannot
        # be changed if the board id is set even if the flags are 0xffffffff.
        original_bid = (self.test_brand, None, cr50_utils.ERASED_BID_INT)
        second_bid = (self.test_brand, None, self.test_flags)
        self.set_board_id_check_response(original_bid, original_bid,
                                         self.SUCCESS)
        self.set_board_id_check_response(second_bid, original_bid,
                                         self.ERR_ALREADY_SET)

        self.eraseflashinfo()
        # Flags can be set if board_id_type and board_id_type_inv are 0xffffffff
        partial_bid = (cr50_utils.ERASED_BID_STR, None, self.test_flags)
        stored_partial_bid = (cr50_utils.ERASED_BID_STR,
                              cr50_utils.ERASED_BID_STR, self.test_flags)
        self.set_board_id_check_response(partial_bid, stored_partial_bid,
                                         self.SUCCESS)
        set_brand = (self.test_brand, None, self.other_flags)
        updated_brand_bid = (self.test_brand, None, self.test_flags)
        self.set_board_id_check_response(set_brand, updated_brand_bid,
                                         self.SUCCESS)

        # Setting the board id type to 0xffffffff on the console will set the
        # type to 0xffffffff and type_inv to 0. This isn't considered a partial
        # board id. Setting the board id a second time will fail.
        bid = (cr50_utils.ERASED_BID_STR, '00000000', self.test_flags)
        new_bid = (self.test_brand, None, self.other_flags)
        self.set_bid_with_dbg_image(bid)
        self.set_board_id_check_response(new_bid, bid, self.ERR_ALREADY_SET)
        if not self.image_flags:
            logging.info('Image is not board id locked. Done')
            return

        self.eraseflashinfo()
        # Plain whitelabel flags will run on any board id locked image.
        bid = (cr50_utils.ERASED_BID_STR, None, self.WHITELABEL_FLAGS)
        self.set_board_id_check_response(bid, cr50_utils.ERASED_CHIP_BID,
                                         self.ERR_BID_MISMATCH)
        # Previous board id was rejected. The board id can still be set.
        basic_bid = (self.test_brand, None, self.image_flags)
        self.set_board_id_check_response(basic_bid, basic_bid, self.SUCCESS)
