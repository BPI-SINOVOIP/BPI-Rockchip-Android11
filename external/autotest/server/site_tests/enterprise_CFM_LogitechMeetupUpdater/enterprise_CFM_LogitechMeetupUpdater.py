# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Autotest for Logitech Meetup firmware updater."""

import logging
import os
import re
import time


from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import power_cycle_usb_util
from autotest_lib.client.common_lib.cros.cfm.usb import cfm_usb_devices
from autotest_lib.server import test


POWER_CYCLE_WAIT_TIME_SEC = 20


class enterprise_CFM_LogitechMeetupUpdater(test.test):
    """
    Logitech Meetup firmware test on Chrome For Meeting devices
    The test follows the following steps
        1) Check if the filesystem is writable
           If not make the filesystem writable and reboot
        2) Backup the existing firmware file on DUT
        3) Copy the older firmware files to DUT
        4) Force update older firmware on Meetup Camera
        5) Restore the original firmware files on DUT
        4) Power cycle usb port to simulate unplug/replug of device which
           should initiate a firmware update
        5) Wait for firmware update to finish and check firmware version
        6) Cleanup

    """

    version = 1

    def initialize(self, host):
        """
        Initializes the class.

        Stores the firmware file path.
        Gets the board type.
        Reads the current firmware versions.
        """

        self.host = host
        self.log_file = '/tmp/logitech-updater.log'
        self.fw_path_base = '/lib/firmware/logitech'
        self.fw_pkg_origin = 'meetup'
        self.fw_pkg_backup = 'meetup_backup'
        self.fw_pkg_test = 'meetup_184'
        self.fw_pkg_files = ['meetup_audio.bin',
                             'meetup_audio_logicool.bin',
                             'meetup_ble.bin',
                             'meetup_codec.bin',
                             'meetup_eeprom_logicool.s19',
                             'meetup_eeprom.s19',
                             'meetup_video.bin',
                             'meetup_audio.bin.sig',
                             'meetup_audio_logicool.bin.sig',
                             'meetup_ble.bin.sig',
                             'meetup_codec.bin.sig',
                             'meetup_eeprom_logicool.s19.sig',
                             'meetup_eeprom.s19.sig',
                             'meetup_video.bin.sig']
        self.fw_path_test = os.path.join(self.fw_path_base,
                                         self.fw_pkg_test)
        self.fw_path_origin = os.path.join(self.fw_path_base,
                                           self.fw_pkg_origin)
        self.fw_path_backup = os.path.join(self.fw_path_base,
                                           self.fw_pkg_backup)
        self.board = self.host.get_board().split(':')[1]
        self.vid = cfm_usb_devices.LOGITECH_MEETUP.vendor_id
        self.pid = cfm_usb_devices.LOGITECH_MEETUP.product_id
        self.org_fw_ver = self.get_image_fw_ver()

    def cleanup(self):
        """
        Cleanups after tests.

        Removes the test firmware.
        Restores the original firmware files.
        Flashes the camera to original firmware if needed.
        """

        # Delete test firmware package.
        cmd = 'rm -rf {}'.format(self.fw_path_test)
        self.host.run(cmd)

        # Delete the symlink created.
        cmd = 'rm {}'.format(self.fw_path_origin)
        self.host.run(cmd)

        # Move the backup package back.
        cmd = 'mv {} {}'.format(self.fw_path_backup, self.fw_path_origin)
        self.host.run(cmd)

        # Do not leave the camera with test (older) firmware.
        if not self.is_device_firmware_equal_to(self.org_fw_ver):
            logging.debug('Meetup device has old firmware after test'
                          'Flashing new firmware')
            self.flash_fw()

        super(enterprise_CFM_LogitechMeetupUpdater, self).cleanup()

    def _run_cmd(self, command, ignore_status=True):
        """
        Runs command line on DUT, wait for completion and return the output.

        @param command: command line to run in dut.
        @param ignore_status: if true ignore the status return by command

        @returns the command output

        """

        logging.debug('Execute: %s', command)

        result = self.host.run(command, ignore_status=ignore_status)
        if result.stderr:
            output = result.stderr
        else:
            output = result.stdout
        logging.debug('Output: %s', output)
        return output

    def make_rootfs_writable(self):
        """Checks and makes root filesystem writable."""

        if not self.is_filesystem_readwrite():
            logging.info('DUT root file system is not writable. '
                         'Converting it writable...')
            self.convert_rootfs_writable()
        else:
            logging.info('DUT root file system is writable.')

    def convert_rootfs_writable(self):
        """Makes DUT rootfs writable."""

        logging.info('Disabling rootfs verification...')
        self.remove_rootfs_verification()

        logging.info('Rebooting...')
        self.host.reboot()

        logging.info('Remounting..')
        cmd = 'mount -o remount,rw /'
        self.host.run(cmd)

    def remove_rootfs_verification(self):
        """Removes rootfs verification."""

        # 2 & 4 are default partitions, and the system boots from one of them.
        # Code from chromite/scripts/deploy_chrome.py
        KERNEL_A_PARTITION = 2
        KERNEL_B_PARTITION = 4

        cmd_template = ('/usr/share/vboot/bin/make_dev_ssd.sh'
                        ' --partitions "%d %d"'
                        ' --remove_rootfs_verification --force')
        cmd = cmd_template % (KERNEL_A_PARTITION, KERNEL_B_PARTITION)
        self.host.run(cmd)

    def is_filesystem_readwrite(self):
        """Checks if the root file system is writable."""

        # Query the DUT's filesystem /dev/root and check whether it is rw

        cmd = 'cat /proc/mounts | grep "/dev/root"'
        result = self._run_cmd(cmd)
        fields = re.split(' |,', result)

        # Result of grep will be of the following format
        # /dev/root / ext2 ro,seclabel <....truncated...> => readonly
        # /dev/root / ext2 rw,seclabel <....truncated...> => readwrite
        is_writable = fields.__len__() >= 4 and fields[3] == 'rw'
        return is_writable

    def fw_ver_from_output_str(self, cmd_output):
        """
        Parse firmware version of logitech-updater output.

        logitech-updater output differs for image_version and device_version
        This function finds the line which contains string "Meetup" and parses
        succeding lines. Each line is split on spaces (after collapsing spaces)
        and index 1 gives component name (ex. Eeprom) and index 3 gives the
        firmware version (ex. 1.14)
        The actual output is given below.

        logitech-updater --image_version

        [INFO:main.cc(105)] PTZ Pro 2 Versions:
        [INFO:main.cc(59)] Video version:  2.0.175
        [INFO:main.cc(61)] Eeprom version: 1.6
        [INFO:main.cc(63)] Mcu2 version:   3.9

        [INFO:main.cc(105)] MeetUp Versions:
        [INFO:main.cc(59)] Video version:  1.0.197
        [INFO:main.cc(61)] Eeprom version: 1.14
        [INFO:main.cc(65)] Audio version:  1.0.239
        [INFO:main.cc(67)] Codec version:  8.0.216
        [INFO:main.cc(69)] BLE version:    1.0.121

        logitech-updater  --device_version

        [INFO:main.cc(88)] Device name:    Logitech MeetUp
        [INFO:main.cc(59)] Video version:  1.0.197
        [INFO:main.cc(61)] Eeprom version: 1.14
        [INFO:main.cc(65)] Audio version:  1.0.239
        [INFO:main.cc(67)] Codec version:  8.0.216
        [INFO:main.cc(69)] BLE version:    1.0.121


        """

        logging.debug('Parsing output from updater %s', cmd_output)
        if 'MeetUp image not found' in cmd_output or 'MeetUp' not in cmd_output:
           raise error.TestFail('MeetUp image not found on DUT')
        try:
            version = {}
            output = cmd_output.split('\n')
            start_line = -1

            # Find the line of the output with string "Meetup
            for i, l in enumerate(output):
                if 'MeetUp' in l:
                    start_line = i
                    break

            if start_line == -1:
                raise error.TestFail('Meetup version not found'
                                     ' in updater output')

            output = output[start_line+1:start_line+6]
            logging.debug('Parsing Meetup firmware info %s', str(output))
            for l in output:

                # Output lines are of the format
                # [INFO:main.cc(59)] Video version:  1.0.197
                l = ' '.join(l.split())  # Collapse multiple spaces to one space
                parts = l.split(' ')  # parts[1] is "Video" parts[3] is 1.0.197
                version[parts[1]] = parts[3]
            logging.debug('Version is %s', str(version))
            return version
        except:
            logging.error('Error while parsing logitech-updater output')
            raise

    def get_updater_output(self, cmd):
        """Get updater output while avoiding transient failures."""

        NUM_RETRIES = 3
        WAIT_TIME = 5
        for _ in range(NUM_RETRIES):
            output = self._run_cmd(cmd)
            if 'Failed to read' in output:
                time.sleep(WAIT_TIME)
                continue
            return output

    def get_image_fw_ver(self):
        """Get the version of firmware on DUT."""

        output = self.get_updater_output('logitech-updater --image_version'
                                         ' --log_to=stdout')
        return self.fw_ver_from_output_str(output)

    def get_device_fw_ver(self):
        """Get the version of firmware on Meetup device."""

        output = self.get_updater_output('logitech-updater --device_version'
                                         ' --log_to=stdout')
        return self.fw_ver_from_output_str(output)

    def copy_test_firmware(self):
        """Copy test firmware from server to DUT."""

        current_dir = os.path.dirname(os.path.realpath(__file__))
        src_firmware_path = os.path.join(current_dir, self.fw_pkg_test)
        dst_firmware_path = self.fw_path_base
        logging.info('Copy firmware from (%s) to (%s).', src_firmware_path,
                     dst_firmware_path)
        self.host.send_file(src_firmware_path, dst_firmware_path,
                            delete_dest=True)

    def trigger_updater(self):
        """Trigger udev rule to run fw updater by power cycling the usb."""

        try:
            power_cycle_usb_util.power_cycle_usb_vidpid(self.host, self.board,
                                                        self.vid, self.pid)
        except KeyError:
            raise error.TestFail('Counld\'t find target device: '
                                 'vid:pid {}:{}'.format(self.vid, self.pid))

    def wait_for_meetup_device(self):
        """
        Wait for Meetup device device to be enumerated.

        Check if a device with given (vid,pid) is present.
        Timeout after wait_time seconds. Default 30 seconds
        """

        TIME_SLEEP = 10
        NUM_ITERATIONS = 3
        WAIT_TIME = TIME_SLEEP * NUM_ITERATIONS

        logging.debug('Waiting for Meetup device')
        for _ in range(NUM_ITERATIONS):
            res = power_cycle_usb_util.get_port_number_from_vidpid(
                self.host, self.vid, self.pid)
            (bus_num, port_num) = res
            if bus_num is not None and port_num is not None:
                logging.debug('Meetup device detected')
                return
            else:
                logging.debug('Meetup device not detected.'
                              'Waiting for (%s) seconds', TIME_SLEEP)
                time.sleep(TIME_SLEEP)

        logging.error('Unable to detect the device after (%s) seconds.'
                      'Timing out...', WAIT_TIME)
        raise error.TestFail('Target device not detected.')

    def setup_fw(self, firmware_package):
        """Setup firmware package that is going to be used for updating."""

        firmware_path = os.path.join(self.fw_path_base, firmware_package)
        cmd = 'ln -sfn {} {}'.format(firmware_path, self.fw_path_origin)
        self.host.run(cmd)

    def flash_fw(self, force=False):
        """Flash certain firmware to device.

        Run logitech firmware updater on DUT to flash the firmware setuped
        to target device (PTZ Pro 2).

        @param force: run with force update, will bypass fw version check.

        """

        cmd = ('/usr/sbin/logitech-updater --log_to=stdout --update_components'
               ' --lock')
        if force:
            cmd += ' --force'
        output = self._run_cmd(cmd)
        return output

    def print_fw_version(self, version, info_str=''):
        """Pretty print Meetup firmware version."""

        if info_str:
            print info_str
        print 'Video version: ', version['Video']
        print 'Eeprom version: ', version['Eeprom']
        print 'Audio version: ', version['Audio']
        print 'Codec version: ', version['Codec']
        print 'BLE version: ', version['BLE']

    def is_device_firmware_equal_to(self, expected_ver):
        """Check that the device fw version is equal to given version."""

        device_fw_version = self.get_device_fw_ver()
        if  device_fw_version != expected_ver:
            logging.error('Device firmware version is not the expected version')
            self.print_fw_version(device_fw_version, 'Device firmware version')
            self.print_fw_version(expected_ver, 'Expected firmware version')
            return False
        else:
            return True

    def flash_old_firmware(self):
        """Flash old (test) version of firmware on the device."""

        # Flash old FW to device.
        self.setup_fw(self.fw_pkg_test)
        test_fw_ver = self.get_image_fw_ver()
        self.print_fw_version(test_fw_ver, 'Test firmware version')
        output = self.flash_fw(force=True)
        time.sleep(POWER_CYCLE_WAIT_TIME_SEC)
        with open(self.log_file, 'w') as f:
            delim = '-' * 8
            f.write('{}Log info for writing old firmware{}'
                    '\n'.format(delim, delim))
            f.write(output)
        if not self.is_device_firmware_equal_to(test_fw_ver):
            raise error.TestFail('Flashing old firmware failed')
        logging.info('Device flashed with test firmware')

    def backup_original_firmware(self):
        """Backup existing firmware on DUT."""
        # Copy old FW to device.
        cmd = 'mv {} {}'.format(self.fw_path_origin, self.fw_path_backup)
        self.host.run(cmd)

    def is_updater_running(self):
        """Checks if the logitech-updater is running."""

        cmd = 'logitech-updater --lock --device_version --log_to=stdout'
        output = self._run_cmd(cmd)
        return 'There is another logitech-updater running' in output

    def wait_for_updater(self):
        """Wait logitech-updater to stop or timeout after 6 minutes."""

        NUM_ITERATION = 12
        WAIT_TIME = 30  # seconds
        logging.debug('Wait for any currently running updater to finish')
        for _ in range(NUM_ITERATION):
            if self.is_updater_running():
                logging.debug('logitech-updater is running.'
                              'Waiting for 30 seconds')
                time.sleep(WAIT_TIME)
            else:
                logging.debug('logitech-updater not running')
                return
        logging.error('logitech-updater is still running after 6 minutes')

    def test_firmware_update(self):
        """Trigger firmware updater and check device firmware version."""

        # Simulate hotplug to run FW updater.
        logging.info('Setup original firmware')
        self.setup_fw(self.fw_pkg_backup)
        logging.info('Simulate hot plugging the device')
        self.trigger_updater()
        self.wait_for_meetup_device()

        # The firmware check will fail if the check runs in a short window
        # between the device being detected and the firmware updater starting.
        # Adding a delay to reduce the chance of that scenerio.
        time.sleep(POWER_CYCLE_WAIT_TIME_SEC)

        self.wait_for_updater()

        if not self.is_device_firmware_equal_to(self.org_fw_ver):
            raise error.TestFail('Camera not updated to new firmware')
        logging.info('Firmware update was completed successfully')

    def run_once(self):
        """
        Entry point for test.

        The following actions are performed in this test.
        - Device is flashed with older firmware.
        - Powercycle usb port to simulate hotplug inorder to start the updater.
        - Check that the device is updated with newer firmware.
        """

        # Check if updater is already running
        self.wait_for_updater()

        self.print_fw_version(self.org_fw_ver,
                              'Original firmware version on DUT')
        self.print_fw_version(self.get_device_fw_ver(),
                              'Firmware version on Meetup device')

        self.make_rootfs_writable()
        self.backup_original_firmware()

        # Flash test firmware version
        self.copy_test_firmware()
        self.flash_old_firmware()

        # Test firmware update
        self.test_firmware_update()
        logging.info('Logitech Meetup firmware updater test was successful')
