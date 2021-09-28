# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code to provide functions for FAFT tests.

These will be exposed via an xmlrpc server running on the DUT.

@note: When adding categories, please also update server/cros/faft/rpc_proxy.pyi
"""
import httplib
import os
import sys
import tempfile
import traceback
import xmlrpclib

from autotest_lib.client.common_lib.cros import cros_config
from autotest_lib.client.cros.faft.utils import (
        cgpt_handler,
        os_interface,
        firmware_check_keys,
        firmware_updater,
        flashrom_handler,
        kernel_handler,
        rootfs_handler,
        tpm_handler,
)


class RPCRouter(object):
    """
    A class which routes RPC methods to the proper servicers.

    Firmware tests are able to call an RPC method via:
        FAFTClient.[category].[method_name](params)
    When XML-RPC is being used, the RPC server routes the called method to:
        RPCHandler._dispatch('[category].[method]', params)
    The method is then dispatched to a Servicer class.
    """

    def __init__(self, os_if):
        """Initialize the servicer for each category.

        @type os_if: os_interface.OSInterface
        """
        self.bios = BiosServicer(os_if)
        self.cgpt = CgptServicer(os_if)
        self.ec = EcServicer(os_if)
        self.kernel = KernelServicer(os_if)
        self.rootfs = RootfsServicer(os_if)
        self.rpc_settings = RpcSettingsServicer(os_if)
        self.system = SystemServicer(os_if)
        self.tpm = TpmServicer(os_if)
        self.updater = UpdaterServicer(os_if)

        self._rpc_servicers = {
                'bios': self.bios,
                'cgpt': self.cgpt,
                'ec': self.ec,
                'kernel': self.kernel,
                'rpc_settings': self.rpc_settings,
                'rootfs': self.rootfs,
                'system': self.system,
                'tpm': self.tpm,
                'updater': self.updater
        }

        self._os_if = os_if

    def _report_error(self, fault_code, message, exc_info=None):
        """Raise the given RPC error text, including information about last
        exception from sys.exc_info().  The log file gets the traceback in text;
        the raised exception keeps the old traceback (but not in text).

        Note: this must be called right after the original exception, or it may
        report the wrong exception.

        @raise: xmlrpclib.Fault

        @param fault_code: the status code to use
        @param message: the string message to include before exception text
        @param exc_info: the tuple from sys.exc_info()
        @return the exception to raise

        @type fault_code: int
        @type message: str
        @type exc_info: bool
        @rtype: Exception
        """
        if exc_info:
            tb = None
            try:
                (exc_class, exc, tb) = sys.exc_info()

                tb_str = ''.join(
                        traceback.format_exception(exc_class, exc, tb))
                self._os_if.log('Error: %s.\n%s' % (message, tb_str.rstrip()))

                exc_str = ''.join(
                        traceback.format_exception_only(exc_class, exc))
                exc = xmlrpclib.Fault(
                        fault_code, '%s. %s' % (message, exc_str.rstrip()))
                raise exc, None, tb
            finally:
                del exc_info
                del tb
        else:
            self._os_if.log('Error: %s' % message)
            return xmlrpclib.Fault(fault_code, message)

    def _dispatch(self, called_method, params):
        """
        Send any RPC call to the appropriate servicer method.

        @param called_method: The method of FAFTClient that was called.
                              Should take the form 'category.method'.
        @param params: The arguments passed into the method.

        @type called_method: str
        @type params: tuple

        @raise: xmlrpclib.Fault (using http error codes for fault codes)
        """
        self._os_if.log('Called: %s%s' % (called_method, params))

        name_pieces = called_method.split('.')
        num_pieces = len(name_pieces)

        if num_pieces < 1:
            raise self._report_error(
                    httplib.BAD_REQUEST,
                    'RPC request is invalid (completely empty): "%s"' %
                    called_method)

        if num_pieces < 2:
            # must be category.func (maybe with .__str__)
            raise self._report_error(
                    httplib.BAD_REQUEST,
                    'RPC request is invalid (must have category.method format):'
                    ' "%s"' % called_method)

        method_name = name_pieces.pop()
        category = '.'.join(name_pieces)

        if (method_name.startswith('_')
                and method_name not in ('__str__', '__repr__', '__call__')):
            # *._private() or *.__special__()
            # Forbid early, to prevent seeing which methods exist.
            raise self._report_error(
                    httplib.FORBIDDEN,
                    'RPC method name is private: %s.[%s]' %
                    (category, method_name))

        if not method_name:
            # anything.()
            raise self._report_error(
                    httplib.BAD_REQUEST,
                    'RPC method name is empty: %s.[%s]' %
                    (category, method_name))

        if category in self._rpc_servicers:
            # system.func()
            holder = self._rpc_servicers[category]
            if not hasattr(holder, method_name):
                raise self._report_error(
                        httplib.NOT_FOUND,
                        'RPC method not found: %s.[%s]' %
                        (category, method_name))

        elif category:
            # invalid.func()
            raise self._report_error(
                    httplib.NOT_FOUND,
                    'RPC category not found: [%s].%s' %
                    (category, method_name))

        else:
            # .invalid()
            raise self._report_error(
                    httplib.BAD_REQUEST,
                    'RPC request is invalid (empty category name): [%s].%s' %
                    (category, method_name))

        try:
            method = getattr(holder, method_name)

        except AttributeError:
            raise self._report_error(
                    httplib.NOT_IMPLEMENTED,
                    'RPC method not found: "%s"' % called_method, exc_info=True)

        try:
            return method(*params)

        except Exception:
            raise self._report_error(
                    httplib.INTERNAL_SERVER_ERROR,
                    'RPC call failed: %s()' % called_method, exc_info=True)


class BiosServicer(object):
    """Class to service all BIOS RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if

        # This attribute is accessed via a property, so it can load lazily
        # when actually used by the test.
        self._real_bios_handler = flashrom_handler.FlashromHandler(
                self._os_if, None, '/usr/share/vboot/devkeys', 'bios')

    @property
    def _bios_handler(self):
        """Return the BIOS flashrom handler, after initializing it if necessary

        @rtype: flashrom_handler.FlashromHandler
        """
        if not self._real_bios_handler.initialized:
            self._real_bios_handler.init()
        return self._real_bios_handler

    def reload(self):
        """Reload the firmware image that may be changed."""
        self._bios_handler.new_image()

    def get_gbb_flags(self):
        """Get the GBB flags.

        @return: An integer of the GBB flags.
        """
        return self._bios_handler.get_gbb_flags()

    def set_gbb_flags(self, flags):
        """Set the GBB flags.

        @param flags: An integer of the GBB flags.
        """
        self._bios_handler.set_gbb_flags(flags, write_through=True)

    def get_preamble_flags(self, section):
        """Get the preamble flags of a firmware section.

        @param section: A firmware section, either 'a' or 'b'.
        @return: An integer of the preamble flags.
        """
        return self._bios_handler.get_section_flags(section)

    def set_preamble_flags(self, section, flags):
        """Set the preamble flags of a firmware section.

        @param section: A firmware section, either 'a' or 'b'.
        @param flags: An integer of preamble flags.
        """
        version = self.get_version(section)
        self._bios_handler.set_section_version(
                section, version, flags, write_through=True)

    def get_body_sha(self, section):
        """Get SHA1 hash of BIOS RW firmware section.

        @param section: A firmware section, either 'a' or 'b'.
        @return: A string of the body SHA1 hash.
        """
        return self._bios_handler.get_section_sha(section)

    def get_sig_sha(self, section):
        """Get SHA1 hash of firmware vblock in section.

        @param section: A firmware section, either 'a' or 'b'.
        @return: A string of the sig SHA1 hash.
        """
        return self._bios_handler.get_section_sig_sha(section)

    def get_section_fwid(self, section=None):
        """Retrieve the RO or RW fwid.

        @param section: A firmware section, either 'a' or 'b'.
        @return: A string of the fwid
        """
        return self._bios_handler.get_section_fwid(section)

    def corrupt_sig(self, section):
        """Corrupt the requested firmware section signature.

        @param section: A firmware section, either 'a' or 'b'.
        """
        self._bios_handler.corrupt_firmware(section)

    def restore_sig(self, section):
        """Restore the previously corrupted firmware section signature.

        @param section: A firmware section, either 'a' or 'b'.
        """
        self._bios_handler.restore_firmware(section)

    def corrupt_body(self, section, corrupt_all=False):
        """Corrupt the requested firmware section body.

        @param section: A firmware section, either 'a' or 'b'.
        @param corrupt_all (optional): Corrupt all bytes of the fw section,
                                       rather than just one byte.
        """
        self._bios_handler.corrupt_firmware_body(section, corrupt_all)

    def restore_body(self, section):
        """Restore the previously corrupted firmware section body.

        @param section: A firmware section, either 'a' or 'b'.
        """
        self._bios_handler.restore_firmware_body(section)

    def _modify_version(self, section, delta):
        """Modify firmware version for the requested section, by adding delta.

        The passed in delta, a positive or a negative number, is added to the
        original firmware version.
        """
        original_version = self.get_version(section)
        new_version = original_version + delta
        flags = self._bios_handler.get_section_flags(section)
        self._os_if.log('Setting firmware section %s version from %d to %d' %
                        (section, original_version, new_version))
        self._bios_handler.set_section_version(
                section, new_version, flags, write_through=True)

    def move_version_backward(self, section):
        """Decrement firmware version for the requested section."""
        self._modify_version(section, -1)

    def move_version_forward(self, section):
        """Increase firmware version for the requested section."""
        self._modify_version(section, 1)

    def get_version(self, section):
        """Retrieve firmware version of a section."""
        return self._bios_handler.get_section_version(section)

    def get_datakey_version(self, section):
        """Return firmware data key version."""
        return self._bios_handler.get_section_datakey_version(section)

    def get_kernel_subkey_version(self, section):
        """Return kernel subkey version."""
        return self._bios_handler.get_section_kernel_subkey_version(section)

    def dump_whole(self, bios_path):
        """Dump the current BIOS firmware to a file, specified by bios_path.

        @param bios_path: The path of the BIOS image to be written.
        """
        self._bios_handler.dump_whole(bios_path)

    def write_whole(self, bios_path):
        """Write the firmware from bios_path to the current system.

        @param bios_path: The path of the source BIOS image
        """
        self._bios_handler.new_image(bios_path)
        self._bios_handler.write_whole()

    def strip_modified_fwids(self):
        """Strip any trailing suffixes (from modify_fwids) out of the FWIDs.

        @return: a dict of any fwids that were adjusted, by section (ro, a, b)
        @rtype: dict
        """
        return self._bios_handler.strip_modified_fwids()

    def set_write_protect_region(self, region, enabled=None):
        """Modify software write protect region and flag in one operation.

        @param region: Region to set (usually WP_RO)
        @param enabled: If True, run --wp-enable; if False, run --wp-disable.
                        If None (default), don't specify either one.
        """
        self._bios_handler.set_write_protect_region(region, enabled)

    def set_write_protect_range(self, start, length, enabled=None):
        """Modify software write protect range and flag in one operation.

        @param start: offset (bytes) from start of flash to start of range
        @param length: offset (bytes) from start of range to end of range
        @param enabled: If True, run --wp-enable; if False, run --wp-disable.
                        If None (default), don't specify either one.
        """
        self._bios_handler.set_write_protect_range(start, length, enabled)

    def get_write_protect_status(self):
        """Get a dict describing the status of the write protection

        @return: {'enabled': True/False, 'start': '0x0', 'length': '0x0', ...}
        @rtype: dict
        """
        return self._bios_handler.get_write_protect_status()

    def is_available(self):
        """Return True if available, False if not."""
        # Use the real handler, to avoid .init() raising an exception
        return self._real_bios_handler.is_available()


