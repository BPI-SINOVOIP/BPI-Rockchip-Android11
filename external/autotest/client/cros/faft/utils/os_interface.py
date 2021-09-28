# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A module to provide interface to OS services."""

import datetime
import os
import re
import struct

import shell_wrapper


class OSInterfaceError(Exception):
    """OS interface specific exception."""
    pass


class Crossystem(object):
    """A wrapper for the crossystem utility."""

    # Code dedicated for user triggering recovery mode through crossystem.
    USER_RECOVERY_REQUEST_CODE = '193'

    def __init__(self, os_if):
        """Init the instance. If running on Mario - adjust the map."""
        self.os_if = os_if

    def __getattr__(self, name):
        """
        Retrieve a crosssystem attribute.

        Attempt to access crossystemobject.name will invoke `crossystem name'
        and return the stdout as the value.
        """
        return self.os_if.run_shell_command_get_output(
                'crossystem %s' % name)[0]

    def __setattr__(self, name, value):
        if name in ('os_if', ):
            self.__dict__[name] = value
        else:
            self.os_if.run_shell_command(
                    'crossystem "%s=%s"' % (name, value), modifies_device=True)

    def request_recovery(self):
        """Request recovery mode next time the target reboots."""

        self.__setattr__('recovery_request', self.USER_RECOVERY_REQUEST_CODE)


