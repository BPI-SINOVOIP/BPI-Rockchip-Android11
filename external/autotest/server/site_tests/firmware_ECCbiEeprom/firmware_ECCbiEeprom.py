# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest

class firmware_ECCbiEeprom(FirmwareTest):
    """Servo-based EC test for Cros Board Info EEPROM"""
    version = 1

    EEPROM_LOCATE_TYPE = 0
    EEPROM_LOCATE_INDEX = 0   # Only one EEPROM ever

    # Test data to attempt to write to EEPROM
    TEST_EEPROM_DATA = ('0xaa ' * 16).strip()
    TEST_EEPROM_DATA_2 = ('0x55 ' * 16).strip()

    # Size of read and write
    PAGE_SIZE = 16
    NO_READ = 0

    # The number of bytes we verify are both writable and write protectable
    MAX_BYTES = 64

    def initialize(self, host, cmdline_args):
        super(firmware_ECCbiEeprom, self).initialize(host, cmdline_args)
        # Don't bother if CBI isn't on this device.
        if not self.check_ec_capability(['cbi']):
            raise error.TestNAError("Nothing needs to be tested on this device")
        cmd = 'ectool locatechip %d %d' % (self.EEPROM_LOCATE_TYPE,
                                           self.EEPROM_LOCATE_INDEX)
        cmd_out = self.faft_client.system.run_shell_command_get_output(
                cmd, True)
        logging.debug('Ran %s on DUT, output was: %s', cmd, cmd_out)

        if len(cmd_out) > 0 and cmd_out[0].startswith('Usage'):
            raise error.TestNAError("I2C lookup not supported yet.")

        if len(cmd_out) < 1:
            cmd_out = ['']

        match = re.search('Bus: I2C; Port: (\w+); Address: (\w+)', cmd_out[0])
        if match is None:
            raise error.TestFail("I2C lookup for EEPROM CBI Failed.  Check "
                                 "debug log for output.")

        # Allow hex value parsing (i.e. base set to 0)
        self.i2c_port = int(match.group(1), 0)
        self.i2c_addr = int(match.group(2), 0)

        # Ensure that the i2c mux is disabled on the servo as the CBI EEPROM
        # i2c lines are shared with the servo lines on some HW designs. We do
        # not want the servo to artificially drive the i2c lines during this
        # test. This only applies to non CCD servo connections
        if not self.servo.get_servo_version().endswith('ccd_cr50'):
            self.servo.set('i2c_mux_en', 'off')

    def _gen_write_command(self, offset, data):
        return ('ectool i2cxfer %d %d %d %d %s' %
               (self.i2c_port, self.i2c_addr, self.NO_READ, offset, data))

    def _read_eeprom(self, offset):
        cmd_out = self.faft_client.system.run_shell_command_get_output(
                  'ectool i2cxfer %d %d %d %d' %
                  (self.i2c_port, self.i2c_addr, self.PAGE_SIZE, offset))
        if len(cmd_out) < 1:
            raise error.TestFail(
                "Could not read EEPROM data at offset %d" % (offset))
        data = re.search('Read bytes: (.+)', cmd_out[0]).group(1)
        if data == '':
            raise error.TestFail(
                "Empty EEPROM read at offset %d" % (offset))
        return data

    def _write_eeprom(self, offset, data):
        # Note we expect this call to fail in certain scenarios, so ignore
        # results
        self.faft_client.system.run_shell_command_get_output(
             self._gen_write_command(offset, data))

    def _read_write_data(self, offset):
        before = self._read_eeprom(offset)
        logging.info("To reset CBI that's in a bad state, run w/ WP off:\n%s",
                     self._gen_write_command(offset, before))

        if before == self.TEST_EEPROM_DATA:
            write_data = self.TEST_EEPROM_DATA_2
        else:
            write_data = self.TEST_EEPROM_DATA

        self._write_eeprom(offset, write_data)

        after = self._read_eeprom(offset)

        return before, write_data, after

    def check_eeprom_write_protected(self):
        """Checks that CBI EEPROM cannot be written to when WP is asserted"""
        self.set_hardware_write_protect(True)
        offset = 0

        for offset in range(0, self.MAX_BYTES, self.PAGE_SIZE):
            before, write_data, after = self._read_write_data(offset)

            if before != after:
                # Set the data back to the original value before failing
                self._write_eeprom(offset, before)

                raise error.TestFail('EEPROM data changed with write protect '
                                     'enabled. Offset %d' % (offset))

        return True

    def check_eeprom_without_write_protected(self):
        """Checks that CBI EEPROM can be written to when WP is de-asserted"""
        self.set_hardware_write_protect(False)
        offset = 0

        for offset in range(0, self.MAX_BYTES, self.PAGE_SIZE):
            before, write_data, after = self._read_write_data(offset)

            if write_data != after:
                raise error.TestFail('EEPROM did not update with write protect '
                                     'disabled. Offset %d' % (offset))

            # Set the data back to the original value
            self._write_eeprom(offset, before)

        return True

    def run_once(self):
        """Execute the main body of the test."""

        logging.info("Checking CBI EEPROM with write protect off...")
        self.check_eeprom_without_write_protected()

        logging.info("Checking CBI EEPROM with write protect on...")
        self.check_eeprom_write_protected()
