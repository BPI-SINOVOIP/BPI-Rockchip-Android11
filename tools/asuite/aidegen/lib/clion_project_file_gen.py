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

"""It is an AIDEGen sub task: generate the CLion project file.

    Usage example:
    json_path = common_util.get_blueprint_json_path(
        constant.BLUEPRINT_CC_JSONFILE_NAME)
    json_dict = common_util.get_soong_build_json_dict(json_path)
    if 'modules' not in json_dict:
        return
    mod_info = json_dict['modules'].get('libui', {})
    if not mod_info:
        return
    CLionProjectFileGenerator(mod_info).generate_cmakelists_file()
"""

import logging
import os

from io import StringIO
from io import TextIOWrapper

from aidegen import constant
from aidegen import templates
from aidegen.lib import common_util
from aidegen.lib import errors
from aidegen.lib import native_module_info

# Flags for writing to CMakeLists.txt section.
_GLOBAL_COMMON_FLAGS = '\n# GLOBAL ALL FLAGS:\n'
_LOCAL_COMMON_FLAGS = '\n# LOCAL ALL FLAGS:\n'
_GLOBAL_CFLAGS = '\n# GLOBAL CFLAGS:\n'
_LOCAL_CFLAGS = '\n# LOCAL CFLAGS:\n'
_GLOBAL_C_ONLY_FLAGS = '\n# GLOBAL C ONLY FLAGS:\n'
_LOCAL_C_ONLY_FLAGS = '\n# LOCAL C ONLY FLAGS:\n'
_GLOBAL_CPP_FLAGS = '\n# GLOBAL CPP FLAGS:\n'
_LOCAL_CPP_FLAGS = '\n# LOCAL CPP FLAGS:\n'
_SYSTEM_INCLUDE_FLAGS = '\n# GLOBAL SYSTEM INCLUDE FLAGS:\n'

# Keys for writing in module_bp_cc_deps.json
_KEY_GLOBAL_COMMON_FLAGS = 'global_common_flags'
_KEY_LOCAL_COMMON_FLAGS = 'local_common_flags'
_KEY_GLOBAL_CFLAGS = 'global_c_flags'
_KEY_LOCAL_CFLAGS = 'local_c_flags'
_KEY_GLOBAL_C_ONLY_FLAGS = 'global_c_only_flags'
_KEY_LOCAL_C_ONLY_FLAGS = 'local_c_only_flags'
_KEY_GLOBAL_CPP_FLAGS = 'global_cpp_flags'
_KEY_LOCAL_CPP_FLAGS = 'local_cpp_flags'
_KEY_SYSTEM_INCLUDE_FLAGS = 'system_include_flags'

# Dictionary maps keys to sections.
_FLAGS_DICT = {
    _KEY_GLOBAL_COMMON_FLAGS: _GLOBAL_COMMON_FLAGS,
    _KEY_LOCAL_COMMON_FLAGS: _LOCAL_COMMON_FLAGS,
    _KEY_GLOBAL_CFLAGS: _GLOBAL_CFLAGS,
    _KEY_LOCAL_CFLAGS: _LOCAL_CFLAGS,
    _KEY_GLOBAL_C_ONLY_FLAGS: _GLOBAL_C_ONLY_FLAGS,
    _KEY_LOCAL_C_ONLY_FLAGS: _LOCAL_C_ONLY_FLAGS,
    _KEY_GLOBAL_CPP_FLAGS: _GLOBAL_CPP_FLAGS,
    _KEY_LOCAL_CPP_FLAGS: _LOCAL_CPP_FLAGS,
    _KEY_SYSTEM_INCLUDE_FLAGS: _SYSTEM_INCLUDE_FLAGS
}

# Keys for parameter types.
_KEY_FLAG = 'flag'
_KEY_SYSTEM_ROOT = 'system_root'
_KEY_RELATIVE = 'relative_file_path'

