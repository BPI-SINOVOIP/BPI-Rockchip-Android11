#!/usr/bin/env python3
#
# Copyright 2019 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""ide_common_util

This module has a collection of helper functions of the ide_util module.
"""

import fnmatch
import glob
import logging
import os
import subprocess

from aidegen import constant

_IDEA_FOLDER = '.idea'
_IML_EXTENSION = '.iml'


def get_script_from_internal_path(ide_paths, ide_name):
    """Get the studio.sh script path from internal path.

    Args:
        ide_paths: A list of IDE installed paths to be checked.
        ide_name: The IDE name.

    Returns:
        The list of the full path of IDE or None if the IDE doesn't exist.
    """
    for ide_path in ide_paths:
        ls_output = glob.glob(ide_path, recursive=True)
        ls_output = sorted(ls_output)
        if ls_output:
            logging.debug('The script%s for %s %s found.',
                          's' if len(ls_output) > 1 else '', ide_name,
                          'are' if len(ls_output) > 1 else 'is')
            return ls_output
    logging.error('There is not any script of %s found.', ide_name)
    return None


def _run_ide_sh(run_sh_cmd, project_path):
    """Run IDE launching script with an IntelliJ project path as argument.

    Args:
        run_sh_cmd: The command to launch IDE.
        project_path: The path of IntelliJ IDEA project content.
    """
    assert run_sh_cmd, 'No suitable IDE installed.'
    logging.debug('Run command: "%s" to launch project.', run_sh_cmd)
    try:
        subprocess.check_call(run_sh_cmd, shell=True)
    except subprocess.CalledProcessError as err:
        logging.error('Launch project path %s failed with error: %s.',
                      project_path, err)


def _walk_tree_find_ide_exe_file(top, ide_script_name):
    """Recursively descend the directory tree rooted at top and filter out the
       IDE executable script we need.

    Args:
        top: the tree root to be checked.
        ide_script_name: IDE file name such i.e. IdeIntelliJ._INTELLIJ_EXE_FILE.

    Returns:
        the IDE executable script file(s) found.
    """
    logging.info('Searching IDE script %s in path: %s.', ide_script_name, top)
    for root, _, files in os.walk(top):
        logging.debug('Search all files under %s to get %s, %s.', top, root,
                      files)
        for file_ in fnmatch.filter(files, ide_script_name):
            exe_file = os.path.join(root, file_)
            if os.access(exe_file, os.X_OK):
                logging.debug('Use file name filter to find %s in path %s.',
                              file_, exe_file)
                yield exe_file


def get_run_ide_cmd(sh_path, project_file):
    """Get the command to launch IDE.

    Args:
        sh_path: The idea.sh path where IDE is installed.
        project_file: The path of IntelliJ IDEA project file.

    Returns:
        A string: The IDE launching command.
    """
    # In command usage, the space ' ' should be '\ ' for correctness.
    return ' '.join([
        constant.NOHUP, sh_path.replace(' ', r'\ '), project_file,
        constant.IGNORE_STD_OUT_ERR_CMD
    ])


def _get_scripts_from_file_path(input_path, ide_file_name):
    """Get IDE executable script file from input file path.

    Args:
        input_path: the file path to be checked.
        ide_file_name: the IDE executable script file name.

    Returns:
        A list of the IDE executable script path if exists otherwise None.
    """
    if os.path.basename(input_path).startswith(ide_file_name):
        files_found = glob.glob(input_path)
        if files_found:
            return sorted(files_found)
    return None


def get_scripts_from_dir_path(input_path, ide_file_name):
    """Get an IDE executable script file from input directory path.

    Args:
        input_path: the directory to be searched.
        ide_file_name: the IDE executable script file name.

    Returns:
        A list of an IDE executable script paths if exist otherwise None.
    """
    logging.debug('Call get_scripts_from_dir_path with %s, and %s', input_path,
                  ide_file_name)
    files_found = list(_walk_tree_find_ide_exe_file(input_path,
                                                    ide_file_name + '*'))
    if files_found:
        return sorted(files_found)
    return None


def launch_ide(project_path, run_ide_cmd, ide_name):
    """Launches relative IDE by opening the passed project file.

    Args:
        project_path: The full path of the IDE project content.
        run_ide_cmd: The command to launch IDE.
        ide_name: the IDE name is to be launched.
    """
    assert project_path, 'Empty content path is not allowed.'
    if ide_name == constant.IDE_ECLIPSE:
        logging.info(
            'Launch %s with workspace: %s.', ide_name, constant.ECLIPSE_WS)
    else:
        logging.info('Launch %s for project content path: %s.', ide_name,
                     project_path)
    _run_ide_sh(run_ide_cmd, project_path)


def is_intellij_project(project_path):
    """Checks if the path passed in is an IntelliJ project content.

    Args:
        project_path: The full path of IDEA project content, which contains
        .idea folder and .iml file(s).

    Returns:
        True if project_path is an IntelliJ project, False otherwise.
    """
    if not os.path.isfile(project_path):
        return os.path.isdir(project_path) and os.path.isdir(
            os.path.join(project_path, _IDEA_FOLDER))

    _, ext = os.path.splitext(os.path.basename(project_path))
    if ext and _IML_EXTENSION == ext.lower():
        path = os.path.dirname(project_path)
        logging.debug('Extracted path is: %s.', path)
        return os.path.isdir(os.path.join(path, _IDEA_FOLDER))
    return False


def get_script_from_input_path(input_path, ide_file_name):
    """Get correct IntelliJ executable script path from input path.

    1. If input_path is a file, check if it is an IDE executable script file.
    2. It input_path is a directory, search if it contains IDE executable script
       file(s).

    Args:
        input_path: input path to be checked if it's an IDE executable
                    script.
        ide_file_name: the IDE executable script file name.

    Returns:
        IDE executable file(s) if exists otherwise None.
    """
    if not input_path:
        return None
    ide_path = []
    if os.path.isfile(input_path):
        ide_path = _get_scripts_from_file_path(input_path, ide_file_name)
    if os.path.isdir(input_path):
        ide_path = get_scripts_from_dir_path(input_path, ide_file_name)
    if ide_path:
        logging.debug('IDE installed path from user input: %s.', ide_path)
        return ide_path
    return None


def get_intellij_version_path(version_path):
    """Locates the IntelliJ IDEA launch script path by version.

    Args:
        version_path: IntelliJ CE or UE version launch script path.

    Returns:
        A list of the sh full paths, or None if no such IntelliJ version is
        installed.
    """
    ls_output = glob.glob(version_path, recursive=True)
    if not ls_output:
        return None
    ls_output = sorted(ls_output, reverse=True)
    logging.debug('Result for checking IntelliJ path %s after sorting:%s.',
                  version_path, ls_output)
    return ls_output


def ask_preference(all_versions, ide_name):
    """Ask users which version they prefer.

    Args:
        all_versions: A list of all CE and UE version launch script paths.
        ide_name: The IDE name is going to be launched.

    Returns:
        An users selected version.
    """
    options = []
    for i, sfile in enumerate(all_versions, 1):
        options.append('\t{}. {}'.format(i, sfile))
    query = ('You installed {} versions of {}:\n{}\nPlease select '
             'one.\t').format(len(all_versions), ide_name, '\n'.join(options))
    return _select_intellij_version(query, all_versions)


def _select_intellij_version(query, all_versions):
    """Select one from different IntelliJ versions users installed.

    Args:
        query: The query message.
        all_versions: A list of all CE and UE version launch script paths.
    """
    all_numbers = []
    for i in range(len(all_versions)):
        all_numbers.append(str(i + 1))
    input_data = input(query)
    while input_data not in all_numbers:
        input_data = input('Please select a number:\t')
    return all_versions[int(input_data) - 1]
