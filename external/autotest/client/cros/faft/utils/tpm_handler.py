# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A module containing TPM handler class used by SAFT."""

FW_NV_ADDRESS = 0x1007
KERNEL_NV_ADDRESS = 0x1008


class TpmError(Exception):
    """An object to represent TPM errors"""
    pass


class TpmNvRam(object):
    """An object representing TPM NvRam.

    @ivar addr: a number, NvRAm address in TPM.
    @ivar size: a number, count of bites in this NvRam section.
    @ivar os_if: an instance of the OS interface (os_interface or a mock object)
    @ivar version_offset: - a number, offset into the NvRam contents where the
        the versions are stored. The total version field size is 4 bytes, the
        first two bytes are the body version, the second two bytes are the key
        version. Numbers are stored in little endian format.
    @ivar pattern: optional, a tuple of two elements, the first element is the
       offset of the pattern expected to be present in the NvRam, and the
       second element is an array of bytes the pattern must match.
    @ivar contents: an array of bytes, the contents of the NvRam.
    @type os_if: autotest_lib.client.cros.faft.utils.os_interface.OSInterface
    """

    def __init__(self, os_if, addr, size, version_offset, data_pattern=None):
        self.os_if = os_if
        self.addr = addr
        self.size = size
        self.version_offset = version_offset
        self.pattern = data_pattern
        self.contents = []

    def init(self):
        """Initialize the object, loading tpm nvram information from the device.
        """
        cmd = 'tpmc read 0x%x 0x%x' % (self.addr, self.size)
        nvram_data = self.os_if.run_shell_command_get_output(cmd)[0].split()
        self.contents = [int(x, 16) for x in nvram_data]
        if self.pattern:
            pattern_offset = self.pattern[0]
            pattern_data = self.pattern[1]
            contents_pattern = self.contents[pattern_offset:pattern_offset +
                                             len(pattern_data)]
            if contents_pattern != pattern_data:
                raise TpmError('Nvram pattern does not match')

    def get_body_version(self):
        return self.contents[self.version_offset + 1] * 256 + self.contents[
                self.version_offset]

    def get_key_version(self):
        return self.contents[self.version_offset + 3] * 256 + self.contents[
                self.version_offset + 2]


class TpmHandler(object):
    """An object to control TPM device's NVRAM.

    @ivar os_if: an instance of the OS interface (os_interface or a mock object)
    @ivar nvrams: A dictionary where the keys are nvram names, and the values
          are instances of TpmNvRam objects, providing access to the
          appropriate TPM NvRam sections.
    @ivar tpm_version: Either "1.2" or "2.0".
    @type os_if: autotest_lib.client.cros.faft.utils.os_interface.OSInterface
    """

    def __init__(self, os_if):
        self.os_if = os_if
        self.nvrams = {
                'kernel':
                TpmNvRam(
                        self.os_if,
                        addr=KERNEL_NV_ADDRESS,
                        size=13,
                        version_offset=5,
                        data_pattern=(1, [0x4c, 0x57, 0x52, 0x47])),
                'bios':
                TpmNvRam(
                        self.os_if,
                        addr=FW_NV_ADDRESS,
                        size=10,
                        version_offset=2)
        }
        self.tpm_version = None
        self.trunksd_started = False
        self.tcsd_started = False
        self.initialized = False

    def init(self):
        """Initialize the values of the nvram sections.

        This is separate from object creation so it can be done only when a test
        actually uses it.
        """
        self.stop_daemon()
        for nvram in self.nvrams.itervalues():
            nvram.init()
        self.restart_daemon()
        self.initialized = True

    def get_fw_version(self):
        return self.nvrams['bios'].get_body_version()

    def get_fw_key_version(self):
        return self.nvrams['bios'].get_key_version()

    def get_kernel_version(self):
        return self.nvrams['kernel'].get_body_version()

    def get_kernel_key_version(self):
        return self.nvrams['kernel'].get_key_version()

    def get_tpm_version(self):
        if self.tpm_version is None:
            self.tpm_version = self.os_if.run_shell_command_get_output(
                    'tpmc tpmver')[0]
        return self.tpm_version

    def stop_daemon(self):
        """Stop TPM related daemon."""
        if self.trunksd_started or self.tcsd_started:
            raise TpmError('Called stop_daemon() before')

        cmd = 'initctl status tcsd || initctl status trunksd'
        status = self.os_if.run_shell_command_get_output(cmd) or ['']
        # Expected status is like ['trunksd start/running, process 2375']
        self.trunksd_started = status[0].startswith('trunksd start/running')
        if self.trunksd_started:
            self.os_if.run_shell_command('stop trunksd')
        else:
            self.tcsd_started = status[0].startswith('tcsd start/running')
            if self.tcsd_started:
                self.os_if.run_shell_command('stop tcsd')

    def restart_daemon(self):
        """Restart TPM related daemon which was stopped by stop_daemon()."""
        if self.trunksd_started:
            self.os_if.run_shell_command('start trunksd')
            self.trunksd_started = False
        elif self.tcsd_started:
            self.os_if.run_shell_command('start tcsd')
            self.tcsd_started = False