# Constants for CMakeLists.txt.
_MIN_VERSION_TOKEN = '@MINVERSION@'
_PROJECT_NAME_TOKEN = '@PROJNAME@'
_ANDOIR_ROOT_TOKEN = '@ANDROIDROOT@'
_MINI_VERSION_SUPPORT = 'cmake_minimum_required(VERSION {})\n'
_MINI_VERSION = '3.5'
_KEY_CLANG = 'clang'
_KEY_CPPLANG = 'clang++'
_SET_C_COMPILER = 'set(CMAKE_C_COMPILER \"{}\")\n'
_SET_CXX_COMPILER = 'set(CMAKE_CXX_COMPILER \"{}\")\n'
_LIST_APPEND_HEADER = 'list(APPEND\n'
_SOURCE_FILES_HEADER = 'SOURCE_FILES'
_SOURCE_FILES_LINE = '     SOURCE_FILES\n'
_END_WITH_ONE_BLANK_LINE = ')\n'
_END_WITH_TWO_BLANK_LINES = ')\n\n'
_SET_RELATIVE_PATH = 'set({} "{} {}={}")\n'
_SET_ALL_FLAGS = 'set({} "{} {}")\n'
_ANDROID_ROOT_SYMBOL = '${ANDROID_ROOT}'
_SYSTEM = 'SYSTEM'
_INCLUDE_DIR = 'include_directories({} \n'
_SET_INCLUDE_FORMAT = '    "{}"\n'
_CMAKE_C_FLAGS = 'CMAKE_C_FLAGS'
_CMAKE_CXX_FLAGS = 'CMAKE_CXX_FLAGS'
_USR = 'usr'
_INCLUDE = 'include'
_INCLUDE_SYSTEM = 'include_directories(SYSTEM "{}")\n'
_GLOB_RECURSE_TMP_HEADERS = 'file (GLOB_RECURSE TMP_HEADERS\n'
_ALL_HEADER_FILES = '    "{}/**/*.h"\n'
_APPEND_SOURCE_FILES = "list (APPEND SOURCE_FILES ${TMP_HEADERS})\n\n"
_ADD_EXECUTABLE_HEADER = '\nadd_executable({} {})'
_PROJECT = 'project({})\n'
_ADD_SUB = 'add_subdirectory({})\n'
_DICT_EMPTY = 'mod_info is empty.'
_DICT_NO_MOD_NAME_KEY = "mod_info does not contain 'module_name' key."
_DICT_NO_PATH_KEY = "mod_info does not contain 'path' key."
_MODULE_INFO_EMPTY = 'The module info dictionary is empty.'


