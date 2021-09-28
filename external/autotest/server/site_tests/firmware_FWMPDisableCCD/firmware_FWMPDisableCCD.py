# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server import autotest
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_FWMPDisableCCD(Cr50Test):
    """A test that uses cryptohome to set the FWMP flags and verifies that
    cr50 disables/enables console unlock."""
    version = 1

    FWMP_DEV_DISABLE_CCD_UNLOCK = (1 << 6)
    GSCTOOL_ERR = 'Error: rv 7, response 7'
    PASSWORD = 'Password'

    def initialize(self, host, cmdline_args, full_args):
        """Initialize servo check if cr50 exists"""
        super(firmware_FWMPDisableCCD, self).initialize(host, cmdline_args,
                full_args)
        self.fast_open(enable_testlab=True)


    def try_set_ccd_level(self, level, fwmp_disabled_ccd):
        """Try setting the ccd level with a password.

        The normal Cr50 ccd path requires entering dev mode. Entering dev mode
        may be disabled by the FWMP flags. Entering dev mode also erases the
        FWMP. The test uses a password to get around the dev mode requirement.
        Send CCD commands from the commandline with the password. Check the
        output to make sure if it fails, it failed because of the FWMP.

        @param level: the desired ccd level: 'open' or 'unlock'.
        @param fwmp_disabled_ccd: True if 'ccd set $LEVEL' should fail
        """
        # Make sure the console is locked
        self.cr50.set_ccd_level('lock')
        try:
            self.cr50.set_ccd_level(level, self.PASSWORD)
            if fwmp_disabled_ccd:
                raise error.TestFail('FWMP failed to prevent %r' % level)
        except error.TestFail, e:
            logging.info(e)
            if fwmp_disabled_ccd:
                if ("FWMP disabled 'ccd open'" in str(e) or
                    'Console unlock not allowed' in str(e)):
                    logging.info('FWMP successfully disabled ccd %s', level)
                    return
                else:
                    raise error.TestFail('FWMP disabled %s in unexpected '
                                         'manner %r' % (level, str(e)))
            raise


    def open_cr50_and_setup_ccd(self):
        """Configure cr50 ccd for the test.

        Open Cr50. Reset ccd, so the capabilities are reset and the password is
        cleared. Set OpenNoTPMWipe to Always, so the FWMP won't be cleared
        during open.
        """
        # Clear the password and relock the console
        self.cr50.send_command('ccd testlab open')
        self.cr50.send_command('ccd reset')
        # Set this so when we run the open test, it won't clear the FWMP
        self.cr50.set_cap('OpenNoTPMWipe', 'Always')


    def cr50_check_lock_control(self, flags):
        """Verify cr50 lock enable/disable works as intended based on flags.

        If flags & self.FWMP_DEV_DISABLE_CCD_UNLOCK is true, lock disable should
        fail.

        This will only run during a test with access to the cr50  console

        @param flags: A string with the FWMP settings.
        """
        fwmp_disabled_ccd = not not (self.FWMP_DEV_DISABLE_CCD_UNLOCK &
                               int(flags, 16))

        start_state = self.cr50.get_ccd_info()['TPM']
        if ('fwmp_lock' in start_state) != fwmp_disabled_ccd:
            raise error.TestFail('Unexpected fwmp state with flags %s' % flags)

        logging.info('Flags are set to %s ccd is%s permitted', flags,
                     ' not' if fwmp_disabled_ccd else '')
        if not self.faft_config.has_powerbutton:
            logging.info('Can not test ccd without power button')
            return

        self.open_cr50_and_setup_ccd()
        # Try setting password after FWMP has been created. Setting password is
        # always allowed. Open and unlock should still be blocked. Opening cr50
        # requires the device is in dev mode unless there's a password set. FWMP
        # flags may disable dev mode. Set a password so we can get around this.
        self.set_ccd_password(self.PASSWORD)

        # run ccd commands with the password. ccd open and unlock should fail
        # when the FWMP has disabled ccd.
        self.try_set_ccd_level('open', fwmp_disabled_ccd)
        self.try_set_ccd_level('unlock', fwmp_disabled_ccd)

        # Clear the password.
        self.open_cr50_and_setup_ccd()
        self.cr50.send_command('ccd lock')


    def check_fwmp(self, flags, clear_fwmp, check_lock=True):
        """Set the flags and verify ccd lock/unlock

        Args:
            flags: A string to used set the FWMP flags. If None, skip running
                   firmware_SetFWMP.
            clear_fwmp: True if the flags should be reset.
            check_lock: Check ccd open
        """
        if clear_fwmp:
            self.clear_fwmp()

        logging.info('setting flags to %s', flags)
        if flags:
            autotest.Autotest(self.host).run_test('firmware_SetFWMP',
                    flags=flags, fwmp_cleared=clear_fwmp,
                    check_client_result=True)

        # Verify ccd lock/unlock with the current flags works as intended.
        if check_lock:
            self.cr50_check_lock_control(flags if flags else '0')


    def run_once(self):
        """Verify FWMP disable with different flag values"""
        # Skip checking ccd open, so the DUT doesn't reboot
        self.check_fwmp('0xaa00', True, check_lock=False)
        # Verify that the flags can be changed on the same boot
        self.check_fwmp('0xbb00', False)

        # Verify setting FWMP_DEV_DISABLE_CCD_UNLOCK disables ccd
        self.check_fwmp(hex(self.FWMP_DEV_DISABLE_CCD_UNLOCK), True)

        # 0x41 is the flag setting when dev boot is disabled. Make sure that
        # nothing unexpected happens.
        self.check_fwmp('0x41', True)

        # Clear the TPM owner and verify lock can still be enabled/disabled when
        # the FWMP has not been created
        self.check_fwmp(None, True)