class CgptServicer(object):
    """Class to service all CGPT RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if
        self._cgpt_handler = cgpt_handler.CgptHandler(self._os_if)

    def get_attributes(self):
        """Get kernel attributes."""
        rootdev = self._os_if.get_root_dev()
        self._cgpt_handler.read_device_info(rootdev)
        return {
                'A': self._cgpt_handler.get_partition(rootdev, 'KERN-A'),
                'B': self._cgpt_handler.get_partition(rootdev, 'KERN-B')
        }

    def set_attributes(self, a=None, b=None):
        """Set kernel attributes for either partition (or both)."""
        partitions = {'A': a, 'B': b}
        rootdev = self._os_if.get_root_dev()
        modifiable_attributes = self._cgpt_handler.ATTR_TO_COMMAND.keys()
        for partition_name in partitions.keys():
            partition = partitions[partition_name]
            if partition is None:
                continue
            attributes_to_set = {
                    key: partition[key]
                    for key in modifiable_attributes
            }
            if attributes_to_set:
                self._cgpt_handler.set_partition(
                        rootdev, 'KERN-%s' % partition_name, attributes_to_set)


class EcServicer(object):
    """Class to service all EC RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if

        # This attribute is accessed via a property, so it can load lazily
        # when actually used by the test.
        self._real_ec_handler = None
        ec_status = self._os_if.run_shell_command_get_status('mosys ec info')
        if ec_status == 0:
            self._real_ec_handler = flashrom_handler.FlashromHandler(
                    self._os_if, 'ec_root_key.vpubk',
                    '/usr/share/vboot/devkeys', 'ec')

        else:
            self._os_if.log('No EC is reported by mosys (rc=%s).' % ec_status)

    @property
    def _ec_handler(self):
        """Return the EC flashrom handler, after initializing it if necessary

        @rtype: flashrom_handler.FlashromHandler
        """
        if not self._real_ec_handler:
            # No EC handler if board has no EC
            return None

        if not self._real_ec_handler.initialized:
            self._real_ec_handler.init()
        return self._real_ec_handler

    def reload(self):
        """Reload the firmware image that may be changed."""
        self._ec_handler.new_image()

    def get_version(self):
        """Get EC version via mosys.

        @return: A string of the EC version.
        """
        return self._os_if.run_shell_command_get_output(
                'mosys ec info | sed "s/.*| //"')[0]

    def get_active_hash(self):
        """Get hash of active EC RW firmware."""
        return self._os_if.run_shell_command_get_output(
                'ectool echash | grep hash: | sed "s/hash:\s\+//"')[0]

    def dump_whole(self, ec_path):
        """Dump the current EC firmware to a file, specified by ec_path.

        @param ec_path: The path of the EC image to be written.
        """
        self._ec_handler.dump_whole(ec_path)

    def write_whole(self, ec_path):
        """Write the firmware from ec_path to the current system.

        @param ec_path: The path of the source EC image.
        """
        self._ec_handler.new_image(ec_path)
        self._ec_handler.write_whole()

    def corrupt_body(self, section):
        """Corrupt the requested EC section body.

        @param section: An EC section, either 'a' or 'b'.
        """
        self._ec_handler.corrupt_firmware_body(section, corrupt_all=True)

    def dump_firmware(self, ec_path):
        """Dump the current EC firmware to a file, specified by ec_path.

        @param ec_path: The path of the EC image to be written.
        """
        self._ec_handler.dump_whole(ec_path)

    def set_write_protect(self, enable):
        """Enable write protect of the EC flash chip.

        @param enable: True if activating EC write protect. Otherwise, False.
        """
        if enable:
            self._ec_handler.enable_write_protect()
        else:
            self._ec_handler.disable_write_protect()

    def is_efs(self):
        """Return True if the EC supports EFS."""
        return self._ec_handler.has_section_body('rw_b')

    def copy_rw(self, from_section, to_section):
        """Copy EC RW from from_section to to_section."""
        self._ec_handler.copy_from_to(from_section, to_section)

    def reboot_to_switch_slot(self):
        """Reboot EC to switch the active RW slot."""
        self._os_if.run_shell_command(
                'ectool reboot_ec cold switch-slot', modifies_device=True)

    def strip_modified_fwids(self):
        """Strip any trailing suffixes (from modify_fwids) out of the FWIDs."""
        return self._ec_handler.strip_modified_fwids()


