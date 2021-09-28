# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50WPG3(Cr50Test):
    """Verify WP in G3."""
    version = 1

    WAIT_FOR_STATE = 10
    WP_REGEX = r'write protect is ((en|dis)abled)\.'
    FLASHROM_WP_CMD = ('sudo flashrom -p raiden_debug_spi:target=AP,serial=%s '
                       '--wp-status')

    def cleanup(self):
        """Reenable servo wp."""
        self.cr50.send_command('rddkeepalive disable')
        self.cr50.send_command('ccdblock IGNORE_SERVO disable')
        self.servo.enable_main_servo_device()
        try:
            if hasattr(self, '_start_fw_wp_vref'):
                self.servo.set_nocheck('fw_wp_state', self._start_fw_wp_state)
                self.servo.set_nocheck('fw_wp_vref', self._start_fw_wp_vref)
        finally:
            super(firmware_Cr50WPG3, self).cleanup()


    def generate_flashrom_wp_cmd(self):
        """Use the cr50 serialname to generate the flashrom command."""
        devid = self.servo.get('cr50_devid')
        serial = devid.replace('0x', '').replace(' ', '-').upper()
        logging.info('CCD serial: %s', serial)
        self._flashrom_wp_cmd = self.FLASHROM_WP_CMD % serial


    def get_wp_state(self):
        """Returns 'on' if write protect is enabled. 'off' if it's disabled."""
        flashrom_output = self.servo.system_output(self._flashrom_wp_cmd)
        match = re.search(self.WP_REGEX, flashrom_output)
        logging.info('WP is %s', match.group(1) if match else 'UKNOWN')
        logging.debug('flashrom output\n%s', flashrom_output)
        if not match:
            raise error.TestError('Unable to find WP status in flashrom output')
        return match.group(1)


    def run_once(self):
        """Verify WP in G3."""
        if self.check_cr50_capability(['wp_on_in_g3'], suppress_warning=True):
            raise error.TestNAError('WP not pulled up in G3')
        if not self.servo.dts_mode_is_valid():
            raise error.TestNAError('Type-C servo v4 required to check wp.')
        try:
            self.cr50.ccd_enable(True)
        except:
            raise error.TestNAError('CCD required to check wp.')
        self.generate_flashrom_wp_cmd()

        self.fast_open(True)
        # faft-cr50 runs with servo micro and type c servo v4. Use ccdblock to
        # get cr50 to ignore the fact servo is connected and allow the test to
        # use ccd to check wp status.
        self.cr50.send_command('ccdblock IGNORE_SERVO enable')
        self.cr50.send_command('rddkeepalive enable')
        self.cr50.get_ccdstate()

        if self.servo.main_device_is_flex():
            self._start_fw_wp_state = self.servo.get('fw_wp_state')
            self._start_fw_wp_vref = self.servo.get('fw_wp_vref')
            # Stop forcing wp using servo, so we can set it with ccd.
            self.servo.set_nocheck('fw_wp_state', 'reset')
            self.servo.set_nocheck('fw_wp_vref', 'off')

        # disable write protect
        self.cr50.send_command('wp disable atboot')

        # Verify we can see it's disabled. This should always work. If it
        # doesn't, it may be a setup issue.
        servo_wp_s0 = self.get_wp_state()
        if servo_wp_s0 == 'enabled':
            raise error.TestError("WP isn't disabled in S0")

        self.faft_client.system.run_shell_command('poweroff')
        time.sleep(self.WAIT_FOR_STATE)
        if hasattr(self, 'ec'):
            self.ec.send_command('hibernate')
            time.sleep(self.WAIT_FOR_STATE)

        servo_wp_g3 = self.get_wp_state()
        self._try_to_bring_dut_up()
        # Some boards don't power the EC_WP signal in G3, so it can't be
        # disabled by cr50 in G3.
        if servo_wp_g3 == 'enabled':
            raise error.TestFail("WP can't be disabled by Cr50 in G3")
