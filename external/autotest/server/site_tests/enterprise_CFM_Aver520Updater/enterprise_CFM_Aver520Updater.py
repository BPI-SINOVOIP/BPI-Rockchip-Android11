# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Autotest for Aver VC520/CAM520 camera firmware updater."""

import logging
import os
import re
import time


from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import power_cycle_usb_util
from autotest_lib.client.common_lib.cros.cfm.usb import usb_device_collector
from autotest_lib.server import test
from autotest_lib.server.cros import filesystem_util


FW_PATH_BASE = '/lib/firmware'
FW_PKG_ORIGIN = 'aver'
FW_PKG_BACKUP = 'aver_backup'
FW_PKG_TEST = 'aver_520_18.02'
LOG_FILE = '/tmp/aver-updater.log'
POWER_CYCLE_WAIT_TIME_SEC = 240


class enterprise_CFM_Aver520Updater(test.test):
    """
    Aver camera firmware test on Chrome For Meeting devices
    This test works for both Aver VC520 and CAM520.
    The test follows the following steps
        1) Check if the filesystem is writable
           If not make the filesystem writable and reboot
        2) Backup the existing firmware file on DUT
        3) Copy the older firmware files to DUT
        4) Force update older firmware on Aver Camera
        5) Restore the original firmware files on DUT
        4) Power cycle usb port to simulate unplug/replug of device which
           should initiate a firmware update
        5) Wait for firmware update to finish and check firmware version
        6) Cleanup

    """

    version = 1

    def initialize(self, host, camera):
        """
        Initializes the class.

        Stores the firmware file path.
        Gets the board type.
        Reads the current firmware versions.
        """

        self.host = host
        self.camera = camera
        self.fw_path_test = os.path.join(FW_PATH_BASE,
                                         FW_PKG_TEST)
        self.fw_path_origin = os.path.join(FW_PATH_BASE,
                                           FW_PKG_ORIGIN)
        self.fw_path_backup = os.path.join(FW_PATH_BASE,
                                           FW_PKG_BACKUP)
        self.board = self.host.get_board().split(':')[1]

        self.device_collector = usb_device_collector.UsbDeviceCollector(
            self.host)

        self.vid_pid = self.camera.vid_pid
        self.usb_spec = self.camera.get_usb_device_spec(self.vid_pid)

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
            logging.debug('Aver 520 camera has old firmware after test'
                          'Flashing new firmware')
            self.flash_fw()

        super(enterprise_CFM_Aver520Updater, self).cleanup()

    def _run_cmd(self, command):
        """
        Runs command line on DUT, wait for completion and return the output.

        @param command: command line to run in dut.

        @returns the command output
        """

        logging.debug('Execute: %s', command)

        result = self.host.run(command, ignore_status=True)
        output = result.stderr if result.stderr else result.stdout
        logging.debug('Output: %s', output)
        return output

    def fw_ver_from_output_str(self, cmd_output):
        """
        Parse firmware version of aver-updater output.

        aver-updater output differs slightly for image_version and
        device_version.
        For image_version strip ".dat" from output
        This function will fail if version is not in the format
        x.x.xxxx.xx where x is in  [0-9]

        The actual output is given below.

        aver-updater --image_version
        [INFO:main.cc(79)] image_version: 0.0.0018.07.dat

        aver-updater --device_version
        [INFO:main.cc(71)] device_version: 0.0.0018.08

        """

        logging.debug('Parsing output from updater %s', cmd_output)
        if 'Error(2) opening /lib/firmware/aver/' in cmd_output:
            raise error.TestFail('Aver firmware image not found on DUT')

        if ('device_version' not in cmd_output and
            'image_version' not in cmd_output):
            raise error.TestFail('Parsing aver firmware version output failed')

        version = ''
        output = cmd_output.split('\n')
        for line in output:
            logging.debug('parsing line %s from output', line)
            if 'device_version' not in line and 'image_version' not in line:
                continue
            parts = line.split(' ')

            if parts[1] == 'device_version:':
                version = parts[2]
            elif parts[1] == 'image_version:':
                version = parts[2].strip('.dat')
            else:
                raise error.TestFail('Unexpected output from updater %s'
                                     % parts)

            version = version.strip('\0')  #  Remove null characters
            logging.debug('Version parsed from output is %s', version)

            if not bool(re.match(r'^\d\.\d\.\d\d\d\d\.\d\d$', version)):
                logging.debug('parsed version is %s ', version)
                raise error.TestFail('Version %s not in'
                                     'expected format' % version)

            logging.debug('Version is %s', str(version))
            return version

    def get_updater_output(self, cmd):
        """Get updater output while avoiding transient failures."""

        NUM_RETRIES = 5
        WAIT_TIME_SEC = 20
        for _ in range(NUM_RETRIES):
            output = self._run_cmd(cmd)
            if ('Open hid fd fail' in output or
                'query data size fail' in output or
                'There is another aver-updater running.' in output or
                'Failed to open the device' in output):
                time.sleep(WAIT_TIME_SEC)
                continue
            return output

    def get_image_fw_ver(self):
        """Get the version of firmware on DUT."""

        output = self.get_updater_output('aver-updater --image_version'
                                         ' --log_to=stdout --lock')
        return self.fw_ver_from_output_str(output)

    def get_device_fw_ver(self):
        """Get the version of firmware on Aver 520 camera."""

        output = self.get_updater_output('aver-updater --device_version'
                                         ' --log_to=stdout --lock')
        return self.fw_ver_from_output_str(output)

    def copy_test_firmware(self):
        """Copy test firmware from server to DUT."""

        current_dir = os.path.dirname(os.path.realpath(__file__))
        src_firmware_path = os.path.join(current_dir, FW_PKG_TEST)
        dst_firmware_path = FW_PATH_BASE
        logging.info('Copy firmware from (%s) to (%s).', src_firmware_path,
                     dst_firmware_path)
        self.host.send_file(src_firmware_path, dst_firmware_path,
                            delete_dest=True)

    def trigger_updater(self):
        """Trigger udev rule to run fw updater by power cycling the usb."""

        try:
            vid = self.camera.vendor_id
            pid = self.camera.product_id
            power_cycle_usb_util.power_cycle_usb_vidpid(self.host, self.board,
                                                        vid, pid)
        except KeyError:
            raise error.TestFail('Could not find target device: '
                                 '{}'.format(self.camera))

    def wait_for_aver_camera(self, wait_time=30):
        """
        Wait for Aver 520 camera to be enumerated.

        Check if a device with given (vid,pid) is present.
        Timeout after wait_time seconds. Default 30 seconds
        """

        TIME_SLEEP = 10
        NUM_ITERATIONS = max(wait_time / TIME_SLEEP, 1)

        logging.debug('Waiting for Aver 520 camera')
        for _ in range(NUM_ITERATIONS):
            if self.device_collector.get_devices_by_spec(self.usb_spec):
                logging.debug('Aver 520 camera detected')
                return
            else:
                logging.debug('Aver 520 camera not detected.'
                              'Waiting for (%s) seconds', TIME_SLEEP)
                time.sleep(TIME_SLEEP)

        logging.error('Unable to detect the device after (%s) seconds.'
                      'Timing out...\n Target device %s not  detected',
                      wait_time, self.camera)
        raise error.TestFail('Target device not detected')

    def setup_fw(self, firmware_package):
        """Setup firmware package that is going to be used for updating."""

        firmware_path = os.path.join(FW_PATH_BASE, firmware_package)
        cmd = 'ln -sfn {} {}'.format(firmware_path, self.fw_path_origin)
        logging.debug('executing cmd %s ', cmd)
        self.host.run(cmd)

    def flash_fw(self, force=False):
        """Flash certain firmware to device.

        Run logitech firmware updater on DUT to flash the firmware setuped
        to target device (PTZ Pro 2).

        @param force: run with force update, will bypass fw version check.

        """

        cmd = ('/usr/sbin/aver-updater --log_to=stdout --update'
               ' --lock')
        if force:
            cmd += ' --force'
        output = self.get_updater_output(cmd)
        return output

    def print_fw_version(self, version, info_str=''):
        """Pretty print Aver 520 camera firmware version."""

        if info_str:
            print info_str,
        print ' Firmware version:', version

    def is_device_firmware_equal_to(self, expected_ver):
        """Check that the device fw version is equal to given version."""

        device_fw_version = self.get_device_fw_ver()
        if  device_fw_version != expected_ver:
            logging.error('Device firmware version is not the expected version')
            self.print_fw_version(device_fw_version, 'Device firmware version')
            self.print_fw_version(expected_ver, 'Expected firmware version')
            return False

        return True

    def flash_old_firmware(self):
        """Flash old (test) version of firmware on the device."""

        # Flash old FW to device.
        self.setup_fw(FW_PKG_TEST)
        test_fw_ver = self.get_image_fw_ver()
        self.print_fw_version(test_fw_ver, 'Test firmware version')
        logging.debug('flashing test firmware on the device')
        output = self.flash_fw(force=True)
        time.sleep(POWER_CYCLE_WAIT_TIME_SEC)
        with open(LOG_FILE, 'w') as f:
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
        """Checks if the aver-updater is running."""

        cmd = 'aver-updater --lock --device_version --log_to=stdout'
        output = self._run_cmd(cmd)
        return 'There is another aver-updater running. Exiting now...' in output

    def wait_for_updater(self):
        """Wait aver-updater to stop or timeout after 6 minutes."""

        NUM_ITERATION = 12
        WAIT_TIME_SEC = 30
        logging.debug('Wait for any currently running updater to finish')
        for _ in range(NUM_ITERATION):
            if self.is_updater_running():
                logging.debug('aver-updater is running.'
                              'Waiting for %s seconds', WAIT_TIME_SEC)
                time.sleep(WAIT_TIME_SEC)
            else:
                logging.debug('aver-updater not running')
                return
        logging.error('aver-updater is still running after 6 minutes')

    def test_firmware_update(self):
        """Trigger firmware updater and check device firmware version."""

        # Simulate hotplug to run FW updater.
        logging.info('Setup original firmware')
        self.setup_fw(FW_PKG_BACKUP)
        self.print_fw_version(self.get_image_fw_ver(), 'Firmware on disk')
        logging.info('Simulate hot plugging the device')
        self.trigger_updater()
        self.wait_for_aver_camera()

        # The firmware check will fail if the check runs in a short window
        # between the device being detected and the firmware updater starting.
        # Adding a delay to reduce the chance of that scenerio.
        logging.info('Waiting for the updater to update the firmware')
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
        logging.info('Testing firmware update on Aver %s camera',
                     self.camera)
        logging.info('Confirm that camera is present')

        self.wait_for_aver_camera(wait_time=0)

        self.wait_for_updater()

        self.print_fw_version(self.org_fw_ver,
                              'Original firmware version on DUT')
        self.print_fw_version(self.get_device_fw_ver(),
                              'Firmware version on  device')

        filesystem_util.make_rootfs_writable(self.host)
        self.backup_original_firmware()

        # Flash test firmware version
        self.copy_test_firmware()
        self.flash_old_firmware()

        # Test firmware update
        self.test_firmware_update()
        logging.info('Aver %s camera firmware updater'
                     'test was successful', self.camera)
