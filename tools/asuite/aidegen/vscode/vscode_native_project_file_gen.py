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

"""It is an AIDEGen sub task: generate VSCode related config files."""

import logging
import os

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import native_module_info
from atest import constants

_C_CPP_PROPERTIES_CONFIG_FILE_NAME = 'c_cpp_properties.json'
_ENV = 'env'
_MY_DEFAULT_INCLUDE_PATH = 'myDefaultIncludePath'
_MY_COMPILER_PATH = 'myCompilerPath'
_CONFIG = 'configurations'
_NAME = 'name'
_LINUX = 'Linux'
_INTELL_SENSE = 'intelliSenseMode'
_GCC_X64 = 'clang-x64'
_INC_PATH = 'includePath'
_SYS_INC_PATH = 'systemIncludePath'
_COMPILE_PATH = 'compilerPath'
_C_STANDARD = 'cStandard'
_C_11 = 'c11'
_CPP_STANDARD = 'cppStandard'
_CPP_17 = 'c++17'
_COMPILE_CMD = 'compileCommands'
_COMPILE_COMMANDS_FILE_DIR = 'out/soong/development/ide/compdb'
_COMPILE_COMMANDS_FILE_NAME = 'compile_commands.json'
_BROWSE = 'browse'
_PATH = 'path'
_WORKSPACE_DIR = 'workspaceFolder'
_LIMIT_SYM = 'limitSymbolsToIncludedHeaders'
_DB_FILE_NAME = 'databaseFilename'
_COMPILER_PATH = '/usr/bin/gcc'
_COMPILER_EMPTY = '"compilerPath" is empty and will skip querying a compiler.'
_FALSE = 'false'
_INTELI_SENSE_ENGINE = 'C_Cpp.intelliSenseEngine'
_DEFAULT = 'Default'


class VSCodeNativeProjectFileGenerator:
    """VSCode native project file generator.

    Attributes:
        mod_dir: A string of native project path.
        config_dir: A string of native project's configuration path.
    """

    def __init__(self, mod_dir):
        """VSCodeNativeProjectFileGenerator initialize.

        Args:
            mod_dir: An absolute path of native project directory.
        """
        self.mod_dir = mod_dir
        config_dir = os.path.join(mod_dir, constant.VSCODE_CONFIG_DIR)
        if not os.path.isdir(config_dir):
            os.mkdir(config_dir)
        self.config_dir = config_dir

    def generate_c_cpp_properties_json_file(self):
        """Generates c_cpp_properties.json file for VSCode project."""
        native_mod_info = native_module_info.NativeModuleInfo()
        root_dir = common_util.get_android_root_dir()
        mod_names = native_mod_info.get_module_names_in_targets_paths(
            [os.path.relpath(self.mod_dir, root_dir)])
        data = self._create_c_cpp_properties_dict(native_mod_info, mod_names)
        common_util.dump_json_dict(
            os.path.join(self.config_dir, _C_CPP_PROPERTIES_CONFIG_FILE_NAME),
            data)

    def _create_c_cpp_properties_dict(self, native_mod_info, mod_names):
        """Creates the dictionary of 'c_cpp_properties.json' file.

        Args:
            native_mod_info: An instance of native_module_info.NativeModuleInfo
                             class.
            mod_names: A list of module names.

        Returns:
            A dictionary contains the formats of c_cpp_properties.json file.
        """
        configs = {}
        configs[_NAME] = _LINUX
        includes = set()
        for mod_name in mod_names:
            includes.update(native_mod_info.get_module_includes(mod_name))
        browse = {_LIMIT_SYM: _FALSE, _INTELI_SENSE_ENGINE: _DEFAULT}
        if includes:
            paths = _make_header_file_paths(includes)
            configs[_INC_PATH] = paths
            browse[_PATH] = paths
        configs[_COMPILE_PATH] = ''
        if os.path.isfile(_COMPILER_PATH):
            configs[_COMPILE_PATH] = _COMPILER_PATH
        if not configs[_COMPILE_PATH]:
            logging.warning(_COMPILER_EMPTY)
        configs[_C_STANDARD] = _C_11
        configs[_CPP_STANDARD] = _CPP_17
        root_var = _make_var(constants.ANDROID_BUILD_TOP)
        configs[_COMPILE_CMD] = os.path.join(
            root_var, _COMPILE_COMMANDS_FILE_DIR, _COMPILE_COMMANDS_FILE_NAME)
        configs[_BROWSE] = browse
        configs[_INTELL_SENSE] = _GCC_X64
        data = {}
        data[_CONFIG] = [configs]
        return data


def _make_var(text):
    """Adds '${}' to make the text become a variable.

    Args:
        text: A string of text to be added '${}'.

    Returns:
        A string after adding '${}'.
    """
    return ''.join(['${', text, '}'])


def _make_header_file_paths(paths):
    """Adds prefix '${ANDROID_BUILD_TOP}' and suffix '**/*.h' to a path.

    Args:
        paths: An iterative set of relative paths' strings.

    Returns:
        A list of new paths after adding prefix and suffix.
    """
    header_list = []
    for path in paths:
        header_list.append(os.path.join(
            _make_var(constants.ANDROID_BUILD_TOP), path))
    return header_list
