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

"""Native project information."""

from __future__ import absolute_import

from aidegen.lib import native_module_info
from aidegen.lib import project_config
from aidegen.lib import project_info


class NativeProjectInfo():
    """Native project information.

    Class attributes:
        modules_info: An AidegenModuleInfo instance whose name_to_module_info is
                      a dictionary of module_bp_cc_deps.json.

    Attributes:
        module_names: A list of the native project's module names.
        need_builds: A set of module names need to be built.
    """

    modules_info = None

    def __init__(self, target):
        """ProjectInfo initialize.

        Args:
            target: A native module or project path from users' input will be
                    checked if they contain include paths need to be generated,
                    e.g., in 'libui' module.
            'out/soong/../android.frameworks.bufferhub@1.0_genc++_headers/gen'
                    we should call 'm android.frameworks.bufferhub@1.0' to
                    generate the include header files in,
                    'android.frameworks.bufferhub@1.0_genc++_headers/gen'
                    direcotry.
        """
        self.module_names = [target] if self.modules_info.is_module(
            target) else self.modules_info.get_module_names_in_targets_paths(
                [target])
        self.need_builds = {
            mod_name
            for mod_name in self.module_names
            if self.modules_info.is_module_need_build(mod_name)
        }

    @classmethod
    def _init_modules_info(cls):
        """Initializes the class attribute: modules_info."""
        if cls.modules_info:
            return
        cls.modules_info = native_module_info.NativeModuleInfo()

    @classmethod
    def generate_projects(cls, targets):
        """Generates a list of projects in one time by a list of module names.

        The method will collect all needed to build modules and build their
        source and include files for them. But if users set the skip build flag
        it won't build anything.
        Usage:
            Call this method before native IDE project files are generated.
            For example,
            native_project_info.NativeProjectInfo.generate_projects(targets)
            native_project_file = native_util.generate_clion_projects(targets)
            ...

        Args:
            targets: A list of native modules or project paths which will be
                     checked if they contain source or include paths need to be
                     generated.
        """
        config = project_config.ProjectConfig.get_instance()
        if config.is_skip_build:
            return
        cls._init_modules_info()
        need_builds = cls._get_need_builds(targets)
        if need_builds:
            project_info.batch_build_dependencies(need_builds)

    @classmethod
    def _get_need_builds(cls, targets):
        """Gets need to be built modules from targets.

        Args:
            targets: A list of native modules or project paths which will be
                     checked if they contain source or include paths need to be
                     generated.

        Returns:
            A set of module names which need to be built.
        """
        need_builds = set()
        for target in targets:
            project = NativeProjectInfo(target)
            if project.need_builds:
                need_builds.update(project.need_builds)
        return need_builds
