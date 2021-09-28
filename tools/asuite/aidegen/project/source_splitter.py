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

"""Separate the sources from multiple projects."""

import os

from aidegen import constant
from aidegen.idea import iml
from aidegen.lib import common_util
from aidegen.lib import project_config

_KEY_SOURCE_PATH = 'source_folder_path'
_KEY_TEST_PATH = 'test_folder_path'
_SOURCE_FOLDERS = [_KEY_SOURCE_PATH, _KEY_TEST_PATH]
_KEY_SRCJAR_PATH = 'srcjar_path'
_KEY_R_PATH = 'r_java_path'
_KEY_JAR_PATH = 'jar_path'
_EXCLUDE_ITEM = '\n            <excludeFolder url="file://%s" />'
# Temporarily exclude test-dump and src_stub folders to prevent symbols from
# resolving failure by incorrect reference. These two folders should be removed
# after b/136982078 is resolved.
_EXCLUDE_FOLDERS = ['.idea', '.repo', 'art', 'bionic', 'bootable', 'build',
                    'dalvik', 'developers', 'device', 'hardware', 'kernel',
                    'libnativehelper', 'pdk', 'prebuilts', 'sdk', 'system',
                    'toolchain', 'tools', 'vendor', 'out',
                    'art/tools/ahat/src/test-dump',
                    'cts/common/device-side/device-info/src_stub']


