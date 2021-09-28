# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.faft.firmware_test import ConnectionError


class firmware_UpdaterModes(FirmwareTest):
    """RO+RW firmware update using chromeos-firmwareupdate with various modes.

    This test uses --emulate, to avoid writing repeatedly to the flash.
    """

    version = 1

    SHELLBALL = '/usr/sbin/chromeos-firmwareupdate'

    def initialize(self, host, cmdline_args, ec_wp=None):
        """
        During initialization, back up the firmware, in case --emulate ever
        breaks in a way that causes real writes.
        """
        super(firmware_UpdaterModes, self).initialize(host, cmdline_args, ec_wp)
        self.backup_firmware()

    def cleanup(self):
        """Restore the original firmware, if it was somehow overwritten."""
        try:
            if self.is_firmware_saved():
                self.restore_firmware()
        except ConnectionError:
            logging.error("ERROR: DUT did not come up after firmware restore!")
        finally:
            super(firmware_UpdaterModes, self).cleanup()

    def get_bios_fwids(self, path=None):
        """Return the BIOS fwids for the given file"""
        return self.faft_client.updater.get_all_installed_fwids('bios', path)

    def run_case(self, mode, write_protected, written, modify_ro=True,
                 should_abort=False, writes_gbb=False):
        """Run chromeos-firmwareupdate with given sub-case

        @param mode: factory or recovery or autoupdate
        @param write_protected: is the flash write protected (--wp)?
        @param modify_ro: should ro fwid be modified?
        @param written: list of bios areas expected to change
        @param should_abort: if True, the updater should abort with no changes
        @param writes_gbb: if True, the updater should rewrite gbb flags.
        @return: a list of failure messages for the case
        """
        self.faft_client.updater.reset_shellball()

        fake_bios_path = self.faft_client.updater.copy_bios('fake-bios.bin')
        self.faft_client.updater.set_image_gbb_flags(0, fake_bios_path)

        before_fwids = {'bios': self.get_bios_fwids(fake_bios_path)}
        before_gbb = self.faft_client.updater.get_image_gbb_flags(
                fake_bios_path)

        case_desc = ('chromeos-firmwareupdate --mode=%s --wp=%s'
                     % (mode, write_protected))

        if modify_ro:
            append = 'ro+rw'
        else:
            case_desc += ' [rw-only]'
            append = 'rw'

        # Repack the shellball with modded fwids
        self.modify_shellball(append, modify_ro)
        modded_fwids = self.identify_shellball()
        image_gbb = self.faft_client.updater.get_image_gbb_flags()

        options = ['--emulate', fake_bios_path, '--wp=%s' % write_protected]

        logging.info("%s (should write %s)", case_desc,
                     ', '.join(written).upper() or 'nothing')
        rc = self.faft_client.updater.run_firmwareupdate(mode, append, options)

        if should_abort and rc != 0:
            logging.debug('updater aborted as expected')

        after_fwids = {'bios': self.get_bios_fwids(fake_bios_path)}
        after_gbb = self.faft_client.updater.get_image_gbb_flags(fake_bios_path)
        expected_written = {'bios': written or []}

        errors = self.check_fwids_written(
                before_fwids, modded_fwids, after_fwids, expected_written)

        if not errors:
            logging.debug('...bios versions correct: %s', after_fwids['bios'])

        if should_abort and rc == 0:
            msg = ("...updater: with current mode and write-protect value, "
                   "should abort (rc!=0) and not modify anything")
            errors.insert(0, msg)

        if writes_gbb:
            if after_gbb != image_gbb:
                # Expect rewritten, but it might not be different from before
                errors.append(
                        "...GBB flags weren't rewritten to match the image: "
                        "before=0x%x, image=0x%x, after=0x%x."
                        % (before_gbb, image_gbb, after_gbb))
        else:
            if after_gbb != before_gbb:
                errors.append(
                        "...GBB flags were unexpectedly rewritten: "
                        "before=0x%x, image=0x%x, after=0x%x."
                        % (before_gbb, image_gbb, after_gbb))

        if self.restore_firmware():
            # If real writes happen, fail immediately to avoid flash wear.
            raise error.TestFail(
                    'With chromeos-firmwareupdate --emulate, real flash device '
                    'was unexpectedly modified.')

        if errors:
            case_message = '%s:\n%s' % (case_desc, '\n'.join(errors))
            logging.error('%s', case_message)
            return [case_message]
        return []

    def run_once(self, host):
        """Run test, iterating through combinations of mode and write-protect"""
        errors = []

        # TODO(dgoyette): Add a test that checks EC versions (can't be emulated)

        # factory: update A, B, and RO; reset gbb flags.  If WP=1, abort.
        errors += self.run_case('factory', 0, ['ro', 'a', 'b'], writes_gbb=True)
        errors += self.run_case('factory', 1, [], should_abort=True)

        # recovery: update A and B, and RO if WP=0.
        errors += self.run_case('recovery', 0, ['ro', 'a', 'b'])
        errors += self.run_case('recovery', 1, ['a', 'b'])

        # autoupdate with changed ro: same as recovery (modify ro only if WP=0)
        errors += self.run_case('autoupdate', 0, ['ro', 'a', 'b'])
        errors += self.run_case('autoupdate', 1, ['b'])

        # autoupdate with unchanged ro: update inactive slot
        errors += self.run_case('autoupdate', 0, ['b'], modify_ro=False)
        errors += self.run_case('autoupdate', 1, ['b'], modify_ro=False)

        if len(errors) == 1:
            raise error.TestFail(errors[0])
        elif errors:
            raise error.TestFail(
                    '%s combinations of mode and write-protect failed:\n%s' %
                    (len(errors), '\n'.join(errors)))
