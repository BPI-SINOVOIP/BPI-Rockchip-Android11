# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.server import test

class platform_InstallTestImage(test.test):
    """Installs a specified test image onto a servo-connected DUT."""
    version = 1

    def initialize(self, host, image_url=None, local=False):
        """ Setup image url to install the image..

        @param host: Host object representing DUT to be re-imaged.
        @param image_url: URL of a test image to be installed.
        @param local: bool indicating it's a local run with an image already
                      on the usb stick of the servo
        """
        self.local = local
        # A 'None' value here indicates to servo below to use the image
        # on the stick.
        self.image_url = None
        if not self.local:
            self.image_url = image_url
            if self.image_url is None:
                image_name = host.get_cros_repair_image_name()
                # Succeeded, so stage the build and get its URL.
                # N.B. Failures from staging the build at this point
                # are fatal by design.
                _, self.image_url = host.stage_image_for_servo(image_name)
                logging.info("Using staged image:  %s", image_url)


    def run_once(self, host):
        """Install image from URL |self.image_url| on |host|.

        @param host Host object representing DUT to be re-imaged.
        """
        host.servo_install(image_url=self.image_url)
