#!/usr/bin/env python3
#
# Copyright 2020 - The Android Open Source Project
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

"""It is an AIDEGen sub task: generate the .code_workspace file.
    Usage:
        path = vscode_workspace_file_gen.generate_code_workspace_file(abs_paths)
        _launch_ide(ide_util_obj, path)
"""

import os

from aidegen import constant
from aidegen.lib import common_util

_NAME = 'name'
_FOLDERS = 'folders'
_VSCODE_WORKSPACE_EXT = '.code-workspace'


def generate_code_workspace_file(abs_paths):
    """Generates .code-workspace file to launch multiple projects in VSCode.

    The rules:
        1. Get all folder names.
        2. Get file name and absolute file path for .code-workspace file.
        3. Generate .code-workspace file content.
        4. Create .code-workspace file.

    Args:
        abs_paths: A list of absolute paths of all modules in the projects.

    Returns:
        A string of the absolute path of ${project}.code-workspace file.
    """
    workspace_dict = {_FOLDERS: []}
    root_dir = common_util.get_android_root_dir()
    for path in abs_paths:
        name = _get_unique_project_name(path, root_dir)
        workspace_dict[_FOLDERS].append({_NAME: name, constant.KEY_PATH: path})
    return _create_code_workspace_file_content(workspace_dict)


def _create_code_workspace_file_content(workspace_dict):
    """Create '${project}.code-workspace' file content with workspace_dict.

    Args:
        workspace_dict: A dictionary contains the 'folders', 'name', 'path'
                        formats. e.g.,
                        {
                            "folders": [
                                {
                                     "name": "frameworks.base",
                                     "path": "/usr/aosp/frameworks/base"
                                },
                                {
                                     "name": "packages.apps.Settings",
                                     "path": "/usr/aosp/packages/apps/Settings"
                                }
                            ]
                        }
    Returns:
        A string of the absolute path of ${project}.code-workspace file.
    """
    folder_name = workspace_dict[_FOLDERS][0][_NAME]
    abs_path = workspace_dict[_FOLDERS][0][constant.KEY_PATH]
    file_name = ''.join([folder_name, _VSCODE_WORKSPACE_EXT])
    file_path = os.path.join(abs_path, file_name)
    common_util.dump_json_dict(file_path, workspace_dict)
    return file_path


def _get_unique_project_name(abs_path, root_dir):
    """Gets an unique project name from the project absolute path.

    If it's the whole Android source case, replace the relative path '.' with
    the root folder name.

    Args:
        abs_path: A string of the project absolute path.
        root_dir: A string of the absolute Android root path.

    Returns:
        A string of an unique project name.
    """
    unique_name = os.path.relpath(abs_path, root_dir).replace(os.sep, '.')
    if unique_name == '.':
        unique_name = os.path.basename(root_dir)
    return unique_name