class KernelServicer(object):
    """Class to service all Kernel RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if
        self._kernel_handler = kernel_handler.KernelHandler()
        self._kernel_handler.init(
                self._os_if,
                dev_key_path='/usr/share/vboot/devkeys',
                internal_disk=True)

    def corrupt_sig(self, section):
        """Corrupt the requested kernel section.

        @param section: A kernel section, either 'a' or 'b'.
        """
        self._kernel_handler.corrupt_kernel(section)

    def restore_sig(self, section):
        """Restore the requested kernel section (previously corrupted).

        @param section: A kernel section, either 'a' or 'b'.
        """
        self._kernel_handler.restore_kernel(section)

    def _modify_version(self, section, delta):
        """Modify kernel version for the requested section, by adding delta.

        The passed in delta, a positive or a negative number, is added to the
        original kernel version.
        """
        original_version = self._kernel_handler.get_version(section)
        new_version = original_version + delta
        self._os_if.log('Setting kernel section %s version from %d to %d' %
                        (section, original_version, new_version))
        self._kernel_handler.set_version(section, new_version)

    def move_version_backward(self, section):
        """Decrement kernel version for the requested section."""
        self._modify_version(section, -1)

    def move_version_forward(self, section):
        """Increase kernel version for the requested section."""
        self._modify_version(section, 1)

    def get_version(self, section):
        """Return kernel version."""
        return self._kernel_handler.get_version(section)

    def get_datakey_version(self, section):
        """Return kernel datakey version."""
        return self._kernel_handler.get_datakey_version(section)

    def diff_a_b(self):
        """Compare kernel A with B.

        @return: True: if kernel A is different with B.
                 False: if kernel A is the same as B.
        """
        rootdev = self._os_if.get_root_dev()
        kernel_a = self._os_if.join_part(rootdev, '2')
        kernel_b = self._os_if.join_part(rootdev, '4')

        # The signature (some kind of hash) for the kernel body is stored in
        # the beginning. So compare the first 64KB (including header, preamble,
        # and signature) should be enough to check them identical.
        header_a = self._os_if.read_partition(kernel_a, 0x10000)
        header_b = self._os_if.read_partition(kernel_b, 0x10000)

        return header_a != header_b

    def resign_with_keys(self, section, key_path=None):
        """Resign kernel with temporary key."""
        self._kernel_handler.resign_kernel(section, key_path)

    def dump(self, section, kernel_path):
        """Dump the specified kernel to a file.

        @param section: The kernel to dump. May be A or B.
        @param kernel_path: The path to the kernel image to be written.
        """
        self._kernel_handler.dump_kernel(section, kernel_path)

    def write(self, section, kernel_path):
        """Write a kernel image to the specified section.

        @param section: The kernel to dump. May be A or B.
        @param kernel_path: The path to the kernel image.
        """
        self._kernel_handler.write_kernel(section, kernel_path)

    def get_sha(self, section):
        """Return the SHA1 hash of the specified kernel section."""
        return self._kernel_handler.get_sha(section)


class RootfsServicer(object):
    """Class to service all RootFS RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if
        self._rootfs_handler = rootfs_handler.RootfsHandler()
        self._rootfs_handler.init(self._os_if)

    def verify_rootfs(self, section):
        """Verifies the integrity of the root FS.

        @param section: The rootfs to verify. May be A or B.
        """
        return self._rootfs_handler.verify_rootfs(section)