class CLionProjectFileGenerator:
    """CLion project file generator.

    Attributes:
        mod_info: A dictionary of the target module's info.
        mod_name: A string of module name.
        mod_path: A string of module's path.
        cc_dir: A string of generated CLion project file's directory.
        cc_path: A string of generated CLion project file's path.
    """

    def __init__(self, mod_info):
        """ProjectFileGenerator initialize.

        Args:
            mod_info: A dictionary of native module's info.
        """
        if not mod_info:
            raise errors.ModuleInfoEmptyError(_MODULE_INFO_EMPTY)
        self.mod_info = mod_info
        self.mod_name = self._get_module_name()
        self.mod_path = CLionProjectFileGenerator.get_module_path(mod_info)
        self.cc_dir = CLionProjectFileGenerator.get_cmakelists_file_dir(
            os.path.join(self.mod_path, self.mod_name))
        if not os.path.exists(self.cc_dir):
            os.makedirs(self.cc_dir)
        self.cc_path = os.path.join(self.cc_dir,
                                    constant.CLION_PROJECT_FILE_NAME)

    def _get_module_name(self):
        """Gets the value of the 'module_name' key if it exists.

        Returns:
            A string of the module's name.

        Raises:
            NoModuleNameDefinedInModuleInfoError if no 'module_name' key in
            mod_info.
        """
        mod_name = self.mod_info.get(constant.KEY_MODULE_NAME, '')
        if not mod_name:
            raise errors.NoModuleNameDefinedInModuleInfoError(
                _DICT_NO_MOD_NAME_KEY)
        return mod_name

    @staticmethod
    def get_module_path(mod_info):
        """Gets the first value of the 'path' key if it exists.

        Args:
            mod_info: A module's info dictionary.

        Returns:
            A string of the module's path.

        Raises:
            NoPathDefinedInModuleInfoError if no 'path' key in mod_info.
        """
        mod_paths = mod_info.get(constant.KEY_PATH, [])
        if not mod_paths:
            raise errors.NoPathDefinedInModuleInfoError(_DICT_NO_PATH_KEY)
        return mod_paths[0]

    @staticmethod
    @common_util.check_args(cc_path=str)
    def get_cmakelists_file_dir(cc_path):
        """Gets module's CMakeLists.txt file path to be created.

        Return a string of $OUT/development/ide/clion/${cc_path}.
        For example, if module name is 'libui'. The return path string would be:
            out/development/ide/clion/frameworks/native/libs/ui/libui

        Args:
            cc_path: A string of absolute path of module's Android.bp file.

        Returns:
            A string of absolute path of module's CMakeLists.txt file to be
            created.
        """
        return os.path.join(common_util.get_android_root_dir(),
                            common_util.get_android_out_dir(),
                            constant.RELATIVE_NATIVE_PATH, cc_path)

    def generate_cmakelists_file(self):
        """Generates CLion project file from the target module's info."""
        with open(self.cc_path, 'w') as hfile:
            self._write_cmakelists_file(hfile)

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_cmakelists_file(self, hfile):
        """Writes CLion project file content with neccessary info.

        Args:
            hfile: A file handler instance.
        """
        self._write_header(hfile)
        self._write_c_compiler_paths(hfile)
        self._write_source_files(hfile)
        self._write_cmakelists_flags(hfile)
        self._write_tail(hfile)

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_header(self, hfile):
        """Writes CLion project file's header messages.

        Args:
            hfile: A file handler instance.
        """
        content = templates.CMAKELISTS_HEADER.replace(
            _MIN_VERSION_TOKEN, _MINI_VERSION)
        content = content.replace(_PROJECT_NAME_TOKEN, self.mod_name)
        content = content.replace(
            _ANDOIR_ROOT_TOKEN, common_util.get_android_root_dir())
        hfile.write(content)

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_c_compiler_paths(self, hfile):
        """Writes CMake compiler paths for C and Cpp to CLion project file.

        Args:
            hfile: A file handler instance.
        """
        hfile.write(_SET_C_COMPILER.format(
            native_module_info.NativeModuleInfo.c_lang_path))
        hfile.write(_SET_CXX_COMPILER.format(
            native_module_info.NativeModuleInfo.cpp_lang_path))

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_source_files(self, hfile):
        """Writes source files' paths to CLion project file.

        Args:
            hfile: A file handler instance.
        """
        if constant.KEY_SRCS not in self.mod_info:
            logging.warning("No source files in %s's module info.",
                            self.mod_name)
            return
        source_files = self.mod_info[constant.KEY_SRCS]
        hfile.write(_LIST_APPEND_HEADER)
        hfile.write(_SOURCE_FILES_LINE)
        for src in source_files:
            hfile.write(''.join([_build_cmake_path(src, '    '), '\n']))
        hfile.write(_END_WITH_ONE_BLANK_LINE)

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_cmakelists_flags(self, hfile):
        """Writes all kinds of flags in CLion project file.

        Args:
            hfile: A file handler instance.
        """
        self._write_flags(hfile, _KEY_GLOBAL_COMMON_FLAGS, True, True)
        self._write_flags(hfile, _KEY_LOCAL_COMMON_FLAGS, True, True)
        self._write_flags(hfile, _KEY_GLOBAL_CFLAGS, True, True)
        self._write_flags(hfile, _KEY_LOCAL_CFLAGS, True, True)
        self._write_flags(hfile, _KEY_GLOBAL_C_ONLY_FLAGS, True, False)
        self._write_flags(hfile, _KEY_LOCAL_C_ONLY_FLAGS, True, False)
        self._write_flags(hfile, _KEY_GLOBAL_CPP_FLAGS, False, True)
        self._write_flags(hfile, _KEY_LOCAL_CPP_FLAGS, False, True)
        self._write_flags(hfile, _KEY_SYSTEM_INCLUDE_FLAGS, True, True)

    @common_util.check_args(hfile=(TextIOWrapper, StringIO))
    @common_util.io_error_handle
    def _write_tail(self, hfile):
        """Writes CLion project file content with necessary info.

        Args:
            hfile: A file handler instance.
        """
        hfile.write(
            _ADD_EXECUTABLE_HEADER.format(
                _cleanup_executable_name(self.mod_name),
                _add_dollar_sign(_SOURCE_FILES_HEADER)))

    @common_util.check_args(
        hfile=(TextIOWrapper, StringIO), key=str, cflags=bool, cppflags=bool)
    @common_util.io_error_handle
    def _write_flags(self, hfile, key, cflags, cppflags):
        """Writes CMake compiler paths of C, Cpp for different kinds of flags.

        Args:
            hfile: A file handler instance.
            key: A string of flag type, e.g., 'global_common_flags' flag.
            cflags: A boolean for setting 'CMAKE_C_FLAGS' flag.
            cppflags: A boolean for setting 'CMAKE_CXX_FLAGS' flag.
        """
        if key not in _FLAGS_DICT:
            return
        hfile.write(_FLAGS_DICT[key])
        params_dict = self._parse_compiler_parameters(key)
        if params_dict:
            _translate_to_cmake(hfile, params_dict, cflags, cppflags)

    @common_util.check_args(flag=str)
    def _parse_compiler_parameters(self, flag):
        """Parses the specific flag data from a module_info dictionary.

        Args:
            flag: The string of key flag, e.g.: _KEY_GLOBAL_COMMON_FLAGS.

        Returns:
            A dictionary with compiled parameters.
        """
        params = self.mod_info.get(flag, {})
        if not params:
            return None
        params_dict = {
            constant.KEY_HEADER: [],
            constant.KEY_SYSTEM: [],
            _KEY_FLAG: [],
            _KEY_SYSTEM_ROOT: '',
            _KEY_RELATIVE: {}
        }
        for key, value in params.items():
            params_dict[key] = value
        return params_dict


