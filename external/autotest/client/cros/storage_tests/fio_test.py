# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fcntl, logging, os, re, stat, struct, time
from autotest_lib.client.bin import fio_util, test, utils
from autotest_lib.client.common_lib import error


class FioTest(test.test):
    """
    Runs several fio jobs and reports results.

    fio (flexible I/O tester) is an I/O tool for benchmark and stress/hardware
    verification.

    """

    version = 7
    DEFAULT_FILE_SIZE = 1024 * 1024 * 1024
    VERIFY_OPTION = 'v'

    # Initialize fail counter used to determine test pass/fail.
    _fail_count = 0
    _error_code = 0

    # 0x1277 is ioctl BLKDISCARD command
    IOCTL_TRIM_CMD = 0x1277

    def __get_disk_size(self):
        """Return the size in bytes of the device pointed to by __filename"""
        self.__filesize = utils.get_disk_size(self.__filename)

        if not self.__filesize:
            raise error.TestNAError(
                'Unable to find the partition %s, please plug in a USB '
                'flash drive and a SD card for testing external storage' %
                self.__filename)


    def __get_device_description(self):
        """Get the device vendor and model name as its description"""

        # Find the block device in sysfs. For example, a card read device may
        # be in /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-5/1-5:1.0/host4/
        # target4:0:0/4:0:0:0/block/sdb.
        # Then read the vendor and model name in its grand-parent directory.

        # Obtain the device name by stripping the partition number.
        # For example, sda3 => sda; mmcblk1p3 => mmcblk1, nvme0n1p3 => nvme0n1.
        device = re.match(r'.*(sd[a-z]|mmcblk[0-9]+|nvme[0-9]+n[0-9]+)p?[0-9]*',
                          self.__filename).group(1)
        findsys = utils.run('find /sys/devices -name %s | grep -v virtual'
                            % device)
        device_path = findsys.stdout.rstrip()

        if "nvme" in device:
            dir_path = utils.run('dirname %s' % device_path).stdout.rstrip()
            model_file = '%s/model' % dir_path
            if os.path.exists(model_file):
                self.__description = utils.read_one_line(model_file).strip()
            else:
                self.__description = ''
        else:
            vendor_file = device_path.replace('block/%s' % device, 'vendor')
            model_file = device_path.replace('block/%s' % device, 'model')
            if os.path.exists(vendor_file) and os.path.exists(model_file):
                vendor = utils.read_one_line(vendor_file).strip()
                model = utils.read_one_line(model_file).strip()
                self.__description = vendor + ' ' + model
            else:
                self.__description = ''


    def initialize(self, dev='', filesize=DEFAULT_FILE_SIZE):
        """
        Set up local variables.

        @param dev: block device / file to test.
                Spare partition on root device by default
        @param filesize: size of the file. 0 means whole partition.
                by default, 1GB.
        """
        if dev != '' and (os.path.isfile(dev) or not os.path.exists(dev)):
            if filesize == 0:
                raise error.TestError(
                    'Nonzero file size is required to test file systems')
            self.__filename = dev
            self.__filesize = filesize
            self.__description = ''
            return

        if not dev:
            dev = utils.get_fixed_dst_drive()

        if dev == utils.get_root_device():
            if filesize == 0:
                raise error.TestError(
                    'Using the root device as a whole is not allowed')
            else:
                self.__filename = utils.get_free_root_partition()
        elif filesize != 0:
            # Use the first partition of the external drive if it exists
            partition = utils.concat_partition(dev, 1)
            if os.path.exists(partition):
                self.__filename = partition
            else:
                self.__filename = dev
        else:
            self.__filename = dev
        self.__get_disk_size()
        self.__get_device_description()

        # Restrict test to use a given file size, default 1GiB
        if filesize != 0:
            self.__filesize = min(self.__filesize, filesize)

        self.__verify_only = False

        logging.info('filename: %s', self.__filename)
        logging.info('filesize: %d', self.__filesize)

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

        if requirements is not None:
            pass
        elif quicktest:
            requirements = [
                ('1m_write', []),
                ('16k_read', [])
            ]
        elif integrity:
            requirements = [
                ('8k_async_randwrite', []),
                ('8k_async_randwrite', [self.VERIFY_OPTION])
            ]
        elif dev in ['', utils.get_root_device()]:
            requirements = [
                ('surfing', []),
                ('boot', []),
                ('login', []),
                ('seq_write', []),
                ('seq_read', []),
                ('16k_write', []),
                ('16k_read', []),
                ('1m_stress', []),
            ]
        else:
            # TODO(waihong@): Add more test cases for external storage
            requirements = [
                ('seq_write', []),
                ('seq_read', []),
                ('16k_write', []),
                ('16k_read', []),
                ('4k_write', []),
                ('4k_read', []),
                ('1m_stress', []),
            ]

        results = {}

        if os.path.exists(self.__filename) and  \
           stat.S_ISBLK(os.stat(self.__filename).st_mode) and \
           self.__filesize != 0 and blkdiscard:
            try:
                fd = os.open(self.__filename, os.O_RDWR)
                fcntl.ioctl(fd, self.IOCTL_TRIM_CMD,
                            struct.pack('QQ', 0, self.__filesize))
            except IOError, err:
                pass
            finally:
                os.close(fd)

        for job, options in requirements:

            # Keys are labeled according to the test case name, which is
            # unique per run, so they cannot clash
            if self.VERIFY_OPTION in options:
                time.sleep(wait)
                self.__verify_only = True
            else:
                self.__verify_only = False
            env_vars = ' '.join(
                ['FILENAME=' + self.__filename,
                 'FILESIZE=' + str(self.__filesize),
                 'VERIFY_ONLY=' + str(int(self.__verify_only))
                ])
            client_dir = os.path.dirname(os.path.dirname(self.bindir))
            storage_dir = os.path.join(client_dir, 'cros/storage_tests')
            job_file = os.path.join(storage_dir, job)
            results.update(fio_util.fio_runner(self, job_file, env_vars))

        # Output keys relevant to the performance, larger filesize will run
        # slower, and sda5 should be slightly slower than sda3 on a rotational
        # disk
        self.write_test_keyval({'filesize': self.__filesize,
                                'filename': self.__filename,
                                'device': self.__description})
        logging.info('Device Description: %s', self.__description)
        self.write_perf_keyval(results)
        for k, v in results.iteritems():
            if k.endswith('_error'):
                self._error_code = int(v)
                if self._error_code != 0 and self._fail_count == 0:
                    self._fail_count = 1
            elif k.endswith('_total_err'):
                self._fail_count = int(v)
        if self._fail_count > 0:
            raise error.TestFail('%s failed verifications, '
                                 'first error code is %s' %
                                 (str(self._fail_count), str(self._error_code)))