class RpcSettingsServicer(object):
    """Class to service RPCs for settings of the RPC server itself"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if

    def enable_test_mode(self):
        """Enable test mode (avoids writing to flash or gpt)"""
        self._os_if.test_mode = True

    def disable_test_mode(self):
        """Disable test mode and return to normal operation"""
        self._os_if.test_mode = False


class SystemServicer(object):
    """Class to service all System RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if
        self._key_checker = firmware_check_keys.firmwareCheckKeys()

    def is_available(self):
        """Function for polling the RPC server availability.

        @return: Always True.
        """
        return True

    def dump_log(self, remove_log=False):
        """Dump the log file.

        @param remove_log: Remove the log file after dump.
        @return: String of the log file content.
        """
        with open(self._os_if.log_file) as f:
            log = f.read()
        if remove_log:
            os.remove(self._os_if.log_file)
        return log

    def run_shell_command(self, command, block=True):
        """Run shell command.

        @param command: A shell command to be run.
        @param block: if True (default), wait for command to finish
        """
        self._os_if.run_shell_command(command, block=block)

    def run_shell_command_check_output(self, command, success_token):
        """Run shell command and check its stdout for a string.

        @param command: A shell command to be run.
        @param success_token: A string to search the output for.
        @return: A Boolean indicating whether the success_token was found in
                the command output.
        """
        return self._os_if.run_shell_command_check_output(
                command, success_token)

    def run_shell_command_get_output(self, command, include_stderr=False):
        """Run shell command and get its console output.

        @param command: A shell command to be run.
        @return: A list of strings stripped of the newline characters.
        """
        return self._os_if.run_shell_command_get_output(command, include_stderr)

    def run_shell_command_get_status(self, command):
        """Run shell command and get its console status.

        @param command: A shell command to be run.
        @return: The returncode of the process
        @rtype: int
        """
        return self._os_if.run_shell_command_get_status(command)

    def get_platform_name(self):
        """Get the platform name of the current system.

        @return: A string of the platform name.
        """
        platform = cros_config.call_cros_config_get_output(
                '/identity platform-name',
                self._os_if.run_shell_command_get_result)
        if not platform:
            raise Exception('Failed getting platform name from cros_config')
        return platform

    def get_model_name(self):
        """Get the model name of the current system.

        @return: A string of the model name.
        """
        model = cros_config.call_cros_config_get_output(
                '/ name', self._os_if.run_shell_command_get_result)
        if not model:
            raise Exception('Failed getting model name from cros_config')
        return model

    def dev_tpm_present(self):
        """Check if /dev/tpm0 is present.

        @return: Boolean true or false.
        """
        return os.path.exists('/dev/tpm0')

    def get_crossystem_value(self, key):
        """Get crossystem value of the requested key.

        @param key: A crossystem key.
        @return: A string of the requested crossystem value.
        """
        return self._os_if.run_shell_command_get_output(
                'crossystem %s' % key)[0]

    def get_root_dev(self):
        """Get the name of root device without partition number.

        @return: A string of the root device without partition number.
        """
        return self._os_if.get_root_dev()

    def get_root_part(self):
        """Get the name of root device with partition number.

        @return: A string of the root device with partition number.
        """
        return self._os_if.get_root_part()

    def set_try_fw_b(self, count=1):
        """Set 'Try Firmware B' flag in crossystem.

        @param count: # times to try booting into FW B
        """
        self._os_if.cs.fwb_tries = count

    def set_fw_try_next(self, next, count=0):
        """Set fw_try_next to A or B.

        @param next: Next FW to reboot to (A or B)
        @param count: # of times to try booting into FW <next>
        """
        self._os_if.cs.fw_try_next = next
        if count:
            self._os_if.cs.fw_try_count = count

    def get_fw_vboot2(self):
        """Get fw_vboot2."""
        try:
            return self._os_if.cs.fw_vboot2 == '1'
        except os_interface.OSInterfaceError:
            return False

    def request_recovery_boot(self):
        """Request running in recovery mode on the restart."""
        self._os_if.cs.request_recovery()

    def get_dev_boot_usb(self):
        """Get dev_boot_usb value which controls developer mode boot from USB.

        @return: True if enable, False if disable.
        """
        return self._os_if.cs.dev_boot_usb == '1'

    def set_dev_boot_usb(self, value):
        """Set dev_boot_usb value which controls developer mode boot from USB.

        @param value: True to enable, False to disable.
        """
        self._os_if.cs.dev_boot_usb = 1 if value else 0

    def is_removable_device_boot(self):
        """Check the current boot device is removable.

        @return: True: if a removable device boots.
                 False: if a non-removable device boots.
        """
        root_part = self._os_if.get_root_part()
        return self._os_if.is_removable_device(root_part)

    def get_internal_device(self):
        """Get the internal disk by given the current disk."""
        root_part = self._os_if.get_root_part()
        return self._os_if.get_internal_disk(root_part)

    def create_temp_dir(self, prefix='backup_'):
        """Create a temporary directory and return the path."""
        return tempfile.mkdtemp(prefix=prefix)

    def remove_file(self, file_path):
        """Remove the file."""
        return self._os_if.remove_file(file_path)

    def remove_dir(self, dir_path):
        """Remove the directory."""
        return self._os_if.remove_dir(dir_path)

    def check_keys(self, expected_sequence):
        """Check the keys sequence was as expected.

        @param expected_sequence: A list of expected key sequences.
        """
        return self._key_checker.check_keys(expected_sequence)


