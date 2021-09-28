# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


""" The autotest performing FW update, both EC and AP."""


import logging
import sys

from autotest_lib.client.common_lib import error
from autotest_lib.server import test


class provision_FirmwareUpdate(test.test):
    """A test that can provision a machine to the correct firmware version."""

    version = 1

    def stage_image_to_usb(self, host):
        """Stage the current ChromeOS image on the USB stick connected to the
        servo.

        @param host:  a CrosHost object of the machine to update.
        """
        info = host.host_info_store.get()
        if not info.build:
            logging.warning('Failed to get build label from the DUT, skip '
                            'staging ChromeOS image on the servo USB stick.')
        else:
            _, update_url = host.stage_image_for_servo(info.build)
            host.servo.image_to_servo_usb(update_url)
            logging.debug('ChromeOS image %s is staged on the USB stick.',
                          info.build)

    def run_once(self, host, value, rw_only=False, stage_image_to_usb=False,
                 fw_path=None):
        """The method called by the control file to start the test.

        @param host:  a CrosHost object of the machine to update.
        @param value: the provisioning value, which is the build version
                      to which we want to provision the machine,
                      e.g. 'link-firmware/R22-2695.1.144'.
        @param rw_only: True to only update the RW firmware.
        @param stage_image_to_usb: True to stage the current ChromeOS image on
                the USB stick connected to the servo. Default is False.
        @param fw_path: Path to local firmware image for installing without
                        devserver.

        @raise TestFail: if the firmware version remains unchanged.
        """

        try:
            host.repair_servo()

            # Stage the current CrOS image to servo USB stick.
            if stage_image_to_usb:
                self.stage_image_to_usb(host)

            host.firmware_install(build=value,
                                  rw_only=rw_only,
                                  dest=self.resultsdir,
                                  local_tarball=fw_path,
                                  verify_version=True,
                                  try_scp=True)
        except Exception as e:
            logging.error(e)
            raise error.TestFail, str(e), sys.exc_info()[2]
