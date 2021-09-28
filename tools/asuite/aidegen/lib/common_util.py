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

"""common_util

This module has a collection of functions that provide helper functions to
other modules.
"""

import inspect
import json
import logging
import os
import re
import sys
import time
import xml.dom.minidom

from functools import partial
from functools import wraps
from xml.etree import ElementTree

from aidegen import constant
from aidegen.lib import errors
from atest import atest_utils
from atest import constants
from atest import module_info


COLORED_INFO = partial(
    atest_utils.colorize, color=constants.MAGENTA, highlight=False)
COLORED_PASS = partial(
    atest_utils.colorize, color=constants.GREEN, highlight=False)
COLORED_FAIL = partial(
    atest_utils.colorize, color=constants.RED, highlight=False)
FAKE_MODULE_ERROR = '{} is a fake module.'
OUTSIDE_ROOT_ERROR = '{} is outside android root.'
PATH_NOT_EXISTS_ERROR = 'The path {} doesn\'t exist.'
NO_MODULE_DEFINED_ERROR = 'No modules defined at {}.'
_REBUILD_MODULE_INFO = '%s We should rebuild module-info.json file for it.'
_ENVSETUP_NOT_RUN = ('Please run "source build/envsetup.sh" and "lunch" before '
                     'running aidegen.')
_LOG_FORMAT = '%(asctime)s %(filename)s:%(lineno)s:%(levelname)s: %(message)s'
_DATE_FORMAT = '%Y-%m-%d %H:%M:%S'
_ARG_IS_NULL_ERROR = "{0}.{1}: argument '{2}' is null."
_ARG_TYPE_INCORRECT_ERROR = "{0}.{1}: argument '{2}': type is {3}, must be {4}."


def time_logged(func=None, *, message='', maximum=1):
    """Decorate a function to find out how much time it spends.

    Args:
        func: a function is to be calculated its spending time.
        message: the message the decorated function wants to show.
        maximum: a interger, minutes. If time exceeds the maximum time show
                 message, otherwise doesn't.

    Returns:
        The wrapper function.
    """
    if func is None:
        return partial(time_logged, message=message, maximum=maximum)

    @wraps(func)
    def wrapper(*args, **kwargs):
        """A wrapper function."""

        start = time.time()
        try:
            return func(*args, **kwargs)
        finally:
            timestamp = time.time() - start
            logging.debug('{}.{} takes: {:.2f}s'.format(
                func.__module__, func.__name__, timestamp))
            if message and timestamp > maximum * 60:
                print(message)

    return wrapper


def get_related_paths(atest_module_info, target=None):
    """Get the relative and absolute paths of target from module-info.

    Args:
        atest_module_info: A ModuleInfo instance.
        target: A string user input from command line. It could be several cases
                such as:
                1. Module name, e.g. Settings
                2. Module path, e.g. packages/apps/Settings
                3. Relative path, e.g. ../../packages/apps/Settings
                4. Current directory, e.g. '.' or no argument
                5. An empty string, which added by AIDEGen, used for generating
                   the iml files for the whole Android repo tree.
                   e.g.
                   1. ~/aosp$ aidegen
                   2. ~/aosp/frameworks/base$ aidegen -a
                6. An absolute path, e.g. /usr/local/home/test/aosp

    Return:
        rel_path: The relative path of a module, return None if no matching
                  module found.
        abs_path: The absolute path of a module, return None if no matching
                  module found.
    """
    rel_path = None
    abs_path = None
    # take the command 'aidegen .' as 'aidegen'.
    if target == '.':
        target = None
    if target:
        # For the case of whole Android repo tree.
        if target == constant.WHOLE_ANDROID_TREE_TARGET:
            rel_path = ''
            abs_path = get_android_root_dir()
        # User inputs an absolute path.
        elif os.path.isabs(target):
            abs_path = target
            rel_path = os.path.relpath(abs_path, get_android_root_dir())
        # User inputs a module name.
        elif atest_module_info.is_module(target):
            paths = atest_module_info.get_paths(target)
            if paths:
                rel_path = paths[0].strip(os.sep)
                abs_path = os.path.join(get_android_root_dir(), rel_path)
        # User inputs a module path or a relative path of android root folder.
        elif (atest_module_info.get_module_names(target)
              or os.path.isdir(os.path.join(get_android_root_dir(), target))):
            rel_path = target.strip(os.sep)
            abs_path = os.path.join(get_android_root_dir(), rel_path)
        # User inputs a relative path of current directory.
        else:
            abs_path = os.path.abspath(os.path.join(os.getcwd(), target))
            rel_path = os.path.relpath(abs_path, get_android_root_dir())
    else:
        # User doesn't input.
        abs_path = os.getcwd()
        if is_android_root(abs_path):
            rel_path = ''
        else:
            rel_path = os.path.relpath(abs_path, get_android_root_dir())
    return rel_path, abs_path


