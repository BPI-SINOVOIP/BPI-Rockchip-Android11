#!/usr/bin/python2

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a module to scan /sys/block/ virtual FS, query udev

It provides a list of all removable or USB devices connected to the machine on
which the module is running.
It can be used from command line or from a python script.

To use it as python module it's enough to call the get_all() function.
@see |get_all| documentation for the output format
|get_all()| output is human readable (as oppposite to python's data structures)
"""

import logging, os, re

# this script can be run at command line on DUT (ie /usr/local/autotest
# contains only the client/ subtree), on a normal autotest
# installation/repository or as a python module used on a client-side test.
import common

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils

INFO_PATH = "/sys/block"
UDEV_CMD_FOR_SERIAL_NUMBER = "udevadm info -a -n %s | grep -iE 'ATTRS{" \
                             "serial}' | head -n 1"
LSUSB_CMD = "lsusb -v | grep -iE '^Device Desc|bcdUSB|iSerial'"
DESC_PATTERN = r'Device Descriptor:'
BCDUSB_PATTERN = r'bcdUSB\s+(\d+\.\d+)'
ISERIAL_PATTERN = r'iSerial\s+\d\s(\S*)'
UDEV_SERIAL_PATTERN = r'=="(.*)"'


def read_file(path_to_file, host=None):
    """Reads the file and returns the file content
    @param path_to_file: Full path to the file
    @param host: DUT object
    @return: Returns the content of file
    """
    if host:
        if not host.path_exists(path_to_file):
            raise error.TestError("No such file or directory %s" % path_to_file)
        return host.run('cat %s' % path_to_file).stdout.strip()

    if not os.path.isfile(path_to_file):
        raise error.TestError("No such file or directory %s" % path_to_file)
    return utils.read_file(path_to_file).strip()


def system_output(command, host=None, ignore_status=False):
    """Executes command on client

    @param host: DUT object
    @param command: command to execute
    @return: output of command
    """
    if host:
        return host.run(command, ignore_status=ignore_status).stdout.strip()

    return utils.system_output(command, ignore_status=ignore_status)


def get_udev_info(blockdev, method='udev', host=None):
    """Get information about |blockdev|

    @param blockdev: a block device, e.g., /dev/sda1 or /dev/sda
    @param method: either 'udev' (default) or 'blkid'
    @param host: DUT object

    @return a dictionary with two or more of the followig keys:
        "ID_BUS", "ID_MODEL": always present
        "ID_FS_UUID", "ID_FS_TYPE", "ID_FS_LABEL": present only if those info
         are meaningul and present for the queried device
    """
    ret = {}
    cmd = None
    ignore_status = False

    if method == "udev":
        cmd = "udevadm info --name %s --query=property" % blockdev
    elif method == "blkid":
        # this script is run as root in a normal autotest run,
        # so this works: It doesn't have access to the necessary info
        # when run as a non-privileged user
        cmd = "blkid -c /dev/null -o udev %s" % blockdev
        ignore_status = True

    if cmd:
        output = system_output(cmd, host, ignore_status=ignore_status)

        udev_keys = ("ID_BUS", "ID_MODEL", "ID_FS_UUID", "ID_FS_TYPE",
                     "ID_FS_LABEL")
        for line in output.splitlines():
            udev_key, udev_val = line.split('=')

            if udev_key in udev_keys:
                ret[udev_key] = udev_val

    return ret


def get_lsusb_info(host=None):
    """Get lsusb info in list format

    @param host: DUT object
    @return: Returns lsusb output in list format
    """

    usb_info_list = []
    # Getting the USB type and Serial number info using 'lsusb -v'. Sample
    # output is shown in below
    # Device Descriptor:
    #      bcdUSB               2.00
    #      iSerial                 3 131BC7
    #      bcdUSB               2.00
    # Device Descriptor:
    #      bcdUSB               2.10
    #      iSerial                 3 001A4D5E8634B03169273995

    lsusb_output = system_output(LSUSB_CMD, host)
    # we are parsing each line and getting the usb info
    for line in lsusb_output.splitlines():
        desc_matched = re.search(DESC_PATTERN, line)
        bcdusb_matched = re.search(BCDUSB_PATTERN, line)
        iserial_matched = re.search(ISERIAL_PATTERN, line)
        if desc_matched:
            usb_info = {}
        elif bcdusb_matched:
            # bcdUSB may appear multiple time. Drop the remaining.
            usb_info['bcdUSB'] = bcdusb_matched.group(1)
        elif iserial_matched:
            usb_info['iSerial'] = iserial_matched.group(1)
            usb_info_list.append(usb_info)
    logging.debug('lsusb output is %s', usb_info_list)
    return usb_info_list


def get_usbdevice_type_and_serial(device, lsusb_info, host=None):
    """Get USB device type and Serial number

    @param device: USB device mount point Example: /dev/sda or /dev/sdb
    @param lsusb_info: lsusb info
    @param host: DUT object
    @return: Returns the information about USB type and the serial number
            of the device
    """

    # Comparing the lsusb serial number with udev output serial number
    # Both serial numbers should be same. Sample udev command output is
    # shown in below.
    # ATTRS{serial}=="001A4D5E8634B03169273995"
    udev_serial_output = system_output(UDEV_CMD_FOR_SERIAL_NUMBER % device,
                                       host)
    udev_serial_matched = re.search(UDEV_SERIAL_PATTERN, udev_serial_output)
    if udev_serial_matched:
        udev_serial = udev_serial_matched.group(1)
        logging.debug("udev serial number is %s", udev_serial)
        for usb_details in lsusb_info:
            if usb_details['iSerial'] == udev_serial:
                return usb_details.get('bcdUSB'), udev_serial
    return None, None

def get_partition_info(part_path, bus, model, partid=None, fstype=None,
                       label=None, block_size=0, is_removable=False,
                       lsusb_info=[], host=None):
    """Return information about a device as a list of dictionaries

    Normally a single device described by the passed parameters will match a
    single device on the system, and thus a single element list as return
    value; although it's possible that a single block device is associated with
    several mountpoints, this scenario will lead to a dictionary for each
    mountpoint.

    @param part_path: full partition path under |INFO_PATH|
                      e.g., /sys/block/sda or /sys/block/sda/sda1
    @param bus: bus, e.g., 'usb' or 'ata', according to udev
    @param model: device moduel, e.g., according to udev
    @param partid: partition id, if present
    @param fstype: filesystem type, if present
    @param label: filesystem label, if present
    @param block_size: filesystem block size
    @param is_removable: whether it is a removable device
    @param host: DUT object
    @param lsusb_info: lsusb info

    @return a list of dictionaries contaning each a partition info.
            An empty list can be returned if no matching device is found
    """
    ret = []
    # take the partitioned device name from the /sys/block/ path name
    part = part_path.split('/')[-1]
    device = "/dev/%s" % part

    if not partid:
        info = get_udev_info(device, "blkid", host=host)
        partid = info.get('ID_FS_UUID', None)
        if not fstype:
            fstype = info.get('ID_FS_TYPE', None)
        if not label:
            label = partid

    readonly = read_file("%s/ro" % part_path, host)
    if not int(readonly):
        partition_blocks = read_file("%s/size" % part_path, host)
        size = block_size * int(partition_blocks)

        stub = {}
        stub['device'] = device
        stub['bus'] = bus
        stub['model'] = model
        stub['size'] = size

        # look for it among the mounted devices first
        mounts = read_file("/proc/mounts", host).splitlines()
        seen = False
        for line in mounts:
            dev, mount, proc_fstype, flags = line.split(' ', 3)

            if device == dev:
                if 'rw' in flags.split(','):
                    seen = True # at least one match occurred

                    # Sorround mountpoint with quotes, to make it parsable in
                    # case of spaces. Also information retrieved from
                    # /proc/mount override the udev passed ones (e.g.,
                    # proc_fstype instead of fstype)
                    dev = stub.copy()
                    dev['fs_uuid'] = partid
                    dev['fstype'] = proc_fstype
                    dev['is_mounted'] = True
                    # When USB device is mounted automatically after login a
                    # non-labelled drive is mounted to:
                    # '/media/removable/USB Drive'
                    # Here an octal unicode '\040' is added to the path
                    # replacing ' ' (space).
                    # Following '.decode('unicode-escape')' handles the same
                    dev['mountpoint'] = mount.decode('unicode-escape')
                    dev['is_removable'] = is_removable
                    dev['usb_type'], dev['serial'] = \
                            get_usbdevice_type_and_serial(dev['device'],
                                                          lsusb_info=lsusb_info,
                                                          host=host)
                    ret.append(dev)

        # If not among mounted devices, it's just attached, print about the
        # same information but suggest a place where the user can mount the
        # device instead
        if not seen:
            # we consider it if it's removable and and a partition id
            # OR it's on the USB bus or ATA bus.
            # Some USB HD do not get announced as removable, but they should be
            # showed.
            # There are good changes that if it's on a USB bus it's removable
            # and thus interesting for us, independently whether it's declared
            # removable
            if (is_removable and partid) or bus in ['usb', 'ata']:
                if not label:
                    info = get_udev_info(device, 'blkid', host=host)
                    label = info.get('ID_FS_LABEL', partid)

                dev = stub.copy()
                dev['fs_uuid'] = partid
                dev['fstype'] = fstype
                dev['is_mounted'] = False
                dev['mountpoint'] = "/media/removable/%s" % label
                dev['is_removable'] = is_removable
                dev['usb_type'], dev['serial'] = \
                        get_usbdevice_type_and_serial(dev['device'],
                                                      lsusb_info=lsusb_info,
                                                      host=host)
                ret.append(dev)
        return ret


def get_device_info(blockdev, lsusb_info, host=None):
    """Retrieve information about |blockdev|

    @see |get_partition_info()| doc for the dictionary format

    @param blockdev: a block device name, e.g., "sda".
    @param host: DUT object
    @param lsusb_info: lsusb info
    @return a list of dictionary, with each item representing a found device
    """
    ret = []

    spath = "%s/%s" % (INFO_PATH, blockdev)
    block_size = int(read_file("%s/queue/physical_block_size" % spath,
                                   host))
    is_removable = bool(int(read_file("%s/removable" % spath, host)))

    info = get_udev_info(blockdev, "udev", host=host)
    dev_bus = info['ID_BUS']
    dev_model = info['ID_MODEL']
    dev_fs = info.get('ID_FS_TYPE', None)
    dev_uuid = info.get('ID_FS_UUID', None)
    dev_label = info.get('ID_FS_LABEL', dev_uuid)

    has_partitions = False
    for basename in system_output('ls %s' % spath, host).splitlines():
        partition_path = "%s/%s" % (spath, basename)
        # we want to check if within |spath| there are subdevices with
        # partitions
        # e.g., if within /sys/block/sda sda1 and other partition are present
        if not re.match("%s[0-9]+" % blockdev, basename):
            continue # ignore what is not a subdevice

        # |blockdev| has subdevices: get info for them
        has_partitions = True
        devs = get_partition_info(partition_path, dev_bus, dev_model,
                                  block_size=block_size,
                                  is_removable=is_removable,
                                  lsusb_info=lsusb_info, host=host)
        ret.extend(devs)

    if not has_partitions:
        devs = get_partition_info(spath, dev_bus, dev_model, dev_uuid, dev_fs,
                                  dev_label, block_size=block_size,
                                  is_removable=is_removable,
                                  lsusb_info=lsusb_info, host=host)
        ret.extend(devs)

    return ret


def get_all(host=None):
    """Return all removable or USB storage devices attached

    @param host: DUT object
    @return a list of dictionaries, each list element describing a device
    """
    ret = []
    lsusb_info = get_lsusb_info(host)
    for dev in system_output('ls %s' % INFO_PATH, host).splitlines():
        # Among block devices we need to filter out what are virtual
        if re.match("s[a-z]+", dev):
            # for each of them try to obtain some info
            ret.extend(get_device_info(dev, lsusb_info, host=host))
    return ret


def main():
    for device in get_all():
        print ("%(device)s %(bus)s %(model)s %(size)d %(fs_uuid)s %(fstype)s "
               "%(is_mounted)d %(mountpoint)s %(usb_type)s %(serial)s" %
               device)


if __name__ == "__main__":
    main()