@common_util.check_args(rel_project_path=str, mod_names=list)
@common_util.io_error_handle
def generate_base_cmakelists_file(cc_module_info, rel_project_path, mod_names):
    """Generates base CLion project file for multiple CLion projects.

    We create a multiple native project file:
    {android_root}/development/ide/clion/{rel_project_path}/CMakeLists.txt
    and use this project file to generate a link:
    {android_root}/out/development/ide/clion/{rel_project_path}/CMakeLists.txt

    Args:
        cc_module_info: An instance of native_module_info.NativeModuleInfo.
        rel_project_path: A string of the base project relative path. For
                          example: frameworks/native/libs/ui.
        mod_names: A list of module names whose project were created under
                   rel_project_path.

    Returns:
        A symbolic link CLion project file path.
    """
    root_dir = common_util.get_android_root_dir()
    cc_dir = os.path.join(root_dir, constant.RELATIVE_NATIVE_PATH,
                          rel_project_path)
    cc_out_dir = os.path.join(root_dir, common_util.get_android_out_dir(),
                              constant.RELATIVE_NATIVE_PATH, rel_project_path)
    if not os.path.exists(cc_dir):
        os.makedirs(cc_dir)
    dst_path = os.path.join(cc_out_dir, constant.CLION_PROJECT_FILE_NAME)
    if os.path.islink(dst_path):
        os.unlink(dst_path)
    src_path = os.path.join(cc_dir, constant.CLION_PROJECT_FILE_NAME)
    if os.path.isfile(src_path):
        os.remove(src_path)
    with open(src_path, 'w') as hfile:
        _write_base_cmakelists_file(hfile, cc_module_info, src_path, mod_names)
    os.symlink(src_path, dst_path)
    return dst_path