def is_target_android_root(atest_module_info, targets):
    """Check if any target is the Android root path.

    Args:
        atest_module_info: A ModuleInfo instance contains data of
                           module-info.json.
        targets: A list of target modules or project paths from user input.

    Returns:
        True if target is Android root, otherwise False.
    """
    for target in targets:
        _, abs_path = get_related_paths(atest_module_info, target)
        if is_android_root(abs_path):
            return True
    return False


def is_android_root(abs_path):
    """Check if an absolute path is the Android root path.

    Args:
        abs_path: The absolute path of a module.

    Returns:
        True if abs_path is Android root, otherwise False.
    """
    return abs_path == get_android_root_dir()


def has_build_target(atest_module_info, rel_path):
    """Determine if a relative path contains buildable module.

    Args:
        atest_module_info: A ModuleInfo instance contains data of
                           module-info.json.
        rel_path: The module path relative to android root.

    Returns:
        True if the relative path contains a build target, otherwise false.
    """
    return any(
        is_source_under_relative_path(mod_path, rel_path)
        for mod_path in atest_module_info.path_to_module_info)


def _check_modules(atest_module_info, targets, raise_on_lost_module=True):
    """Check if all targets are valid build targets.

    Args:
        atest_module_info: A ModuleInfo instance contains data of
                           module-info.json.
        targets: A list of target modules or project paths from user input.
                When locating the path of the target, given a matched module
                name has priority over path. Below is the priority of locating a
                target:
                1. Module name, e.g. Settings
                2. Module path, e.g. packages/apps/Settings
                3. Relative path, e.g. ../../packages/apps/Settings
                4. Current directory, e.g. . or no argument
        raise_on_lost_module: A boolean, pass to _check_module to determine if
                ProjectPathNotExistError or NoModuleDefinedInModuleInfoError
                should be raised.

    Returns:
        True if any _check_module return flip the True/False.
    """
    for target in targets:
        if not check_module(atest_module_info, target, raise_on_lost_module):
            return False
    return True


def check_module(atest_module_info, target, raise_on_lost_module=True):
    """Check if a target is valid or it's a path containing build target.

    Args:
        atest_module_info: A ModuleInfo instance contains the data of
                module-info.json.
        target: A target module or project path from user input.
                When locating the path of the target, given a matched module
                name has priority over path. Below is the priority of locating a
                target:
                1. Module name, e.g. Settings
                2. Module path, e.g. packages/apps/Settings
                3. Relative path, e.g. ../../packages/apps/Settings
                4. Current directory, e.g. . or no argument
                5. An absolute path, e.g. /usr/local/home/test/aosp
        raise_on_lost_module: A boolean, handles if ProjectPathNotExistError or
                NoModuleDefinedInModuleInfoError should be raised.

    Returns:
        1. If there is no error _check_module always return True.
        2. If there is a error,
            a. When raise_on_lost_module is False, _check_module will raise the
               error.
            b. When raise_on_lost_module is True, _check_module will return
               False if module's error is ProjectPathNotExistError or
               NoModuleDefinedInModuleInfoError else raise the error.

    Raises:
        Raise ProjectPathNotExistError and NoModuleDefinedInModuleInfoError only
        when raise_on_lost_module is True, others don't subject to the limit.
        The rules for raising exceptions:
        1. Absolute path of a module is None -> FakeModuleError
        2. Module doesn't exist in repo root -> ProjectOutsideAndroidRootError
        3. The given absolute path is not a dir -> ProjectPathNotExistError
        4. If the given abs path doesn't contain any target and not repo root
           -> NoModuleDefinedInModuleInfoError
    """
    rel_path, abs_path = get_related_paths(atest_module_info, target)
    if not abs_path:
        err = FAKE_MODULE_ERROR.format(target)
        logging.error(err)
        raise errors.FakeModuleError(err)
    if not is_source_under_relative_path(abs_path, get_android_root_dir()):
        err = OUTSIDE_ROOT_ERROR.format(abs_path)
        logging.error(err)
        raise errors.ProjectOutsideAndroidRootError(err)
    if not os.path.isdir(abs_path):
        err = PATH_NOT_EXISTS_ERROR.format(rel_path)
        if raise_on_lost_module:
            logging.error(err)
            raise errors.ProjectPathNotExistError(err)
        logging.debug(_REBUILD_MODULE_INFO, err)
        return False
    if (not has_build_target(atest_module_info, rel_path)
            and not is_android_root(abs_path)):
        err = NO_MODULE_DEFINED_ERROR.format(rel_path)
        if raise_on_lost_module:
            logging.error(err)
            raise errors.NoModuleDefinedInModuleInfoError(err)
        logging.debug(_REBUILD_MODULE_INFO, err)
        return False
    return True


