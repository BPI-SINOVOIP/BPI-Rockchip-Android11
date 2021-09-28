#!/usr/bin/env python3
#
# Copyright 2018 - The Android Open Source Project
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

"""module_info_util

This module receives a module path which is relative to its root directory and
makes a command to generate two json files, one for mk files and one for bp
files. Then it will load these two json files into two json dictionaries,
merge them into one dictionary and return the merged dictionary to its caller.

Example usage:
merged_dict = generate_merged_module_info()
"""

import glob
import logging
import os
import sys

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import errors
from aidegen.lib import project_config

from atest import atest_utils

_MERGE_NEEDED_ITEMS = [
    constant.KEY_CLASS,
    constant.KEY_PATH,
    constant.KEY_INSTALLED,
    constant.KEY_DEPENDENCIES,
    constant.KEY_SRCS,
    constant.KEY_SRCJARS,
    constant.KEY_CLASSES_JAR,
    constant.KEY_TAG,
    constant.KEY_COMPATIBILITY,
    constant.KEY_AUTO_TEST_CONFIG,
    constant.KEY_MODULE_NAME,
    constant.KEY_TEST_CONFIG
]
_INTELLIJ_PROJECT_FILE_EXT = '*.iml'
_LAUNCH_PROJECT_QUERY = (
    'There exists an IntelliJ project file: %s. Do you want '
    'to launch it (yes/No)?')
_BUILD_BP_JSON_ENV_OFF = {
    constant.GEN_JAVA_DEPS: 'false',
    constant.GEN_CC_DEPS: 'false',
    constant.GEN_COMPDB: 'false'
}
_BUILD_BP_JSON_ENV_ON = {
    constant.GEN_JAVA_DEPS: 'true',
    constant.GEN_CC_DEPS: 'true',
    constant.GEN_COMPDB: 'true'
}
_GEN_JSON_FAILED = (
    'Generate new {0} failed, AIDEGen will proceed and reuse the old {1}.')
_WARN_MSG = '\n{} {}\n'
_TARGET = 'nothing'


# pylint: disable=dangerous-default-value
@common_util.back_to_cwd
@common_util.time_logged
def generate_merged_module_info(env_off=_BUILD_BP_JSON_ENV_OFF,
                                env_on=_BUILD_BP_JSON_ENV_ON):
    """Generate a merged dictionary.

    Linked functions:
        _build_bp_info(module_info, project, verbose, skip_build)
        _get_soong_build_json_dict()
        _merge_dict(mk_dict, bp_dict)

    Args:
        env_off: A dictionary of environment settings to be turned off, the
                 default value is _BUILD_BP_JSON_ENV_OFF.
        env_on: A dictionary of environment settings to be turned on, the
                default value is _BUILD_BP_JSON_ENV_ON.

    Returns:
        A merged dictionary from module-info.json and module_bp_java_deps.json.
    """
    config = project_config.ProjectConfig.get_instance()
    module_info = config.atest_module_info
    projects = config.targets
    verbose = True
    skip_build = config.is_skip_build
    main_project = projects[0] if projects else None
    _build_bp_info(
        module_info, main_project, verbose, skip_build, env_off, env_on)
    json_path = common_util.get_blueprint_json_path(
        constant.BLUEPRINT_JAVA_JSONFILE_NAME)
    bp_dict = common_util.get_json_dict(json_path)
    return _merge_dict(module_info.name_to_module_info, bp_dict)


def _build_bp_info(module_info, main_project=None, verbose=False,
                   skip_build=False, env_off=_BUILD_BP_JSON_ENV_OFF,
                   env_on=_BUILD_BP_JSON_ENV_ON):
    """Make nothing to create module_bp_java_deps.json, module_bp_cc_deps.json.

    Use atest build method to build the target 'nothing' by setting env config
    SOONG_COLLECT_JAVA_DEPS to false then true. By this way, we can trigger the
    process of collecting dependencies and generate module_bp_java_deps.json.

    Args:
        module_info: A ModuleInfo instance contains data of module-info.json.
        main_project: A string of the main project name.
        verbose: A boolean, if true displays full build output.
        skip_build: A boolean, if true, skip building if
                    get_blueprint_json_path(file_name) file exists, otherwise
                    build it.
        env_off: A dictionary of environment settings to be turned off, the
                 default value is _BUILD_BP_JSON_ENV_OFF.
        env_on: A dictionary of environment settings to be turned on, the
                default value is _BUILD_BP_JSON_ENV_ON.

    Build results:
        1. Build successfully return.
        2. Build failed:
           1) There's no project file, raise BuildFailureError.
           2) There exists a project file, ask users if they want to
              launch IDE with the old project file.
              a) If the answer is yes, return.
              b) If the answer is not yes, sys.exit(1)
    """
    file_paths = _get_generated_json_files(env_on)
    files_exist = all([os.path.isfile(fpath) for fpath in file_paths])
    files = '\n'.join(file_paths)
    if skip_build and files_exist:
        logging.info('Files:\n%s exist, skipping build.', files)
        return
    original_file_mtimes = {f: None for f in file_paths}
    if files_exist:
        original_file_mtimes = {f: os.path.getmtime(f) for f in file_paths}

    logging.warning(
        '\nGenerate files:\n %s by atest build method.', files)
    build_with_off_cmd = atest_utils.build([_TARGET], verbose, env_off)
    build_with_on_cmd = atest_utils.build([_TARGET], verbose, env_on)

    if build_with_off_cmd and build_with_on_cmd:
        logging.info('\nGenerate blueprint json successfully.')
    else:
        if not all([_is_new_json_file_generated(
                f, original_file_mtimes[f]) for f in file_paths]):
            if files_exist:
                _show_files_reuse_message(file_paths)
            else:
                _show_build_failed_message(module_info, main_project)


