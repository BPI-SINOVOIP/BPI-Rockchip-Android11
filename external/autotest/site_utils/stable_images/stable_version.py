#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for viewing and changing software version assignments.

Usage:
    stable_version [ -w SERVER ] [ -n ] [ -t TYPE ]
    stable_version [ -w SERVER ] [ -n ] [ -t TYPE ] BOARD/MODEL
    stable_version [ -w SERVER ] [ -n ] -t TYPE -d BOARD/MODEL
    stable_version [ -w SERVER ] [ -n ] -t TYPE BOARD/MODEL VERSION

Available options:
-w SERVER | --web SERVER
    Used to specify an alternative server for the AFE RPC interface.

-n | --dry-run
    When specified, the command reports what would be done, but makes no
    changes.

-t TYPE | --type TYPE
    Specifies the type of version mapping to use.  This option is
    required for operations to change or delete mappings.  When listing
    mappings, the option may be omitted, in which case all mapping types
    are listed.

-d | --delete
    Delete the mapping for the given board or model argument.

Command arguments:
BOARD/MODEL
    When specified, indicates the board or model to use as a key when
    listing, changing, or deleting mappings.

VERSION
    When specified, indicates that the version name should be assigned
    to the given board or model.

With no arguments, the command will list all available mappings of all
types.  The `--type` option will restrict the listing to only mappings of
the given type.

With only a board or model specified (and without the `--delete`
option), will list all mappings for the given board or model.  The
`--type` option will restrict the listing to only mappings of the given
type.

With the `--delete` option, will delete the mapping for the given board
or model.  The `--type` option is required in this case.

