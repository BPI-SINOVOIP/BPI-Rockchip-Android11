# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, os, subprocess

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error

class hardware_UnsafeMemory(test.test):
    """
      This test runs for the specified number of seconds
      checking for user-controllable memory corruption using
      the rowhammer-test tools:
         https://code.google.com/a/google.com/p/rowhammer-test
    """

    version = 1
    _DIR_NAME = 'rowhammer-test-4d619293e1c7'

    def setup(self):
        os.chdir(os.path.join(self.srcdir, self._DIR_NAME))
        utils.make('clean')
        utils.make('all')

    def get_thermals(self):
        therm0 = '-'
        therm1 = '-'
        try:
            therm0 = utils.read_file(
                '/sys/devices/virtual/thermal/thermal_zone0/temp')
        except:
            pass
        try:
            therm1 = utils.read_file(
                '/sys/devices/virtual/thermal/thermal_zone1/temp')
        except:
            pass
        return (therm0, therm1)

    def run_once(self, sec=(60*25)):
        """
        Executes the test and logs the output.

        @param sec: seconds to test memory
        """
        self._hammer_path = os.path.join(self.srcdir, self._DIR_NAME,
                                         'rowhammer_test')
        logging.info('cmd: %s %d' % (self._hammer_path, sec))
        # Grab the CPU temperature before hand if possible.
        logging.info('start temp: %s %s' % self.get_thermals())
        try:
            output = subprocess.check_output([self._hammer_path, '%d' % sec])
            logging.info("run complete. Output below:")
            logging.info(output)
        except subprocess.CalledProcessError, e:
            logging.error("Unsafe memory found!")
            logging.error(e.output)
            logging.info('end temp: %s %s' % self.get_thermals())
            raise error.TestFail('Unsafe memory found!')
        logging.info('end temp: %s %s' % self.get_thermals())
        return True
