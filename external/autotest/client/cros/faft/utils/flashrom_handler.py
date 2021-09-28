#!/usr/bin/python2
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A module to support automated testing of ChromeOS firmware.

Utilizes services provided by saft_flashrom_util.py read/write the
flashrom chip and to parse the flash rom image.

See docstring for FlashromHandler class below.
"""

import hashlib
import os
import struct
import tempfile

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chip_utils
from autotest_lib.client.cros.faft.utils import saft_flashrom_util


class FvSection(object):
    """An object to hold information about a firmware section.

    This includes file names for the signature header and the body, and the
    version number.
    """

    def __init__(self, sig_name, body_name, fwid_name=None):
        """
        @param sig_name: name of signature section in fmap
        @param body_name: name of body section in fmap
        @param fwid_name: name of fwid section in fmap
        @type sig_name: str | None
        @type body_name: str | None
        @type fwid_name: str | None
        """
        self._sig_name = sig_name
        self._body_name = body_name
        self._fwid_name = fwid_name
        self._version = -1  # Is not set on construction.
        self._flags = 0  # Is not set on construction.
        self._sha = None  # Is not set on construction.
        self._sig_sha = None  # Is not set on construction.
        self._datakey_version = -1  # Is not set on construction.
        self._kernel_subkey_version = -1  # Is not set on construction.

    def names(self):
        """Return the desired file names for the signature, body, and fwid."""
        return (self._sig_name, self._body_name, self._fwid_name)

    def get_sig_name(self):
        return self._sig_name

    def get_body_name(self):
        return self._body_name

    def get_fwid_name(self):
        return self._fwid_name

    def get_version(self):
        return self._version

    def get_flags(self):
        return self._flags

    def get_sha(self):
        return self._sha

    def get_sig_sha(self):
        return self._sig_sha

    def get_datakey_version(self):
        return self._datakey_version

    def get_kernel_subkey_version(self):
        return self._kernel_subkey_version

    def set_version(self, version):
        self._version = version

    def set_flags(self, flags):
        self._flags = flags

    def set_sha(self, sha):
        self._sha = sha

    def set_sig_sha(self, sha):
        self._sig_sha = sha

    def set_datakey_version(self, version):
        self._datakey_version = version

    def set_kernel_subkey_version(self, version):
        self._kernel_subkey_version = version


class FlashromHandlerError(Exception):
    """An object to represent Flashrom errors"""
    pass


class FlashromHandler(object):
    """An object to provide logical services for automated flashrom testing."""

    DELTA = 1  # value to add to a byte to corrupt a section contents

    # File in the state directory to store public root key.
    PUB_KEY_FILE_NAME = 'root.pubkey'
    FW_KEYBLOCK_FILE_NAME = 'firmware.keyblock'
    FW_PRIV_DATA_KEY_FILE_NAME = 'firmware_data_key.vbprivk'
    KERNEL_SUBKEY_FILE_NAME = 'kernel_subkey.vbpubk'
    EC_EFS_KEY_FILE_NAME = 'key_ec_efs.vbprik2'
    FWID_MOD_DELIMITER = '~'

    def __init__(
            self,
            os_if,
            pub_key_file=None,
            dev_key_path='./',
            target='bios',
            subdir=None
    ):
        """The flashrom handler is not fully initialized upon creation

        @param os_if: an object providing interface to OS services
        @param pub_key_file: the name of the file contaning a public key to
                             use for verifying both existing and new firmware.
        @param dev_key_path: path to directory containing *.vpubk and *.vbprivk
                             files, for use in signing
        @param target: flashrom target ('bios' or 'ec')
        @param subdir: name of subdirectory of state dir, to use for sections
                    Default: same as target, resulting in
                    '/usr/local/tmp/faft/bios'
        @type os_if: client.cros.faft.utils.os_interface.OSInterface
        @type pub_key_file: str | None
        @type dev_key_path: str
        @type target: str
        """
        self.fum = None
        self.image = ''
        self.os_if = os_if
        self.initialized = False
        self._available = None
        self._unavailable_err = None

        if subdir is None:
            subdir = target
        self.subdir = subdir

        self.pub_key_file = pub_key_file
        self.dev_key_path = dev_key_path

        self.target = target
        if self.target == 'bios':
            self.fum = saft_flashrom_util.flashrom_util(
                    self.os_if, target_is_ec=False)
            self.fv_sections = {
                    'ro': FvSection(None, None, 'RO_FRID'),
                    'a': FvSection('VBOOTA', 'FVMAIN', 'RW_FWID_A'),
                    'b': FvSection('VBOOTB', 'FVMAINB', 'RW_FWID_B'),
                    'rec': FvSection(None, 'RECOVERY_MRC_CACHE'),
                    'ec_a': FvSection(None, 'ECMAINA'),
                    'ec_b': FvSection(None, 'ECMAINB'),
                    'rw_legacy': FvSection(None, 'RW_LEGACY'),
            }
        elif self.target == 'ec':
            self.fum = saft_flashrom_util.flashrom_util(
                    self.os_if, target_is_ec=True)
            self.fv_sections = {
                    'ro': FvSection(None, None, 'RO_FRID'),
                    'rw': FvSection(None, 'EC_RW', 'RW_FWID'),
                    'rw_b': FvSection(None, 'EC_RW_B'),
            }
        else:
            raise FlashromHandlerError("Invalid target.")

    def is_available(self):
        """Check if the programmer is available, by specifying no commands.

        @rtype: bool
        """
        if self._available is None:
            # Cache the status to avoid trying flashrom every time.
            try:
                self.fum.check_target()
                self._available = True
            except error.CmdError as e:
                # First line: "Command <flashrom -p host> failed, rc=2"
                self._unavailable_err = str(e).split('\n', 1)[0]
                self._available = False
        return self._available

    def section_file(self, *paths):
        """
        Return a full path for the given basename, in this handler's subdir.
        Example: subdir 'bios' -> '/usr/local/tmp/faft/bios/FV_GBB'

        @param paths: variable number of path pieces, same as in os.path.join
        @return: an absolute path from this handler's subdir and the pieces.
        """
        if any(os.path.isabs(x) for x in paths):
            raise FlashromHandlerError(
                    "Absolute paths are not allowed in section_file()")

        return os.path.join(self.os_if.state_dir, self.subdir, *paths)

    def init(self, image_file=None, allow_fallback=False):
        """Initialize the object, by reading the image.

        This is separate from new_image, to isolate the implementation detail of
        self.image being non-empty.

        @param image_file: the path of the image file to read.
                If None or empty string, read the flash device instead.
        @param allow_fallback: if True, fall back to reading the flash device
                if the image file doesn't exist.
        @type image_file: str
        @type allow_fallback: bool

        @raise FlashromHandlerError: if no target flash device was usable.
        """
        # Raise an exception early if there's no usable flash.
        if not self.is_available():
            # Can't tell for sure whether it's broken or simply nonexistent.
            raise FlashromHandlerError(
                    "Could not detect a usable %s flash device: %s."
                    % (self.target, self._unavailable_err))

        if image_file and allow_fallback and not os.path.isfile(image_file):
            self.os_if.log(
                    "Using %s flash contents instead of missing image: %s"
                    % (self.target.upper(), image_file))
            image_file = None

        self.new_image(image_file)
        self.initialized = True

    def deinit(self):
        """Clear the in-memory image data, and mark self uninitialized."""
        self.image = ''
        self.os_if.remove_dir(self.section_file())
        self.initialized = False

    def dump_flash(self, target_filename):
        """Copy the flash device's data into a file, but don't parse it.

        @param target_filename: the file to create
        """
        self.fum.dump_flash(target_filename)

    def new_image(self, image_file=None):
        """Parse the full flashrom image and store sections into files.

        @param image_file: the name of the file containing a full ChromeOS
                       flashrom image. If not passed in or empty, the actual
                       flash device is read and its contents are saved into a
                       temporary file which is used instead.
        @type image_file: str | None

        The input file is parsed and the sections of importance (as defined in
        self.fv_sections) are saved in separate files in the state directory
        as defined in the os_if object.
        """

        if image_file:
            with open(image_file, 'rb') as image_f:
                self.image = image_f.read()
            self.fum.set_firmware_layout(image_file)
        else:
            self.image = self.fum.read_whole()

        self.os_if.create_dir(self.section_file())

        for section in self.fv_sections.itervalues():
            for subsection_name in section.names():
                if not subsection_name:
                    continue
                blob = self.fum.get_section(self.image, subsection_name)
                if blob:
                    blob_filename = self.section_file(subsection_name)
                    with open(blob_filename, 'wb') as blob_f:
                        blob_f.write(blob)

            blob = self.fum.get_section(self.image, section.get_body_name())
            if blob:
                s = hashlib.sha1()
                s.update(blob)
                section.set_sha(s.hexdigest())

            # If there is no "sig" subsection, skip reading version and flags.
            if not section.get_sig_name():
                continue

            # Now determine this section's version number.
            vb_section = self.fum.get_section(self.image,
                                              section.get_sig_name())

            section.set_version(self.os_if.retrieve_body_version(vb_section))
            section.set_flags(self.os_if.retrieve_preamble_flags(vb_section))
            section.set_datakey_version(
                    self.os_if.retrieve_datakey_version(vb_section))
            section.set_kernel_subkey_version(
                    self.os_if.retrieve_kernel_subkey_version(vb_section))

            s = hashlib.sha1()
            s.update(self.fum.get_section(self.image, section.get_sig_name()))
            section.set_sig_sha(s.hexdigest())

        if not self.pub_key_file:
            self._retrieve_pub_key()

    def _retrieve_pub_key(self):
        """Retrieve root public key from the firmware GBB section."""

        gbb_header_format = '<4s20s2I'
        pubk_header_format = '<2Q'

        gbb_section = self.fum.get_section(self.image, 'FV_GBB')

        # do some sanity checks
        try:
            sig, _, rootk_offs, rootk_size = struct.unpack_from(
                    gbb_header_format, gbb_section)
        except struct.error, e:
            raise FlashromHandlerError(e)

        if sig != '$GBB' or (rootk_offs + rootk_size) > len(gbb_section):
            raise FlashromHandlerError('Bad gbb header')

        key_body_offset, key_body_size = struct.unpack_from(
                pubk_header_format, gbb_section, rootk_offs)

        # Generally speaking the offset field can be anything, but in case of
        # GBB section the key is stored as a standalone entity, so the offset
        # of the key body is expected to be equal to the key header size of
        # 0x20.
        # Should this convention change, the check below would fail, which
        # would be a good prompt for revisiting this test's behavior and
        # algorithms.
        if key_body_offset != 0x20 or key_body_size > rootk_size:
            raise FlashromHandlerError('Bad public key format')

        # All checks passed, let's store the key in a file.
        self.pub_key_file = self.os_if.state_dir_file(self.PUB_KEY_FILE_NAME)
        with open(self.pub_key_file, 'w') as key_f:
            key = gbb_section[rootk_offs:rootk_offs + key_body_offset +
                              key_body_size]
            key_f.write(key)

    def verify_image(self):
        """Confirm the image's validity.

        Using the file supplied to init() as the public key container verify
        the two sections' (FirmwareA and FirmwareB) integrity. The contents of
        the sections is taken from the files created by new_image()

        In case there is an integrity error raises FlashromHandlerError
        exception with the appropriate error message text.
        """

        for section in self.fv_sections.itervalues():
            if section.get_sig_name():
                cmd = 'vbutil_firmware --verify %s --signpubkey %s  --fv %s' % (
                        self.section_file(section.get_sig_name()),
                        self.pub_key_file,
                        self.section_file(section.get_body_name()))
                self.os_if.run_shell_command(cmd)

    def _modify_section(self,
                        section,
                        delta,
                        body_or_sig=False,
                        corrupt_all=False):
        """Modify a firmware section inside the image, either body or signature.

        If corrupt_all is set, the passed in delta is added to all bytes in the
        section. Otherwise, the delta is added to the value located at 2% offset
        into the section blob, either body or signature.

        Calling this function again for the same section the complimentary
        delta value would restore the section contents.
        """

        if not self.image:
            raise FlashromHandlerError(
                    'Attempt at using an uninitialized object')
        if section not in self.fv_sections:
            raise FlashromHandlerError('Unknown FW section %s' % section)

        # Get the appropriate section of the image.
        if body_or_sig:
            subsection_name = self.fv_sections[section].get_body_name()
        else:
            subsection_name = self.fv_sections[section].get_sig_name()
        blob = self.fum.get_section(self.image, subsection_name)

        # Modify the byte in it within 2% of the section blob.
        modified_index = len(blob) / 50
        if corrupt_all:
            blob_list = [('%c' % ((ord(x) + delta) % 0x100)) for x in blob]
        else:
            blob_list = list(blob)
            blob_list[modified_index] = (
                    '%c' % ((ord(blob[modified_index]) + delta) % 0x100))
        self.image = self.fum.put_section(self.image, subsection_name,
                                          ''.join(blob_list))

        return subsection_name

    def corrupt_section(self, section, corrupt_all=False):
        """Corrupt a section signature of the image"""

        return self._modify_section(
                section,
                self.DELTA,
                body_or_sig=False,
                corrupt_all=corrupt_all)

    def corrupt_section_body(self, section, corrupt_all=False):
        """Corrupt a section body of the image"""

        return self._modify_section(
                section, self.DELTA, body_or_sig=True, corrupt_all=corrupt_all)

    def restore_section(self, section, restore_all=False):
        """Restore a previously corrupted section signature of the image."""

        return self._modify_section(
                section,
                -self.DELTA,
                body_or_sig=False,
                corrupt_all=restore_all)

    def restore_section_body(self, section, restore_all=False):
        """Restore a previously corrupted section body of the image."""

        return self._modify_section(
                section,
                -self.DELTA,
                body_or_sig=True,
                corrupt_all=restore_all)

    def corrupt_firmware(self, section, corrupt_all=False):
        """Corrupt a section signature in the FLASHROM!!!"""

        subsection_name = self.corrupt_section(
                section, corrupt_all=corrupt_all)
        self.fum.write_partial(self.image, (subsection_name, ))

    def corrupt_firmware_body(self, section, corrupt_all=False):
        """Corrupt a section body in the FLASHROM!!!"""

        subsection_name = self.corrupt_section_body(
                section, corrupt_all=corrupt_all)
        self.fum.write_partial(self.image, (subsection_name, ))

    def restore_firmware(self, section, restore_all=False):
        """Restore the previously corrupted section sig in the FLASHROM!!!"""

        subsection_name = self.restore_section(
                section, restore_all=restore_all)
        self.fum.write_partial(self.image, (subsection_name, ))

    def restore_firmware_body(self, section, restore_all=False):
        """Restore the previously corrupted section body in the FLASHROM!!!"""

        subsection_name = self.restore_section_body(section, restore_all=False)
        self.fum.write_partial(self.image, (subsection_name, ))

    def firmware_sections_equal(self):
        """Check if firmware sections A and B are equal.

        This function presumes that the entire BIOS image integrity has been
        verified, so different signature sections mean different images and
        vice versa.
        """
        sig_a = self.fum.get_section(self.image,
                                     self.fv_sections['a'].get_sig_name())
        sig_b = self.fum.get_section(self.image,
                                     self.fv_sections['b'].get_sig_name())
        return sig_a == sig_b

    def copy_from_to(self, src, dst):
        """Copy one firmware image section to another.

        This function copies both signature and body of one firmware section
        into another. After this function runs both sections are identical.
        """
        src_sect = self.fv_sections[src]
        dst_sect = self.fv_sections[dst]
        self.image = self.fum.put_section(
                self.image, dst_sect.get_body_name(),
                self.fum.get_section(self.image, src_sect.get_body_name()))
        # If there is no "sig" subsection, skip copying signature.
        if src_sect.get_sig_name() and dst_sect.get_sig_name():
            self.image = self.fum.put_section(
                    self.image, dst_sect.get_sig_name(),
                    self.fum.get_section(self.image, src_sect.get_sig_name()))
        self.write_whole()

    def write_whole(self):
        """Write the whole image into the flashrom."""

        if not self.image:
            raise FlashromHandlerError(
                    'Attempt at using an uninitialized object')
        self.fum.write_whole(self.image)

    def write_partial(self, subsection_name, blob=None, write_through=True):
        """Write the subsection part into the flashrom.

        One can pass a blob to update the data of the subsection before write
        it into the flashrom.
        """

        if not self.image:
            raise FlashromHandlerError(
                    'Attempt at using an uninitialized object')

        if blob is not None:
            self.image = self.fum.put_section(self.image, subsection_name,
                                              blob)

        if write_through:
            self.dump_partial(
                    subsection_name, self.section_file(subsection_name))
            self.fum.write_partial(self.image, (subsection_name, ))

    def dump_whole(self, filename):
        """Write the whole image into a file."""

        if not self.image:
            raise FlashromHandlerError(
                    'Attempt at using an uninitialized object')
        open(filename, 'w').write(self.image)

    def dump_partial(self, subsection_name, filename):
        """Write the subsection part into a file."""

        if not self.image:
            raise FlashromHandlerError(
                    'Attempt at using an uninitialized object')
        blob = self.fum.get_section(self.image, subsection_name)
        open(filename, 'w').write(blob)

    def dump_section_body(self, section, filename):
        """Write the body of a firmware section into a file"""
        subsection_name = self.fv_sections[section].get_body_name()
        self.dump_partial(subsection_name, filename)

    def get_section_hash(self, section):
        """Retrieve the hash of the body of a firmware section"""
        ecrw = chip_utils.ecrw()

        # add a dot to avoid set_from_file breaking if tmpname has an underscore
        with tempfile.NamedTemporaryFile(prefix=ecrw.chip_name + '.') as f:
            self.dump_section_body(section, f.name)
            ecrw.set_from_file(f.name)
            result = ecrw.compute_hash_bytes()
        return result

    def get_gbb_flags(self):
        """Retrieve the GBB flags"""
        gbb_header_format = '<12sL'
        gbb_section = self.fum.get_section(self.image, 'FV_GBB')
        try:
            _, gbb_flags = struct.unpack_from(gbb_header_format, gbb_section)
        except struct.error, e:
            raise FlashromHandlerError(e)
        return gbb_flags

    def set_gbb_flags(self, flags, write_through=False):
        """Retrieve the GBB flags"""
        gbb_header_format = '<L'
        section_name = 'FV_GBB'
        gbb_section = self.fum.get_section(self.image, section_name)
        try:
            formatted_flags = struct.pack(gbb_header_format, flags)
        except struct.error, e:
            raise FlashromHandlerError(e)
        gbb_section = gbb_section[:12] + formatted_flags + gbb_section[16:]
        self.write_partial(section_name, gbb_section, write_through)

    def enable_write_protect(self):
        """Enable write protect of the flash chip"""
        self.fum.enable_write_protect()

    def disable_write_protect(self):
        """Disable write protect of the flash chip"""
        self.fum.disable_write_protect()

    def set_write_protect_region(self, region, enabled=None):
        """
        Set write protection region by name, using current image's layout.

        The name should match those seen in `futility dump_fmap <image>`, and
        is not checked against self.firmware_layout, due to different naming.

        @param region: Region to set (usually WP_RO)
        @param enabled: If True, run --wp-enable; if False, run --wp-disable.
                        If None (default), don't specify either one.
        """
        if region is None:
            raise FlashromHandlerError("Region must not be None")
        image_file = self.os_if.create_temp_file('wp_')
        self.os_if.write_file(image_file, self.image)

        self.fum.set_write_protect_region(image_file, region, enabled)
        self.os_if.remove_file(image_file)

    def set_write_protect_range(self, start, length, enabled=None):
        """
        Set write protection range by offsets, using current image's layout.

        @param start: offset (bytes) from start of flash to start of range
        @param length: offset (bytes) from start of range to end of range
        @param enabled: If True, run --wp-enable; if False, run --wp-disable.
                        If None (default), don't specify either one.
        """
        self.fum.set_write_protect_range(start, length, enabled)

    def get_write_protect_status(self):
        """Get a dict describing the status of the write protection

        @return: {'enabled': True/False, 'start': '0x0', 'length': '0x0', ...}
        @rtype: dict
        """
        return self.fum.get_write_protect_status()

    def get_section_sig_sha(self, section):
        """Retrieve SHA1 hash of a firmware vblock section"""
        return self.fv_sections[section].get_sig_sha()

    def get_section_sha(self, section):
        """Retrieve SHA1 hash of a firmware body section"""
        return self.fv_sections[section].get_sha()

    def get_section_version(self, section):
        """Retrieve version number of a firmware section"""
        return self.fv_sections[section].get_version()

    def get_section_flags(self, section):
        """Retrieve preamble flags of a firmware section"""
        return self.fv_sections[section].get_flags()

    def get_section_datakey_version(self, section):
        """Retrieve data key version number of a firmware section"""
        return self.fv_sections[section].get_datakey_version()

    def get_section_kernel_subkey_version(self, section):
        """Retrieve kernel subkey version number of a firmware section"""
        return self.fv_sections[section].get_kernel_subkey_version()

    def get_section_body(self, section):
        """Retrieve body of a firmware section"""
        subsection_name = self.fv_sections[section].get_body_name()
        blob = self.fum.get_section(self.image, subsection_name)
        return blob

    def has_section_body(self, section):
        """Return True if the section body is in the image"""
        return bool(self.get_section_body(section))

    def get_section_sig(self, section):
        """Retrieve vblock of a firmware section"""
        subsection_name = self.fv_sections[section].get_sig_name()
        blob = self.fum.get_section(self.image, subsection_name)
        return blob

    def get_section_fwid(self, section, strip_null=True):
        """
        Retrieve fwid blob of a firmware section.

        @param section: Name of the section whose fwid to return.
        @param strip_null: If True, remove \0 from the end of the blob.
        @return: fwid of the section

        @type section: str
        @type strip_null: bool
        @rtype: str | None

        """
        subsection_name = self.fv_sections[section].get_fwid_name()
        if not subsection_name:
            return None
        blob = self.fum.get_section(self.image, subsection_name)
        if strip_null:
            blob = blob.rstrip('\0')
        return blob

    def set_section_body(self, section, blob, write_through=False):
        """Put the supplied blob to the body of the firmware section"""
        subsection_name = self.fv_sections[section].get_body_name()
        self.write_partial(subsection_name, blob, write_through)

    def set_section_sig(self, section, blob, write_through=False):
        """Put the supplied blob to the vblock of the firmware section"""
        subsection_name = self.fv_sections[section].get_sig_name()
        self.write_partial(subsection_name, blob, write_through)

    def set_section_fwid(self, section, blob, write_through=False):
        """Put the supplied blob to the fwid of the firmware section"""
        subsection_name = self.fv_sections[section].get_fwid_name()
        self.write_partial(subsection_name, blob, write_through)

    def resign_ec_rwsig(self):
        """Resign the EC image using rwsig."""
        key_ec_efs = os.path.join(self.dev_key_path, self.EC_EFS_KEY_FILE_NAME)
        # Dump whole EC image to a file and execute the sign command.
        with tempfile.NamedTemporaryFile() as f:
            self.dump_whole(f.name)
            self.os_if.run_shell_command(
                    'futility sign --type rwsig --prikey %s %s' % (key_ec_efs,
                                                                   f.name))
            self.new_image(f.name)

    def set_section_version(self, section, version, flags,
                            write_through=False):
        """
        Re-sign the firmware section using the supplied version number and
        flag.
        """
        if (self.get_section_version(section) == version
                    and self.get_section_flags(section) == flags):
            return  # No version or flag change, nothing to do.
        if version < 0:
            raise FlashromHandlerError(
                    'Attempt to set version %d on section %s' % (version,
                                                                 section))
        fv_section = self.fv_sections[section]
        sig_name = self.section_file(fv_section.get_sig_name())
        sig_size = os.path.getsize(sig_name)

        # Construct the command line
        args = ['--vblock %s' % sig_name]
        args.append('--keyblock %s' % os.path.join(self.dev_key_path,
                                                   self.FW_KEYBLOCK_FILE_NAME))
        args.append('--fv %s' % self.section_file(fv_section.get_body_name()))
        args.append('--version %d' % version)
        args.append('--kernelkey %s' % os.path.join(
                self.dev_key_path, self.KERNEL_SUBKEY_FILE_NAME))
        args.append('--signprivate %s' % os.path.join(
                self.dev_key_path, self.FW_PRIV_DATA_KEY_FILE_NAME))
        args.append('--flags %d' % flags)
        cmd = 'vbutil_firmware %s' % ' '.join(args)
        self.os_if.run_shell_command(cmd)

        #  Pad the new signature.
        with open(sig_name, 'a') as sig_f:
            f_size = os.fstat(sig_f.fileno()).st_size
            pad = '\0' * (sig_size - f_size)
            sig_f.write(pad)

        # Inject the new signature block into the image
        with open(sig_name, 'r') as sig_f:
            new_sig = sig_f.read()
        self.write_partial(fv_section.get_sig_name(), new_sig, write_through)

    def _modify_section_fwid(self, section):
        """Modify a section's fwid on the handler, adding a tilde and the
        section name (in caps) to the end: ~RO, ~RW, ~A, ~B.

        @param section: the single section to act on
        @return: the new fwid

        @type section: str
        @rtype: str
        """

        fwid = self.get_section_fwid(section, strip_null=False)

        if fwid is None:
            return None

        fwid_size = len(fwid)

        if not fwid:
            raise FlashromHandlerError(
                    "FWID (%s, %s) is empty: %s" %
                    (self.target.upper(), section.upper(), repr(fwid)))

        fwid = fwid.rstrip('\0')
        suffix = self.FWID_MOD_DELIMITER + section.upper()

        if suffix in fwid:
            raise FlashromHandlerError(
                    "FWID (%s, %s) is already modified: %s" %
                    (self.target.upper(), section.upper(), repr(fwid)))

        # Append a suffix, after possibly chopping off characters to make room.
        if len(fwid) + len(suffix) > fwid_size:
            fwid = fwid[:fwid_size - len(suffix)]
        fwid += suffix

        padded_fwid = fwid.ljust(fwid_size, '\0')
        self.set_section_fwid(section, padded_fwid)
        return fwid

    def _strip_section_fwid(self, section, write_through=True):
        """Modify a section's fwid on the handler, stripping any suffix added
        by _modify_section_fwid: ~RO, ~RW, ~A, ~B.

        @param section: the single section to act on
        @param write_through: if True (default), write to flash immediately
        @return: the suffix that was stripped

        @type section: str
        @type write_through: bool
        @rtype: str | None
        """

        fwid = self.get_section_fwid(section, strip_null=False)
        if fwid is None:
            return None

        fwid_size = len(fwid)

        if not fwid:
            raise FlashromHandlerError(
                    "FWID (%s, %s) is empty: %s" %
                    (self.target.upper(), section.upper(), repr(fwid)))

        fwid = fwid.rstrip('\0')
        mod_indicator = self.FWID_MOD_DELIMITER + section.upper()

        # Remove any suffix, and return the suffix if found.
        if mod_indicator in fwid:
            (stripped_fwid, remainder) = fwid.split(mod_indicator, 1)

            padded_fwid = stripped_fwid.ljust(fwid_size, '\0')
            self.set_section_fwid(section, padded_fwid, write_through)

            return fwid
        return None

    def modify_fwids(self, sections):
        """Modify the fwid in the in-memory image.

        @param sections: section(s) to modify.
        @return: fwids for the modified sections, as {section: fwid}

        @type sections: tuple | list
        @rtype: dict
        """
        fwids = {}
        for section in sections:
            fwids[section] = self._modify_section_fwid(section)

        return fwids

    def strip_modified_fwids(self):
        """Strip any trailing suffixes (from modify_fwids) out of the FWIDs.

        @return: a dict of any fwids that were adjusted, by section (ro, a, b)
        @rtype: dict
        """

        suffixes = {}
        for section in self.fv_sections:
            suffix = self._strip_section_fwid(section)
            if suffix is not None:
                suffixes[section] = suffix

        return suffixes
