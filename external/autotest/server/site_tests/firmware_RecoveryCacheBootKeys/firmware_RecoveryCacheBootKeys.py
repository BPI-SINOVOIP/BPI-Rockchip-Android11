# Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_RecoveryCacheBootKeys(FirmwareTest):
    """
    This test ensures that when booting to recovery mode the device will use the
    cache instead training memory every boot.
    """
    version = 1

    USED_CACHE_MSG = ('MRC: Hash comparison successful. '
                      'Using data from RECOVERY_MRC_CACHE')
    REBUILD_CACHE_MSG = "MRC: cache data 'RECOVERY_MRC_CACHE' needs update."
    RECOVERY_CACHE_SECTION = 'RECOVERY_MRC_CACHE'
    FIRMWARE_LOG_CMD = 'cbmem -1' + ' | grep ' + REBUILD_CACHE_MSG[:3]
    FMAP_CMD = 'mosys eeprom map'
    RECOVERY_REASON_REBUILD_CMD = 'crossystem recovery_request=0xC4'

    def initialize(self, host, cmdline_args, dev_mode=False):
        super(firmware_RecoveryCacheBootKeys, self).initialize(
                host, cmdline_args)

        if not self.faft_config.rec_force_mrc:
            raise error.TestNAError('DUT cannot force memory training.')

        self.client = host
        self.dev_mode = dev_mode
        self.switcher.setup_mode('dev' if dev_mode else 'normal')
        self.setup_usbkey(usbkey=True, host=False)

    def cleanup(self):
        super(firmware_RecoveryCacheBootKeys, self).cleanup()
        self.switcher.simple_reboot()

    def boot_to_recovery(self, rebuild_mrc_cache=False):
        """Boots the device into recovery mode."""
        if rebuild_mrc_cache:
            self.switcher.reboot_to_mode(to_mode='rec_force_mrc')
        else:
            self.switcher.reboot_to_mode(to_mode='rec')

        self.check_state((self.checkers.crossystem_checker, {
                'mainfw_type': 'recovery'
        }))

    def cache_exist(self):
        """Checks the firmware log to ensure that the recovery cache exists.

        @return True if cache exists
        """
        logging.info("Checking if device has RECOVERY_MRC_CACHE")

        return self.faft_client.system.run_shell_command_check_output(
                self.FMAP_CMD, self.RECOVERY_CACHE_SECTION)

    def check_cache_used(self):
        """Checks the firmware log to ensure that the recovery cache was used
        during recovery boot.

        @return True if cache used
        """
        logging.info('Checking if cache was used.')

        return self.faft_client.system.run_shell_command_check_output(
                self.FIRMWARE_LOG_CMD, self.USED_CACHE_MSG)

    def check_cache_rebuilt(self):
        """Checks the firmware log to ensure that the recovery cache was rebuilt
        during recovery boot.

        @return True if cache rebuilt
        """
        logging.info('Checking if cache was rebuilt.')

        return self.faft_client.system.run_shell_command_check_output(
                self.FIRMWARE_LOG_CMD, self.REBUILD_CACHE_MSG)

    def run_once(self):
        """Runs a single iteration of the test."""
        if not self.cache_exist():
            raise error.TestNAError('No RECOVERY_MRC_CACHE was found on DUT.')

        logging.info('Ensure we\'ve done memory training.')
        self.boot_to_recovery()

        # With EFS, when the EC is in RO and we do a recovery boot, it
        # causes the EC to do a soft reboot, which will in turn cause
        # a PMIC reset (double reboot) on SKL/KBL power architectures.
        # This causes us to lose the request to do MRC training for
        # this test.  The solution is to make sure that the EC is in
        # RW before doing a recovery boot to ensure that the double
        # reboot does not occur and information/requests are not lost.
        self.switcher.simple_reboot('cold')

        logging.info('Checking recovery boot.')
        self.boot_to_recovery()

        if not self.check_cache_used():
            raise error.TestFail('Recovery Cache was not used.')

        self.switcher.simple_reboot('cold')

        logging.info('Checking recovery boot with forced MRC cache training.')
        self.boot_to_recovery(rebuild_mrc_cache=True)
        self.switcher.wait_for_client()

        if not self.check_cache_rebuilt():
            raise error.TestFail('Recovery Cache was not rebuilt.')

        logging.info('Reboot out of Recovery')
        self.switcher.simple_reboot()
