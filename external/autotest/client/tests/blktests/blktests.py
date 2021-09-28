# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os, re, logging
from autotest_lib.client.common_lib import error
from autotest_lib.client.bin import test, utils

class blktests(test.test):
    """
    Runs the blktests suite.
    """

    version = 1

    BLKTESTS_PATH = '/mnt/stateful_partition/unencrypted/cache'
    BLKTESTS_TEST_DIR = "/usr/local/blktests"
    CONFIG_FILE = '/usr/local/blktests/config'

    FAILED_RE = re.compile(r'.*\[failed\].*', re.DOTALL)
    DEVICE_RE = re.compile(r'/dev/(sd[a-z]|mmcblk[0-9]+|nvme[0-9]+)p?[0-9]*')

    devs=[]
    loop_devs=[]
    files=[]
    exclude=[]

    def setup_configs(self, devices):
        """
        Setup the blk devices to test.
        @param devs: The desired blk devices to test (BLK: real blk
               device, LOOP_FILE: loop device over file, or LOOP_BLK:
               loop device over real blk device).
        """

        for dev in devices:
            if dev == 'BLK':
                dev_name = utils.get_free_root_partition()
                self.devs.append(dev_name)
                # block/013 tries to reread the partition table of the device
                # This won't work when run on a block device partition, so we
                # will exclude the test.
                self.exclude.append("block/013")
            elif dev == 'LOOP_FILE':
                file_name = 'blktests_test'
                file_loc = os.path.join(self.BLKTESTS_PATH, file_name)
                utils.system('fallocate -l 10M %s' % file_loc)
                loop_dev = utils.system_output('losetup -f -P --show %s'
                                               % file_loc)
                self.devs.append(loop_dev)
                self.loop_devs.append(loop_dev)
                self.files.append(file_loc)
            elif dev == 'LOOP_BLK':
                blk_dev = utils.get_free_root_partition()
                loop_dev = utils.system_output('losetup -f -P --show %s'
                                               % blk_dev)
                self.devs.append(loop_dev)
                self.loop_devs.append(loop_dev)
            elif self.DEVICE_RE.match(dev):
                if dev == utils.get_root_partition():
                    raise error.TestError("Can't run the test on the root "
                                          "partition.")
                elif dev == utils.get_kernel_partition():
                    raise error.TestError("Can't run the test on the kernel "
                                          "partition.")
                elif dev == utils.concat_partition(utils.get_root_device(), 1):
                    raise error.TestError("Can't run the test on the stateful "
                                          "partition.")
                self.devs.append(dev)
            else:
                raise error.TestError("Invalid device specified")
        test_devs = ' '.join(self.devs)
        exclusions = ""
        if self.exclude:
            exclusions = "EXCLUDE=(%s)" % ' '.join(self.exclude)
        config = "TEST_DEVS=(%s) %s" % (test_devs, exclusions)
        logging.debug("Test config: %s", config)
        configFile = open(self.CONFIG_FILE, 'w')
        configFile.write(config)
        configFile.close()

    def cleanup(self):
        """
        Clean up the environment by removing any created files and loop devs.
        """
        for dev in self.loop_devs:
            utils.system('losetup -d %s' % dev)
        for f in self.files:
            utils.system('rm %s' % f, ignore_status=True)
        if os.path.isfile(self.CONFIG_FILE):
            os.remove(self.CONFIG_FILE)

    def run_once(self, devices=['LOOP_FILE']):
        """
        Setup the config file and run blktests.

        @param devices: The desired block devices to test (BLK: real block
               device, LOOP_FILE: loop device over file, or LOOP_BLK:
               loop device over real block device).
        """
        os.chdir(self.BLKTESTS_TEST_DIR)
        self.setup_configs(devices)
        output = utils.system_output('bash ./check',
                                     ignore_status=True, retain_output=True)
        if self.FAILED_RE.match(output):
            raise error.TestError('Test error, check debug logs for complete '
                                  'test output')