def get_abs_path(rel_path):
    """Get absolute path from a relative path.

    Args:
        rel_path: A string, a relative path to get_android_root_dir().

    Returns:
        abs_path: A string, an absolute path starts with
                  get_android_root_dir().
    """
    if not rel_path:
        return get_android_root_dir()
    if is_source_under_relative_path(rel_path, get_android_root_dir()):
        return rel_path
    return os.path.join(get_android_root_dir(), rel_path)


def is_target(src_file, src_file_extensions):
    """Check if src_file is a type of src_file_extensions.

    Args:
        src_file: A string of the file path to be checked.
        src_file_extensions: A list of file types to be checked

    Returns:
        True if src_file is one of the types of src_file_extensions, otherwise
        False.
    """
    return any(src_file.endswith(x) for x in src_file_extensions)


def get_atest_module_info(targets=None):
    """Get the right version of atest ModuleInfo instance.

    The rules:
        1. If targets is None:
           We just want to get an atest ModuleInfo instance.
        2. If targets isn't None:
           Check if the targets don't exist in ModuleInfo, we'll regain a new
           atest ModleInfo instance by setting force_build=True and call
           _check_modules again. If targets still don't exist, raise exceptions.

    Args:
        targets: A list of targets to be built.

    Returns:
        An atest ModuleInfo instance.
    """
    amodule_info = module_info.ModuleInfo()
    if targets and not _check_modules(
            amodule_info, targets, raise_on_lost_module=False):
        amodule_info = module_info.ModuleInfo(force_build=True)
        _check_modules(amodule_info, targets)
    return amodule_info


def get_blueprint_json_path(file_name):
    """Assemble the path of blueprint json file.

    Args:
        file_name: A string of blueprint json file name.

    Returns:
        The path of json file.
    """
    return os.path.join(get_soong_out_path(), file_name)


def back_to_cwd(func):
    """Decorate a function change directory back to its original one.

    Args:
        func: a function is to be changed back to its original directory.

    Returns:
        The wrapper function.
    """

    @wraps(func)
    def wrapper(*args, **kwargs):
        """A wrapper function."""
        cwd = os.getcwd()
        try:
            return func(*args, **kwargs)
        finally:
            os.chdir(cwd)

    return wrapper


def get_android_out_dir():
    """Get out directory in different conditions.

    Returns:
        Android out directory path.
    """
    android_root_path = get_android_root_dir()
    android_out_dir = os.getenv(constants.ANDROID_OUT_DIR)
    out_dir_common_base = os.getenv(constant.OUT_DIR_COMMON_BASE_ENV_VAR)
    android_out_dir_common_base = (os.path.join(
        out_dir_common_base, os.path.basename(android_root_path))
                                   if out_dir_common_base else None)
    return (android_out_dir or android_out_dir_common_base
            or constant.ANDROID_DEFAULT_OUT)


def get_android_root_dir():
    """Get Android root directory.

    If the path doesn't exist show message to ask users to run envsetup.sh and
    lunch first.

    Returns:
        Android root directory path.
    """
    android_root_path = os.environ.get(constants.ANDROID_BUILD_TOP)
    if not android_root_path:
        _show_env_setup_msg_and_exit()
    return android_root_path


