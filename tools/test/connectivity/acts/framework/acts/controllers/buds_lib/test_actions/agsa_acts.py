#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import datetime
import re

from acts.controllers.adb import AdbError
from acts.controllers.buds_lib.test_actions.base_test_actions import BaseTestAction
from acts.controllers.buds_lib.test_actions.base_test_actions import timed_action

PHONE_DFU_PATH = ('/storage/emulated/0/Android/data/com.google.android'
                  '.googlequicksearchbox/files/download_cache/apollo.dfu')

AGSA_BROADCAST = (
    'am  broadcast -a \'action_ota\' --es dfu_url %s --es build_label 9.9.9 '
    '--ez is_force %s com.google.android.googlequicksearchbox/com.google'
    '.android.apps.gsa.broadcastreceiver.CommonBroadcastReceiver')


class AgsaOTAError(Exception):
    """OTA Error"""


class AgsaTestActions(BaseTestAction):
    """AGSA test action library."""

    def __init__(self, android_dev, logger=None):
        """
        Simple init code to keep the android object for future reference.
        Args:
           android_dev: devcontrollers.android_device.AndroidDevice
        """
        super(AgsaTestActions, self).__init__(logger)
        self.dut = android_dev

    @timed_action
    def _initiate_agsa_ota(self, file_path, destination=None, force=True):
        """Pushes the dfu file to phone and issues broadcast to start AGSA OTA

        Args:
            file_path: (string) path of dfu file
            destination: (string) destination path on the phone uses
                         $PHONE_DFU_PATH if not specified
            force: (bool) option to force the issued broadcast?
        """
        if not destination:
            destination = PHONE_DFU_PATH
        if self.dut.push_file_to_phone(file_path, destination):
            if force:
                force = 'true'
            else:
                force = 'false'

            command = AGSA_BROADCAST % (destination, force)
            output = self.dut.adb.shell(command.split())
            if 'result=0' in output:
                self.logger.info('Agsa broadcast successful!')
                return True
            else:
                self.logger.error('Agsa broadcast failed')
                return False

    @timed_action
    def _wait_for_ota_to_finish(self, timeout=660):
        """Logcat is continuously read to keep track of the OTA transfer

        Args:
           timeout: (int) time to wait before timing out.

        Returns:
            True on success

        Raises: AgsaOTAError if the timeout is reached.
        """
        # regex that confirms completion
        transfer_completion_match = \
            re.compile('OTA progress: 100 %|OTA img rcvd')
        # time now + $timeout
        expiry_time = datetime.datetime.now() + \
                      datetime.timedelta(seconds=timeout)
        self.logger.info('Waiting for OTA transfer to complete....')
        while True:
            # time now - 1 minute: to be used in getting logs from a minute back
            now_plus_minute = datetime.datetime.now() - \
                              datetime.timedelta(seconds=5)
            try:
                # grep logcat for 'DeviceLog'
                filtered_log = self.dut.logcat_filter_message(
                    now_plus_minute.strftime('%m-%d %H:%M:%S.000'),
                    'Devicelog:')
                if filtered_log and \
                        transfer_completion_match.search(filtered_log):
                    self.logger.info('Transfer completed!')
                    break
            except AdbError:
                # gets thrown if no matching string is found
                pass
            if datetime.datetime.now() > expiry_time:
                self.logger.error('Timed out waiting for OTA to complete.')
                raise AgsaOTAError('Timed out waiting for OTA to complete.')
        return True

    @timed_action
    def initiate_agsa_and_wait_until_transfer(self, file_path, destination=None,
                                              force=True, timeout=660):
        """Calls _initiate_agsa_ota and _wait_for_ota_to_finish

        Returns:
            True on success and False otherwise
        """
        self._initiate_agsa_ota(file_path, destination, force)
        return self._wait_for_ota_to_finish(timeout)

    @timed_action
    def install_agsa(self, version, force=False):
        """
        Installs the specified version of AGSA if different from the one
        currently installed, unless force is set to True.

        Args:
            version: (string) ex: '7.14.21.release'
            force: (bool) installs only if currently installed version is
                   different than the one to be installed. True installs
                   by-passing version check
        Return:
            True on Success and False otherwise
        """
        # get currently installed version, and install agsa only if different
        # from what is requested
        current_version = self.dut.get_agsa_version()
        if (not (version.replace('alpha', '').replace('release', '')
                 in current_version)) or force:
            self.logger.info('Current AGSA version is %s' % current_version)
            self.logger.info('Installing AGSA version %s...' % version)
            if self.and_actions.install_agsa(version):
                self.logger.info('Install success!')
                return True
            else:
                self.logger.error('Failed to install version %s' % version)
                return False
