# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros.storage_tests import fio_test

class hardware_StorageFio(fio_test.FioTest):
    """
    Runs several fio jobs and reports results.

    fio (flexible I/O tester) is an I/O tool for benchmark and stress/hardware
    verification.

    """

    version = 7
    DEFAULT_FILE_SIZE = 1024 * 1024 * 1024

    def initialize(self, dev='', filesize=DEFAULT_FILE_SIZE):
        """
        Set up local variables.

        @param dev: block device / file to test.
                Spare partition on root device by default
        @param filesize: size of the file. 0 means whole partition.
                by default, 1GB.
        """
        super(hardware_StorageFio, self).initialize(dev=dev, filesize=filesize)

    def run_once(self, dev='', quicktest=False, requirements=None,
                 integrity=False, wait=60 * 60 * 72, blkdiscard=True):
        """
        Runs several fio jobs and reports results.

        @param dev: block device to test
        @param quicktest: short test
        @param requirements: list of jobs for fio to run
        @param integrity: test to check data integrity
        @param wait: seconds to wait between a write and subsequent verify
        @param blkdiscard: do a blkdiscard before running fio

        """
        super(hardware_StorageFio, self).run_once(dev=dev, quicktest=quicktest,
                                                  requirements=requirements,
                                                  integrity=integrity,
                                                  wait=wait,
                                                  blkdiscard=blkdiscard)
