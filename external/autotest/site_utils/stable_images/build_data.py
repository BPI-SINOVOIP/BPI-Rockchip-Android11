# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for reading build information from GoogleStorage.

This module contains functions providing access to basic data about
Chrome OS builds:
  * Functions for finding information about the Chrome OS versions
    currently being served by Omaha for various boards/hardware models.
  * Functions for finding information about the firmware delivered by
    any given build of Chrome OS.

The necessary data is stored in JSON files in well-known locations in
GoogleStorage.
"""

import json
import subprocess

import common
from autotest_lib.client.common_lib import utils
from autotest_lib.server import frontend


# _OMAHA_STATUS - URI of a file in GoogleStorage with a JSON object
# summarizing all versions currently being served by Omaha.
#
# The principal data is in an array named 'omaha_data'.  Each entry
# in the array contains information relevant to one image being
# served by Omaha, including the following information:
#   * The board name of the product, as known to Omaha.
#   * The channel associated with the image.
#   * The Chrome and Chrome OS version strings for the image
#     being served.
#
_OMAHA_STATUS = 'gs://chromeos-build-release-console/omaha_status.json'


# _BUILD_METADATA_PATTERN - Format string for the URI of a file in
# GoogleStorage with a JSON object that contains metadata about
# a given build.  The metadata includes the version of firmware
# bundled with the build.
#
_BUILD_METADATA_PATTERN = 'gs://chromeos-image-archive/%s/metadata.json'


# _FIRMWARE_UPGRADE_BLACKLIST - a set of boards that are exempt from
# automatic stable firmware version assignment.  This blacklist is
# here out of an abundance of caution, on the general principle of "if
# it ain't broke, don't fix it."  Specifically, these are old, legacy
# boards and:
#   * They're working fine with whatever firmware they have in the lab
#     right now.
#   * Because of their age, we can expect that they will never get any
#     new firmware updates in future.
#   * Servo support is spotty or missing, so there's no certainty that
#     DUTs bricked by a firmware update can be repaired.
#   * Because of their age, they are somewhere between hard and
#     impossible to replace.  In some cases, they are also already in
#     short supply.
#
# N.B.  HARDCODED BOARD NAMES ARE EVIL!!!  This blacklist uses hardcoded
# names because it's meant to define a list of legacies that will shrivel
# and die over time.
#
# DO NOT ADD TO THIS LIST.  If there's a new use case that requires
# extending the blacklist concept, you should find a maintainable
# solution that deletes this code.
#
# TODO(jrbarnette):  When any board is past EOL, and removed from the
# lab, it can be removed from the blacklist.  When all the boards are
# past EOL, the blacklist should be removed.

_FIRMWARE_UPGRADE_BLACKLIST = set([
        'butterfly',
        'daisy',
        'daisy_skate',
        'daisy_spring',
        'lumpy',
        'parrot',
        'parrot_ivb',
        'peach_pi',
        'peach_pit',
        'stout',
        'stumpy',
        'x86-alex',
        'x86-mario',
        'x86-zgb',
    ])


def _read_gs_json_data(gs_uri):
    """Read and parse a JSON file from GoogleStorage.

    This is a wrapper around `gsutil cat` for the specified URI.
    The standard output of the command is parsed as JSON, and the
    resulting object returned.

    @param gs_uri   URI of the JSON file in GoogleStorage.
    @return A JSON object parsed from `gs_uri`.
    """
    with open('/dev/null', 'w') as ignore_errors:
        sp = subprocess.Popen(['gsutil', 'cat', gs_uri],
                              stdout=subprocess.PIPE,
                              stderr=ignore_errors)
        try:
            json_object = json.load(sp.stdout)
        finally:
            sp.stdout.close()
            sp.wait()
    return json_object


def _read_build_metadata(board, cros_version):
    """Read and parse the `metadata.json` file for a build.

    Given the board and version string for a potential CrOS image,
    find the URI of the build in GoogleStorage, and return a Python
    object for the associated `metadata.json`.

    @param board         Board for the build to be read.
    @param cros_version  Build version string.
    """
    image_path = frontend.format_cros_image_name(board, cros_version)
    return _read_gs_json_data(_BUILD_METADATA_PATTERN % image_path)


def _get_by_key_path(dictdict, key_path):
    """Traverse a sequence of keys in a dict of dicts.

    The `dictdict` parameter is a dict of nested dict values, and
    `key_path` a list of keys.

    A single-element key path returns `dictdict[key_path[0]]`, a
    two-element path returns `dictdict[key_path[0]][key_path[1]]`, and
    so forth.  If any key in the path is not found, return `None`.

    @param dictdict   A dictionary of nested dictionaries.
    @param key_path   The sequence of keys to look up in `dictdict`.
    @return The value found by successive dictionary lookups, or `None`.
    """
    value = dictdict
    for key in key_path:
        value = value.get(key)
        if value is None:
            break
    return value


def _get_model_firmware_versions(metadata_json, board):
    """Get the firmware version for all models in a unibuild board.

    @param metadata_json    The metadata_json dict parsed from the
                            metadata.json file generated by the build.
    @param board            The board name of the unibuild.
    @return If the board has no models, return {board: None}.
            Otherwise, return a dict mapping each model name to its
            firmware version.
    """
    model_firmware_versions = {}
    key_path = ['board-metadata', board, 'models']
    model_versions = _get_by_key_path(metadata_json, key_path)

    if model_versions is not None:
        for model, fw_versions in model_versions.iteritems():
            fw_version = (fw_versions.get('main-readwrite-firmware-version') or
                          fw_versions.get('main-readonly-firmware-version'))
            model_firmware_versions[model] = fw_version
    else:
        model_firmware_versions[board] = None

    return model_firmware_versions


def get_omaha_version_map():
    """Convert omaha versions data to a versions mapping.

    Returns a dictionary mapping board names to the currently preferred
    version for the Beta channel as served by Omaha.  The mappings are
    provided by settings in the JSON object read from `_OMAHA_STATUS`.

    The board names are the names as known to Omaha:  If the board name
    in the AFE contains '_', the corresponding Omaha name uses '-'
    instead.  The boards mapped may include boards not in the list of
    managed boards in the lab.

    @return A dictionary mapping Omaha boards to Beta versions.
    """
    def _entry_valid(json_entry):
        return json_entry['channel'] == 'beta'

    def _get_omaha_data(json_entry):
        board = json_entry['board']['public_codename']
        milestone = json_entry['milestone']
        build = json_entry['chrome_os_version']
        version = 'R%d-%s' % (milestone, build)
        return (board, version)

    omaha_status = _read_gs_json_data(_OMAHA_STATUS)
    return dict(_get_omaha_data(e) for e in omaha_status['omaha_data']
                    if _entry_valid(e))


def get_omaha_upgrade(omaha_map, board, version):
    """Get the later of a build in `omaha_map` or `version`.

    Read the Omaha version for `board` from `omaha_map`, and compare it
    to `version`.  Return whichever version is more recent.

    N.B. `board` is the name of a board as known to the AFE.  Board
    names as known to Omaha are different; see
    `get_omaha_version_map()`, above.  This function is responsible
    for translating names as necessary.

    @param omaha_map  Mapping of Omaha board names to preferred builds.
    @param board      Name of the board to look up, as known to the AFE.
    @param version    Minimum version to be accepted.

    @return Returns a Chrome OS version string in standard form
            R##-####.#.#.  Will return `None` if `version` is `None` and
            no Omaha entry is found.
    """
    omaha_version = omaha_map.get(board.replace('_', '-'))
    if version is None:
        return omaha_version
    if omaha_version is not None:
        if utils.compare_versions(version, omaha_version) < 0:
            return omaha_version
    return version


def get_firmware_versions(board, cros_version):
    """Get the firmware versions for a given board and CrOS version.

    During the CrOS auto-update process, the system will check firmware
    on the target device, and update that firmware if needed.  This
    function finds the version string of the firmware that would be
    installed from a given CrOS build.

    A build may have firmware for more than one hardware model, so the
    returned value is a dictionary mapping models to firmware version
    strings.

    The returned firmware version value will be `None` if the build
    isn't found in storage, if there is no firmware found for the build,
    or if the board is blacklisted from firmware updates in the test
    lab.

    @param board          The board for the firmware version to be
                          determined.
    @param cros_version   The CrOS version bundling the firmware.
    @return A dict mapping from board to firmware version string for
            non-unibuild board, or a dict mapping from models to firmware
            versions for a unibuild board (see return type of
            _get_model_firmware_versions)
    """
    if board in _FIRMWARE_UPGRADE_BLACKLIST:
        return {board: None}
    try:
        metadata_json = _read_build_metadata(board, cros_version)
        unibuild = bool(_get_by_key_path(metadata_json, ['unibuild']))
        if unibuild:
            return _get_model_firmware_versions(metadata_json, board)
        else:
            key_path = ['board-metadata', board, 'main-firmware-version']
            return {board: _get_by_key_path(metadata_json, key_path)}
    except Exception as e:
        # TODO(jrbarnette): If we get here, it likely means that the
        # build for this board doesn't exist.  That can happen if a
        # board doesn't release on the Beta channel for at least 6 months.
        #
        # We can't allow this error to propagate up the call chain
        # because that will kill assigning versions to all the other
        # boards that are still OK, so for now we ignore it.  Probably,
        # we should do better.
        return {board: None}
