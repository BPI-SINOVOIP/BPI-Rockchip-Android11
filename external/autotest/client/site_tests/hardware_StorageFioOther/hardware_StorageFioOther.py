# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.storage_tests import fio_test

class hardware_StorageFioOther(fio_test.FioTest):
    """
    Runs several fio jobs on the non-root device and reports results.

    fio (flexible I/O tester) is an I/O tool for benchmark and stress/hardware
    verification.
    """

    version = 1
    DEFAULT_FILE_SIZE = 1024 * 1024 * 1024

    def initialize(self, dev='', filesize=DEFAULT_FILE_SIZE):
        """
        Set up local variables.

        @param dev: block device / file to test.
        @param filesize: size of the file. 0 means whole partition.
                by default, 1GB.
        """
        if dev != '':
            self._dev = dev
        else:
            devs = utils.get_other_device()
            if len(devs) == 0:
                raise error.TestFail("No other storage devices found")
            elif len(devs) > 1:
                raise error.TestFail("More than one other storage device found")
            self._dev = devs[0]
        super(hardware_StorageFioOther, self).initialize(dev=self._dev,
                                                         filesize=filesize)

    def run_once(self, dev='', quicktest=False, requirements=None,
                 integrity=False, wait=60 * 60 * 72):
        """
        Determines the non-root device to test, and then runs the
        hardware_StorageFio test, which runs several fio jobs and reports
        results.

        @param dev: block device to test
        @param quicktest: short test
        @param requirements: list of jobs for fio to run
        @param integrity: test to check data integrity
        @param wait: seconds to wait between a write and subsequent verify
        """
        super(hardware_StorageFioOther,
              self).run_once(dev=self._dev, quicktest=quicktest,
                             requirements=requirements,
                             integrity=integrity,
                             wait=wait)
