# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import autotest, test

@contextlib.contextmanager
def ensure_tpm_reset(client):
    """This context manager ensures we clears the TPM and restore the system to
    a reasonable state at the end of the test, even when an error occurs.
    """
    try:
        yield
    finally:
        tpm_utils.ClearTPMOwnerRequest(client)

class platform_CompromisedStatefulPartition(test.test):
    """Tests how the system recovers with the corrupted stateful partition.
    """
    version = 2

    CMD_CORRUPT = 'rm -fr /mnt/stateful_partition/*.*'
    OOBE_FILE = '/home/chronos/.oobe_completed'
    _WAIT_DELAY = 2
    FILES_LIST = [
        '/mnt/stateful_partition/dev_image',
        '/mnt/stateful_partition/encrypted.block',
        '/mnt/stateful_partition/unencrypted',
    ]
    ENCRYPTION_KEY_FILES = [
        '/mnt/stateful_partition/encrypted.key',
        '/mnt/stateful_partition/encrypted.needs-finalization',
    ]

    def _test_stateful_corruption(self, autotest_client, host, client_test):
        """Corrupts the stateful partition and reboots; checks if the client
        goes through OOBE again and if encstateful is recreated.
        """
        if not host.run(self.CMD_CORRUPT,
                        ignore_status=True).exit_status == 0:
            raise error.TestFail('Unable to corrupt stateful partition')
        host.run('sync', ignore_status=True)
        time.sleep(self._WAIT_DELAY)
        host.reboot()
        host.run('sync', ignore_status=True)
        time.sleep(self._WAIT_DELAY)
        if host.path_exists(self.OOBE_FILE):
            raise error.TestFail('Did not get OOBE screen after '
                                 'rebooting the device with '
                                 'corrupted statefull partition')
        autotest_client.run_test(client_test,
                                 exit_without_logout=True)
        time.sleep(self._WAIT_DELAY)
        for new_file in self.FILES_LIST:
            if not host.path_exists(new_file):
                raise error.TestFail('%s is missing after rebooting '
                                     'the device with corrupted '
                                     'stateful partition' % new_file)
        encryption_key_exists = False
        for encryption_key_file in self.ENCRYPTION_KEY_FILES:
            if host.path_exists(encryption_key_file):
                encryption_key_exists = True
        if encryption_key_exists is False:
            raise error.TestFail('An encryption key is missing after '
                                 'rebooting the device with corrupted stateful '
                                 'partition')

    def run_once(self, host, client_test):
        """This test verify that user should get OOBE after booting
        the device with corrupted stateful partition.
        Test fails if not able to recover the device with corrupted
        stateful partition.
        TPM is reset at the end to restore the system to a reasonable state.
        """
        if host.get_board_type() == 'OTHER':
            raise error.TestNAError('Test can not processed on OTHER board '
                                    'type devices')
        autotest_client = autotest.Autotest(host)
        host.reboot()
        autotest_client.run_test(client_test,
                                 exit_without_logout=True)
        with ensure_tpm_reset(host):
            self._test_stateful_corruption(autotest_client, host, client_test)
