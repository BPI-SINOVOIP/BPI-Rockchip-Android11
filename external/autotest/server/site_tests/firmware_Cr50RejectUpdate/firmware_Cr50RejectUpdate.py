# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50RejectUpdate(Cr50Test):
    """Verify cr50 rejects certain updates."""
    version = 1
    OLD_IMAGE_VER = '0.0.18'
    # We dont care that it actually matches the device devid. It will be
    # rejected before the update. This is just one that I picked from google
    # storage
    IMAGE_DEVID = '0x1000d059 0x04657208'
    # No boards use the bid TEST. Use this image to verify cr50 rejects images
    # with the wrong board id.
    BID = 'TEST:0000ffff:0000ff00'
    TEST_PATH = '/tmp/test_image.bin'


    def initialize(self, host, cmdline_args, full_args):
        """Initialize servo and download images"""
        super(firmware_Cr50RejectUpdate, self).initialize(host, cmdline_args,
                full_args, restore_cr50_image=True)

        if not hasattr(self, 'cr50'):
            raise error.TestNAError('Test can only be run on devices with '
                                    'access to the Cr50 console')

        if 'DBG' in self.cr50.get_version():
            raise error.TestNAError('Update rules are wonky on DBG images')

        if cr50_utils.GetChipBoardId(host) == cr50_utils.ERASED_CHIP_BID:
            raise error.TestNAError('Set Cr50 board id to run test')

        self.bid_path = self.download_cr50_debug_image(self.IMAGE_DEVID,
                self.BID)[0]
        self.old_path = self.download_cr50_release_image(self.OLD_IMAGE_VER)[0]
        self.original_path = self.get_saved_cr50_original_path()
        self.host = host
        # Wait until cr50 can accept an update, so cr50 update rate limiting
        # won't interfere with the test.
        self.cr50.wait_until_update_is_allowed()


    def try_update(self, arg, path, err=0, stdout='', wait=True):
        """Run gsctool with the given image and args. Verify the result

        Args:
            arg: strings with the gsctool args
            path: local path to the test image
            err: The error number
            stdout: a string that must be included in the cmd stdout
            wait: Wait for cr50 to have been up for 60 seconds before attempting
                  the update.

        Raises:
            TestFail if there is an unexpected result.
        """
        # Copy the image to the DUT
        self.host.send_file(path, self.TEST_PATH)

        # Wait for cr50 to have been up for 60 seconds, so it won't
        # automatically reject the image.
        if wait:
            self.cr50.wait_until_update_is_allowed()

        # Try to update
        result = self.host.run('gsctool -a %s %s' % (arg, self.TEST_PATH),
                ignore_status=True, ignore_timeout=True, timeout=60)

        # Check the result
        stderr = 'Error %d' % err
        if err and stderr not in result.stderr:
            raise error.TestFail('"%s" not in "%s"' % (stderr, result.stderr))
        if stdout and stdout not in result.stdout:
            raise error.TestFail('"%s" not in "%s"' % (stdout, result.stdout))


    def run_once(self):
        """Verify cr50 rejects certain updates"""
        # Cr50 rejects a mismatched board id no matter what
        self.try_update('-u', self.bid_path, err=12)
        self.try_update('', self.bid_path, err=12)

        # With the '-u' option cr50 rejects any images with old/same header
        # with 'nothing to do'
        self.try_update('-u', self.old_path, stdout='nothing to do')
        self.try_update('-u', self.original_path, stdout='nothing to do')

        # Without '-u' cr50 rejects old headers
        self.try_update('', self.old_path, err=8)

        # Cr50 will accept images with the same header version if -u is omitted.
        # original_path is the image already on cr50, so this won't have any
        # real effect. It will just reboot Cr50.
        self.try_update('', self.original_path, stdout='image updated')
        if not self.faft_config.ap_up_after_cr50_reboot:
            self.servo.get_power_state_controller().reset()
        # After reboot, if the DUT hasn't responded within 45 seconds, it's not
        # going to.
        time.sleep(45)
        if not self.host.is_up_fast():
            raise error.TestError('DUT did not respond')

        # Wait for the host to respond to ping. Make sure it responded within
        # 60 seconds. The whole point of this case is that cr50 will reject
        # images in the first 60 seconds of boot, so it won't work if cr50
        # has been up for a while.
        if self.cr50.gettime() >= 60:
            raise error.TestError('Cannot complete test. Took >60 seconds for '
                                  'DUT to come back online')
        # After any reboot, cr50 will reject images for 60 seconds
        self.try_update('', self.original_path, err=9, wait=False)
