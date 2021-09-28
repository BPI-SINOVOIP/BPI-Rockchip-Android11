# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A test to verify the muxable USB port on servo works with a stick."""

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test

class servo_USBMuxVerification(test.test):
    """Test to expose mux to both servo and dut host as well as power off."""
    version = 1

    # The file names to find the stick's vid, pid, serial.
    STICK_ID_FILE_NAMES = ('idVendor', 'idProduct', 'serial')

    def _validate_state(self):
      """Validate the current state of the mux and the visibility of the stick.

      Given the mux direction, and the power state, validates that the stick
      is showing up on the host it is expected to show up, and not on the
      host(s) it is not.

      Raises:
        error.TestFail: if the USB stick is visible when it should not be or if
        the USB stick is not visible when it should be.
      """
      # A small sleep count to ensure everyone is in the right state and the
      # caches values/sysfs files are all updated.
      time.sleep(2)
      host_map = {'dut': self.dut_host,
                  'servo': self.servo_host}
      direction = self.servo.get('image_usbkey_direction')
      pwr = self.servo.get('image_usbkey_pwr')
      expectation = {}
      expectation['dut'] = expectation['servo'] = True
      if pwr == 'off':
        expectation['dut'] = expectation['servo'] = False
      else:
        if direction == 'dut_sees_usbkey':
          expectation['servo'] = False
        elif direction == 'servo_sees_usbkey':
          expectation['dut'] = False
        else:
          raise error.TestNA('Unknown servo usbkey direction %s' % direction)
      for host in expectation:
        if expectation[host] and not self.is_usb_stick_visible(host_map[host]):
          raise error.TestFail ('Usb stick not visible on %s side even though '
                                'it should be.' % host)
        if not expectation[host] and self.is_usb_stick_visible(host_map[host]):
          raise error.TestFail ('Usb stick visible on %s side even though it '
                                'should not be.' % host)

    def is_usb_stick_visible(self, host):
      """Check whether the stick is visible on |host|.

      On |host| check whether a usb device exists that has the
      (vid, pid, serial) that was originally read out from the stick.

      Args:
        host: dut or servo host to inspect

      Returns:
        True if the stick is visible on |host|, False otherwise

      Raises:
        error.TestError: if more than one usb device have the vid, pid, serial
        that was originally identified by the init sequence
      """
      usb_dirs = []
      for value, fname in zip(self.stick_id, self.STICK_ID_FILE_NAMES):
        fs = host.run('grep -lr %s $(find /sys/bus/usb/devices/*/ -maxdepth 1 '
                      '-name %s)' % (value, fname),
                      ignore_status=True).stdout.strip().split()
        # Remove the file name to have the usb sysfs dir
        dirnames = ['/'.join(f.split('/')[:-1]) for f in fs]
        usb_dirs.append(set(dirnames))
      common_usb_dev = usb_dirs.pop()
      while usb_dirs:
        # This will only leave the usb device dirs that share the same
        # vid, pid, serial. Ideally, that's 1 - the stick, or 0, if the stick
        # is not visible
        common_usb_dev = common_usb_dev & usb_dirs.pop()
      if len(common_usb_dev) > 1:
        raise error.TestError('More than one usb device detected on host %s '
                              'with vid:0x%s pid:0x%s serial:%s.'
                              % ((host.hostname,) + self.stick_id))
      return len(common_usb_dev) == 1

    def get_usb_stick_id(self):
      """Helper to retrieve usb stick's vid, pid, and serial.

      Returns:
        (vid, pid, serial) tuple of the stick

      Raises:
        error.TestFail: if the usb stick cannot be found or if reading
        any of the idVendor, idProduct, serial files fails.
      """
      # Getting the stick id by pointing it to the servo host.
      self.servo.set('image_usbkey_direction', 'servo_sees_usbkey')
      # Get the usb sysfs directory of the stick
      symbolic_dev_name = self.servo.get('image_usbkey_dev')
      if not symbolic_dev_name:
        raise error.TestFail('No usb stick dev file found.')
      # This will get just the sdx portion of /dev/sdx
      path_cmd = 'realpath /sys/block/%s' % symbolic_dev_name.split('/')[-1]
      # This sed command essentially anchors the path on the usb device's
      # interface's pattern i.e. the bus-port.port...:x.x pattern. Then
      # it removes anything beyond it and it itself, leaving the usb
      # device file's sysfs directory that houses the ID files.
      sed_cmd =  r"sed -nr 's|/[0-9]+\-[1-9.]+\:[0-9.]+/.*$|/|p'"
      cmd = r'%s | %s' % (path_cmd, sed_cmd)
      logging.debug('Using cmd: %s over ssh to see the stick\'s usb sysfs '
                    'device file directory.', cmd)
      dev_dir = self.servo_host.run(cmd).stdout.strip()
      # Get vid, pid, serial
      if not dev_dir:
        raise error.TestFail('Failed to find the usb stick usb sysfs device '
                             'directory on the servo host.')
      id_elems = []
      for id_file in self.STICK_ID_FILE_NAMES:
        cmd = 'sudo cat %s%s' % (dev_dir, id_file)
        try:
          elem = self.servo_host.run(cmd).stdout.strip()
          if not elem:
            raise error.TestFail('Device id file %s not found.' % id_file)
        except (error.AutoservRunError, error.TestFail) as e:
          raise error.TestFail('Failed to get %s file for dev %s' % (id_file,
                                                                     dev_dir))
        id_elems.append(elem)
      return tuple(id_elems)

    def initialize(self, host):
      """Initialize test by extracting usb stick information."""
      self.dut_host = host
      self.servo_host = host._servo_host
      self.servo = host.servo
      self._original_pwr = self.servo.get('image_usbkey_pwr')
      self._original_direction = self.servo.get('image_usbkey_direction')
      self.servo.set('image_usbkey_pwr', 'on')
      logging.info('Turning the stick to the servo host.')
      # Stick id is a (vid, pid, serial) tuple for the stick.
      self.stick_id = self.get_usb_stick_id()

    def run_once(self, host):
      """Run through the test cases.

      - Facing the servo host
        - power off
      - Facing the DUT
        - power off
      """
      self.servo.set('image_usbkey_direction', 'servo_sees_usbkey')
      # Sleep for 2s to let the device properly enumerate.
      self._validate_state()
      logging.info('Turning the power to the stick off.')
      self.servo.set('image_usbkey_pwr', 'off')
      self._validate_state()
      logging.info('Turning the power to the stick on.')
      self.servo.set('image_usbkey_pwr', 'on')
      logging.info('Turning the stick to the dut host.')
      self.servo.set('image_usbkey_direction', 'dut_sees_usbkey')
      # Sleep for 2s to let the device properly enumerate.
      self._validate_state()
      logging.info('Turning the power to the stick off.')
      self.servo.set('image_usbkey_pwr', 'off')
      self._validate_state()

    def cleanup(self, host):
      """Restore usb mux to its original state."""
      self.servo.set('image_usbkey_pwr', self._original_pwr)
      self.servo.set('image_usbkey_direction', self._original_direction)