def get_aidegen_root_dir():
    """Get AIDEGen root directory.

    Returns:
        AIDEGen root directory path.
    """
    return os.path.join(get_android_root_dir(), constant.AIDEGEN_ROOT_PATH)


def _show_env_setup_msg_and_exit():
    """Show message if users didn't run envsetup.sh and lunch."""
    print(_ENVSETUP_NOT_RUN)
    sys.exit(0)


def get_soong_out_path():
    """Assemble out directory's soong path.

    Returns:
        Out directory's soong path.
    """
    return os.path.join(get_android_root_dir(), get_android_out_dir(), 'soong')


def configure_logging(verbose):
    """Configure the logger.

    Args:
        verbose: A boolean. If true, display DEBUG level logs.
    """
    log_format = _LOG_FORMAT
    datefmt = _DATE_FORMAT
    level = logging.DEBUG if verbose else logging.INFO
    # Clear all handlers to prevent setting level not working.
    logging.getLogger().handlers = []
    logging.basicConfig(level=level, format=log_format, datefmt=datefmt)


def exist_android_bp(abs_path):
    """Check if the Android.bp exists under specific folder.

    Args:
        abs_path: An absolute path string.

    Returns: A boolean, true if the Android.bp exists under the folder,
             otherwise false.
    """
    return os.path.isfile(os.path.join(abs_path, constant.ANDROID_BP))


def exist_android_mk(abs_path):
    """Check if the Android.mk exists under specific folder.

    Args:
        abs_path: An absolute path string.

    Returns: A boolean, true if the Android.mk exists under the folder,
             otherwise false.
    """
    return os.path.isfile(os.path.join(abs_path, constant.ANDROID_MK))


def is_source_under_relative_path(source, relative_path):
    """Check if a source file is a project relative path file.

    Args:
        source: Android source file path.
        relative_path: Relative path of the module.

    Returns:
        True if source file is a project relative path file, otherwise False.
    """
    return re.search(
        constant.RE_INSIDE_PATH_CHECK.format(relative_path), source)


def remove_user_home_path(data):
    """Replace the user home path string with a constant string.

    Args:
        data: A string of xml content or an attributeError of error message.

    Returns:
        A string which replaced the user home path to $USER_HOME$.
    """
    return str(data).replace(os.path.expanduser('~'), constant.USER_HOME)


def io_error_handle(func):
    """Decorates a function of handling io error and raising exception.

    Args:
        func: A function is to be raised exception if writing file failed.

    Returns:
        The wrapper function.
    """

    @wraps(func)
    def wrapper(*args, **kwargs):
        """A wrapper function."""
        try:
            return func(*args, **kwargs)
        except (OSError, IOError) as err:
            print('{0}.{1} I/O error: {2}'.format(
                func.__module__, func.__name__, err))
            raise
    return wrapper


def check_args(**decls):
    """Decorates a function to check its argument types.

    Usage:
        @check_args(name=str, text=str)
        def parse_rule(name, text):
            ...

    Args:
        decls: A dictionary with keys as arguments' names and values as
             arguments' types.

    Returns:
        The wrapper function.
    """

    def decorator(func):
        """A wrapper function."""
        fmodule = func.__module__
        fname = func.__name__
        fparams = inspect.signature(func).parameters

        @wraps(func)
        def decorated(*args, **kwargs):
            """A wrapper function."""
            params = dict(zip(fparams, args))
            for arg_name, arg_type in decls.items():
                try:
                    arg_val = params[arg_name]
                except KeyError:
                    # If arg_name can't be found in function's signature, it
                    # might be a case of a partial function or default
                    # parameters, we'll neglect it.
                    if arg_name not in kwargs:
                        continue
                    arg_val = kwargs.get(arg_name)
                if arg_val is None:
                    raise TypeError(_ARG_IS_NULL_ERROR.format(
                        fmodule, fname, arg_name))
                if not isinstance(arg_val, arg_type):
                    raise TypeError(_ARG_TYPE_INCORRECT_ERROR.format(
                        fmodule, fname, arg_name, type(arg_val), arg_type))
            return func(*args, **kwargs)
        return decorated

    return decorator