class TpmServicer(object):
    """Class to service all TPM RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if

        # This attribute is accessed via a property, so it can load lazily
        # when actually used by the test.
        self._real_tpm_handler = tpm_handler.TpmHandler(self._os_if)

    @property
    def _tpm_handler(self):
        """Handler for the TPM

        @rtype: tpm_handler.TpmHandler
        """
        if not self._real_tpm_handler.initialized:
            self._real_tpm_handler.init()
        return self._real_tpm_handler

    def get_firmware_version(self):
        """Retrieve tpm firmware body version."""
        return self._tpm_handler.get_fw_version()

    def get_firmware_datakey_version(self):
        """Retrieve tpm firmware data key version."""
        return self._tpm_handler.get_fw_key_version()

    def get_kernel_version(self):
        """Retrieve tpm kernel body version."""
        return self._tpm_handler.get_kernel_version()

    def get_kernel_datakey_version(self):
        """Retrieve tpm kernel data key version."""
        return self._tpm_handler.get_kernel_key_version()

    def get_tpm_version(self):
        """Returns '1.2' or '2.0' as a string."""
        # tpmc can return this without stopping daemons, so access real handler.
        return self._real_tpm_handler.get_tpm_version()

    def stop_daemon(self):
        """Stop tpm related daemon."""
        return self._tpm_handler.stop_daemon()

    def restart_daemon(self):
        """Restart tpm related daemon which was stopped by stop_daemon()."""
        return self._tpm_handler.restart_daemon()


class UpdaterServicer(object):
    """Class to service all Updater RPCs"""

    def __init__(self, os_if):
        """
        @type os_if: os_interface.OSInterface
        """
        self._os_if = os_if
        self._real_updater = firmware_updater.FirmwareUpdater(self._os_if)

    @property
    def _updater(self):
        """Handler for the updater

        @rtype: firmware_updater.FirmwareUpdater
        """
        if not self._real_updater.initialized:
            self._real_updater.init()
        return self._real_updater

    def cleanup(self):
        """Clean up the temporary directory"""
        # Use the updater directly, to avoid initializing it just to clean it up
        self._real_updater.cleanup_temp_dir()

    def stop_daemon(self):
        """Stop update-engine daemon."""
        return self._real_updater.stop_daemon()

    def start_daemon(self):
        """Start update-engine daemon."""
        return self._real_updater.start_daemon()

    def get_section_fwid(self, target='bios', section=None):
        """Retrieve shellball's RW or RO fwid."""
        return self._updater.get_section_fwid(target, section)

    def get_all_fwids(self, target='bios'):
        """Retrieve shellball's RW and/or RO fwids for all sections."""
        return self._updater.get_all_fwids(target)

    def get_all_installed_fwids(self, target='bios', filename=None):
        """Retrieve installed (possibly emulated) fwids for the target."""
        return self._updater.get_all_installed_fwids(target, filename)

    def modify_fwids(self, target='bios', sections=None):
        """Modify the fwid in the image, but don't flash it."""
        return self._updater.modify_fwids(target, sections)

    def modify_ecid_and_flash_to_bios(self):
        """Modify ecid, put it to AP firmware, and flash it to the system."""
        self._updater.modify_ecid_and_flash_to_bios()

    def corrupt_diagnostics_image(self, local_filename):
        """Corrupts a diagnostics image in the CBFS working directory.

        @param local_filename: Filename for storing the diagnostics image in the
            CBFS working directory
        """
        self._updater.corrupt_diagnostics_image(local_filename)

    def get_ec_hash(self):
        """Return the hex string of the EC hash."""
        blob = self._updater.get_ec_hash()
        # Format it to a hex string
        return ''.join('%02x' % ord(c) for c in blob)

    def resign_firmware(self, version):
        """Resign firmware with version.

        @param version: new version number.
        """
        self._updater.resign_firmware(version)

    def extract_shellball(self, append=None):
        """Extract shellball with the given append suffix.

        @param append: use for the shellball name.
        """
        return self._updater.extract_shellball(append)

    def repack_shellball(self, append=None):
        """Repack shellball with new fwid.

        @param append: use for the shellball name.
        """
        return self._updater.repack_shellball(append)

    def reset_shellball(self):
        """Revert to the stock shellball"""
        self._updater.reset_shellball()

    def reload_images(self):
        """Reload handlers from the on-disk images, in case they've changed."""
        self._updater.reload_images()

    def run_firmwareupdate(self, mode, append=None, options=()):
        """Run updater with the given options

        @param mode: mode for the updater
        @param append: extra string appended to shellball filename to run
        @param options: options for chromeos-firmwareupdate
        @return: returncode of the updater
        @rtype: int
        """
        return self._updater.run_firmwareupdate(mode, append, options)

    def run_autoupdate(self, append):
        """Run chromeos-firmwareupdate with autoupdate mode."""
        options = ['--noupdate_ec', '--wp=1']
        self._updater.run_firmwareupdate(
                mode='autoupdate', append=append, options=options)

    def run_factory_install(self):
        """Run chromeos-firmwareupdate with factory_install mode."""
        options = ['--noupdate_ec', '--wp=0']
        self._updater.run_firmwareupdate(
                mode='factory_install', options=options)

    def run_bootok(self, append):
        """Run chromeos-firmwareupdate with bootok mode."""
        self._updater.run_firmwareupdate(mode='bootok', append=append)

    def run_recovery(self):
        """Run chromeos-firmwareupdate with recovery mode."""
        options = ['--noupdate_ec', '--nocheck_keys', '--force', '--wp=1']
        self._updater.run_firmwareupdate(mode='recovery', options=options)

    def cbfs_setup_work_dir(self):
        """Sets up cbfstool work directory."""
        return self._updater.cbfs_setup_work_dir()

    def cbfs_extract_chip(self, fw_name):
        """Runs cbfstool to extract chip firmware.

        @param fw_name: Name of chip firmware to extract.
        @return: Boolean success status.
        """
        return self._updater.cbfs_extract_chip(fw_name)

    def cbfs_extract_diagnostics(self, diag_name, local_filename):
        """Runs cbfstool to extract a diagnostics image.

        @param diag_name: Name of the diagnostics image in CBFS
        @param local_filename: Filename for storing the diagnostics image in the
            CBFS working directory
        """
        self._updater.cbfs_extract_diagnostics(diag_name, local_filename)

    def cbfs_replace_diagnostics(self, diag_name, local_filename):
        """Runs cbfstool to replace a diagnostics image in the firmware image.

        @param diag_name: Name of the diagnostics image in CBFS
        @param local_filename: Filename for storing the diagnostics image in the
            CBFS working directory
        """
        self._updater.cbfs_replace_diagnostics(diag_name, local_filename)

    def cbfs_get_chip_hash(self, fw_name):
        """Gets the chip firmware hash blob.

        @param fw_name: Name of chip firmware whose hash blob to return.
        @return: Hex string of hash blob.
        """
        return self._updater.cbfs_get_chip_hash(fw_name)

    def cbfs_replace_chip(self, fw_name):
        """Runs cbfstool to replace chip firmware.

        @param fw_name: Name of chip firmware to extract.
        @return: Boolean success status.
        """
        return self._updater.cbfs_replace_chip(fw_name)

    def cbfs_sign_and_flash(self):
        """Runs cbfs signer and flash it.

        @param fw_name: Name of chip firmware to extract.
        @return: Boolean success status.
        """
        return self._updater.cbfs_sign_and_flash()

    def get_temp_path(self):
        """Get updater's temp directory path."""
        return self._updater.get_temp_path()

    def get_keys_path(self):
        """Get updater's keys directory path."""
        return self._updater.get_keys_path()

    def get_work_path(self):
        """Get updater's work directory path."""
        return self._updater.get_work_path()

    def get_bios_relative_path(self):
        """Gets the relative path of the bios image in the shellball."""
        return self._updater.get_bios_relative_path()

    def get_ec_relative_path(self):
        """Gets the relative path of the ec image in the shellball."""
        return self._updater.get_ec_relative_path()

    def copy_bios(self, filename):
        """Make a copy of the shellball bios.bin"""
        return self._updater.copy_bios(filename)

    def get_image_gbb_flags(self, filename=None):
        """Get the GBB flags in the given image (shellball image if unspecified)

        @param filename: the image path to act on (None to use shellball image)
        @return: An integer of the GBB flags.
        """
        return self._updater.get_image_gbb_flags(filename)

    def set_image_gbb_flags(self, flags, filename=None):
        """Set the GBB flags in the given image (shellball image if unspecified)

        @param flags: the flags to set
        @param filename: the image path to act on (None to use shellball image)

        @type flags: int
        @type filename: str | None
        """
        return self._updater.set_image_gbb_flags(flags, filename)