class ProjectSplitter:
    """Splits the sources from multiple projects.

    It's a specific solution to deal with the source folders in multiple
    project case. Since the IntelliJ does not allow duplicate source folders,
    AIDEGen needs to separate the source folders for each project. The single
    project case has no different with current structure.

    Usage:
    project_splitter = ProjectSplitter(projects)

    # Find the dependencies between the projects.
    project_splitter.get_dependencies()

    # Clear the source folders for each project.
    project_splitter.revise_source_folders()

    Attributes:
        _projects: A list of ProjectInfo.
        _all_srcs: A dictionary contains all sources of multiple projects.
                   e.g.
                   {
                       'module_name': 'test',
                       'path': ['path/to/module'],
                       'srcs': ['src_folder1', 'src_folder2'],
                       'tests': ['test_folder1', 'test_folder2']
                       'jars': ['jar1.jar'],
                       'srcjars': ['1.srcjar', '2.srcjar'],
                       'dependencies': ['framework_srcjars', 'base'],
                       'iml_name': '/abs/path/to/iml.iml'
                   }
        _framework_exist: A boolean, True if framework is one of the projects.
        _framework_iml: A string, the name of the framework's iml.
        _full_repo: A boolean, True if loading with full Android sources.
        _full_repo_iml: A string, the name of the Android folder's iml.
    """
    def __init__(self, projects):
        """ProjectSplitter initialize.

        Args:
            projects: A list of ProjectInfo object.
        """
        self._projects = projects
        self._all_srcs = dict(projects[0].source_path)
        self._framework_iml = None
        self._framework_exist = any(
            {p.project_relative_path == constant.FRAMEWORK_PATH
             for p in self._projects})
        if self._framework_exist:
            self._framework_iml = iml.IMLGenerator.get_unique_iml_name(
                os.path.join(common_util.get_android_root_dir(),
                             constant.FRAMEWORK_PATH))
        self._full_repo = project_config.ProjectConfig.get_instance().full_repo
        if self._full_repo:
            self._full_repo_iml = os.path.basename(
                common_util.get_android_root_dir())

    def revise_source_folders(self):
        """Resets the source folders of each project.

        There should be no duplicate source root path in IntelliJ. The issue
        doesn't happen in single project case. Once users choose multiple
        projects, there could be several same source paths of different
        projects. In order to prevent that, we should remove the source paths
        in dependencies.iml which are duplicate with the paths in [module].iml
        files.

        Steps to prevent the duplicate source root path in IntelliJ:
        1. Copy all sources from sub-projects to main project.
        2. Delete the source and test folders which are not under the
           sub-projects.
        3. Delete the sub-projects' source and test paths from the main project.
        """
        self._collect_all_srcs()
        self._keep_local_sources()
        self._remove_duplicate_sources()

    def _collect_all_srcs(self):
        """Copies all projects' sources to a dictionary."""
        for project in self._projects[1:]:
            for key, value in project.source_path.items():
                self._all_srcs[key].update(value)

    def _keep_local_sources(self):
        """Removes source folders which are not under the project's path.

        1. Remove the source folders which are not under the project.
        2. Remove the duplicate project's source folders from the _all_srcs.
        """
        for project in self._projects:
            srcs = project.source_path
            relpath = project.project_relative_path
            is_root = not relpath
            for key in _SOURCE_FOLDERS:
                srcs[key] = {s for s in srcs[key]
                             if common_util.is_source_under_relative_path(
                                 s, relpath) or is_root}
                self._all_srcs[key] -= srcs[key]

    def _remove_duplicate_sources(self):
        """Removes the duplicate source folders from each sub project.

        Priority processing with the longest path length, e.g.
        frameworks/base/packages/SettingsLib must have priority over
        frameworks/base.
        """
        for child in sorted(self._projects, key=lambda k: len(
                k.project_relative_path), reverse=True):
            for parent in self._projects:
                is_root = not parent.project_relative_path
                if parent is child:
                    continue
                if (common_util.is_source_under_relative_path(
                        child.project_relative_path,
                        parent.project_relative_path) or is_root):
                    for key in _SOURCE_FOLDERS:
                        parent.source_path[key] -= child.source_path[key]

    def get_dependencies(self):
        """Gets the dependencies between the projects.

        Check if the current project's source folder exists in other projects.
        If do, the current project is a dependency module to the other.
        """
        for project in sorted(self._projects, key=lambda k: len(
                k.project_relative_path)):
            proj_path = project.project_relative_path
            project.dependencies = [constant.FRAMEWORK_SRCJARS]
            if self._framework_exist and proj_path != constant.FRAMEWORK_PATH:
                project.dependencies.append(self._framework_iml)
            if self._full_repo and proj_path:
                project.dependencies.append(self._full_repo_iml)
            srcs = (project.source_path[_KEY_SOURCE_PATH]
                    | project.source_path[_KEY_TEST_PATH])
            for dep_proj in sorted(self._projects, key=lambda k: len(
                    k.project_relative_path)):
                dep_path = dep_proj.project_relative_path
                is_root = not dep_path
                is_child = common_util.is_source_under_relative_path(dep_path,
                                                                     proj_path)
                is_dep = any({s for s in srcs
                              if common_util.is_source_under_relative_path(
                                  s, dep_path) or is_root})
                if dep_proj is project or is_child or not is_dep:
                    continue
                dep = iml.IMLGenerator.get_unique_iml_name(os.path.join(
                    common_util.get_android_root_dir(), dep_path))
                if dep not in project.dependencies:
                    project.dependencies.append(dep)
            project.dependencies.append(constant.KEY_DEPENDENCIES)

    def gen_framework_srcjars_iml(self):
        """Generates the framework-srcjars.iml.

        Create the iml file with only the srcjars of module framework-all. These
        srcjars will be separated from the modules under frameworks/base.

        Returns:
            A string of the framework_srcjars.iml's absolute path.
        """
        mod = dict(self._projects[0].dep_modules[constant.FRAMEWORK_ALL])
        mod[constant.KEY_DEPENDENCIES] = []
        mod[constant.KEY_IML_NAME] = constant.FRAMEWORK_SRCJARS
        if self._framework_exist:
            mod[constant.KEY_DEPENDENCIES].append(self._framework_iml)
        if self._full_repo:
            mod[constant.KEY_DEPENDENCIES].append(self._full_repo_iml)
        mod[constant.KEY_DEPENDENCIES].append(constant.KEY_DEPENDENCIES)
        framework_srcjars_iml = iml.IMLGenerator(mod)
        framework_srcjars_iml.create({constant.KEY_SRCJARS: True,
                                      constant.KEY_DEPENDENCIES: True})
        self._all_srcs[_KEY_SRCJAR_PATH] -= set(mod[constant.KEY_SRCJARS])
        return framework_srcjars_iml.iml_path

    def _gen_dependencies_iml(self):
        """Generates the dependencies.iml."""
        mod = {
            constant.KEY_SRCS: self._all_srcs[_KEY_SOURCE_PATH],
            constant.KEY_TESTS: self._all_srcs[_KEY_TEST_PATH],
            constant.KEY_JARS: self._all_srcs[_KEY_JAR_PATH],
            constant.KEY_SRCJARS: (self._all_srcs[_KEY_R_PATH]
                                   | self._all_srcs[_KEY_SRCJAR_PATH]),
            constant.KEY_DEPENDENCIES: [constant.FRAMEWORK_SRCJARS],
            constant.KEY_PATH: [self._projects[0].project_relative_path],
            constant.KEY_MODULE_NAME: constant.KEY_DEPENDENCIES,
            constant.KEY_IML_NAME: constant.KEY_DEPENDENCIES
        }
        if self._framework_exist:
            mod[constant.KEY_DEPENDENCIES].append(self._framework_iml)
        if self._full_repo:
            mod[constant.KEY_DEPENDENCIES].append(self._full_repo_iml)
        dep_iml = iml.IMLGenerator(mod)
        dep_iml.create({constant.KEY_DEP_SRCS: True,
                        constant.KEY_SRCJARS: True,
                        constant.KEY_JARS: True,
                        constant.KEY_DEPENDENCIES: True})

    def gen_projects_iml(self):
        """Generates the projects' iml file."""
        root_path = common_util.get_android_root_dir()
        excludes = project_config.ProjectConfig.get_instance().exclude_paths
        for project in self._projects:
            relpath = project.project_relative_path
            exclude_folders = []
            if not relpath:
                exclude_folders.extend(get_exclude_content(root_path))
            if excludes:
                exclude_folders.extend(get_exclude_content(root_path, excludes))
            mod_info = {
                constant.KEY_EXCLUDES: ''.join(exclude_folders),
                constant.KEY_SRCS: project.source_path[_KEY_SOURCE_PATH],
                constant.KEY_TESTS: project.source_path[_KEY_TEST_PATH],
                constant.KEY_DEPENDENCIES: project.dependencies,
                constant.KEY_PATH: [relpath],
                constant.KEY_MODULE_NAME: project.module_name,
                constant.KEY_IML_NAME: iml.IMLGenerator.get_unique_iml_name(
                    os.path.join(root_path, relpath))
            }
            dep_iml = iml.IMLGenerator(mod_info)
            dep_iml.create({constant.KEY_SRCS: True,
                            constant.KEY_DEPENDENCIES: True})
            project.iml_path = dep_iml.iml_path
        self._gen_dependencies_iml()


def get_exclude_content(root_path, excludes=None):
    """Get the exclude folder content list.

    It returns the exclude folders content list.
    e.g.
    ['<excludeFolder url="file://a/.idea" />',
    '<excludeFolder url="file://a/.repo" />']

    Args:
        root_path: Android source file path.
        excludes: A list of exclusive directories, the default value is None but
                  will be assigned to _EXCLUDE_FOLDERS.

    Returns:
        String: exclude folder content list.
    """
    exclude_items = []
    if not excludes:
        excludes = _EXCLUDE_FOLDERS
    for folder in excludes:
        folder_path = os.path.join(root_path, folder)
        if os.path.isdir(folder_path):
            exclude_items.append(_EXCLUDE_ITEM % folder_path)
    return exclude_items
