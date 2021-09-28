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

"""Project information."""

from __future__ import absolute_import

import logging
import os

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import errors
from aidegen.lib import module_info
from aidegen.lib import project_config
from aidegen.lib import source_locator

from atest import atest_utils

_CONVERT_MK_URL = ('https://android.googlesource.com/platform/build/soong/'
                   '#convert-android_mk-files')
_ANDROID_MK_WARN = (
    '{} contains Android.mk file(s) in its dependencies:\n{}\nPlease help '
    'convert these files into blueprint format in the future, otherwise '
    'AIDEGen may not be able to include all module dependencies.\nPlease visit '
    '%s for reference on how to convert makefile.' % _CONVERT_MK_URL)
_ROBOLECTRIC_MODULE = 'Robolectric_all'
_NOT_TARGET = ('Module %s\'s class setting is %s, none of which is included in '
               '%s, skipping this module in the project.')
# The module fake-framework have the same package name with framework but empty
# content. It will impact the dependency for framework when referencing the
# package from fake-framework in IntelliJ.
_EXCLUDE_MODULES = ['fake-framework']
# When we use atest_utils.build(), there is a command length limit on
# soong_ui.bash. We reserve 5000 characters for rewriting the command line
# in soong_ui.bash.
_CMD_LENGTH_BUFFER = 5000
# For each argument, it need a space to separate following argument.
_BLANK_SIZE = 1
_CORE_MODULES = [constant.FRAMEWORK_ALL, constant.CORE_ALL,
                 'org.apache.http.legacy.stubs.system']


