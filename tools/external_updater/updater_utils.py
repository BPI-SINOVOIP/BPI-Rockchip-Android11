# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Helper functions for updaters."""

import os
import re
import subprocess
import sys


def create_updater(metadata, proj_path, updaters):
    """Creates corresponding updater object for a project.

    Args:
      metadata: Parsed proto for METADATA file.
      proj_path: Absolute path for the project.

    Returns:
      An updater object.

    Raises:
      ValueError: Occurred when there's no updater for all urls.
    """
    for url in metadata.third_party.url:
        for updater in updaters:
            try:
                return updater(url, proj_path, metadata)
            except ValueError:
                pass

    raise ValueError('No supported URL.')


def replace_package(source_dir, target_dir):
    """Invokes a shell script to prepare and update a project.

    Args:
      source_dir: Path to the new downloaded and extracted package.
      target_dir: The path to the project in Android source tree.
    """

    print('Updating {} using {}.'.format(target_dir, source_dir))
    script_path = os.path.join(
        os.path.dirname(
            sys.argv[0]),
        'update_package.sh')
    subprocess.check_call(['bash', script_path, source_dir, target_dir])

VERSION_SPLITTER_PATTERN = r'[\.\-_]'
VERSION_PATTERN = (r'^(?P<prefix>[^\d]*)' +
                   r'(?P<version>\d+(' + VERSION_SPLITTER_PATTERN + r'\d+)*)' +
                   r'(?P<suffix>.*)$')
VERSION_RE = re.compile(VERSION_PATTERN)
VERSION_SPLITTER_RE = re.compile(VERSION_SPLITTER_PATTERN)


def _parse_version(version):
    match = VERSION_RE.match(version)
    if match is None:
        raise ValueError('Invalid version.')
    try:
        return match.group('prefix', 'version', 'suffix')
    except IndexError:
        raise ValueError('Invalid version.')


def _match_and_get_version(prefix, suffix, version):
    try:
        version_prefix, version, version_suffix = _parse_version(version)
    except ValueError:
        return []

    right_format = (version_prefix == prefix and version_suffix == suffix)

    return [right_format] + [int(v) for v in VERSION_SPLITTER_RE.split(version)]


def get_latest_version(current_version, version_list):
    """Gets the latest version name from a list of versions.

    The new version must have the same prefix and suffix with old version.
    If no matched version is newer, current version name will be returned.
    """
    prefix, _, suffix = _parse_version(current_version)

    latest = max(version_list,
                 key=lambda ver: _match_and_get_version(
                     prefix, suffix, ver),
                 default=[])
    if not latest:
        raise ValueError('No matching version.')
    return latest
