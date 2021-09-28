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

"""ModuleData information."""

from __future__ import absolute_import

import glob
import logging
import os
import re

from aidegen import constant
from aidegen.lib import common_util
from aidegen.lib import module_info
from aidegen.lib import project_config

# Parse package name from the package declaration line of a java.
# Group matches "foo.bar" of line "package foo.bar;" or "package foo.bar"
_PACKAGE_RE = re.compile(r'\s*package\s+(?P<package>[^(;|\s)]+)\s*', re.I)
_ANDROID_SUPPORT_PATH_KEYWORD = 'prebuilts/sdk/current/'

# File extensions
_JAVA_EXT = '.java'
_KOTLIN_EXT = '.kt'
_SRCJAR_EXT = '.srcjar'
_TARGET_FILES = [_JAVA_EXT, _KOTLIN_EXT]
_JARJAR_RULES_FILE = 'jarjar-rules.txt'
_KEY_JARJAR_RULES = 'jarjar_rules'
_NAME_AAPT2 = 'aapt2'
_TARGET_R_SRCJAR = 'R.srcjar'
_TARGET_AAPT2_SRCJAR = _NAME_AAPT2 + _SRCJAR_EXT
_TARGET_BUILD_FILES = [_TARGET_AAPT2_SRCJAR, _TARGET_R_SRCJAR]
_IGNORE_DIRS = [
    # The java files under this directory have to be ignored because it will
    # cause duplicated classes by libcore/ojluni/src/main/java.
    'libcore/ojluni/src/lambda/java'
]
_ANDROID = 'android'
_REPACKAGES = 'repackaged'
_FRAMEWORK_SRCJARS_PATH = os.path.join(constant.FRAMEWORK_PATH,
                                       constant.FRAMEWORK_SRCJARS)


