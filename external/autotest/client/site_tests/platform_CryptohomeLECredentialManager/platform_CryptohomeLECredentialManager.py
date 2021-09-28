# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging
import time

from autotest_lib.client.bin import test, utils
from autotest_lib.client.cros import cryptohome
from autotest_lib.client.common_lib import error


class platform_CryptohomeLECredentialManager(test.test):
    """Tests the le_credential_manager functionality of cryptohome.
    """

    version = 1

    USER = 'testing@gmail.com'
    USER2 = 'testing2@gmail.com'
    KEY_LABEL = 'lecred0'
    KEY_LABEL2 = 'lecred2'
    GOOD_PIN = '123456'
    BAD_PIN = '000000'
    TEST_PASSWORD = '~'

    def get_known_le_credentials(self):
        """ Returns the set of LE credentials present on the device.
        """
        list_result = utils.run('ls /home/.shadow/low_entropy_creds')
        labels_str = list_result.stdout
        return set(labels_str.split())

    def run_once(self, pre_reboot=None):
        """Runs the platform_CryptohomeLECredentialManager test.
        """
        supported_policies = cryptohome.get_supported_key_policies()
        if (not supported_policies or
                not supported_policies.get('low_entropy_credentials', False)):
            raise error.TestNAError(
                'Low-entropy credentials are not supported.')

        if pre_reboot is None or pre_reboot == True:
            logging.info('Performing cleanup!')
            utils.run('stop cryptohomed')
            utils.run('rm -rf /home/.shadow/low_entropy_creds')
            try:
                cryptohome.remove_vault(self.USER)
                cryptohome.remove_vault(self.USER2)
            except cryptohome.ChromiumOSError:
                pass
            utils.run('start cryptohomed')

            logging.info('Waiting on cryptohomed to startup!')
            time.sleep(3)
            # Cleanup any existing mounts

            cryptohome.unmount_vault()

            logging.info('Setting up LE credential!')
            # The following operations shall all succeed:
            cryptohome.mount_vault(user=self.USER, password=self.TEST_PASSWORD,
                                   create=True, key_label='default')
            cryptohome.add_le_key(
                user=self.USER, password=self.TEST_PASSWORD,
                new_key_label=self.KEY_LABEL, new_password=self.GOOD_PIN)
            cryptohome.unmount_vault()

        logging.info('Testing authentication!')
        # The following operations shall all succeed:
        cryptohome.mount_vault(user=self.USER, password=self.GOOD_PIN,
                               key_label=self.KEY_LABEL)
        cryptohome.unmount_vault()

        logging.info('Testing lockout!')
        # The following operations fail, as they attempt to use the wrong PIN 5
        # times and then good PIN also stops working until reset:
        for i in range(5):
            try:
                cryptohome.mount_vault(user=self.USER, password=self.BAD_PIN,
                                       key_label=self.KEY_LABEL)
                raise cryptohome.ChromiumOSError(
                    'Mount succeeded where it should have failed (try %d)' % i)
            except cryptohome.ChromiumOSError:
                pass
        try:
            cryptohome.mount_vault(user=self.USER, password=self.GOOD_PIN,
                                   key_label=self.KEY_LABEL)
            raise cryptohome.ChromiumOSError(
                'Mount succeeded where it should have failed')
        except cryptohome.ChromiumOSError:
            pass

        logging.info('Testing reset!')
        # The following operations shall all succeed:
        cryptohome.mount_vault(user=self.USER, password=self.TEST_PASSWORD,
                               key_label='default')
        cryptohome.unmount_vault()
        cryptohome.mount_vault(user=self.USER, password=self.GOOD_PIN,
                               key_label=self.KEY_LABEL)
        cryptohome.unmount_vault()

        logging.info('Testing LE cred removal on user removal!')

        # Create a new user to test removal.
        cryptohome.mount_vault(user=self.USER2, password=self.TEST_PASSWORD,
                               create=True, key_label='default')
        lecreds_before_add = self.get_known_le_credentials()

        cryptohome.add_le_key(
            user=self.USER2, password=self.TEST_PASSWORD,
            new_key_label=self.KEY_LABEL, new_password=self.GOOD_PIN)
        cryptohome.add_le_key(
            user=self.USER2, password=self.TEST_PASSWORD,
            new_key_label=self.KEY_LABEL2, new_password=self.GOOD_PIN)
        cryptohome.unmount_vault()
        lecreds_after_add = self.get_known_le_credentials()

        cryptohome.remove_vault(self.USER2)
        lecreds_after_remove = self.get_known_le_credentials()

        if lecreds_after_add == lecreds_before_add:
            raise cryptohome.ChromiumOSError(
                'LE creds not added successfully')

        if lecreds_after_remove != lecreds_before_add:
            raise cryptohome.ChromiumOSError(
                'LE creds not deleted succesfully on user deletion!')

        if pre_reboot is None or pre_reboot == False:
            logging.info('Testing remove credential!')
            #The following operations shall all succeed:
            cryptohome.remove_key(user=self.USER, password=self.TEST_PASSWORD,
                                  remove_key_label=self.KEY_LABEL)
            logging.info('Cleanup of test user!')
            cryptohome.remove_vault(self.USER)

        logging.info('Tests passed!')
