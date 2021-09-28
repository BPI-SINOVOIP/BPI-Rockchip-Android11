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

"""Project config class."""

import os

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import errors

SKIP_BUILD_INFO = ('If you are sure the related modules and dependencies have '
                   'been already built, please try to use command {} to skip '
                   'the building process.')
_SKIP_BUILD_CMD = 'aidegen {} -s'
_SKIP_BUILD_WARN = (
    'You choose "--skip-build". Skip building jar and module might increase '
    'the risk of the absence of some jar or R/AIDL/logtags java files and '
    'cause the red lines to appear in IDE tool.')
_INSTANCE_NOT_EXIST_ERROR = ('The instance of {} does not exist. Please '
                             'initialize it before using.')


class ProjectConfig():
    """A singleton class manages AIDEGen's configurations.

    ProjectConfig is a singleton class that can be accessed in other modules.

    Usage:
        1. Main module should do it once by instantiating a ProjectConfig with
           users' input arguments and calling init_environment().
           args = aidegen_main.main(sys.argv[1:])
           project_config.ProjectConfig(args).init_environment()
        2. All others can get the ProjectConfig instance by calling
           get_instance().
           project_config.ProjectConfig.get_instance()

    Class attributes:
        _instance: A singleton instance of ProjectConfig.

    Attributes:
        ide_name: The IDE name which user prefer to launch.
        is_launch_ide: A boolean for launching IDE in the end of AIDEGen.
        depth: The depth of module referenced by source.
        full_repo: A boolean decides import whole Android source repo.
        is_skip_build: A boolean decides skipping building jars or modules.
        targets: A string list with Android module names or paths.
        verbose: A boolean. If true, display DEBUG level logs.
        ide_installed_path: A string of IDE installed path.
        config_reset: A boolean if true to reset all saved configurations.
        atest_module_info: A ModuleInfo instance.
    """

    _instance = None

    def __init__(self, args):
        """ProjectConfig initialize.

        Args:
            An argparse.Namespace object holds parsed args.
        """
        self.ide_name = constant.IDE_NAME_DICT[args.ide[0]]
        self.is_launch_ide = not args.no_launch
        self.depth = args.depth
        self.full_repo = args.android_tree
        self.is_skip_build = args.skip_build
        self.targets = args.targets.copy()
        self.verbose = args.verbose
        self.ide_installed_path = args.ide_installed_path
        self.config_reset = args.config_reset
        self.exclude_paths = args.exclude_paths
        self.atest_module_info = None
        ProjectConfig._instance = self

    def init_environment(self):
        """Initialize the environment settings for the whole project."""
        self._show_skip_build_msg()
        self.atest_module_info = common_util.get_atest_module_info(self.targets)
        self.exclude_paths = _transform_exclusive_paths(
            self.atest_module_info, self.exclude_paths)
        self.targets = _check_whole_android_tree(self.targets, self.full_repo)
        self.full_repo = (self.targets[0] == constant.WHOLE_ANDROID_TREE_TARGET)

    def _show_skip_build_msg(self):
        """Display different messages if users skip building targets or not."""
        if self.is_skip_build:
            print('\n{} {}\n'.format(
                common_util.COLORED_INFO('Warning:'), _SKIP_BUILD_WARN))
        else:
            msg = SKIP_BUILD_INFO.format(
                common_util.COLORED_INFO(
                    _SKIP_BUILD_CMD.format(' '.join(self.targets))))
            print('\n{} {}\n'.format(common_util.COLORED_INFO('INFO:'), msg))

    @classmethod
    def get_instance(cls):
        """Get a singleton's instance.

        Returns:
           A singleton instance of ProjectConfig.

        Raises:
           An exception of errors.InstanceNotExistError if users didn't
           instantiate a ProjectConfig object before calling this method.
        """
        if not cls._instance:
            raise errors.InstanceNotExistError(
                _INSTANCE_NOT_EXIST_ERROR.format(str(cls)))
        return cls._instance


def _check_whole_android_tree(targets, android_tree):
    """Check if it's a building project file for the whole Android tree.

    The rules:
    1. If users command aidegen under Android root, e.g.,
       root$ aidegen
       that implies users would like to launch the whole Android tree, AIDEGen
       should set the flag android_tree True.
    2. If android_tree is True, add whole Android tree to the project.

    Args:
        targets: A list of targets to be imported.
        android_tree: A boolean, True if it's a whole Android tree case,
                      otherwise False.

    Returns:
        A list of targets to be built.
    """
    if common_util.is_android_root(os.getcwd()) and targets == ['']:
        return [constant.WHOLE_ANDROID_TREE_TARGET]
    new_targets = targets.copy()
    if android_tree:
        new_targets.insert(0, constant.WHOLE_ANDROID_TREE_TARGET)
    return new_targets


def is_whole_android_tree(targets, android_tree):
    """Checks is AIDEGen going to process whole android tree.

    Args:
        targets: A list of targets to be imported.
        android_tree: A boolean, True if it's a whole Android tree case,
                      otherwise False.
    Returns:
        A boolean, True when user is going to import whole Android tree.
    """
    return (android_tree or
            (common_util.is_android_root(os.getcwd()) and targets == ['']))


def _transform_exclusive_paths(atest_module_info, exclude_paths):
    """Transforms exclusive paths to relative paths.

    Args:
        exclude_paths: A list of strings of exclusive paths.

    Returns:
        A list of relative paths.
    """
    if not exclude_paths:
        return None
    excludes = []
    for path in exclude_paths:
        exclude_path, _ = common_util.get_related_paths(atest_module_info, path)
        excludes.append(exclude_path)
    return excludes