class ProjectInfo:
    """Project information.

    Users should call config_project first before starting using ProjectInfo.

    Class attributes:
        modules_info: An AidegenModuleInfo instance whose name_to_module_info is
                      combining module-info.json with module_bp_java_deps.json.

    Attributes:
        project_absolute_path: The absolute path of the project.
        project_relative_path: The relative path of the project to
                               common_util.get_android_root_dir().
        project_module_names: A set of module names under project_absolute_path
                              directory or it's subdirectories.
        dep_modules: A dict has recursively dependent modules of
                     project_module_names.
        iml_path: The project's iml file path.
        source_path: A dictionary to keep following data:
                     source_folder_path: A set contains the source folder
                                         relative paths.
                     test_folder_path: A set contains the test folder relative
                                       paths.
                     jar_path: A set contains the jar file paths.
                     jar_module_path: A dictionary contains the jar file and
                                      the module's path mapping, only used in
                                      Eclipse.
                     r_java_path: A set contains the relative path to the
                                  R.java files, only used in Eclipse.
                     srcjar_path: A source content descriptor only used in
                                  IntelliJ.
                                  e.g. out/.../aapt2.srcjar!/
                                  The "!/" is a content descriptor for
                                  compressed files in IntelliJ.
        is_main_project: A boolean to verify the project is main project.
        dependencies: A list of dependency projects' iml file names, e.g. base,
                      framework-all.
    """

    modules_info = None

    def __init__(self, target=None, is_main_project=False):
        """ProjectInfo initialize.

        Args:
            target: Includes target module or project path from user input, when
                    locating the target, project with matching module name of
                    the given target has a higher priority than project path.
            is_main_project: A boolean, default is False. True if the target is
                             the main project, otherwise False.
        """
        rel_path, abs_path = common_util.get_related_paths(
            self.modules_info, target)
        self.module_name = self.get_target_name(target, abs_path)
        self.is_main_project = is_main_project
        self.project_module_names = set(
            self.modules_info.get_module_names(rel_path))
        self.project_relative_path = rel_path
        self.project_absolute_path = abs_path
        self.iml_path = ''
        self._set_default_modues()
        self._init_source_path()
        if target == constant.FRAMEWORK_ALL:
            self.dep_modules = self.get_dep_modules([target])
        else:
            self.dep_modules = self.get_dep_modules()
        self._filter_out_modules()
        self._display_convert_make_files_message()
        self.dependencies = []

    def _set_default_modues(self):
        """Append default hard-code modules, source paths and jar files.

        1. framework: Framework module is always needed for dependencies but it
            might not always be located by module dependency.
        2. org.apache.http.legacy.stubs.system: The module can't be located
            through module dependency. Without it, a lot of java files will have
            error of "cannot resolve symbol" in IntelliJ since they import
            packages android.Manifest and com.android.internal.R.
        """
        # Set the default modules framework-all and core-all as the core
        # dependency modules.
        self.project_module_names.update(_CORE_MODULES)

    def _init_source_path(self):
        """Initialize source_path dictionary."""
        self.source_path = {
            'source_folder_path': set(),
            'test_folder_path': set(),
            'jar_path': set(),
            'jar_module_path': dict(),
            'r_java_path': set(),
            'srcjar_path': set()
        }

    def _display_convert_make_files_message(self):
        """Show message info users convert their Android.mk to Android.bp."""
        mk_set = set(self._search_android_make_files())
        if mk_set:
            print('\n{} {}\n'.format(
                common_util.COLORED_INFO('Warning:'),
                _ANDROID_MK_WARN.format(self.module_name, '\n'.join(mk_set))))

    def _search_android_make_files(self):
        """Search project and dependency modules contain Android.mk files.

        If there is only Android.mk but no Android.bp, we'll show the warning
        message, otherwise we won't.

        Yields:
            A string: the relative path of Android.mk.
        """
        if (common_util.exist_android_mk(self.project_absolute_path) and
                not common_util.exist_android_bp(self.project_absolute_path)):
            yield '\t' + os.path.join(self.project_relative_path,
                                      constant.ANDROID_MK)
        for mod_name in self.dep_modules:
            rel_path, abs_path = common_util.get_related_paths(
                self.modules_info, mod_name)
            if rel_path and abs_path:
                if (common_util.exist_android_mk(abs_path)
                        and not common_util.exist_android_bp(abs_path)):
                    yield '\t' + os.path.join(rel_path, constant.ANDROID_MK)

    def _get_modules_under_project_path(self, rel_path):
        """Find modules under the rel_path.

        Find modules whose class is qualified to be included as a target module.

        Args:
            rel_path: A string, the project's relative path.

        Returns:
            A set of module names.
        """
        logging.info('Find modules whose class is in %s under %s.',
                     constant.TARGET_CLASSES, rel_path)
        modules = set()
        for name, data in self.modules_info.name_to_module_info.items():
            if module_info.AidegenModuleInfo.is_project_path_relative_module(
                    data, rel_path):
                if module_info.AidegenModuleInfo.is_target_module(data):
                    modules.add(name)
                else:
                    logging.debug(_NOT_TARGET, name, data.get('class', ''),
                                  constant.TARGET_CLASSES)
        return modules

    def _get_robolectric_dep_module(self, modules):
        """Return the robolectric module set as dependency if any module is a
           robolectric test.

        Args:
            modules: A set of modules.

        Returns:
            A set with a robolectric_all module name if one of the modules
            needs the robolectric test module. Otherwise return empty list.
        """
        for module in modules:
            if self.modules_info.is_robolectric_test(module):
                return {_ROBOLECTRIC_MODULE}
        return set()

    def _filter_out_modules(self):
        """Filter out unnecessary modules."""
        for module in _EXCLUDE_MODULES:
            self.dep_modules.pop(module, None)

    def get_dep_modules(self, module_names=None, depth=0):
        """Recursively find dependent modules of the project.

        Find dependent modules by dependencies parameter of each module.
        For example:
            The module_names is ['m1'].
            The modules_info is
            {
                'm1': {'dependencies': ['m2'], 'path': ['path_to_m1']},
                'm2': {'path': ['path_to_m4']},
                'm3': {'path': ['path_to_m1']}
                'm4': {'path': []}
            }
            The result dependent modules are:
            {
                'm1': {'dependencies': ['m2'], 'path': ['path_to_m1']
                       'depth': 0},
                'm2': {'path': ['path_to_m4'], 'depth': 1},
                'm3': {'path': ['path_to_m1'], 'depth': 0}
            }
            Note that:
                1. m4 is not in the result as it's not among dependent modules.
                2. m3 is in the result as it has the same path to m1.

        Args:
            module_names: A set of module names.
            depth: An integer shows the depth of module dependency referenced by
                   source. Zero means the max module depth.

        Returns:
            deps: A dict contains all dependent modules data of given modules.
        """
        dep = {}
        children = set()
        if not module_names:
            module_names = self.project_module_names
            module_names.update(
                self._get_modules_under_project_path(
                    self.project_relative_path))
            module_names.update(self._get_robolectric_dep_module(module_names))
            self.project_module_names = set()
        for name in module_names:
            if (name in self.modules_info.name_to_module_info
                    and name not in self.project_module_names):
                dep[name] = self.modules_info.name_to_module_info[name]
                dep[name][constant.KEY_DEPTH] = depth
                self.project_module_names.add(name)
                if (constant.KEY_DEPENDENCIES in dep[name]
                        and dep[name][constant.KEY_DEPENDENCIES]):
                    children.update(dep[name][constant.KEY_DEPENDENCIES])
        if children:
            dep.update(self.get_dep_modules(children, depth + 1))
        return dep

    @staticmethod
    def generate_projects(targets):
        """Generate a list of projects in one time by a list of module names.

        Args:
            targets: A list of target modules or project paths from user input,
                     when locating the target, project with matched module name
                     of the target has a higher priority than project path.

        Returns:
            List: A list of ProjectInfo instances.
        """
        return [ProjectInfo(target, i == 0) for i, target in enumerate(targets)]

    @staticmethod
    def get_target_name(target, abs_path):
        """Get target name from target's absolute path.

        If the project is for entire Android source tree, change the target to
        source tree's root folder name. In this way, we give IDE project file
        a more specific name. e.g, master.iml.

        Args:
            target: Includes target module or project path from user input, when
                    locating the target, project with matching module name of
                    the given target has a higher priority than project path.
            abs_path: A string, target's absolute path.

        Returns:
            A string, the target name.
        """
        if abs_path == common_util.get_android_root_dir():
            return os.path.basename(abs_path)
        return target

    def locate_source(self, build=True):
        """Locate the paths of dependent source folders and jar files.

        Try to reference source folder path as dependent module unless the
        dependent module should be referenced to a jar file, such as modules
        have jars and jarjar_rules parameter.
        For example:
            Module: asm-6.0
                java_import {
                    name: 'asm-6.0',
                    host_supported: true,
                    jars: ['asm-6.0.jar'],
                }
            Module: bouncycastle
                java_library {
                    name: 'bouncycastle',
                    ...
                    target: {
                        android: {
                            jarjar_rules: 'jarjar-rules.txt',
                        },
                    },
                }

        Args:
            build: A boolean default to true. If false, skip building jar and
                   srcjar files, otherwise build them.

        Example usage:
            project.source_path = project.locate_source()
            E.g.
                project.source_path = {
                    'source_folder_path': ['path/to/source/folder1',
                                           'path/to/source/folder2', ...],
                    'test_folder_path': ['path/to/test/folder', ...],
                    'jar_path': ['path/to/jar/file1', 'path/to/jar/file2', ...]
                }
        """
        if not hasattr(self, 'dep_modules') or not self.dep_modules:
            raise errors.EmptyModuleDependencyError(
                'Dependent modules dictionary is empty.')
        rebuild_targets = set()
        for module_name, module_data in self.dep_modules.items():
            module = self._generate_moduledata(module_name, module_data)
            module.locate_sources_path()
            self.source_path['source_folder_path'].update(set(module.src_dirs))
            self.source_path['test_folder_path'].update(set(module.test_dirs))
            self.source_path['r_java_path'].update(set(module.r_java_paths))
            self.source_path['srcjar_path'].update(set(module.srcjar_paths))
            self._append_jars_as_dependencies(module)
            rebuild_targets.update(module.build_targets)
        config = project_config.ProjectConfig.get_instance()
        if config.is_skip_build:
            return
        if rebuild_targets:
            if build:
                batch_build_dependencies(rebuild_targets)
                self.locate_source(build=False)
            else:
                logging.warning('Jar or srcjar files build skipped:\n\t%s.',
                                '\n\t'.join(rebuild_targets))

    def _generate_moduledata(self, module_name, module_data):
        """Generate a module class to collect dependencies in IDE.

        The rules of initialize a module data instance: if ide_object isn't None
        and its ide_name is 'eclipse', we'll create an EclipseModuleData
        instance otherwise create a ModuleData instance.

        Args:
            module_name: Name of the module.
            module_data: A dictionary holding a module information.

        Returns:
            A ModuleData class.
        """
        ide_name = project_config.ProjectConfig.get_instance().ide_name
        if ide_name == constant.IDE_ECLIPSE:
            return source_locator.EclipseModuleData(
                module_name, module_data, self.project_relative_path)
        depth = project_config.ProjectConfig.get_instance().depth
        return source_locator.ModuleData(module_name, module_data, depth)

    def _append_jars_as_dependencies(self, module):
        """Add given module's jar files into dependent_data as dependencies.

        Args:
            module: A ModuleData instance.
        """
        if module.jar_files:
            self.source_path['jar_path'].update(module.jar_files)
            for jar in list(module.jar_files):
                self.source_path['jar_module_path'].update({
                    jar:
                    module.module_path
                })
        # Collecting the jar files of default core modules as dependencies.
        if constant.KEY_DEPENDENCIES in module.module_data:
            self.source_path['jar_path'].update([
                x for x in module.module_data[constant.KEY_DEPENDENCIES]
                if common_util.is_target(x, constant.TARGET_LIBS)
            ])

    @classmethod
    def multi_projects_locate_source(cls, projects):
        """Locate the paths of dependent source folders and jar files.

        Args:
            projects: A list of ProjectInfo instances. Information of a project
                      such as project relative path, project real path, project
                      dependencies.
        """
        for project in projects:
            project.locate_source()


