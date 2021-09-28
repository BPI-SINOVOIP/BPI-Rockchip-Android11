#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Automatically update the afe_stable_versions table.

This command updates the stable repair version for selected boards
in the lab.  For each board, if the version that Omaha is serving
on the Beta channel for the board is more recent than the current
stable version in the AFE database, then the AFE is updated to use
the version on Omaha.

The upgrade process is applied to every "managed board" in the test
lab.  Generally, a managed board is a board with both spare and
critical scheduling pools.

See `autotest_lib.site_utils.lab_inventory` for the full definition
of "managed board".

The command supports a `--dry-run` option that reports changes that
would be made, without making the actual RPC calls to change the
database.

"""

import argparse
import logging

import common
from autotest_lib.server.cros.dynamic_suite import frontend_wrappers
from autotest_lib.site_utils import lab_inventory
from autotest_lib.site_utils import loglib
from autotest_lib.site_utils.stable_images import build_data
from chromite.lib import ts_mon_config
from chromite.lib import metrics


# _DEFAULT_BOARD - The distinguished board name used to identify a
# stable version mapping that is used for any board without an explicit
# mapping of its own.
#
# _DEFAULT_VERSION_TAG - A string used to signify that there is no
# mapping for a board, in other words, the board is mapped to the
# default version.
#
_DEFAULT_BOARD = 'DEFAULT'
_DEFAULT_VERSION_TAG = '(default)'

_METRICS_PREFIX = 'chromeos/autotest/assign_stable_images'


class _VersionUpdater(object):
    """
    Class to report and apply version changes.

    This class is responsible for the low-level logic of applying
    version upgrades and reporting them as command output.

    This class exists to solve two problems:
     1. To distinguish "normal" vs. "dry-run" modes.  Each mode has a
        subclass; methods that perform actual AFE updates are
        implemented for the normal mode subclass only.
     2. To provide hooks for unit tests.  The unit tests override both
        the reporting and modification behaviors, in order to test the
        higher level logic that decides what changes are needed.

    Methods meant merely to report changes to command output have names
    starting with "report" or "_report".  Methods that are meant to
    change the AFE in normal mode have names starting with "_do"
    """

    def __init__(self, afe, dry_run):
        """Initialize us.

        @param afe:     A frontend.AFE object.
        @param dry_run: A boolean indicating whether to execute in dry run mode.
                        No updates are persisted to the afe in dry run.
        """
        self._dry_run = dry_run
        image_types = [afe.CROS_IMAGE_TYPE, afe.FIRMWARE_IMAGE_TYPE]
        self._version_maps = {
            image_type: afe.get_stable_version_map(image_type)
                for image_type in image_types
        }
        self._cros_map = self._version_maps[afe.CROS_IMAGE_TYPE]
        self._selected_map = None

    def select_version_map(self, image_type):
        """
        Select an AFE version map object based on `image_type`.

        This creates and remembers an AFE version mapper object to be
        used for making changes in normal mode.

        @param image_type   Image type parameter for the version mapper
                            object.
        """
        self._selected_map = self._version_maps[image_type]
        return self._selected_map.get_all_versions()

    def report_default_changed(self, old_default, new_default):
        """
        Report that the default version mapping is changing.

        This merely reports a text description of the pending change
        without executing it.

        @param old_default  The original default version.
        @param new_default  The new default version to be applied.
        """
        logging.debug('Default %s -> %s', old_default, new_default)

    def _report_board_changed(self, board, old_version, new_version):
        """
        Report a change in one board's assigned version mapping.

        This merely reports a text description of the pending change
        without executing it.

        @param board        The board with the changing version.
        @param old_version  The original version mapped to the board.
        @param new_version  The new version to be applied to the board.
        """
        logging.debug('    %-22s %s -> %s', board, old_version, new_version)

    def report_board_unchanged(self, board, old_version):
        """
        Report that a board's version mapping is unchanged.

        This reports that a board has a non-default mapping that will be
        unchanged.

        @param board        The board that is not changing.
        @param old_version  The board's version mapping.
        """
        self._report_board_changed(board, '(no change)', old_version)

    def _do_set_mapping(self, board, new_version):
        """
        Change one board's assigned version mapping.

        @param board        The board with the changing version.
        @param new_version  The new version to be applied to the board.
        """
        if self._dry_run:
            logging.info('DRYRUN: Would have set %s version to %s',
                         board, new_version)
        else:
            self._selected_map.set_version(board, new_version)

    def _do_delete_mapping(self, board):
        """
        Delete one board's assigned version mapping.

        @param board        The board with the version to be deleted.
        """
        if self._dry_run:
            logging.info('DRYRUN: Would have deleted version for %s', board)
        else:
            self._selected_map.delete_version(board)

    def set_mapping(self, board, old_version, new_version):
        """
        Change and report a board version mapping.

        @param board        The board with the changing version.
        @param old_version  The original version mapped to the board.
        @param new_version  The new version to be applied to the board.
        """
        self._report_board_changed(board, old_version, new_version)
        self._do_set_mapping(board, new_version)

    def upgrade_default(self, new_default):
        """
        Apply a default version change.

        @param new_default  The new default version to be applied.
        """
        self._do_set_mapping(_DEFAULT_BOARD, new_default)

    def delete_mapping(self, board, old_version):
        """
        Delete a board version mapping, and report the change.

        @param board        The board with the version to be deleted.
        @param old_version  The board's verson prior to deletion.
        """
        assert board != _DEFAULT_BOARD
        self._report_board_changed(board,
                                   old_version,
                                   _DEFAULT_VERSION_TAG)
        self._do_delete_mapping(board)


def _get_upgrade_versions(cros_versions, omaha_versions, boards):
    """
    Get the new stable versions to which we should update.

    The new versions are returned as a tuple of a dictionary mapping
    board names to versions, plus a new default board setting.  The
    new default is determined as the most commonly used version
    across the given boards.

    The new dictionary will have a mapping for every board in `boards`.
    That mapping will be taken from `cros_versions`, unless the board has
    a mapping in `omaha_versions` _and_ the omaha version is more recent
    than the AFE version.

    @param cros_versions    The current board->version mappings in the
                            AFE.
    @param omaha_versions   The current board->version mappings from
                            Omaha for the Beta channel.
    @param boards           Set of boards to be upgraded.
    @return Tuple of (mapping, default) where mapping is a dictionary
            mapping boards to versions, and default is a version string.
    """
    upgrade_versions = {}
    version_counts = {}
    afe_default = cros_versions[_DEFAULT_BOARD]
    for board in boards:
        version = build_data.get_omaha_upgrade(
                omaha_versions, board,
                cros_versions.get(board, afe_default))
        upgrade_versions[board] = version
        version_counts.setdefault(version, 0)
        version_counts[version] += 1
    return (upgrade_versions,
            max(version_counts.items(), key=lambda x: x[1])[0])


def _get_firmware_upgrades(cros_versions):
    """
    Get the new firmware versions to which we should update.

    @param cros_versions    Current board->cros version mappings in the
                            AFE.
    @return A dictionary mapping boards/models to firmware upgrade versions.
            If the build is unibuild, the key is a model name; else, the key
            is a board name.
    """
    firmware_upgrades = {}
    for board, version in cros_versions.iteritems():
        firmware_upgrades.update(
            build_data.get_firmware_versions(board, version))
    return firmware_upgrades


def _apply_cros_upgrades(updater, old_versions, new_versions,
                         new_default):
    """
    Change CrOS stable version mappings in the AFE.

    The input `old_versions` dictionary represents the content of the
    `afe_stable_versions` database table; it contains mappings for a
    default version, plus exceptions for boards with non-default
    mappings.

    The `new_versions` dictionary contains a mapping for every board,
    including boards that will be mapped to the new default version.

    This function applies the AFE changes necessary to produce the new
    AFE mappings indicated by `new_versions` and `new_default`.  The
    changes are ordered so that at any moment, every board is mapped
    either according to the old or the new mapping.

    @param updater        Instance of _VersionUpdater responsible for
                          making the actual database changes.
    @param old_versions   The current board->version mappings in the
                          AFE.
    @param new_versions   New board->version mappings obtained by
                          applying Beta channel upgrades from Omaha.
    @param new_default    The new default build for the AFE.
    """
    old_default = old_versions[_DEFAULT_BOARD]
    if old_default != new_default:
        updater.report_default_changed(old_default, new_default)
    logging.info('Applying stable version changes:')
    default_count = 0
    for board, new_build in new_versions.items():
        if new_build == new_default:
            default_count += 1
        elif board in old_versions and new_build == old_versions[board]:
            updater.report_board_unchanged(board, new_build)
        else:
            old_build = old_versions.get(board)
            if old_build is None:
                old_build = _DEFAULT_VERSION_TAG
            updater.set_mapping(board, old_build, new_build)
    if old_default != new_default:
        updater.upgrade_default(new_default)
    for board, new_build in new_versions.items():
        if new_build == new_default and board in old_versions:
            updater.delete_mapping(board, old_versions[board])
    logging.info('%d boards now use the default mapping', default_count)


def _apply_firmware_upgrades(updater, old_versions, new_versions):
    """
    Change firmware version mappings in the AFE.

    The input `old_versions` dictionary represents the content of the
    firmware mappings in the `afe_stable_versions` database table.
    There is no default version; missing boards simply have no current
    version.

    This function applies the AFE changes necessary to produce the new
    AFE mappings indicated by `new_versions`.

    TODO(jrbarnette) This function ought to remove any mapping not found
    in `new_versions`.  However, in theory, that's only needed to
    account for boards that are removed from the lab, and that hasn't
    happened yet.

    @param updater        Instance of _VersionUpdater responsible for
                          making the actual database changes.
    @param old_versions   The current board->version mappings in the
                          AFE.
    @param new_versions   New board->version mappings obtained by
                          applying Beta channel upgrades from Omaha.
    """
    unchanged = 0
    no_version = 0
    for board, new_firmware in new_versions.items():
        if new_firmware is None:
            no_version += 1
        elif board not in old_versions:
            updater.set_mapping(board, '(nothing)', new_firmware)
        else:
            old_firmware = old_versions[board]
            if new_firmware != old_firmware:
                updater.set_mapping(board, old_firmware, new_firmware)
            else:
                unchanged += 1
    logging.info('%d boards have no firmware mapping', no_version)
    logging.info('%d boards are unchanged', unchanged)


def _assign_stable_images(arguments):
    afe = frontend_wrappers.RetryingAFE(server=arguments.web)
    updater = _VersionUpdater(afe, dry_run=arguments.dry_run)

    cros_versions = updater.select_version_map(afe.CROS_IMAGE_TYPE)
    omaha_versions = build_data.get_omaha_version_map()
    upgrade_versions, new_default = (
            _get_upgrade_versions(cros_versions, omaha_versions,
                                  lab_inventory.get_managed_boards(afe)))
    _apply_cros_upgrades(updater, cros_versions,
                         upgrade_versions, new_default)

    logging.info('Applying firmware updates.')
    fw_versions = updater.select_version_map(afe.FIRMWARE_IMAGE_TYPE)
    firmware_upgrades = _get_firmware_upgrades(upgrade_versions)
    _apply_firmware_upgrades(updater, fw_versions, firmware_upgrades)


def main():
    """Standard main routine."""
    parser = argparse.ArgumentParser(
            description='Update the stable repair version for all '
                        'boards')
    parser.add_argument('-n', '--dry-run',
                        action='store_true',
                        help='print changes without executing them')
    loglib.add_logging_options(parser)
    # TODO(crbug/888046) Make these arguments required once puppet is updated to
    # pass them in.
    parser.add_argument('--web',
                        default='cautotest',
                        help='URL to the AFE to update.')

    arguments = parser.parse_args()
    loglib.configure_logging_with_args(parser, arguments)

    tsmon_args = {
            'service_name': parser.prog,
            'indirect': False,
            'auto_flush': False,
    }
    if arguments.dry_run:
        logging.info('DRYRUN: No changes will be made.')
        # metrics will be logged to logging stream anyway.
        tsmon_args['debug_file'] = '/dev/null'

    try:
        with ts_mon_config.SetupTsMonGlobalState(**tsmon_args):
            with metrics.SuccessCounter(_METRICS_PREFIX + '/tick',
                                        fields={'afe': arguments.web}):
                _assign_stable_images(arguments)
    finally:
        metrics.Flush()

if __name__ == '__main__':
    main()
