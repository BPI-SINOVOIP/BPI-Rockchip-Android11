# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error


class vpd_ReadWrite(test.test):
    """Tests reading from and writing to vpd."""
    version = 1


    def _write_to_vpd(self, vpd_field, value):
        """
        Writes a value to a vpd field.

        @param vpd_field: The vpd field name
        @param value: The value to write to the field.

        @returns True if the write was successful, else False

        """
        try:
            result = utils.run('vpd -i RW_VPD -s %s=%d' % (vpd_field, value))
            logging.debug(result)
            return True
        except error.CmdError as err:
            logging.info('Failed to write %d to %s vpd field: %s', value,
                         vpd_field, err)
            return False


    def _read_from_vpd(self, vpd_field):
        """
        Reads a value from a vpd field.

        @param vpd_field: The vpd field name to read from.

        @returns The value of the vpd field specified or None if it failed.

        """
        try:
            result = utils.run('vpd -i RW_VPD -g %s' % vpd_field)
            logging.debug(result)
            return int(result.stdout)
        except error.CmdError as err:
            logging.info('Failed to read %s vpd field: %s', vpd_field, err)
            return None


    def _execute_read_write_cycle(self, repetitions, vpd_field):
        write_failures = 0
        read_failures = 0

        for value in range(repetitions):
            if not self._write_to_vpd(vpd_field, value):
                write_failures += 1
                continue

            value_from_vpd = self._read_from_vpd(vpd_field)

            if value_from_vpd is None:
                read_failures += 1
            elif value_from_vpd != value:
                write_failures += 1
                logging.info('No error when writing to vpd but reading showed '
                             'a different value than we expected. Expected: '
                             '%d, Actual: %d', value, value_from_vpd)

        if write_failures > 0 and read_failures > 0:
            raise error.TestFail('There were %d/%d write failures and %d/%d '
                                 'read failures.' % (write_failures,
                                                     repetitions,
                                                     read_failures,
                                                     repetitions))
        elif write_failures > 0:
            raise error.TestFail('There were %d/%d write failures' % (
                write_failures, repetitions))
        elif read_failures > 0:
            raise error.TestFail('There were %d/%d write failures' % (
                read_failures, repetitions))


    def run_once(self, repetitions):
        """
        Entry point to the test.

        @param repetitions: The number of times to cycle through the test.

        """
        self._execute_read_write_cycle(repetitions, 'should_send_rlz_ping')
        self._execute_read_write_cycle(repetitions,
                                       'first_active_omaha_ping_sent')
        logging.info('There were no read or write failures. Test successful')
