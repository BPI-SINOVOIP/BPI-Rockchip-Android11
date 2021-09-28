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

"""native_module_info

Module Info class used to hold cached module_bp_cc_deps.json.
"""

import logging
import os
import re

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import module_info

_CLANG = 'clang'
_CPPLANG = 'clang++'
_MODULES = 'modules'
_INCLUDE_TAIL = '_genc++_headers'
_SRC_GEN_CHECK = r'^out/soong/.intermediates/.+/gen/.+\.(c|cc|cpp)'
_INC_GEN_CHECK = r'^out/soong/.intermediates/.+/gen($|/.+)'


class NativeModuleInfo(module_info.AidegenModuleInfo):
    """Class that offers fast/easy lookup for module related details.

    Class Attributes:
        c_lang_path: Make C files compiler path.
        cpp_lang_path: Make C++ files compiler path.
    """

    c_lang_path = ''
    cpp_lang_path = ''

    def __init__(self, force_build=False, module_file=None):
        """Initialize the NativeModuleInfo object.

        Load up the module_bp_cc_deps.json file and initialize the helper vars.
        """
        if not module_file:
            module_file = common_util.get_blueprint_json_path(
                constant.BLUEPRINT_CC_JSONFILE_NAME)
        if not os.path.isfile(module_file):
            force_build = True
        super().__init__(force_build, module_file)

    def _load_module_info_file(self, force_build, module_file):
        """Load the module file.

        Args:
            force_build: Boolean to indicate if we should rebuild the
                         module_info file regardless if it's created or not.
            module_file: String of path to file to load up. Used for testing.

        Returns:
            Tuple of module_info_target and dict of json.
        """
        if force_build:
            self._discover_mod_file_and_target(True)
        mod_info = common_util.get_json_dict(module_file)
        NativeModuleInfo.c_lang_path = mod_info.get(_CLANG, '')
        NativeModuleInfo.cpp_lang_path = mod_info.get(_CPPLANG, '')
        name_to_module_info = mod_info.get(_MODULES, {})
        root_dir = common_util.get_android_root_dir()
        module_info_target = os.path.relpath(module_file, root_dir)
        return module_info_target, name_to_module_info

    def get_module_names_in_targets_paths(self, targets):
        """Gets module names exist in native_module_info.

        Args:
            targets: A list of build targets to be checked.

        Returns:
            A list of native projects' names if native projects exist otherwise
            return None.
        """
        projects = []
        for target in targets:
            if target == constant.WHOLE_ANDROID_TREE_TARGET:
                print('Do not deal with whole source tree in native projects.')
                continue
            rel_path, _ = common_util.get_related_paths(self, target)
            for path in self.path_to_module_info:
                if common_util.is_source_under_relative_path(path, rel_path):
                    projects.extend(self.get_module_names(path))
        return projects

    def get_module_includes(self, mod_name):
        """Gets module's include paths from module name.

        The include paths contain in 'header_search_path' and
        'system_search_path' of all flags in native module info.

        Args:
            mod_name: A string of module name.

        Returns:
            A set of module include paths relative to android root.
        """
        includes = set()
        mod_info = self.name_to_module_info.get(mod_name, {})
        if not mod_info:
            logging.warning('%s module name %s does not exist.',
                            common_util.COLORED_INFO('Warning:'), mod_name)
            return includes
        for flag in mod_info:
            for header in (constant.KEY_HEADER, constant.KEY_SYSTEM):
                if header in mod_info[flag]:
                    includes.update(set(mod_info[flag][header]))
        return includes

    def is_module_need_build(self, mod_name):
        """Checks if a module need to be built by its module name.

        If a module's source files or include files contain a path looks like,
        'out/soong/.intermediates/../gen/sysprop/charger.sysprop.cpp' or
        'out/soong/.intermediates/../android.bufferhub@1.0_genc++_headers/gen'
        and the paths do not exist, that means the module needs to be built to
        generate relative source or include files.

        Args:
            mod_name: A string of module name.

        Returns:
            A boolean, True if it needs to be generated else False.
        """
        mod_info = self.name_to_module_info.get(mod_name, {})
        if not mod_info:
            logging.warning('%s module name %s does not exist.',
                            common_util.COLORED_INFO('Warning:'), mod_name)
            return False
        if self._is_source_need_build(mod_info):
            return True
        if self._is_include_need_build(mod_info):
            return True
        return False

    def _is_source_need_build(self, mod_info):
        """Checks if a module's source files need to be built.

        If a module's source files contain a path looks like,
        'out/soong/.intermediates/../gen/sysprop/charger.sysprop.cpp'
        and the paths do not exist, that means the module needs to be built to
        generate relative source files.

        Args:
            mod_info: A dictionary of module info to check.

        Returns:
            A boolean, True if it needs to be generated else False.
        """
        if constant.KEY_SRCS not in mod_info:
            return False
        for src in mod_info[constant.KEY_SRCS]:
            if re.search(_INC_GEN_CHECK, src) and not os.path.isfile(src):
                return True
        return False

    def _is_include_need_build(self, mod_info):
        """Checks if a module needs to be built by its module name.

        If a module's include files contain a path looks like,
        'out/soong/.intermediates/../android.bufferhub@1.0_genc++_headers/gen'
        and the paths do not exist, that means the module needs to be built to
        generate relative include files.

        Args:
            mod_info: A dictionary of module info to check.

        Returns:
            A boolean, True if it needs to be generated else False.
        """
        for flag in mod_info:
            for header in (constant.KEY_HEADER, constant.KEY_SYSTEM):
                if header not in mod_info[flag]:
                    continue
                for include in mod_info[flag][header]:
                    match = re.search(_INC_GEN_CHECK, include)
                    if match and not os.path.isdir(include):
                        return True
        return False

    def is_suite_in_compatibility_suites(self, suite, mod_info):
        """Check if suite exists in the compatibility_suites of module-info.

        Args:
            suite: A string of suite name.
            mod_info: Dict of module info to check.

        Returns:
            True if it exists in mod_info, False otherwise.
        """
        raise NotImplementedError()

    def get_testable_modules(self, suite=None):
        """Return the testable modules of the given suite name.

        Args:
            suite: A string of suite name. Set to None to return all testable
            modules.

        Returns:
            List of testable modules. Empty list if non-existent.
            If suite is None, return all the testable modules in module-info.
        """
        raise NotImplementedError()

    def is_testable_module(self, mod_info):
        """Check if module is something we can test.

        A module is testable if:
          - it's installed, or
          - it's a robolectric module (or shares path with one).

        Args:
            mod_info: Dict of module info to check.

        Returns:
            True if we can test this module, False otherwise.
        """
        raise NotImplementedError()

    def has_test_config(self, mod_info):
        """Validate if this module has a test config.

        A module can have a test config in the following manner:
          - AndroidTest.xml at the module path.
          - test_config be set in module-info.json.
          - Auto-generated config via the auto_test_config key in
            module-info.json.

        Args:
            mod_info: Dict of module info to check.

        Returns:
            True if this module has a test config, False otherwise.
        """
        raise NotImplementedError()

    def get_robolectric_test_name(self, module_name):
        """Returns runnable robolectric module name.

        There are at least 2 modules in every robolectric module path, return
        the module that we can run as a build target.

        Arg:
            module_name: String of module.

        Returns:
            String of module that is the runnable robolectric module, None if
            none could be found.
        """
        raise NotImplementedError()

    def is_robolectric_test(self, module_name):
        """Check if module is a robolectric test.

        A module can be a robolectric test if the specified module has their
        class set as ROBOLECTRIC (or shares their path with a module that does).

        Args:
            module_name: String of module to check.

        Returns:
            True if the module is a robolectric module, else False.
        """
        raise NotImplementedError()

    def is_auto_gen_test_config(self, module_name):
        """Check if the test config file will be generated automatically.

        Args:
            module_name: A string of the module name.

        Returns:
            True if the test config file will be generated automatically.
        """
        raise NotImplementedError()

    def is_robolectric_module(self, mod_info):
        """Check if a module is a robolectric module.

        Args:
            mod_info: ModuleInfo to check.

        Returns:
            True if module is a robolectric module, False otherwise.
        """
        raise NotImplementedError()

    def is_native_test(self, module_name):
        """Check if the input module is a native test.

        Args:
            module_name: A string of the module name.

        Returns:
            True if the test is a native test, False otherwise.
        """
        raise NotImplementedError()
