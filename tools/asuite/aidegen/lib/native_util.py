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

"""native_util

This module has a collection of functions that provide helper functions for
launching native projects in IDE.
"""

import fnmatch
import os

from aidegen import constant
from aidegen.lib import clion_project_file_gen
from aidegen.lib import common_util
from aidegen.lib import native_module_info


def generate_clion_projects(targets):
    """Generates CLion projects by targets.

    Generates base CLion project file in the common parent directory of the
    targets.

    Usages:
        >>>aidegen frameworks/native/libs/ui frameworks/native/lib/gui
        the base will be generated in,
            out/development/ide/clion/frameworks/native/libs/
        >>>aidegen frameworks/native/libs/ui art/libnativeloader
        the base will be generated in,
            out/development/ide/clion/
        but we expect normally native devs rarely use it in this way.

    Args:
        targets: A list of targets to check and generate their native projects.

    Returns:
        A symbolic link CLion project file path.
    """
    cc_module_info = native_module_info.NativeModuleInfo()
    parent_dir, targets = _get_merged_native_target(cc_module_info, targets)
    module_names = []
    for target in targets:
        mod_info = cc_module_info.get_module_info(target)
        clion_gen = clion_project_file_gen.CLionProjectFileGenerator(mod_info)
        clion_gen.generate_cmakelists_file()
        module_names.append(mod_info[constant.KEY_MODULE_NAME])
    rel_path = os.path.relpath(parent_dir, common_util.get_android_root_dir())
    # If the relative path is Android root, we won't show '.' in the path.
    if rel_path == '.':
        rel_path = ''
    return clion_project_file_gen.generate_base_cmakelists_file(
        cc_module_info, rel_path, module_names)


def _find_parent(abs_path, current_parent):
    """Finds parent directory of two directories.

    Args:
        abs_path: A string of an absolute path of a directory.
        current_parent: A string of the absolute path of current parent
                        directory. It could be None int the beginning.

    Returns:
        A string of new parent directory.
    """
    if not current_parent:
        return abs_path
    if common_util.is_source_under_relative_path(abs_path, current_parent):
        return current_parent
    if common_util.is_source_under_relative_path(current_parent, abs_path):
        return abs_path
    return _find_parent(
        os.path.dirname(abs_path), os.path.dirname(current_parent))


def _filter_out_modules(targets, filter_func):
    """Filters out target from targets if it passes the filter function.

    Args:
        targets: A list of targets to be analyzed.
        filter_func: A filter function reference.

    Returns:
        A tuple of a list of filtered module target and a list of lefted
        targets.
    """
    jtargets = []
    lefts = []
    for target in targets:
        if filter_func(target):
            jtargets.append(target)
            continue
        lefts.append(target)
    return jtargets, lefts


def _get_merged_native_target(cc_module_info, targets):
    """Gets merged native parent target from original native targets.

    If a target is a module, we put it directly into the new list. If a traget
    is a path we put all the native modules under the path into the new list.

    Args:
        cc_module_info: A ModuleInfo instance contains the data of
                        module_bp_cc_deps.json.
        targets: A list of targets to be merged.

    Returns:
        A tuple of a string of merged native project's relative path and a list
        of new targets we have dealt with.
    """
    parent_folder = None
    new_targets = []
    for target in targets:
        _, abs_path = common_util.get_related_paths(cc_module_info, target)
        parent_folder = _find_parent(abs_path, parent_folder)
        if cc_module_info.is_module(target):
            new_targets.append(target)
            continue
        mod_names = cc_module_info.get_module_names_in_targets_paths([target])
        new_targets.extend(mod_names)
    return parent_folder, new_targets


def get_native_and_java_projects(atest_module_info, cc_module_info, targets):
    """Gets native and java projects from targets.

    Separates native and java projects from targets.
    1. If it's a native module, add it to native projects.
    2. If it's a java module, add it to java projects.
    3. Calls _analyze_native_and_java_projects to analyze the remaining targets.

    Args:
        atest_module_info: A ModuleInfo instance contains the merged data of
                           module-info.json and module_bp_java_deps.json.
        cc_module_info: A ModuleInfo instance contains the data of
                        module_bp_cc_deps.json.
        targets: A list of targets to be analyzed.

    Returns:
        A tuple of a list of java build targets and a list of native build
        targets.
    """
    ctargets, lefts = _filter_out_modules(targets, cc_module_info.is_module)
    jtargets, lefts = _filter_out_modules(lefts, atest_module_info.is_module)
    path_info = cc_module_info.path_to_module_info
    jtars, ctars = _analyze_native_and_java_projects(
        atest_module_info, path_info, lefts)
    ctargets.extend(ctars)
    jtargets.extend(jtars)
    return jtargets, ctargets


def _analyze_native_and_java_projects(atest_module_info, path_info, targets):
    """Analyzes native and java projects from targets.

    Args:
        atest_module_info: A ModuleInfo instance contains the merged data of
                           module-info.json and module_bp_java_deps.json.
        path_info: A dictionary contains native projects' path as key
                   and module's info dictionary as value.
        targets: A list of targets to be analyzed.

    Returns:
        A tuple of a list of java build targets and a list of native build
        targets.
    """
    jtargets = []
    ctargets = []
    for target in targets:
        rel_path, abs_path = common_util.get_related_paths(
            atest_module_info, target)
        if _check_java_file_exists(abs_path):
            jtargets.append(target)
        if _check_native_project_exists(path_info, rel_path):
            ctargets.append(target)
    return jtargets, ctargets


def _check_java_file_exists(abs_path):
    """Checks if any Java files exist in an abs_path directory.

    Args:
        abs_path: A string of absolute path of a directory to be check.

    Returns:
        True if any Java files exist otherwise False.
    """
    for _, _, filenames in os.walk(abs_path):
        if fnmatch.filter(filenames, constant.JAVA_FILES):
            return True
    return False


def _check_native_project_exists(path_to_module_info, rel_path):
    """Checks if any native project exists in a rel_path directory.

    Args:
        path_to_module_info: A dictionary contains data of relative path as key
                             and module info dictionary as value.
        rel_path: A string of relative path of a directory to be check.

    Returns:
        True if any native project exists otherwise False.
    """
    for path in path_to_module_info:
        if common_util.is_source_under_relative_path(path, rel_path):
            return True
    return False