class ModuleData:
    """ModuleData class.

    Attributes:
        All following relative paths stand for the path relative to the android
        repo root.

        module_path: A string of the relative path to the module.
        src_dirs: A list to keep the unique source folder relative paths.
        test_dirs: A list to keep the unique test folder relative paths.
        jar_files: A list to keep the unique jar file relative paths.
        r_java_paths: A list to keep the R folder paths to use in Eclipse.
        srcjar_paths: A list to keep the srcjar source root paths to use in
                      IntelliJ.
        dep_paths: A list to keep the dependency modules' path.
        referenced_by_jar: A boolean to check if the module is referenced by a
                           jar file.
        build_targets: A set to keep the unique build target jar or srcjar file
                       relative paths which are ready to be rebuld.
        missing_jars: A set to keep the jar file relative paths if it doesn't
                      exist.
        specific_soong_path: A string of the relative path to the module's
                             intermediates folder under out/.
    """

    def __init__(self, module_name, module_data, depth):
        """Initialize ModuleData.

        Args:
            module_name: Name of the module.
            module_data: A dictionary holding a module information.
            depth: An integer shows the depth of module dependency referenced by
                   source. Zero means the max module depth.
            For example:
                {
                    'class': ['APPS'],
                    'path': ['path/to/the/module'],
                    'depth': 0,
                    'dependencies': ['bouncycastle', 'ims-common'],
                    'srcs': [
                        'path/to/the/module/src/com/android/test.java',
                        'path/to/the/module/src/com/google/test.java',
                        'out/soong/.intermediates/path/to/the/module/test/src/
                         com/android/test.srcjar'
                    ],
                    'installed': ['out/target/product/generic_x86_64/
                                   system/framework/framework.jar'],
                    'jars': ['settings.jar'],
                    'jarjar_rules': ['jarjar-rules.txt']
                }
        """
        assert module_name, 'Module name can\'t be null.'
        assert module_data, 'Module data of %s can\'t be null.' % module_name
        self.module_name = module_name
        self.module_data = module_data
        self._init_module_path()
        self._init_module_depth(depth)
        self.src_dirs = []
        self.test_dirs = []
        self.jar_files = []
        self.r_java_paths = []
        self.srcjar_paths = []
        self.dep_paths = []
        self.referenced_by_jar = False
        self.build_targets = set()
        self.missing_jars = set()
        self.specific_soong_path = os.path.join(
            'out/soong/.intermediates', self.module_path, self.module_name)

    def _is_app_module(self):
        """Check if the current module's class is APPS"""
        return self._check_key('class') and 'APPS' in self.module_data['class']

    def _is_target_module(self):
        """Check if the current module is a target module.

        A target module is the target project or a module under the
        target project and it's module depth is 0.
        For example: aidegen Settings framework
            The target projects are Settings and framework so they are also
            target modules. And the dependent module SettingsUnitTests's path
            is packages/apps/Settings/tests/unit so it also a target module.
        """
        return self.module_depth == 0

    def _collect_r_srcs_paths(self):
        """Collect the source folder of R.java.

        Check if the path of aapt2.srcjar or R.srcjar exists, these are both the
        values of key "srcjars" in module_data. If neither of the cases exists,
        build it onto an intermediates directory.

        For IntelliJ, we can set the srcjar file as a source root for
        dependency. For Eclipse, we still use the R folder as dependencies until
        we figure out how to set srcjar file as dependency.
        # TODO(b/135594800): Set aapt2.srcjar or R.srcjar as a dependency in
                             Eclipse.
        """
        if (self._is_app_module() and self._is_target_module()
                and self._check_key(constant.KEY_SRCJARS)):
            for srcjar in self.module_data[constant.KEY_SRCJARS]:
                if not os.path.exists(common_util.get_abs_path(srcjar)):
                    self.build_targets.add(srcjar)
                self._collect_srcjar_path(srcjar)
                r_dir = self._get_r_dir(srcjar)
                if r_dir and r_dir not in self.r_java_paths:
                    self.r_java_paths.append(r_dir)

    def _collect_srcjar_path(self, srcjar):
        """Collect the source folders from a srcjar path.

        Set the aapt2.srcjar or R.srcjar as source root:
        Case aapt2.srcjar:
            The source path string is
            out/.../Bluetooth_intermediates/aapt2.srcjar.
        Case R.srcjar:
            The source path string is out/soong/.../gen/android/R.srcjar.

        Args:
            srcjar: A file path string relative to ANDROID_BUILD_TOP, the build
                    target of the module to generate R.java.
        """
        if (os.path.basename(srcjar) in _TARGET_BUILD_FILES
                and srcjar not in self.srcjar_paths):
            self.srcjar_paths.append(srcjar)

    def _collect_all_srcjar_paths(self):
        """Collect all srcjar files of target module as source folders.

        Since the aidl files are built to *.java and collected in the
        aidl.srcjar file by the build system. AIDEGen needs to collect these
        aidl.srcjar files as the source root folders in IntelliJ. Furthermore,
        AIDEGen collects all *.srcjar files for other cases to fulfil the same
        purpose.
        """
        if self._is_target_module() and self._check_key(constant.KEY_SRCJARS):
            for srcjar in self.module_data[constant.KEY_SRCJARS]:
                if not os.path.exists(common_util.get_abs_path(srcjar)):
                    self.build_targets.add(srcjar)
                if srcjar not in self.srcjar_paths:
                    self.srcjar_paths.append(srcjar)

    @staticmethod
    def _get_r_dir(srcjar):
        """Get the source folder of R.java for Eclipse.

        Get the folder contains the R.java of aapt2.srcjar or R.srcjar:
        Case aapt2.srcjar:
            If the relative path of the aapt2.srcjar is a/b/aapt2.srcjar, the
            source root of the R.java is a/b/aapt2
        Case R.srcjar:
            If the relative path of the R.srcjar is a/b/android/R.srcjar, the
            source root of the R.java is a/b/aapt2/R

        Args:
            srcjar: A file path string, the build target of the module to
                    generate R.java.

        Returns:
            A relative source folder path string, and return None if the target
            file name is not aapt2.srcjar or R.srcjar.
        """
        target_folder, target_file = os.path.split(srcjar)
        base_dirname = os.path.basename(target_folder)
        if target_file == _TARGET_AAPT2_SRCJAR:
            return os.path.join(target_folder, _NAME_AAPT2)
        if target_file == _TARGET_R_SRCJAR and base_dirname == _ANDROID:
            return os.path.join(os.path.dirname(target_folder),
                                _NAME_AAPT2, 'R')
        return None

    def _init_module_path(self):
        """Inintialize self.module_path."""
        self.module_path = (self.module_data[constant.KEY_PATH][0]
                            if self._check_key(constant.KEY_PATH) else '')

    def _init_module_depth(self, depth):
        """Initialize module depth's settings.

        Set the module's depth from module info when user have -d parameter.
        Set the -d value from user input, default to 0.

        Args:
            depth: the depth to be set.
        """
        self.module_depth = (int(self.module_data[constant.KEY_DEPTH])
                             if depth else 0)
        self.depth_by_source = depth

    def _is_android_supported_module(self):
        """Determine if this is an Android supported module."""
        return common_util.is_source_under_relative_path(
            self.module_path, _ANDROID_SUPPORT_PATH_KEYWORD)

    def _check_jarjar_rules_exist(self):
        """Check if jarjar rules exist."""
        return (_KEY_JARJAR_RULES in self.module_data and
                self.module_data[_KEY_JARJAR_RULES][0] == _JARJAR_RULES_FILE)

    def _check_jars_exist(self):
        """Check if jars exist."""
        return self._check_key(constant.KEY_JARS)

    def _check_classes_jar_exist(self):
        """Check if classes_jar exist."""
        return self._check_key(constant.KEY_CLASSES_JAR)

    def _collect_srcs_paths(self):
        """Collect source folder paths in src_dirs from module_data['srcs']."""
        if self._check_key(constant.KEY_SRCS):
            scanned_dirs = set()
            for src_item in self.module_data[constant.KEY_SRCS]:
                src_dir = None
                src_item = os.path.relpath(src_item)
                if common_util.is_target(src_item, _TARGET_FILES):
                    # Only scan one java file in each source directories.
                    src_item_dir = os.path.dirname(src_item)
                    if src_item_dir not in scanned_dirs:
                        scanned_dirs.add(src_item_dir)
                        src_dir = self._get_source_folder(src_item)
                else:
                    # To record what files except java and kt in the srcs.
                    logging.debug('%s is not in parsing scope.', src_item)
                if src_dir:
                    self._add_to_source_or_test_dirs(
                        self._switch_repackaged(src_dir))

    def _check_key(self, key):
        """Check if key is in self.module_data and not empty.

        Args:
            key: the key to be checked.
        """
        return key in self.module_data and self.module_data[key]

    def _add_to_source_or_test_dirs(self, src_dir):
        """Add folder to source or test directories.

        Args:
            src_dir: the directory to be added.
        """
        if (src_dir not in _IGNORE_DIRS and src_dir not in self.src_dirs
                and src_dir not in self.test_dirs):
            if self._is_test_module(src_dir):
                self.test_dirs.append(src_dir)
            else:
                self.src_dirs.append(src_dir)

    @staticmethod
    def _is_test_module(src_dir):
        """Check if the module path is a test module path.

        Args:
            src_dir: the directory to be checked.

        Returns:
            True if module path is a test module path, otherwise False.
        """
        return constant.KEY_TESTS in src_dir.split(os.sep)

    def _get_source_folder(self, java_file):
        """Parsing a java to get the package name to filter out source path.

        Args:
            java_file: A string, the java file with relative path.
                       e.g. path/to/the/java/file.java

        Returns:
            source_folder: A string of path to source folder(e.g. src/main/java)
                           or none when it failed to get package name.
        """
        abs_java_path = common_util.get_abs_path(java_file)
        if os.path.exists(abs_java_path):
            package_name = self._get_package_name(abs_java_path)
            if package_name:
                return self._parse_source_path(java_file, package_name)
        return None

    @staticmethod
    def _parse_source_path(java_file, package_name):
        """Parse the source path by filter out the package name.

        Case 1:
        java file: a/b/c/d/e.java
        package name: c.d
        The source folder is a/b.

        Case 2:
        java file: a/b/c.d/e.java
        package name: c.d
        The source folder is a/b.

        Case 3:
        java file: a/b/c/d/e.java
        package name: x.y
        The source folder is a/b/c/d.

        Case 4:
        java file: a/b/c.d/e/c/d/f.java
        package name: c.d
        The source folder is a/b/c.d/e.

        Case 5:
        java file: a/b/c.d/e/c.d/e/f.java
        package name: c.d.e
        The source folder is a/b/c.d/e.

        Args:
            java_file: A string of the java file relative path.
            package_name: A string of the java file's package name.

        Returns:
            A string, the source folder path.
        """
        java_file_name = os.path.basename(java_file)
        pattern = r'%s/%s$' % (package_name, java_file_name)
        search_result = re.search(pattern, java_file)
        if search_result:
            return java_file[:search_result.start()].strip(os.sep)
        return os.path.dirname(java_file)

    @staticmethod
    def _switch_repackaged(src_dir):
        """Changes the directory to repackaged if it does exist.

        Args:
            src_dir: a string of relative path.

        Returns:
            The source folder under repackaged if it exists, otherwise the
            original one.
        """
        root_path = common_util.get_android_root_dir()
        dir_list = src_dir.split(os.sep)
        for i in range(1, len(dir_list)):
            tmp_dir = dir_list.copy()
            tmp_dir.insert(i, _REPACKAGES)
            real_path = os.path.join(root_path, os.path.join(*tmp_dir))
            if os.path.exists(real_path):
                return os.path.relpath(real_path, root_path)
        return src_dir

    @staticmethod
    def _get_package_name(abs_java_path):
        """Get the package name by parsing a java file.

        Args:
            abs_java_path: A string of the java file with absolute path.
                           e.g. /root/path/to/the/java/file.java

        Returns:
            package_name: A string of package name.
        """
        package_name = None
        with open(abs_java_path, encoding='utf8') as data:
            for line in data.read().splitlines():
                match = _PACKAGE_RE.match(line)
                if match:
                    package_name = match.group('package')
                    break
        return package_name

    def _append_jar_file(self, jar_path):
        """Append a path to the jar file into self.jar_files if it's exists.

        Args:
            jar_path: A path supposed to be a jar file.

        Returns:
            Boolean: True if jar_path is an existing jar file.
        """
        if common_util.is_target(jar_path, constant.TARGET_LIBS):
            self.referenced_by_jar = True
            if os.path.isfile(common_util.get_abs_path(jar_path)):
                if jar_path not in self.jar_files:
                    self.jar_files.append(jar_path)
            else:
                self.missing_jars.add(jar_path)
            return True
        return False

    def _append_classes_jar(self):
        """Append the jar file as dependency for prebuilt modules."""
        for jar in self.module_data[constant.KEY_CLASSES_JAR]:
            if self._append_jar_file(jar):
                break

    def _append_jar_from_installed(self, specific_dir=None):
        """Append a jar file's path to the list of jar_files with matching
        path_prefix.

        There might be more than one jar in "installed" parameter and only the
        first jar file is returned. If specific_dir is set, the jar file must be
        under the specific directory or its sub-directory.

        Args:
            specific_dir: A string of path.
        """
        if self._check_key(constant.KEY_INSTALLED):
            for jar in self.module_data[constant.KEY_INSTALLED]:
                if specific_dir and not jar.startswith(specific_dir):
                    continue
                if self._append_jar_file(jar):
                    break

    def _set_jars_jarfile(self):
        """Append prebuilt jars of module into self.jar_files.

        Some modules' sources are prebuilt jar files instead of source java
        files. The jar files can be imported into IntelliJ as a dependency
        directly. There is only jar file name in self.module_data['jars'], it
        has to be combined with self.module_data['path'] to append into
        self.jar_files.
        Once the file doesn't exist, it's not assumed to be a prebuilt jar so
        that we can ignore it.
        # TODO(b/141959125): Collect the correct prebuilt jar files by jdeps.go.

        For example:
        'asm-6.0': {
            'jars': [
                'asm-6.0.jar'
            ],
            'path': [
                'prebuilts/misc/common/asm'
            ],
        },
        Path to the jar file is prebuilts/misc/common/asm/asm-6.0.jar.
        """
        if self._check_key(constant.KEY_JARS):
            for jar_name in self.module_data[constant.KEY_JARS]:
                if self._check_key(constant.KEY_INSTALLED):
                    self._append_jar_from_installed()
                else:
                    jar_path = os.path.join(self.module_path, jar_name)
                    jar_abs = common_util.get_abs_path(jar_path)
                    if not os.path.isfile(jar_abs) and jar_name.endswith(
                            'prebuilt.jar'):
                        rel_path = self._get_jar_path_from_prebuilts(jar_name)
                        if rel_path:
                            jar_path = rel_path
                    if os.path.exists(common_util.get_abs_path(jar_path)):
                        self._append_jar_file(jar_path)

    @staticmethod
    def _get_jar_path_from_prebuilts(jar_name):
        """Get prebuilt jar file from prebuilts folder.

        If the prebuilt jar file we get from method _set_jars_jarfile() does not
        exist, we should search the prebuilt jar file in prebuilts folder.
        For example:
        'platformprotos': {
            'jars': [
                'platformprotos-prebuilt.jar'
            ],
            'path': [
                'frameworks/base'
            ],
        },
        We get an incorrect path: 'frameworks/base/platformprotos-prebuilt.jar'
        If the file does not exist, we should search the file name from
        prebuilts folder. If we can get the correct path from 'prebuilts', we
        can replace it with the incorrect path.

        Args:
            jar_name: The prebuilt jar file name.

        Return:
            A relative prebuilt jar file path if found, otherwise None.
        """
        rel_path = ''
        search = os.sep.join(
            [common_util.get_android_root_dir(), 'prebuilts/**', jar_name])
        results = glob.glob(search, recursive=True)
        if results:
            jar_abs = results[0]
            rel_path = os.path.relpath(
                jar_abs, common_util.get_android_root_dir())
        return rel_path

    def _collect_specific_jars(self):
        """Collect specific types of jar files."""
        if self._is_android_supported_module():
            self._append_jar_from_installed()
        elif self._check_jarjar_rules_exist():
            self._append_jar_from_installed(self.specific_soong_path)
        elif self._check_jars_exist():
            self._set_jars_jarfile()

    def _collect_classes_jars(self):
        """Collect classes jar files."""
        # If there is no source/tests folder of the module, reference the
        # module by jar.
        if not self.src_dirs and not self.test_dirs:
            # Add the classes.jar from the classes_jar attribute as
            # dependency if it exists. If the classes.jar doesn't exist,
            # find the jar file from the installed attribute and add the jar
            # as dependency.
            if self._check_classes_jar_exist():
                self._append_classes_jar()
            else:
                self._append_jar_from_installed()

    def _collect_srcs_and_r_srcs_paths(self):
        """Collect source and R source folder paths for the module."""
        self._collect_specific_jars()
        self._collect_srcs_paths()
        self._collect_classes_jars()
        self._collect_r_srcs_paths()
        self._collect_all_srcjar_paths()

    def _collect_missing_jars(self):
        """Collect missing jar files to rebuild them."""
        if self.referenced_by_jar and self.missing_jars:
            self.build_targets |= self.missing_jars

    def _collect_dep_paths(self):
        """Collects the path of dependency modules."""
        config = project_config.ProjectConfig.get_instance()
        modules_info = config.atest_module_info
        self.dep_paths = []
        if self.module_path != constant.FRAMEWORK_PATH:
            self.dep_paths.append(constant.FRAMEWORK_PATH)
        self.dep_paths.append(_FRAMEWORK_SRCJARS_PATH)
        if self.module_path != constant.LIBCORE_PATH:
            self.dep_paths.append(constant.LIBCORE_PATH)
        for module in self.module_data.get(constant.KEY_DEPENDENCIES, []):
            for path in modules_info.get_paths(module):
                if path not in self.dep_paths and path != self.module_path:
                    self.dep_paths.append(path)

    def locate_sources_path(self):
        """Locate source folders' paths or jar files."""
        # Check if users need to reference source according to source depth.
        if not self.module_depth <= self.depth_by_source:
            self._append_jar_from_installed(self.specific_soong_path)
        else:
            self._collect_srcs_and_r_srcs_paths()
        self._collect_missing_jars()