class MultiProjectsInfo(ProjectInfo):
    """Multiple projects info.

    Usage example:
        project = MultiProjectsInfo(['module_name'])
        project.collect_all_dep_modules()
    """

    def __init__(self, targets=None):
        """MultiProjectsInfo initialize.

        Args:
            targets: A list of module names or project paths from user's input.
        """
        super().__init__(targets[0], True)
        self._targets = targets

    def collect_all_dep_modules(self):
        """Collects all dependency modules for the projects."""
        self.project_module_names = set()
        module_names = set(_CORE_MODULES)
        for target in self._targets:
            relpath, _ = common_util.get_related_paths(self.modules_info,
                                                       target)
            module_names.update(self._get_modules_under_project_path(relpath))
        module_names.update(self._get_robolectric_dep_module(module_names))
        self.dep_modules = self.get_dep_modules(module_names)


def batch_build_dependencies(rebuild_targets):
    """Batch build the jar or srcjar files of the modules if they don't exist.

    Command line has the max length limit, MAX_ARG_STRLEN, and
    MAX_ARG_STRLEN = (PAGE_SIZE * 32).
    If the build command is longer than MAX_ARG_STRLEN, this function will
    separate the rebuild_targets into chunks with size less or equal to
    MAX_ARG_STRLEN to make sure it can be built successfully.

    Args:
        rebuild_targets: A set of jar or srcjar files which do not exist.
    """
    logging.info('Ready to build the jar or srcjar files. Files count = %s',
                 str(len(rebuild_targets)))
    arg_max = os.sysconf('SC_PAGE_SIZE') * 32 - _CMD_LENGTH_BUFFER
    rebuild_targets = list(rebuild_targets)
    for start, end in iter(_separate_build_targets(rebuild_targets, arg_max)):
        _build_target(rebuild_targets[start:end])