def _get_generated_json_files(env_on=_BUILD_BP_JSON_ENV_ON):
    """Gets the absolute paths of the files which is going to be generated.

    Determine the files which will be generated by the environment on dictionary
    and the default blueprint json files' dictionary.
    The generation of json files depends on env_on. If the env_on looks like,
    _BUILD_BP_JSON_ENV_ON = {
        'SOONG_COLLECT_JAVA_DEPS': 'true',
        'SOONG_COLLECT_CC_DEPS': 'true'
    }
    We want to generate only two files: module_bp_java_deps.json and
    module_bp_cc_deps.json. And in get_blueprint_json_files_relative_dict
    function, there are three json files by default. We get the result list by
    comparsing with these two dictionaries.

    Args:
        env_on: A dictionary of environment settings to be turned on, the
                default value is _BUILD_BP_JSON_ENV_ON.

    Returns:
        A list of the absolute paths of the files which is going to be
        generated.
    """
    json_files_dict = common_util.get_blueprint_json_files_relative_dict()
    file_paths = []
    for key in env_on:
        if not env_on[key] == 'true' or key not in json_files_dict:
            continue
        file_paths.append(json_files_dict[key])
    return file_paths


def _show_files_reuse_message(file_paths):
    """Shows the message of build failure but files existing and reusing them.

    Args:
        file_paths: A list of absolute file paths to be checked.
    """
    failed_or_file = ' or '.join(file_paths)
    failed_and_file = ' and '.join(file_paths)
    message = _GEN_JSON_FAILED.format(failed_or_file, failed_and_file)
    print(_WARN_MSG.format(common_util.COLORED_INFO('Warning:'), message))


def _show_build_failed_message(module_info, main_project=None):
    """Show build failed message.

    Args:
        module_info: A ModuleInfo instance contains data of module-info.json.
        main_project: A string of the main project name.
    """
    if main_project:
        _, main_project_path = common_util.get_related_paths(
            module_info, main_project)
        _build_failed_handle(main_project_path)


def _is_new_json_file_generated(json_path, original_file_mtime):
    """Check the new file is generated or not.

    Args:
        json_path: The path of the json file being to check.
        original_file_mtime: the original file modified time.

    Returns:
        A boolean, True if the json_path file is new generated, otherwise False.
    """
    if not os.path.isfile(json_path):
        return False
    return original_file_mtime != os.path.getmtime(json_path)


def _build_failed_handle(main_project_path):
    """Handle build failures.

    Args:
        main_project_path: The main project directory.

    Handle results:
        1) There's no project file, raise BuildFailureError.
        2) There exists a project file, ask users if they want to
           launch IDE with the old project file.
           a) If the answer is yes, return.
           b) If the answer is not yes, sys.exit(1)
    """
    project_file = glob.glob(
        os.path.join(main_project_path, _INTELLIJ_PROJECT_FILE_EXT))
    if project_file:
        query = _LAUNCH_PROJECT_QUERY % project_file[0]
        input_data = input(query)
        if not input_data.lower() in ['yes', 'y']:
            sys.exit(1)
    else:
        raise errors.BuildFailureError(
            'Failed to generate %s.' % common_util.get_blueprint_json_path(
                constant.BLUEPRINT_JAVA_JSONFILE_NAME))


def _merge_module_keys(m_dict, b_dict):
    """Merge a module's dictionary into another module's dictionary.

    Merge b_dict module data into m_dict.

    Args:
        m_dict: The module dictionary is going to merge b_dict into.
        b_dict: Soong build system module dictionary.
    """
    for key, b_modules in b_dict.items():
        m_dict[key] = sorted(list(set(m_dict.get(key, []) + b_modules)))


def _copy_needed_items_from(mk_dict):
    """Shallow copy needed items from Make build system module info dictionary.

    Args:
        mk_dict: Make build system dictionary is going to be copied.

    Returns:
        A merged dictionary.
    """
    merged_dict = dict()
    for module in mk_dict.keys():
        merged_dict[module] = dict()
        for key in mk_dict[module].keys():
            if key in _MERGE_NEEDED_ITEMS and mk_dict[module][key] != []:
                merged_dict[module][key] = mk_dict[module][key]
    return merged_dict


def _merge_dict(mk_dict, bp_dict):
    """Merge two dictionaries.

    Linked function:
        _merge_module_keys(m_dict, b_dict)

    Args:
        mk_dict: Make build system module info dictionary.
        bp_dict: Soong build system module info dictionary.

    Returns:
        A merged dictionary.
    """
    merged_dict = _copy_needed_items_from(mk_dict)
    for module in bp_dict.keys():
        if module not in merged_dict.keys():
            merged_dict[module] = dict()
        _merge_module_keys(merged_dict[module], bp_dict[module])
    return merged_dict