class EclipseModuleData(ModuleData):
    """Deal with modules data for Eclipse

    Only project target modules use source folder type and the other ones use
    jar as their source. We'll combine both to establish the whole project's
    dependencies. If the source folder used to build dependency jar file exists
    in Android, we should provide the jar file path as <linkedResource> item in
    source data.
    """

    def __init__(self, module_name, module_data, project_relpath):
        """Initialize EclipseModuleData.

        Only project target modules apply source folder type, so set the depth
        of module referenced by source to 0.

        Args:
            module_name: String type, name of the module.
            module_data: A dictionary contains a module information.
            project_relpath: A string stands for the project's relative path.
        """
        super().__init__(module_name, module_data, depth=0)
        related = module_info.AidegenModuleInfo.is_project_path_relative_module(
            module_data, project_relpath)
        self.is_project = related

    def locate_sources_path(self):
        """Locate source folders' paths or jar files.

        Only collect source folders for the project modules and collect jar
        files for the other dependent modules.
        """
        if self.is_project:
            self._locate_project_source_path()
        else:
            self._locate_jar_path()
        self._collect_classes_jars()
        self._collect_missing_jars()

    def _add_to_source_or_test_dirs(self, src_dir):
        """Add a folder to source list if it is not in ignored directories.

        Override the parent method since the tests folder has no difference
        with source folder in Eclipse.

        Args:
            src_dir: a string of relative path to the Android root.
        """
        if src_dir not in _IGNORE_DIRS and src_dir not in self.src_dirs:
            self.src_dirs.append(src_dir)

    def _locate_project_source_path(self):
        """Locate the source folder paths of the project module.

        A project module is the target modules or paths that users key in
        aidegen command. Collecting the source folders is necessary for
        developers to edit code. And also collect the central R folder for the
        dependency of resources.
        """
        self._collect_srcs_paths()
        self._collect_r_srcs_paths()

    def _locate_jar_path(self):
        """Locate the jar path of the module.

        Use jar files for dependency modules for Eclipse. Collect the jar file
        path with different cases.
        """
        if self._check_jarjar_rules_exist():
            self._append_jar_from_installed(self.specific_soong_path)
        elif self._check_jars_exist():
            self._set_jars_jarfile()
        elif self._check_classes_jar_exist():
            self._append_classes_jar()
        else:
            self._append_jar_from_installed()