@common_util.check_args(
    hfile=(TextIOWrapper, StringIO), abs_project_path=str, mod_names=list)
@common_util.io_error_handle
def _write_base_cmakelists_file(hfile, cc_module_info, abs_project_path,
                                mod_names):
    """Writes base CLion project file content.

    When we write module info into base CLion project file, first check if the
    module's CMakeLists.txt exists. If file exists, write content,
        add_subdirectory({'relative_module_path'})

    Args:
        hfile: A file handler instance.
        cc_module_info: An instance of native_module_info.NativeModuleInfo.
        abs_project_path: A string of the base project absolute path.
                          For example,
                              ${ANDROID_BUILD_TOP}/frameworks/native/libs/ui.
        mod_names: A list of module names whose project were created under
                   abs_project_path.
    """
    hfile.write(_MINI_VERSION_SUPPORT.format(_MINI_VERSION))
    project_dir = os.path.dirname(abs_project_path)
    hfile.write(_PROJECT.format(os.path.basename(project_dir)))
    root_dir = common_util.get_android_root_dir()
    for mod_name in mod_names:
        mod_info = cc_module_info.get_module_info(mod_name)
        mod_path = CLionProjectFileGenerator.get_module_path(mod_info)
        file_dir = CLionProjectFileGenerator.get_cmakelists_file_dir(
            os.path.join(mod_path, mod_name))
        file_path = os.path.join(file_dir, constant.CLION_PROJECT_FILE_NAME)
        if not os.path.isfile(file_path):
            logging.warning("%s the project file %s doesn't exist.",
                            common_util.COLORED_INFO('Warning:'), file_path)
            continue
        link_project_dir = os.path.join(root_dir,
                                        common_util.get_android_out_dir(),
                                        os.path.relpath(project_dir, root_dir))
        rel_mod_path = os.path.relpath(file_dir, link_project_dir)
        hfile.write(_ADD_SUB.format(rel_mod_path))


@common_util.check_args(
    hfile=(TextIOWrapper, StringIO), params_dict=dict, cflags=bool,
    cppflags=bool)
def _translate_to_cmake(hfile, params_dict, cflags, cppflags):
    """Translates parameter dict's contents into CLion project file format.

    Args:
        hfile: A file handler instance.
        params_dict: A dict contains data to be translated into CLion
                     project file format.
        cflags: A boolean is to set 'CMAKE_C_FLAGS' flag.
        cppflags: A boolean is to set 'CMAKE_CXX_FLAGS' flag.
    """
    _write_all_include_directories(
        hfile, params_dict[constant.KEY_SYSTEM], True)
    _write_all_include_directories(
        hfile, params_dict[constant.KEY_HEADER], False)

    if cflags:
        _write_all_relative_file_path_flags(hfile, params_dict[_KEY_RELATIVE],
                                            _CMAKE_C_FLAGS)
        _write_all_flags(hfile, params_dict[_KEY_FLAG], _CMAKE_C_FLAGS)

    if cppflags:
        _write_all_relative_file_path_flags(hfile, params_dict[_KEY_RELATIVE],
                                            _CMAKE_CXX_FLAGS)
        _write_all_flags(hfile, params_dict[_KEY_FLAG], _CMAKE_CXX_FLAGS)

    if params_dict[_KEY_SYSTEM_ROOT]:
        path = os.path.join(params_dict[_KEY_SYSTEM_ROOT], _USR, _INCLUDE)
        hfile.write(_INCLUDE_SYSTEM.format(_build_cmake_path(path)))


