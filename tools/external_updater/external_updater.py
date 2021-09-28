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
"""A commandline tool to check and update packages in external/

Example usage:
updater.sh checkall
updater.sh update kotlinc
"""

import argparse
import json
import os
import sys
import subprocess
import time

from google.protobuf import text_format    # pylint: disable=import-error

from git_updater import GitUpdater
from github_archive_updater import GithubArchiveUpdater
import fileutils
import git_utils
import updater_utils


UPDATERS = [GithubArchiveUpdater, GitUpdater]

USE_COLOR = sys.stdout.isatty()

def color_string(string, color):
    """Changes the color of a string when print to terminal."""
    if not USE_COLOR:
        return string
    colors = {
        'FRESH': '\x1b[32m',
        'STALE': '\x1b[31;1m',
        'ERROR': '\x1b[31m',
    }
    end_color = '\033[0m'
    return colors[color] + string + end_color


def build_updater(proj_path):
    """Build updater for a project specified by proj_path.

    Reads and parses METADATA file. And builds updater based on the information.

    Args:
      proj_path: Absolute or relative path to the project.

    Returns:
      The updater object built. None if there's any error.
    """

    proj_path = fileutils.get_absolute_project_path(proj_path)
    try:
        metadata = fileutils.read_metadata(proj_path)
    except text_format.ParseError as err:
        print('{} {}.'.format(color_string('Invalid metadata file:', 'ERROR'),
                              err))
        return None

    try:
        updater = updater_utils.create_updater(metadata, proj_path, UPDATERS)
    except ValueError:
        print(color_string('No supported URL.', 'ERROR'))
        return None
    return updater


def has_new_version(updater):
    """Whether an updater found a new version."""
    return updater.get_current_version() != updater.get_latest_version()


def _message_for_calledprocesserror(error):
    return '\n'.join([error.stdout.decode('utf-8'),
                      error.stderr.decode('utf-8')])


def check_update(proj_path):
    """Checks updates for a project. Prints result on console.

    Args:
      proj_path: Absolute or relative path to the project.
    """

    print(
        'Checking {}. '.format(fileutils.get_relative_project_path(proj_path)),
        end='')
    updater = build_updater(proj_path)
    if updater is None:
        return (None, 'Failed to create updater')
    try:
        updater.check()
        if has_new_version(updater):
            print(color_string(' Out of date!', 'STALE'))
        else:
            print(color_string(' Up to date.', 'FRESH'))
        return (updater, None)
    except (IOError, ValueError) as err:
        print('{} {}.'.format(color_string('Failed.', 'ERROR'),
                              err))
        return (updater, str(err))
    except subprocess.CalledProcessError as err:
        msg = _message_for_calledprocesserror(err)
        print('{}\n{}'.format(msg, color_string('Failed.', 'ERROR')))
        return (updater, msg)


def _process_update_result(path):
    res = {}
    updater, err = check_update(path)
    if err is not None:
        res['error'] = str(err)
    else:
        res['current'] = updater.get_current_version()
        res['latest'] = updater.get_latest_version()
    return res


def _check_some(paths, delay):
    results = {}
    for path in paths:
        relative_path = fileutils.get_relative_project_path(path)
        results[relative_path] = _process_update_result(path)
        time.sleep(delay)
    return results


def _check_all(delay):
    results = {}
    for path, dirs, files in os.walk(fileutils.EXTERNAL_PATH):
        dirs.sort(key=lambda d: d.lower())
        if fileutils.METADATA_FILENAME in files:
            # Skip sub directories.
            dirs[:] = []
            relative_path = fileutils.get_relative_project_path(path)
            results[relative_path] = _process_update_result(path)
            time.sleep(delay)
    return results


def check(args):
    """Handler for check command."""
    if args.all:
        results = _check_all(args.delay)
    else:
        results = _check_some(args.paths, args.delay)

    if args.json_output is not None:
        with open(args.json_output, 'w') as f:
            json.dump(results, f, sort_keys=True, indent=4)


def update(args):
    """Handler for update command."""
    try:
        _do_update(args)
    except subprocess.CalledProcessError as err:
        msg = _message_for_calledprocesserror(err)
        print(
            '{}\n{}'.format(
                msg,
                color_string(
                    'Failed to upgrade.',
                    'ERROR')))


TMP_BRANCH_NAME = 'tmp_auto_upgrade'


def _do_update(args):
    updater, _ = check_update(args.path)
    if updater is None:
        return
    if not has_new_version(updater) and not args.force:
        return

    full_path = fileutils.get_absolute_project_path(args.path)
    if args.branch_and_commit:
        git_utils.checkout(full_path, args.remote_name + '/master')
        try:
            git_utils.delete_branch(full_path, TMP_BRANCH_NAME)
        except subprocess.CalledProcessError:
            # Still continue if the branch doesn't exist.
            pass
        git_utils.start_branch(full_path, TMP_BRANCH_NAME)

    updater.update()

    if args.branch_and_commit:
        msg = 'Upgrade {} to {}\n\nTest: None'.format(
            args.path, updater.get_latest_version())
        git_utils.add_file(full_path, '*')
        git_utils.commit(full_path, msg)

    if args.push_change:
        git_utils.push(full_path, args.remote_name)

    if args.branch_and_commit:
        git_utils.checkout(full_path, args.remote_name + '/master')


def parse_args():
    """Parses commandline arguments."""

    parser = argparse.ArgumentParser(
        description='Check updates for third party projects in external/.')
    subparsers = parser.add_subparsers(dest='cmd')
    subparsers.required = True

    # Creates parser for check command.
    check_parser = subparsers.add_parser(
        'check', help='Check update for one project.')
    check_parser.add_argument(
        'paths', nargs='*',
        help='Paths of the project. '
        'Relative paths will be resolved from external/.')
    check_parser.add_argument(
        '--json_output',
        help='Path of a json file to write result to.')
    check_parser.add_argument(
        '--all', action='store_true',
        help='If set, check updates for all supported projects.')
    check_parser.add_argument(
        '--delay', default=0, type=int,
        help='Time in seconds to wait between checking two projects.')
    check_parser.set_defaults(func=check)

    # Creates parser for update command.
    update_parser = subparsers.add_parser('update', help='Update one project.')
    update_parser.add_argument(
        'path',
        help='Path of the project. '
        'Relative paths will be resolved from external/.')
    update_parser.add_argument(
        '--force',
        help='Run update even if there\'s no new version.',
        action='store_true')
    update_parser.add_argument(
        '--branch_and_commit', action='store_true',
        help='Starts a new branch and commit changes.')
    update_parser.add_argument(
        '--push_change', action='store_true',
        help='Pushes change to Gerrit.')
    update_parser.add_argument(
        '--remote_name', default='aosp', required=False,
        help='Upstream remote name.')
    update_parser.set_defaults(func=update)

    return parser.parse_args()


def main():
    """The main entry."""

    args = parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
