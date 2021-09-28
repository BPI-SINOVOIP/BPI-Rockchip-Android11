# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from autotest_lib.client.common_lib import error


def make_rootfs_writable(host):
    """
    Checks and makes root filesystem writable.

    Checks if root filesytem is writable. If not
    it converts root filesystem to writable.
    This function will reboot the DUT

    @param host: An Autotest host object

    @raises TestError: If making root fs  writable fails.

    """

    if  is_rootfs_writable(host):
        logging.info('DUT root file system is writable.')
    else:
        logging.info('DUT root file system is not writable. '
                     'Converting it writable...')
        convert_rootfs_writable(host)
        if not is_rootfs_writable(host):
            raise error.TestError('Failed to make root filesystem writable')
        logging.info('DUT root filesystem converted to writable')


def convert_rootfs_writable(host):
    """
    Makes CrOS rootfs writable.

    Remove root fs verification, reboot host
    and remounts the root fs

    @param host: An Autotest host object.

    @raises TestError: If executing the command on CrOS fails.
    """

    logging.info('Disabling rootfs verification.')
    remove_rootfs_verification(host)

    logging.info('Rebooting the host')
    host.reboot()

    logging.info('Remounting root filesystem')
    cmd = 'mount -o remount,rw /'
    res = host.run(cmd)

    if res.exit_status != 0:
        raise error.TestError('Executing remount command on DUT failed')


def remove_rootfs_verification(host):
    """
    Removes root fs verification from CrOS host.

    @param host: an Autotest host object.

    @raises TestError: if executing the command on CrOS fails.
    """

    # 2 & 4 are default partitions, and the system boots from one of them.
    # Code from chromite/scripts/deploy_chrome.py
    KERNEL_A_PARTITION = 2
    KERNEL_B_PARTITION = 4

    cmd_template = ('/usr/share/vboot/bin/make_dev_ssd.sh'
                    ' --partitions "%d %d"'
                    ' --remove_rootfs_verification --force')
    cmd = cmd_template % (KERNEL_A_PARTITION, KERNEL_B_PARTITION)
    res = host.run(cmd)
    if res.exit_status != 0:
        raise error.TestError('Executing command on DUT failed')


def is_rootfs_writable(host):
    """
    Checks if the root file system is writable.

    @param host: an Autotest host object.

    @returns Boolean denoting where root filesytem is writable or not.

    @raises TestError: if executing the command on CrOS fails.
    """

    # Query the DUT's filesystem /dev/root and check whether it is rw

    cmd = 'cat /proc/mounts | grep "/dev/root"'
    result = host.run(cmd)
    if result.exit_status > 0:
        raise error.TestError('Executing command on DUT failed')
    fields = re.split(' |,', result.stdout)

    # Result of grep will be of the following format
    # /dev/root / ext2 ro,seclabel <....truncated...> => readonly
    # /dev/root / ext2 rw,seclabel <....truncated...> => readwrite
    if fields.__len__() < 4 or fields[3] not in ['rw', 'ro']:
        raise error.TestError('Command output not in expected format')

    return fields[3] == 'rw'
