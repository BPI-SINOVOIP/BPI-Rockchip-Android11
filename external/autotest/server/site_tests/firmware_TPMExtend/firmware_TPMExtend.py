# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib, logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class firmware_TPMExtend(FirmwareTest):
    """Test to ensure TPM PCRs are extended correctly."""
    version = 1

    def initialize(self, host, cmdline_args):
        super(firmware_TPMExtend, self).initialize(host, cmdline_args)
        self.switcher.setup_mode('normal')
        self.setup_usbkey(usbkey=True, host=False)

    def _tpm1_check_pcr(self, num, hash_obj):
        pcrs_file = '/sys/class/*/tpm0/device/pcrs'
        pcrs = '\n'.join(self.faft_client.system.run_shell_command_get_output(
                        'cat %s' % pcrs_file))
        logging.debug('Dumping PCRs read from device: \n%s', pcrs)
        extended = hashlib.sha1(b'\0' * 20 + hash_obj.digest()[:20]).hexdigest()
        spaced = ' '.join(extended[i:i+2] for i in xrange(0, len(extended), 2))
        logging.debug('PCR %d should contain hash: %s', num, spaced)
        return ('PCR-%.2d: %s' % (num, spaced.upper())) in pcrs

    def _tpm2_check_pcr(self, num, hash_obj):
        out = self.faft_client.system.run_shell_command_get_output(
                'trunks_client --read_pcr --index=%d' % num)[0]
        logging.debug('PCR %d read from device: %s', num, out)
        padded = (hash_obj.digest() + b'\0' * 12)[:32]
        extended = hashlib.sha256(b'\0' * 32 + padded).hexdigest().upper()
        logging.debug('PCR %d should contain hash: %s', num, extended)
        return extended in out

    def _check_pcr(self, num, hash_obj):
        """Returns true iff PCR |num| was extended with hashlib |hash_obj|."""
        if '1.' in self.faft_client.tpm.get_tpm_version():
            return self._tpm1_check_pcr(num, hash_obj)
        else:
            return self._tpm2_check_pcr(num, hash_obj)

    def run_once(self):
        """Runs a single iteration of the test."""
        logging.info('Verifying HWID digest in PCR1')
        hwid = self.faft_client.system.run_shell_command_get_output(
                'crossystem hwid')[0]
        logging.debug('HWID reported by device is: %s', hwid)
        if not self._check_pcr(1, hashlib.sha256(hwid)):
            raise error.TestFail('PCR1 was not extended with SHA256 of HWID!')

        logging.info('Verifying bootmode digest in PCR0 in normal mode')
        self.check_state((self.checkers.crossystem_checker, {
                            'devsw_boot': '0',
                            'mainfw_type': 'normal'
                            }))
        # dev_mode: 0, rec_mode: 0, keyblock_flags: "normal" (1)
        if not self._check_pcr(0, hashlib.sha1(chr(0) + chr(0) + chr(1))):
            raise error.TestFail('PCR0 was not extended with bootmode 0|0|1!')

        logging.info('Verifying bootmode digest in PCR0 in recovery mode')
        self.switcher.reboot_to_mode(to_mode='rec')
        self.check_state((self.checkers.crossystem_checker, {
                            'devsw_boot': '0',
                            'mainfw_type': 'recovery'
                            }))
        # dev_mode: 0, rec_mode: 1, keyblock_flags: "unknown" (0)
        if not self._check_pcr(0, hashlib.sha1(chr(0) + chr(1) + chr(0))):
            raise error.TestFail('PCR0 was not extended with bootmode 0|1|0!')

        logging.info('Transitioning to dev mode for next test')
        self.switcher.reboot_to_mode(to_mode='dev')

        logging.info('Verifying bootmode digest in PCR0 in developer mode')
        self.check_state((self.checkers.crossystem_checker, {
                            'devsw_boot': '1',
                            'mainfw_type': 'developer'
                            }))
        # dev_mode: 1, rec_mode: 0, keyblock_flags: "normal" (1)
        if not self._check_pcr(0, hashlib.sha1(chr(1) + chr(0) + chr(1))):
            raise error.TestFail('PCR0 was not extended with bootmode 1|0|1!')

        logging.info('Verifying bootmode digest in PCR0 in dev-recovery mode')
        self.switcher.reboot_to_mode(to_mode='rec')
        self.check_state((self.checkers.crossystem_checker, {
                            'devsw_boot': '1',
                            'mainfw_type': 'recovery'
                            }))
        # dev_mode: 1, rec_mode: 1, keyblock_flags: "unknown" (0)
        if not self._check_pcr(0, hashlib.sha1(chr(1) + chr(1) + chr(0))):
            raise error.TestFail('PCR0 was not extended with bootmode 1|1|0!')

        logging.info('All done, returning to normal mode')
        self.switcher.reboot_to_mode(to_mode='normal')
