#!/usr/bin/env python3
#
# Copyright 2019, The Android Open Source Project
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

"""Module Info class used to hold cached merged_module_info.json.json."""

import logging
import os

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import module_info_util
from aidegen.lib.singleton import Singleton

from atest import constants
from atest import module_info


class AidegenModuleInfo(module_info.ModuleInfo, metaclass=Singleton):
    """Class that offers fast/easy lookup for Module related details."""

    @staticmethod
    def _discover_mod_file_and_target(force_build):
        """Find the module file.

        If force_build is True, we'll remove module_bp_java_deps.json first and
        let module_info_util.generate_merged_module_info regenerate it again.

        Args:
            force_build: Boolean to indicate if we should rebuild the
                         module_info file regardless if it's created or not.

        Returns:
            Tuple of the relative and absolute paths of the merged module info
            file.
        """
        module_file_path = common_util.get_blueprint_json_path(
            constant.BLUEPRINT_JAVA_JSONFILE_NAME)
        if force_build and os.path.isfile(module_file_path):
            os.remove(module_file_path)
        merged_file_path = os.path.join(common_util.get_soong_out_path(),
                                        constant.MERGED_MODULE_INFO)
        if not os.path.isfile(module_file_path):
            logging.debug(
                'Generating %s - this is required for the initial runs.',
                merged_file_path)
        data = module_info_util.generate_merged_module_info()
        common_util.dump_json_dict(merged_file_path, data)
        merged_file_rel_path = os.path.relpath(
            merged_file_path, common_util.get_android_root_dir())
        return merged_file_rel_path, merged_file_path

    @staticmethod
    def is_target_module(mod_info):
        """Determine if the module is a target module.

        Determine if a module's class is in TARGET_CLASSES.

        Args:
            mod_info: A module's module-info dictionary to be checked.

        Returns:
            A boolean, true if it is a target module, otherwise false.
        """
        if mod_info:
            return any(
                x in mod_info.get(constants.MODULE_CLASS, [])
                for x in constant.TARGET_CLASSES)
        return False

    @staticmethod
    def is_project_path_relative_module(mod_info, rel_path):
        """Determine if the given project path is relative to the module.

        The rules:
           1. If constant.KEY_PATH not in mod_info, we can't tell if it's a
              module return False.
           2. If rel_path is empty, it's under Android root, return True.
           3. If module's path equals or starts with rel_path return True,
              otherwise return False.

        Args:
            mod_info: the module-info dictionary of the checked module.
            rel_path: project's relative path

        Returns:
            True if it's the given project path is relative to the module,
            otherwise False.
        """
        if constant.KEY_PATH not in mod_info:
            return False
        path = mod_info[constant.KEY_PATH][0]
        if rel_path == '':
            return True
        if (constant.KEY_CLASS in mod_info
                and common_util.is_source_under_relative_path(path, rel_path)):
            return True
        return False
