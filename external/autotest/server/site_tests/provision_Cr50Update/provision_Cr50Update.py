# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


""" The autotest performing Cr50 update."""


import logging
import os

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros import filesystem_util
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class provision_Cr50Update(Cr50Test):
    """A test that can provision a machine to the correct cr50 version and
    board id.

    The value is the image rw version/image board id. The chip board id will be
    set to the image board id unless the image board id is empty (''). If it's
    empty, the chip board id will be set to the device brand:0x7f80.

    value=0.0.23/ZZAF:ffffffff:7f00
    value=0.3.25/

    """
    version = 1

    def initialize(self, host, cmdline_args, full_args, value='',
                   release_path='', force=False):
        """Initialize get the cr50 update version information"""
        super(provision_Cr50Update, self).initialize(host, cmdline_args,
            full_args, provision_update=True)
        # TODO(mruthven): remove once the test is successfully scheduled.
        logging.info('SUCCESSFULLY SCHEDULED PROVISION CR50 UPDATE with %r',
                     value)
        if not force:
            return
        self.host = host

        image_info = None

        if os.path.isfile(release_path):
            image_info = self.init_local_image(release_path)
        else:
            # The value is rw_ver/image_bid. The image_bid will also be used for
            # the chip board id.
            rw_ver, bid = value.split('/')
            image_info = self.download_cr50_release_image(rw_ver, bid)

        if not image_info:
            raise error.TestError('Could not find new cr50 image')

        self.local_path, self.image_ver = image_info
        self.image_rw = self.image_ver[1]
        self.image_bid = self.image_ver[2]
        self.chip_bid = cr50_utils.GetChipBIDFromImageBID(
                self.image_bid, self.get_device_brand())


    def init_local_image(self, release_path):
        """Get the version of the local image.

        Args:
            release_path: The local path to the cr50 image

        Returns:
            the local path, image version tuple
        """
        ver = cr50_utils.InstallImage(self.host, release_path,
                                      '/tmp/release.bin')[1]
        return release_path, ver


    def run_once(self, force=False):
        """The method called by the control file to start the update."""
        # TODO(mruthven): remove once the test is successfully scheduled.
        if not force:
            logging.info('skipping update')
            return
        update_state = {}
        update_state['chip_bid'] = self.chip_bid
        update_state['prepvt_version'] = self.image_ver
        update_state['prod_version'] = self.image_ver
        update_state['running_image_bid'] = self.image_ver[2]
        # The test can't rollback RO. The newest RO should be running at the end
        # of the test. max_ro will be none if the versions are the same. Use the
        # running_ro in that case.
        running_ro = self.get_saved_cr50_original_version()[0]
        max_ro = cr50_utils.GetNewestVersion(running_ro, self.image_ver[0])
        update_state['running_image_ver'] = (max_ro or running_ro,
                                             self.image_ver[1],
                                             None)

        mismatch = self._check_running_image_and_board_id(update_state)
        if not mismatch:
            logging.info('No update needed.')
            return

        filesystem_util.make_rootfs_writable(self.host)
        if self._cleanup_required(mismatch, self.DBG_IMAGE):
            logging.info('Updating to image %s with chip board id %s',
                self.image_ver, '%08x:%08x:%08x' % self.chip_bid)
            self.update_cr50_image_and_board_id(self.local_path, self.chip_bid)

        # Copy the image onto the DUT. cr50-update uses both cr50.bin.prod and
        # cr50.bin.prepvt in /opt/google/cr50/firmware/, so copy it to both
        # places. Rootfs verification has to be disabled to do the copy.
        cr50_utils.InstallImage(self.host, self.local_path,
                                cr50_utils.CR50_PREPVT)
        cr50_utils.InstallImage(self.host, self.local_path,
                                cr50_utils.CR50_PROD)

        # Verify everything updated correctly
        mismatch = self._check_running_image_and_board_id(update_state)
        if mismatch:
            raise error.TestFail('Failed to update cr50: %s', mismatch)