@common_util.check_args(hfile=(TextIOWrapper, StringIO), is_system=bool)
@common_util.io_error_handle
def _write_all_include_directories(hfile, includes, is_system):
    """Writes all included directories' paths to the CLion project file.

    Args:
        hfile: A file handler instance.
        includes: A list of included file paths.
        is_system: A boolean of whether it's a system flag.
    """
    if not includes:
        return
    _write_all_includes(hfile, includes, is_system)
    _write_all_headers(hfile, includes)


@common_util.check_args(
    hfile=(TextIOWrapper, StringIO), rel_paths_dict=dict, tag=str)
@common_util.io_error_handle
def _write_all_relative_file_path_flags(hfile, rel_paths_dict, tag):
    """Writes all relative file path flags' parameters.

    Args:
        hfile: A file handler instance.
        rel_paths_dict: A dict contains data of flag as a key and the relative
                        path string as its value.
        tag: A string of tag, such as 'CMAKE_C_FLAGS'.
    """
    for flag, path in rel_paths_dict.items():
        hfile.write(
            _SET_RELATIVE_PATH.format(tag, _add_dollar_sign(tag), flag,
                                      _build_cmake_path(path)))


@common_util.check_args(hfile=(TextIOWrapper, StringIO), flags=list, tag=str)
@common_util.io_error_handle
def _write_all_flags(hfile, flags, tag):
    """Wrties all flags to the project file.

    Args:
        hfile: A file handler instance.
        flags: A list of flag strings to be added.
        tag: A string to be added a dollar sign.
    """
    for flag in flags:
        hfile.write(_SET_ALL_FLAGS.format(tag, _add_dollar_sign(tag), flag))


def _add_dollar_sign(tag):
    """Adds dollar sign to a string, e.g.: 'ANDROID_ROOT' -> '${ANDROID_ROOT}'.

    Args:
        tag: A string to be added a dollar sign.

    Returns:
        A dollar sign added string.
    """
    return ''.join(['${', tag, '}'])


def _build_cmake_path(path, tag=''):
    """Joins tag, '${ANDROID_ROOT}' and path into a new string.

    Args:
        path: A string of a path.
        tag: A string to be added in front of a dollar sign

    Returns:
        The composed string.
    """
    return ''.join([tag, _ANDROID_ROOT_SYMBOL, os.path.sep, path])


@common_util.check_args(hfile=(TextIOWrapper, StringIO), is_system=bool)
@common_util.io_error_handle
def _write_all_includes(hfile, includes, is_system):
    """Writes all included directories' paths to the CLion project file.

    Args:
        hfile: A file handler instance.
        includes: A list of included file paths.
        is_system: A boolean of whether it's a system flag.
    """
    if not includes:
        return
    system = ''
    if is_system:
        system = _SYSTEM
    hfile.write(_INCLUDE_DIR.format(system))
    for include in includes:
        hfile.write(_SET_INCLUDE_FORMAT.format(_build_cmake_path(include)))
    hfile.write(_END_WITH_TWO_BLANK_LINES)


@common_util.check_args(hfile=(TextIOWrapper, StringIO))
@common_util.io_error_handle
def _write_all_headers(hfile, includes):
    """Writes all header directories' paths to the CLion project file.

    Args:
        hfile: A file handler instance.
        includes: A list of included file paths.
    """
    if not includes:
        return
    hfile.write(_GLOB_RECURSE_TMP_HEADERS)
    for include in includes:
        hfile.write(_ALL_HEADER_FILES.format(_build_cmake_path(include)))
    hfile.write(_END_WITH_ONE_BLANK_LINE)
    hfile.write(_APPEND_SOURCE_FILES)


def _cleanup_executable_name(mod_name):
    """Clean up an executable name to be suitable for CMake.

    Replace the last '@' of a module name with '-' and make it become a suitable
    executable name for CMake.

    Args:
        mod_name: A string of module name to be cleaned up.

    Returns:
        A string of the executable name.
    """
    return mod_name[::-1].replace('@', '-', 1)[::-1]