@io_error_handle
def dump_json_dict(json_path, data):
    """Dumps a dictionary of data into a json file.

    Args:
        json_path: An absolute json file path string.
        data: A dictionary of data to be written into a json file.
    """
    with open(json_path, 'w') as json_file:
        json.dump(data, json_file, indent=4)


@io_error_handle
def get_json_dict(json_path):
    """Loads a json file from path and convert it into a json dictionary.

    Args:
        json_path: An absolute json file path string.

    Returns:
        A dictionary loaded from the json_path.
    """
    with open(json_path) as jfile:
        return json.load(jfile)


@io_error_handle
def read_file_line_to_list(file_path):
    """Read a file line by line and write them into a list.

    Args:
        file_path: A string of a file's path.

    Returns:
        A list of the file's content by line.
    """
    files = []
    with open(file_path, encoding='utf8') as infile:
        for line in infile:
            files.append(line.strip())
    return files


@io_error_handle
def read_file_content(path, encode_type='utf8'):
    """Read file's content.

    Args:
        path: Path of input file.
        encode_type: A string of encoding name, default to UTF-8.

    Returns:
        String: Content of the file.
    """
    with open(path, encoding=encode_type) as template:
        return template.read()


@io_error_handle
def file_generate(path, content):
    """Generate file from content.

    Args:
        path: Path of target file.
        content: String content of file.
    """
    if not os.path.exists(os.path.dirname(path)):
        os.makedirs(os.path.dirname(path))
    with open(path, 'w') as target:
        target.write(content)


def get_lunch_target():
    """Gets the Android lunch target in current console.

    Returns:
        A json format string of lunch target in current console.
    """
    product = os.environ.get(constant.TARGET_PRODUCT)
    build_variant = os.environ.get(constant.TARGET_BUILD_VARIANT)
    if product and build_variant:
        return json.dumps(
            {constant.LUNCH_TARGET: "-".join([product, build_variant])})
    return None


def get_blueprint_json_files_relative_dict():
    """Gets a dictionary with key: environment variable, value: absolute path.

    Returns:
        A dictionary  with key: environment variable and value: absolute path of
        the file generated by the environment varialbe.
    """
    data = {}
    root_dir = get_android_root_dir()
    bp_java_path = os.path.join(
        root_dir, get_blueprint_json_path(
            constant.BLUEPRINT_JAVA_JSONFILE_NAME))
    data[constant.GEN_JAVA_DEPS] = bp_java_path
    bp_cc_path = os.path.join(
        root_dir, get_blueprint_json_path(constant.BLUEPRINT_CC_JSONFILE_NAME))
    data[constant.GEN_CC_DEPS] = bp_cc_path
    data[constant.GEN_COMPDB] = os.path.join(get_soong_out_path(),
                                             constant.RELATIVE_COMPDB_PATH,
                                             constant.COMPDB_JSONFILE_NAME)
    return data


def to_pretty_xml(root, indent="  "):
    """Gets pretty xml from an xml.etree.ElementTree root.

    Args:
        root: An element tree root.
        indent: The indent of XML.
    Returns:
        A string of pretty xml.
    """
    xml_string = xml.dom.minidom.parseString(
        ElementTree.tostring(root)).toprettyxml(indent)
    # Remove the xml declaration since IntelliJ doesn't use it.
    xml_string = xml_string.split("\n", 1)[1]
    # Remove the weird newline issue from toprettyxml.
    return os.linesep.join([s for s in xml_string.splitlines() if s.strip()])


def to_boolean(str_bool):
    """Converts a string to a boolean.

    Args:
        str_bool: A string in the expression of boolean type.

    Returns:
        A boolean True if the string is one of ('True', 'true', 'T', 't', '1')
        else False.
    """
    return str_bool and str_bool.lower() in ('true', 't', '1')


def find_git_root(relpath):
    """Finds the parent directory which has a .git folder from the relpath.

    Args:
        relpath: A string of relative path.

    Returns:
        A string of the absolute path which contains a .git, otherwise, none.
    """
    dir_list = relpath.split(os.sep)
    for i in range(len(dir_list), 0, -1):
        real_path = os.path.join(get_android_root_dir(),
                                 os.sep.join(dir_list[:i]),
                                 constant.GIT_FOLDER_NAME)
        if os.path.exists(real_path):
            return os.path.dirname(real_path)
    logging.warning('%s can\'t find its .git folder.', relpath)
    return None