class OSInterface(object):
    """An object to encapsulate OS services functions."""

    def __init__(self, state_dir=None, log_file=None, test_mode=False):
        """Object initialization (side effect: creates the state_dir)

        @param state_dir: the name of the directory to use for storing state.
                            The contents of this directory persist over system
                            restarts and power cycles.
        @param log_file: the name of the log file kept in the state directory.
        @param test_mode: if true, skip (and just log) any shell call
                          marked with modifies_device=True
        """

        # We keep the state of FAFT test in a permanent directory over reboots.
        if state_dir is None:
            state_dir = '/usr/local/tmp/faft'

        if log_file is None:
            log_file = 'faft_client.log'

        if not os.path.isabs(log_file):
            log_file = os.path.join(state_dir, log_file)

        self.state_dir = state_dir
        self.log_file = log_file
        self.test_mode = test_mode

        self.shell = shell_wrapper.LocalShell(self)
        self.host_shell = None

        self.create_dir(self.state_dir)

        self.cs = Crossystem(self)

    def run_shell_command(self, cmd, block=True, modifies_device=False):
        """Run a shell command.

        @param cmd: the command to run
        @param block: if True (default), wait for command to finish
        @param modifies_device: If True and running in test mode, just log
                                the command, but don't actually run it.
                                This should be set for RPC commands that alter
                                the OS or firmware in some persistent way.

        @raise autotest_lib.client.common_lib.error.CmdError: if command fails
        """
        if self.test_mode and modifies_device:
            self.log('[SKIPPED] %s' % cmd)
        else:
            self.shell.run_command(cmd, block=block)

    def run_shell_command_check_output(self, cmd, success_token):
        """Run shell command and check its stdout for a string."""
        return self.shell.run_command_check_output(cmd, success_token)

    def run_shell_command_get_result(self, cmd, ignore_status=False):
        """Run shell command and get a CmdResult object as a result.

        @param cmd: the command to run
        @param ignore_status: if True, do not raise CmdError, even if rc != 0.
        @rtype: autotest_lib.client.common_lib.utils.CmdResult
        @raise autotest_lib.client.common_lib.error.CmdError: if command fails
        """
        return self.shell.run_command_get_result(cmd, ignore_status)

    def run_shell_command_get_status(self, cmd):
        """Run shell command and return its return code."""
        return self.shell.run_command_get_status(cmd)

    def run_shell_command_get_output(self, cmd, include_stderr=False):
        """Run shell command and return its console output."""
        return self.shell.run_command_get_output(cmd, include_stderr)

    def read_file(self, path):
        """Read the content of the file."""
        return self.shell.read_file(path)

    def write_file(self, path, data):
        """Write the data to the file."""
        self.shell.write_file(path, data)

    def append_file(self, path, data):
        """Append the data to the file."""
        self.shell.append_file(path, data)

    def path_exists(self, path):
        """Return True if the path exists on DUT."""
        cmd = 'test -e %s' % path
        return self.run_shell_command_get_status(cmd) == 0

    def is_dir(self, path):
        """Return True if the path is a directory."""
        cmd = 'test -d %s' % path
        return self.run_shell_command_get_status(cmd) == 0

    def create_dir(self, path):
        """Create a new directory."""
        cmd = 'mkdir -p %s' % path
        return self.run_shell_command(cmd)

    def create_temp_file(self, prefix):
        """Create a temporary file with a prefix."""
        tmp_path = '/tmp'
        cmd = 'mktemp -p %s %sXXXXXX' % (tmp_path, prefix)
        return self.run_shell_command_get_output(cmd)[0]

    def copy_file(self, from_path, to_path):
        """Copy the file."""
        cmd = 'cp -f %s %s' % (from_path, to_path)
        return self.run_shell_command(cmd)

    def copy_dir(self, from_path, to_path):
        """Copy the directory."""
        cmd = 'cp -rf %s %s' % (from_path, to_path)
        return self.run_shell_command(cmd)

    def remove_file(self, path):
        """Remove the file."""
        cmd = 'rm -f %s' % path
        return self.run_shell_command(cmd)

    def remove_dir(self, path):
        """Remove the directory."""
        cmd = 'rm -rf %s' % path
        return self.run_shell_command(cmd)

    def get_file_size(self, path):
        """Get the size of the file."""
        cmd = 'stat -c %%s %s' % path
        return int(self.run_shell_command_get_output(cmd)[0])

    def target_hosted(self):
        """Return True if running on DUT."""
        with open('/etc/lsb-release', 'r') as lsb_release:
            signature = lsb_release.readlines()[0]
        return bool(re.search(r'chrom(ium|e)os', signature, re.IGNORECASE))

    def state_dir_file(self, file_name):
        """Get a full path of a file in the state directory."""
        return os.path.join(self.state_dir, file_name)

    def log(self, text):
        """Write text to the log file and print it on the screen, if enabled.

        The entire log (maintained across reboots) can be found in
        self.log_file.
        """
        if not self.log_file or not os.path.exists(self.state_dir):
            # Called before environment was initialized, ignore.
            return

        timestamp = datetime.datetime.strftime(datetime.datetime.now(),
                                               '%I:%M:%S %p:')

        with open(self.log_file, 'a') as log_f:
            log_f.write('%s %s\n' % (timestamp, text))
            log_f.flush()
            os.fdatasync(log_f.fileno())

    def is_removable_device(self, device):
        """Check if a certain storage device is removable.

        device - a string, file name of a storage device or a device partition
                 (as in /dev/sda[0-9] or /dev/mmcblk0p[0-9]).

        Returns True if the device is removable, False if not.
        """
        if not self.target_hosted():
            return False

        # Drop trailing digit(s) and letter(s) (if any)
        base_dev = self.strip_part(device.split('/')[2])
        removable = int(self.read_file('/sys/block/%s/removable' % base_dev))

        return removable == 1

    def get_internal_disk(self, device):
        """Get the internal disk by given the current disk.

        If device is removable device, internal disk is decided by which kind
        of divice (arm or x86). Otherwise, return device itself.

        device - a string, file name of a storage device or a device partition
                 (as in /dev/sda[0-9] or /dev/mmcblk0p[0-9]).

        Return internal kernel disk.
        """
        if self.is_removable_device(device):
            for p in ('/dev/mmcblk0', '/dev/mmcblk1', '/dev/nvme0n1'):
                if self.path_exists(p):
                    return p
            return '/dev/sda'
        else:
            return self.strip_part(device)

    def get_root_part(self):
        """Return a string, the name of root device with partition number"""
        return self.run_shell_command_get_output('rootdev -s')[0]

    def get_root_dev(self):
        """Return a string, the name of root device without partition number"""
        return self.strip_part(self.get_root_part())

    def join_part(self, dev, part):
        """Return a concatenated string of device and partition number"""
        if dev.endswith(tuple(str(i) for i in range(0, 10))):
            return dev + 'p' + part
        else:
            return dev + part

    def strip_part(self, dev_with_part):
        """Return a stripped string without partition number"""
        dev_name_stripper = re.compile('p?[0-9]+$')
        return dev_name_stripper.sub('', dev_with_part)

    def retrieve_body_version(self, blob):
        """Given a blob, retrieve body version.

        Currently works for both, firmware and kernel blobs. Returns '-1' in
        case the version can not be retrieved reliably.
        """
        header_format = '<8s8sQ'
        preamble_format = '<40sQ'
        magic, _, kb_size = struct.unpack_from(header_format, blob)

        if magic != 'CHROMEOS':
            return -1  # This could be a corrupted version case.

        _, version = struct.unpack_from(preamble_format, blob, kb_size)
        return version

    def retrieve_datakey_version(self, blob):
        """Given a blob, retrieve firmware data key version.

        Currently works for both, firmware and kernel blobs. Returns '-1' in
        case the version can not be retrieved reliably.
        """
        header_format = '<8s96sQ'
        magic, _, version = struct.unpack_from(header_format, blob)
        if magic != 'CHROMEOS':
            return -1  # This could be a corrupted version case.
        return version

    def retrieve_kernel_subkey_version(self, blob):
        """Given a blob, retrieve kernel subkey version.

        It is in firmware vblock's preamble.
        """

        header_format = '<8s8sQ'
        preamble_format = '<72sQ'
        magic, _, kb_size = struct.unpack_from(header_format, blob)

        if magic != 'CHROMEOS':
            return -1

        _, version = struct.unpack_from(preamble_format, blob, kb_size)
        return version

    def retrieve_preamble_flags(self, blob):
        """Given a blob, retrieve preamble flags if available.

        It only works for firmware. If the version of preamble header is less
        than 2.1, no preamble flags supported, just returns 0.
        """
        header_format = '<8s8sQ'
        preamble_format = '<32sII64sI'
        magic, _, kb_size = struct.unpack_from(header_format, blob)

        if magic != 'CHROMEOS':
            return -1  # This could be a corrupted version case.

        _, ver, subver, _, flags = struct.unpack_from(preamble_format, blob,
                                                      kb_size)

        if ver > 2 or (ver == 2 and subver >= 1):
            return flags
        else:
            return 0  # Returns 0 if preamble flags not available.

    def read_partition(self, partition, size):
        """Read the requested partition, up to size bytes."""
        tmp_file = self.state_dir_file('part.tmp')
        self.run_shell_command(
                'dd if=%s of=%s bs=1 count=%d' % (partition, tmp_file, size))
        data = self.read_file(tmp_file)
        self.remove_file(tmp_file)
        return data
