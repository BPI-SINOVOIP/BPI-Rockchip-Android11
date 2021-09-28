# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Auto test for Bizlink firmware updater functionality and udev rule."""

from __future__ import print_function
import logging
import os
import re
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test

UPDATER_WAIT_TIME = 180     # seconds
CMD_CHECK_FW_UPDATE_OUTPUT = 'grep bizlink-updater /var/log/messages'
FW_PATH = '/lib/firmware/bizlink/'
OLD_FW_NAME = 'megachips-firmware-old.bin'
NEW_FW_NAME = 'megachips-firmware.bin'


class enterprise_CFM_BizlinkUpdater(test.test):
    """
    Bizlink dongle firmware updater functionality test in Chrome Box.

    The procedure of the test is:
    1. flash old version FW to device,
    2. Reboot the device, which should be able to trigger udev rule and run the
         updater,
    3. wait for the updater to finish,
    4. run fw updater again and verify that the FW in device is consistent with
         latest FW within system by checking the output.
    """

    version = 1


    def initialize(self, host):
        self.host = host
        self.old_fw_path = os.path.join(FW_PATH, OLD_FW_NAME)
        self.new_fw_path = os.path.join(FW_PATH, NEW_FW_NAME)

    def cleanup(self):
        cmd = 'rm -f {}'.format(self.old_fw_path)
        self.host.run(cmd)
        super(enterprise_CFM_BizlinkUpdater, self).cleanup()

    def check_update_result(self, expected_output=''):
        """
        Checks FW update result.

        Queries the syslog and checks if expected_output occurs in it.

        @param expected_output: the string to query syslog for.

        @returns True if expected_output is in syslog. Otherwise return false.
        """
        result = self.host.run(CMD_CHECK_FW_UPDATE_OUTPUT)
        # Only check last 5 logs for the most recent run.
        messages = result.stdout.strip().split('\n')
        if len(messages) < 5:
            return False
        messages = ''.join(messages[-5:])
        if expected_output in messages:
            return True
        else:
            return False

    def convert_rootfs_writable(self):
        """
        Removes rootfs verification on DUT and reboots.
        """
        logging.info('Disabling rootfs verification...')
        self.remove_rootfs_verification()

        logging.info('Rebooting...')
        self.host.reboot()

    def remove_rootfs_verification(self):
        """Removes rootfs verification."""
        # 2 & 4 are default partitions, and the system boots from one of them.
        # Code from chromite/scripts/deploy_chrome.py
        KERNEL_A_PARTITION = 2
        KERNEL_B_PARTITION = 4

        cmd_template = ('/usr/share/vboot/bin/make_dev_ssd.sh --partitions %d '
                        '--remove_rootfs_verification --force')
        for partition in (KERNEL_A_PARTITION, KERNEL_B_PARTITION):
            cmd = cmd_template % partition
            self.host.run(cmd)

    def is_filesystem_readwrite(self):
        """Checks if the root file system is read-writable.

        Queries the DUT's filesystem /dev/root, checks for keyword 'rw'.

        @returns True if /dev/root is read-writable. False otherwise.
        """
        cmd = 'cat /proc/mounts | grep "/dev/root"'
        result = self.host.run(cmd)
        if result.stderr:
            output = result.stderr
        else:
            output = result.stdout
        fields = re.split(' |,', output)
        return True if fields.__len__() >= 4 and fields[3] == 'rw' else False

    def copy_firmware(self):
        """Copies test firmware from server to DUT."""
        current_dir = os.path.dirname(os.path.realpath(__file__))
        src_firmware_path = os.path.join(current_dir, OLD_FW_NAME)
        dst_firmware_path = FW_PATH
        logging.info('Copy firmware from {} to {}.'.format(src_firmware_path,
                                                           dst_firmware_path))
        self.host.send_file(src_firmware_path, dst_firmware_path,
                            delete_dest=True)

    def triger_updater(self):
        """Trigers udev rule to run fw updater."""
        self.host.reboot()

    def flash_fw(self, fw_path):
        """
        Flashes certain firmware to device.

        Runs Bizlink firmware updater on DUT to flashes the firmware given
        by fw_path to target device.

        @param fw_path: the path to the firmware to flash.

        """
        cmd_run_updater = ('/usr/sbin/bizlink-updater --update=true '
                           '--fw_path={}'.format(fw_path))
        logging.info('executing {}'.format(cmd_run_updater))
        self.host.run(cmd_run_updater)

    def run_once(self):
        # Make the DUT filesystem writable.
        if not self.is_filesystem_readwrite():
            logging.info('DUT root file system is not read-writable. '
                         'Converting it read-writable...')
            self.convert_rootfs_writable()
        else:
            logging.info('DUT is read-writable.')

        # Copy old FW to device.
        self.copy_firmware()

        # Flash old FW to device.
        self.flash_fw(self.old_fw_path)
        expect_output = 'FW update succeed.'
        succeed = self.check_update_result(expected_output=expect_output)
        if not succeed:
            raise error.TestFail('Expect \'{}\' in output, '
                                 'but didn\'t find it.'.format(expect_output))

        self.triger_updater()

        # Wait for fw updater to finish.
        time.sleep(UPDATER_WAIT_TIME)

        # Try flash the new firmware, should detect same fw version.
        expect_output = 'Same FW version, no update required.'
        self.flash_fw(self.new_fw_path)
        succeed = self.check_update_result(expected_output=expect_output)
        if not succeed:
            raise error.TestFail('Expect {} in output '
                                 'but didn\'t find it.'.format(expect_output))