def _build_target(targets):
    """Build the jar or srcjar files.

    Use -k to keep going when some targets can't be built or build failed.
    Use -j to speed up building.

    Args:
        targets: A list of jar or srcjar files which need to build.
    """
    build_cmd = ['-k', '-j']
    build_cmd.extend(list(targets))
    verbose = True
    if not atest_utils.build(build_cmd, verbose):
        message = ('Build failed!\n{}\nAIDEGen will proceed but dependency '
                   'correctness is not guaranteed if not all targets being '
                   'built successfully.'.format('\n'.join(targets)))
        print('\n{} {}\n'.format(common_util.COLORED_INFO('Warning:'), message))


def _separate_build_targets(build_targets, max_length):
    """Separate the build_targets by limit the command size to max command
    length.

    Args:
        build_targets: A list to be separated.
        max_length: The max number of each build command length.

    Yields:
        The start index and end index of build_targets.
    """
    arg_len = 0
    first_item_index = 0
    for i, item in enumerate(build_targets):
        arg_len = arg_len + len(item) + _BLANK_SIZE
        if arg_len > max_length:
            yield first_item_index, i
            first_item_index = i
            arg_len = len(item) + _BLANK_SIZE
    if first_item_index < len(build_targets):
        yield first_item_index, len(build_targets)