With both a board or model and a version specified, will assign the
version to the given board or model.  The `--type` option is required in
this case.
"""

import argparse
import os
import sys

import common
from autotest_lib.server import frontend
from autotest_lib.site_utils.stable_images import build_data


class _CommandError(Exception):
    """Exception to indicate an error in command processing."""


class _VersionMapHandler(object):
    """An internal class to wrap data for version map operations.

    This is a simple class to gather in one place data associated
    with higher-level command line operations.

    @property _description  A string description used to describe the
                            image type when printing command output.
    @property _dry_run      Value of the `--dry-run` command line
                            operation.
    @property _afe          AFE RPC object.
    @property _version_map  AFE version map object for the image type.
    """

    # Subclasses are required to redefine both of these to a string with
    # an appropriate value.
    TYPE = None
    DESCRIPTION = None

    def __init__(self, afe, dry_run):
        self._afe = afe
        self._dry_run = dry_run
        self._version_map = afe.get_stable_version_map(self.TYPE)

    @property
    def _description(self):
        return self.DESCRIPTION

    def _format_key_data(self, key):
        return '%-10s %-12s' % (self._description, key)

    def _format_operation(self, opname, key):
        return '%-9s %s' % (opname, self._format_key_data(key))

    def get_mapping(self, key):
        """Return the mapping for `key`.

        @param key  Board or model key to use for look up.
        """
        return self._version_map.get_version(key)

    def print_all_mappings(self):
        """Print all mappings in `self._version_map`"""
        print '%s version mappings:' % self._description
        mappings = self._version_map.get_all_versions()
        if not mappings:
            return
        key_list = mappings.keys()
        key_width = max(12, len(max(key_list, key=len)))
        format = '%%-%ds  %%s' % key_width
        for k in sorted(key_list):
            print format % (k, mappings[k])

    def print_mapping(self, key):
        """Print the mapping for `key`.

        Prints a single mapping for the board/model specified by
        `key`.  Print nothing if no mapping exists.

        @param key  Board or model key to use for look up.
        """
        version = self.get_mapping(key)
        if version is not None:
            print '%s  %s' % (self._format_key_data(key), version)

    def set_mapping(self, key, new_version):
        """Change the mapping for `key`, and report the action.

        The mapping for the board or model specifed by `key` is set
        to `new_version`.  The setting is reported to the user as
        added, changed, or unchanged based on the current mapping in
        the AFE.

        This operation honors `self._dry_run`.

        @param key          Board or model key for assignment.
        @param new_version  Version to be assigned to `key`.
        """
        old_version = self.get_mapping(key)
        if old_version is None:
            print '%s -> %s' % (
                self._format_operation('Adding', key), new_version)
        elif old_version != new_version:
            print '%s -> %s to %s' % (
                self._format_operation('Updating', key),
                old_version, new_version)
        else:
            print '%s -> %s' % (
                self._format_operation('Unchanged', key), old_version)
        if not self._dry_run and old_version != new_version:
            self._version_map.set_version(key, new_version)

    def delete_mapping(self, key):
        """Delete the mapping for `key`, and report the action.

        The mapping for the board or model specifed by `key` is removed
        from `self._version_map`.  The change is reported to the user.

        Requests to delete non-existent keys are ignored.

        This operation honors `self._dry_run`.

        @param key  Board or model key to be deleted.
        """
        version = self.get_mapping(key)
        if version is not None:
            print '%s -> %s' % (
                self._format_operation('Delete', key), version)
            if not self._dry_run:
                self._version_map.delete_version(key)
        else:
            print self._format_operation('Unmapped', key)


class _FirmwareVersionMapHandler(_VersionMapHandler):
    TYPE = frontend.AFE.FIRMWARE_IMAGE_TYPE
    DESCRIPTION = 'Firmware'


class _CrOSVersionMapHandler(_VersionMapHandler):
    TYPE = frontend.AFE.CROS_IMAGE_TYPE
    DESCRIPTION = 'Chrome OS'

    def set_mapping(self, board, version):
        """Assign the Chrome OS mapping for the given board.

        This function assigns the given Chrome OS version to the given
        board.  Additionally, for any model with firmware bundled in the
        assigned build, that model will be assigned the firmware version
        found for it in the build.

        @param board    Chrome OS board to be assigned a new version.
        @param version  New Chrome OS version to be assigned to the
                        board.
        """
        new_version = build_data.get_omaha_upgrade(
            build_data.get_omaha_version_map(), board, version)
        if new_version != version:
            print 'Force %s version from Omaha:  %-12s -> %s' % (
                self._description, board, new_version)
        super(_CrOSVersionMapHandler, self).set_mapping(board, new_version)
        fw_versions = build_data.get_firmware_versions(board, new_version)
        fw_handler = _FirmwareVersionMapHandler(self._afe, self._dry_run)
        for model, fw_version in fw_versions.iteritems():
            if fw_version is not None:
                fw_handler.set_mapping(model, fw_version)

    def delete_mapping(self, board):
        """Delete the Chrome OS mapping for the given board.

        This function handles deletes the Chrome OS version mapping for the
        given board.  Additionally, any R/W firmware mapping that existed
        because of the OS mapping will be deleted as well.

        @param board    Chrome OS board to be deleted from the mapping.
        """
        version = self.get_mapping(board)
        super(_CrOSVersionMapHandler, self).delete_mapping(board)
        fw_versions = build_data.get_firmware_versions(board, version)
        fw_handler = _FirmwareVersionMapHandler(self._afe, self._dry_run)
        for model in fw_versions.iterkeys():
            fw_handler.delete_mapping(model)


class _FAFTVersionMapHandler(_VersionMapHandler):
    TYPE = frontend.AFE.FAFT_IMAGE_TYPE
    DESCRIPTION = 'FAFT'


_IMAGE_TYPE_CLASSES = [
    _CrOSVersionMapHandler,
    _FirmwareVersionMapHandler,
    _FAFTVersionMapHandler,
]
_ALL_IMAGE_TYPES = [cls.TYPE for cls in _IMAGE_TYPE_CLASSES]
_IMAGE_TYPE_HANDLERS = {cls.TYPE: cls for cls in _IMAGE_TYPE_CLASSES}


def _create_version_map_handler(image_type, afe, dry_run):
    return _IMAGE_TYPE_HANDLERS[image_type](afe, dry_run)


def _requested_mapping_handlers(afe, image_type):
    """Iterate through the image types for a listing operation.

    When listing all mappings, or when listing by board, the listing can
    be either for all available image types, or just for a single type
    requested on the command line.

    This function takes the value of the `-t` option, and yields a
    `_VersionMapHandler` object for either the single requested type, or
    for all of the types.

    @param afe          AFE RPC interface object; created from SERVER.
    @param image_type   Argument to the `-t` option.  A non-empty string
                        indicates a single image type; value of `None`
                        indicates all types.
    """
    if image_type:
        yield _create_version_map_handler(image_type, afe, True)
    else:
        for cls in _IMAGE_TYPE_CLASSES:
            yield cls(afe, True)


def list_all_mappings(afe, image_type):
    """List all mappings in the AFE.

    This function handles the following syntax usage case:

        stable_version [-w SERVER] [-t TYPE]

    @param afe          AFE RPC interface object; created from SERVER.
    @param image_type   Argument to the `-t` option.
    """
    need_newline = False
    for handler in _requested_mapping_handlers(afe, image_type):
        if need_newline:
            print
        handler.print_all_mappings()
        need_newline = True


def list_mapping_by_key(afe, image_type, key):
    """List all mappings for the given board or model.

    This function handles the following syntax usage case:

        stable_version [-w SERVER] [-t TYPE] BOARD/MODEL

    @param afe          AFE RPC interface object; created from SERVER.
    @param image_type   Argument to the `-t` option.
    @param key          Value of the BOARD/MODEL argument.
    """
    for handler in _requested_mapping_handlers(afe, image_type):
        handler.print_mapping(key)


def _validate_set_mapping(arguments):
    """Validate syntactic requirements to assign a mapping.

    The given arguments specified assigning version to be assigned to
    a board or model; check the arguments for errors that can't be
    discovered by `ArgumentParser`.  Errors are reported by raising
    `_CommandError`.

    @param arguments  `Namespace` object returned from argument parsing.
    """
    if not arguments.type:
        raise _CommandError('The -t/--type option is required to assign a '
                            'version')
    if arguments.type == _FirmwareVersionMapHandler.TYPE:
        msg = ('Cannot assign %s versions directly; '
               'must assign the %s version instead.')
        descriptions = (_FirmwareVersionMapHandler.DESCRIPTION,
                        _CrOSVersionMapHandler.DESCRIPTION)
        raise _CommandError(msg % descriptions)


def set_mapping(afe, image_type, key, version, dry_run):
    """Assign a version mapping to the given board or model.

    This function handles the following syntax usage case:

        stable_version [-w SERVER] [-n] -t TYPE BOARD/MODEL VERSION

    @param afe          AFE RPC interface object; created from SERVER.
    @param image_type   Argument to the `-t` option.
    @param key          Value of the BOARD/MODEL argument.
    @param key          Value of the VERSION argument.
    @param dry_run      Whether the `-n` option was supplied.
    """
    if dry_run:
        print 'Dry run; no mappings will be changed.'
    handler = _create_version_map_handler(image_type, afe, dry_run)
    handler.set_mapping(key, version)


def _validate_delete_mapping(arguments):
    """Validate syntactic requirements to delete a mapping.

    The given arguments specified the `-d` / `--delete` option; check
    the arguments for errors that can't be discovered by
    `ArgumentParser`.  Errors are reported by raising `_CommandError`.

    @param arguments  `Namespace` object returned from argument parsing.
    """
    if arguments.key is None:
        raise _CommandError('Must specify BOARD_OR_MODEL argument '
                            'with -d/--delete')
    if arguments.version is not None:
        raise _CommandError('Cannot specify VERSION argument with '
                            '-d/--delete')
    if not arguments.type:
        raise _CommandError('-t/--type required with -d/--delete option')


def delete_mapping(afe, image_type, key, dry_run):
    """Delete the version mapping for the given board or model.

    This function handles the following syntax usage case:

        stable_version [-w SERVER] [-n] -t TYPE -d BOARD/MODEL

    @param afe          AFE RPC interface object; created from SERVER.
    @param image_type   Argument to the `-t` option.
    @param key          Value of the BOARD/MODEL argument.
    @param dry_run      Whether the `-n` option was supplied.
    """
    if dry_run:
        print 'Dry run; no mappings will be deleted.'
    handler = _create_version_map_handler(image_type, afe, dry_run)
    handler.delete_mapping(key)


def _parse_args(argv):
    """Parse the given arguments according to the command syntax.

    @param argv   Full argument vector, with argv[0] being the command
                  name.
    """
    parser = argparse.ArgumentParser(
        prog=os.path.basename(argv[0]),
        description='Set and view software version assignments')
    parser.add_argument('-w', '--web', default=None,
                        metavar='SERVER',
                        help='Specify the AFE to query.')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='Report what would be done without making '
                             'changes.')
    parser.add_argument('-t', '--type', default=None,
                        choices=_ALL_IMAGE_TYPES,
                        help='Specify type of software image to be assigned.')
    parser.add_argument('-d', '--delete', action='store_true',
                        help='Delete the BOARD_OR_MODEL argument from the '
                             'mappings.')
    parser.add_argument('key', nargs='?', metavar='BOARD_OR_MODEL',
                        help='Board, model, or other key for which to get or '
                             'set a version')
    parser.add_argument('version', nargs='?', metavar='VERSION',
                        help='Version to be assigned')
    return parser.parse_args(argv[1:])


def _dispatch_command(afe, arguments):
    if arguments.delete:
        _validate_delete_mapping(arguments)
        delete_mapping(afe, arguments.type, arguments.key,
                       arguments.dry_run)
    elif arguments.key is None:
        list_all_mappings(afe, arguments.type)
    elif arguments.version is None:
        list_mapping_by_key(afe, arguments.type, arguments.key)
    else:
        _validate_set_mapping(arguments)
        set_mapping(afe, arguments.type, arguments.key,
                    arguments.version, arguments.dry_run)


def main(argv):
    """Standard main routine.

    @param argv  Command line arguments including `sys.argv[0]`.
    """
    arguments = _parse_args(argv)
    afe = frontend.AFE(server=arguments.web)
    try:
        _dispatch_command(afe, arguments)
    except _CommandError as exc:
        print >>sys.stderr, 'Error: %s' % str(exc)
        sys.exit(1)


if __name__ == '__main__':
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
