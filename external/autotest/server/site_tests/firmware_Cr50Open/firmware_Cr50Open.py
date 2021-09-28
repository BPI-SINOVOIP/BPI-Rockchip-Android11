# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50Open(Cr50Test):
    """Verify cr50 open."""
    version = 1

    def initialize(self, host, cmdline_args, ccd_open_restricted, full_args):
        """Initialize the test"""
        super(firmware_Cr50Open, self).initialize(host, cmdline_args,
                full_args)

        if not self.faft_config.has_powerbutton:
            raise error.TestNAError('No power button. Unable to test ccd open')

        self.ccd_open_restricted = ccd_open_restricted
        self.fast_open(enable_testlab=True)
        self.cr50.send_command('ccd reset')
        self.cr50.set_ccd_level('lock')


    def check_cr50_open(self, dev_mode, batt_pres):
        """Verify you can't open ccd unless dev mode is enabled.

        Make sure the ability to open ccd corresponds with the device being in
        dev mode. When the device is in dev mode, open should be accessible from
        the AP. When the device is in normal mode it shouldn't be accessible.
        Open will never work from the console.

        Args:
            dev_mode: bool reflecting whether the device is in dev mode. If
                    True, the device is in dev mode. If False, the device is in
                    normal mode.
            batt_pres: True if the battery is connected
        """
        self.cr50.set_ccd_level('lock')
        self.cr50.get_ccd_info()

        #Make sure open doesn't work from the console.
        try:
            self.cr50.set_ccd_level('open')
        except error.TestFail, e:
            if not batt_pres:
                raise error.TestFail('Unable to open cr50 from console with '
                                     'batt disconnected: %s' % str(e))
            # If ccd open is limited, open should fail with access denied
            #
            # TODO: move logic to set_ccd_level.
            if 'Access Denied' in str(e) and self.ccd_open_restricted:
                logging.info('console ccd open successfully rejected')
            else:
                raise
        else:
            if self.ccd_open_restricted and batt_pres:
                raise error.TestFail('Open should not be accessible from the '
                                     'console')
        self.cr50.set_ccd_level('lock')

        #Make sure open only works from the AP when the device is in dev mode.
        try:
            self.ccd_open_from_ap()
        except error.TestFail, e:
            logging.info(e)
            if not batt_pres:
                raise error.TestFail('Unable to open cr50 from AP with batt '
                                     'disconnected')
            # ccd open should work if the device is in dev mode or ccd open
            # isn't restricted. If open failed for some reason raise the error.
            if dev_mode or not self.ccd_open_restricted:
                raise


    def run_once(self):
        """Check open only works when the device is in dev mode."""
        self.cr50.send_command('ccd testlab open')
        self.cr50.set_batt_pres_state('connected', True)
        self.switcher.reboot_to_mode(to_mode='dev')
        self.check_cr50_open(True, True)
        self.switcher.reboot_to_mode(to_mode='normal')
        self.check_cr50_open(False, True)

        self.cr50.send_command('ccd testlab open')
        self.cr50.set_batt_pres_state('disconnected', True)
        self.check_cr50_open(False, False)
