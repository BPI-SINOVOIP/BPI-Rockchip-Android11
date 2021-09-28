# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from hashlib import sha256
import logging
from pprint import pformat

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import pinweaver_client
from autotest_lib.server import test


def compute_empty_tree_auxilary_hashes(bits_per_level=2, height=6):
    """Returns a binary string representation of the auxilary digests of an
    empty path in a Merkle tree with the specified parameters.
    """
    num_siblings = 2 ^ bits_per_level - 1
    child = '\0' * 32
    result = ''
    for _ in range(height):
        part = child * num_siblings
        child = sha256(part + child).digest()
        result += part
    return result

class firmware_Cr50PinWeaverServer(test.test):
    """Tests the PinWeaver functionality on Cr50 using pinweaver_client through
    trunksd.
    """

    version = 1

    RESULT_CODE_SUCCESS = 'EC_SUCCESS'
    RESULT_CODE_AUTH_FAILED = 'PW_ERR_LOWENT_AUTH_FAILED'
    RESULT_CODE_RATE_LIMITED = 'PW_ERR_RATE_LIMIT_REACHED'

    def run_once(self, host):
        """Runs the firmware_Cr50PinWeaverServer test.
        This test is made up of the pinweaver_client self test, and a test that
        checks that PinWeaver works as expected across device reboots.
        """

        # Run "pinweaver_client selftest".
        try:
            if not pinweaver_client.SelfTest(host):
                raise error.TestFail('Failed SelfTest: %s' %
                                     self.__class__.__name__)
        except pinweaver_client.PinWeaverNotAvailableError:
            logging.info('PinWeaver not supported!')
            raise error.TestNAError('PinWeaver is not available')

        # Check PinWeaver logic across reboots including the reboot counter.
        # Insert an entry.
        #
        # Label 0 is guaranteed to be empty because the self test above resets
        # the tree and removes the leaf it adds.
        label = 0
        h_aux = compute_empty_tree_auxilary_hashes().encode('hex')
        le_secret = sha256('1234').hexdigest()
        he_secret = sha256('ag3#l4Z9').hexdigest()
        reset_secret = sha256('W8oE@Ja2mq.R1').hexdigest()
        delay_schedule = '5 %d' % 0x00ffffffff
        result = pinweaver_client.InsertLeaf(host, label, h_aux, le_secret,
                                             he_secret, reset_secret,
                                             delay_schedule)
        logging.info('Insert: %s', pformat(result))
        if (result['result_code']['name'] !=
            firmware_Cr50PinWeaverServer.RESULT_CODE_SUCCESS):
            raise error.TestFail('Failed InsertLeaf: %s' %
                                 self.__class__.__name__)
        cred_metadata = result['cred_metadata']

        # Exhaust the allowed number of attempts.
        for i in range(6):
            result = pinweaver_client.TryAuth(host, h_aux, '0' * 64,
                                              cred_metadata)
            if result['cred_metadata']:
                cred_metadata = result['cred_metadata']
            logging.info('TryAuth: %s', pformat(result))
            if ((i <= 4 and result['result_code']['name'] !=
                 firmware_Cr50PinWeaverServer.RESULT_CODE_AUTH_FAILED) or
                (i > 4 and result['result_code']['name'] !=
                 firmware_Cr50PinWeaverServer.RESULT_CODE_RATE_LIMITED)):
                raise error.TestFail('Failed TryAuth: %s' %
                                     self.__class__.__name__)

        if result['seconds_to_wait'] == 0:
            raise error.TestFail('Failed TryAuth: %s' %
                                 self.__class__.__name__)

        # Reboot the device. This calls TPM_startup() which reloads the Merkle
        # tree from NVRAM. Note that this doesn't reset the timer on Cr50, so
        # restart_count doesn't increment.
        host.reboot()

        # Verify that the lockout is still enforced.
        result = pinweaver_client.TryAuth(host, h_aux, le_secret, cred_metadata)
        logging.info('TryAuth: %s', pformat(result))
        if (result['result_code']['name'] !=
            firmware_Cr50PinWeaverServer.RESULT_CODE_RATE_LIMITED):
            raise error.TestFail('Failed TryAuth: %s' %
                                 self.__class__.__name__)
        if result['seconds_to_wait'] == 0:
            raise error.TestFail('Failed TryAuth: %s' %
                                 self.__class__.__name__)

        # Perform a reset.
        result = pinweaver_client.ResetAuth(host, h_aux, reset_secret,
                                            cred_metadata)
        if (result['result_code']['name'] !=
            firmware_Cr50PinWeaverServer.RESULT_CODE_SUCCESS):
            raise error.TestFail('Failed ResetAuth: %s' %
                                 self.__class__.__name__)
        cred_metadata = result['cred_metadata']
        logging.info('ResetAuth: %s', pformat(result))

        # Verify that using a PIN would work.
        result = pinweaver_client.TryAuth(host, h_aux, le_secret, cred_metadata)
        mac = result['mac']
        logging.info('TryAuth: %s', pformat(result))
        if (result['result_code']['name'] !=
            firmware_Cr50PinWeaverServer.RESULT_CODE_SUCCESS):
            raise error.TestFail('Failed TryAuth: %s' %
                                 self.__class__.__name__)

        # Remove the leaf.
        result = pinweaver_client.RemoveLeaf(host, label, h_aux, mac)
        logging.info('RemoveLeaf: %s', pformat(result))
        if (result['result_code']['name'] !=
            firmware_Cr50PinWeaverServer.RESULT_CODE_SUCCESS):
            raise error.TestFail('Failed RemoveLeaf: %s' %
                                 self.__class__.__name__)
