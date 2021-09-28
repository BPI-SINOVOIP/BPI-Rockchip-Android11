#!/usr/bin/env python
#
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
#
"""Downloads prebuilt from the build server."""
import argparse
import logging
import os
import shutil
import stat
import textwrap


class InstallEntry(object):
    def __init__(self, target, name, install_path,
                 need_strip=False, need_exec=False, need_unzip=False):
        self.target = target
        self.name = name
        self.install_path = install_path
        self.need_strip = need_strip
        self.need_exec = need_exec
        self.need_unzip = need_unzip

def logger():
    """Returns the main logger for this module."""
    return logging.getLogger(__name__)


def check_call(cmd):
    """Proxy for subprocess.check_call with logging."""
    import subprocess
    logger().debug('check_call `%s`', ' '.join(cmd))
    subprocess.check_call(cmd)


def fetch_artifact(branch, build, target, pattern):
    """Fetches artifact from the build server."""
    logger().info('Fetching %s from %s %s (artifacts matching %s)', build,
                  target, branch, pattern)
    if target.startswith('local:'):
        shutil.copyfile(target[6:], pattern)
        return
    fetch_artifact_path = '/google/data/ro/projects/android/fetch_artifact'
    cmd = [fetch_artifact_path, '--branch', branch, '--target', target,
           '--bid', build, pattern]
    check_call(cmd)


def start_branch(build):
    """Creates a new branch in the project."""
    branch_name = 'update-' + (build or 'latest')
    logger().info('Creating branch %s', branch_name)
    check_call(['repo', 'start', branch_name, '.'])


def commit(prebuilts, branch, build, add_paths):
    """Commits the new prebuilts."""
    logger().info('Making commit')
    check_call(['git', 'add'] + add_paths)
    message = textwrap.dedent("""\
        Update {prebuilts} prebuilts to build {build}.

        Taken from branch {branch}.""").format(prebuilts=prebuilts, branch=branch, build=build)
    check_call(['git', 'commit', '-m', message])


def list_installed_files(install_list, extracted_list):
    """List all prebuilts in current directory."""
    result = []
    for entry in install_list:
      result += [entry.install_path]
    for entry in extracted_list:
      result += [entry.install_path]
    return result


def remove_old_files(install_list, extracted_list):
    """Removes the old files."""
    old_files = list_installed_files(install_list, extracted_list)
    if not old_files:
        return
    logger().info('Removing old files %s', old_files)
    check_call(['git', 'rm', '-rf', '--ignore-unmatch'] + old_files)

    # Need to check again because git won't remove directories if they have
    # non-git files in them.
    check_call(['rm', '-rf'] + old_files)


def install_new_files(branch, build, install_list, extracted_list):
    """Installs the new release."""
    for entry in install_list:
        install_entry(branch, build, entry)
    for entry in extracted_list:
        if entry.need_strip:
            check_call(['strip', entry.name])


def install_entry(branch, build, entry):
    """Installs one file specified by entry."""
    target = entry.target
    name = entry.name
    install_path = entry.install_path
    need_strip = entry.need_strip
    need_exec = entry.need_exec
    need_unzip = entry.need_unzip

    fetch_artifact(branch, build, target, name)
    if need_strip:
        check_call(['strip', name])
    if need_exec:
        check_call(['chmod', 'a+x', name])
    dir = os.path.dirname(install_path)
    if dir and os.path.isdir(dir):
        os.makedirs(dir)
    shutil.move(name, install_path)

    if need_unzip:
        unzip(install_path)

def unzip(zip_path):
    check_call(['unzip', zip_path])


def get_args():
    """Parses and returns command line arguments."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-b', '--branch', default='aosp-master',
        help='Branch to pull build from.')
    parser.add_argument('--build', required=True, help='Build number to pull.')
    parser.add_argument(
        '--use-current-branch', action='store_true',
        help='Perform the update in the current branch. Do not repo start.')
    parser.add_argument(
        '-v', '--verbose', action='count', default=0,
        help='Increase output verbosity.')

    return parser.parse_args()


def main(work_dir, prebuilts, install_list, extracted_list):
    """Program entry point."""
    os.chdir(work_dir)

    args = get_args()
    verbose_map = (logging.WARNING, logging.INFO, logging.DEBUG)
    verbosity = args.verbose
    if verbosity > 2:
        verbosity = 2
    logging.basicConfig(level=verbose_map[verbosity])

    if not args.use_current_branch:
        start_branch(args.build)
    remove_old_files(install_list, extracted_list)
    install_new_files(args.branch, args.build, install_list, extracted_list)
    files = list_installed_files(install_list, extracted_list)
    commit(prebuilts, args.branch, args.build, files)
