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

"""It is an AIDEGen sub task: generate the .project file for Eclipse."""

import os

from aidegen import constant
from aidegen import templates
from aidegen.lib import common_util
from aidegen.lib import project_file_gen


class EclipseConf(project_file_gen.ProjectFileGenerator):
    """Class to generate project file under the module path for Eclipse.

    Attributes:
        module_abspath: The absolute path of the target project.
        module_relpath: The relative path of the target project.
        module_name: The name of the target project.
        jar_module_paths: A dict records a mapping of jar file and module path.
        r_java_paths: A list contains the relative folder paths of the R.java
                      files.
        project_file: The absolutely path of .project file.
        project_content: A string ready to be written into project_file.
        src_paths: A list contains the project's source paths.
        classpath_file: The absolutely path of .classpath file.
        classpath_content: A string ready to be written into classpath_file.
    """
    # Constants of .project file
    _PROJECT_LINK = ('                <link><name>{}</name><type>2</type>'
                     '<location>{}</location></link>\n')
    _PROJECT_FILENAME = '.project'
    _OUTPUT_BIN_SYMBOLIC_NAME = 'bin'

    # constans of .classpath file
    _CLASSPATH_SRC_ENTRY = '    <classpathentry kind="src" path="{}"/>\n'
    _EXCLUDE_ANDROID_BP_ENTRY = ('    <classpathentry excluding="Android.bp" '
                                 'kind="src" path="{}"/>\n')
    _CLASSPATH_LIB_ENTRY = ('    <classpathentry exported="true" kind="lib" '
                            'path="{}" sourcepath="{}"/>\n')
    _CLASSPATH_FILENAME = '.classpath'


    def __init__(self, project):
        """Initialize class.

        Args:
            project: A ProjectInfo instance.
        """
        super().__init__(project)
        self.module_abspath = project.project_absolute_path
        self.module_relpath = project.project_relative_path
        self.module_name = project.module_name
        self.jar_module_paths = project.source_path['jar_module_path']
        self.r_java_paths = list(project.source_path['r_java_path'])
        # Related value for generating .project.
        self.project_file = os.path.join(self.module_abspath,
                                         self._PROJECT_FILENAME)
        self.project_content = ''
        # Related value for generating .classpath.
        self.src_paths = list(project.source_path['source_folder_path'])
        self.src_paths.extend(project.source_path['test_folder_path'])
        self.classpath_file = os.path.join(self.module_abspath,
                                           self._CLASSPATH_FILENAME)
        self.classpath_content = ''

    def _gen_r_link(self):
        """Generate the link resources of the R paths.

        E.g.
            <link>
                <name>dependencies/out/target/common/R</name>
                <type>2</type>
                <location>{ANDROID_ROOT_PATH}/out/target/common/R</location>
            </link>

        Returns: A set contains R paths link resources strings.
        """
        return {self._gen_link(r_path) for r_path in self.r_java_paths}

    def _gen_src_links(self, relpaths):
        """Generate the link resources from relpaths.

        The link resource is linked to a folder outside the module's path. It
        cannot be a folder under the module's path.

        Args:
            relpaths: A list of module paths which are relative to
                      ANDROID_BUILD_TOP.
                      e.g. ['relpath/to/module1', 'relpath/to/module2', ...]

        Returns: A set includes all unique link resources.
        """
        src_links = set()
        for src_path in relpaths:
            if not common_util.is_source_under_relative_path(
                    src_path, self.module_relpath):
                src_links.add(self._gen_link(src_path))
        return src_links

    @classmethod
    def _gen_link(cls, relpath):
        """Generate a link resource from a relative path.

         E.g.
            <link>
                <name>dependencies/path/to/relpath</name>
                <type>2</type>
                <location>/absolute/path/to/relpath</location>
            </link>

        Args:
            relpath: A string of a relative path to Android_BUILD_TOP.

        Returns: A string of link resource.
        """
        alias_name = os.path.join(constant.KEY_DEPENDENCIES, relpath)
        abs_path = os.path.join(common_util.get_android_root_dir(), relpath)
        return cls._PROJECT_LINK.format(alias_name, abs_path)

    def _gen_bin_link(self):
        """Generate the link resource of the bin folder.

        The bin folder will be set as default in the module's root path. But
        doing so causes issues with the android build system. We should instead
        move the bin under the out folder.
        For example:
        <link>
                <name>bin</name>
                <type>2</type>
                <location>/home/user/aosp/out/Eclipse/framework</location>
        </link>

        Returns: A set includes a link resource of the bin folder.
        """
        real_bin_path = os.path.join(common_util.get_android_root_dir(),
                                     common_util.get_android_out_dir(),
                                     constant.IDE_ECLIPSE,
                                     self.module_name)
        if not os.path.exists(real_bin_path):
            os.makedirs(real_bin_path)
        return {self._PROJECT_LINK.format(self._OUTPUT_BIN_SYMBOLIC_NAME,
                                          real_bin_path)}

    def _get_other_src_folders(self):
        """Get the source folders outside the module's path.

        Some source folders are generated by build system and placed under the
        out folder. They also need to be set as link resources in Eclipse.

        Returns: A list of source folder paths.
        """
        return [p for p in self.src_paths
                if not common_util.is_source_under_relative_path(
                    p, self.module_relpath)]

    def _create_project_content(self):
        """Create the project file .project under the module."""
        # links is a set to save unique link resources.
        links = self._gen_src_links(self.jar_module_paths.values())
        links.update(self._gen_src_links(self._get_other_src_folders()))
        links.update(self._gen_r_link())
        links.update(self._gen_bin_link())
        self.project_content = templates.ECLIPSE_PROJECT_XML.format(
            PROJECTNAME=self.module_name,
            LINKEDRESOURCES=''.join(sorted(list(links))))

    def _gen_r_path_entries(self):
        """Generate the class path entries for the R paths.

        E.g.
            <classpathentry kind="src"
                path="dependencies/out/target/common/R"/>
            <classpathentry kind="src"
                path="dependencies/out/soong/.intermediates/packages/apps/
                      Settings/Settings/android_common/gen/aapt2/R"/>

        Returns: A list of the R path's class path entry.
        """
        r_entry_list = []
        for r_path in self.r_java_paths:
            alias_path = os.path.join(constant.KEY_DEPENDENCIES, r_path)
            r_entry_list.append(self._CLASSPATH_SRC_ENTRY.format(alias_path))
        return r_entry_list

    def _gen_src_path_entries(self):
        """Generate the class path entries from srcs.

        If the Android.bp file exists, generate the following content in
        .classpath file to avoid copying the Android.bp to the bin folder under
        current project directory.
        <classpathentry excluding="Android.bp" kind="src"
                        path="clearcut_client"/>

        If the source folder is under the module's path, revise the source
        folder path as a relative path to the module's path.
        E.g.
            The source folder paths list:
                ['packages/apps/Settings/src',
                 'packages/apps/Settings/tests/robotests/src',
                 'packages/apps/Settings/tests/uitests/src',
                 'packages/apps/Settings/tests/unit/src'
                ]
            It will generate the related <classpathentry> list:
                ['<classpathentry kind="src" path="src"/>',
                 '<classpathentry kind="src" path="tests/robotests/src"/>',
                 '<classpathentry kind="src" path="tests/uitests/src"/>',
                 '<classpathentry kind="src" path="tests/unit/src"/>'
                ]

        Some source folders are not under the module's path:
        e.g. out/path/src
        The class path entry would be:
        <classpathentry kind="src" path="dependencies/out/path/src"/>

        Returns: A list of source folders' class path entries.
        """
        src_path_entries = []
        for src in self.src_paths:
            src_abspath = os.path.join(common_util.get_android_root_dir(), src)
            if common_util.is_source_under_relative_path(
                    src, self.module_relpath):
                src = src.replace(self.module_relpath, '').strip(os.sep)
            else:
                src = os.path.join(constant.KEY_DEPENDENCIES, src)
            if common_util.exist_android_bp(src_abspath):
                src_path_entries.append(
                    self._EXCLUDE_ANDROID_BP_ENTRY.format(src))
            else:
                src_path_entries.append(self._CLASSPATH_SRC_ENTRY.format(src))
        return src_path_entries

    def _gen_jar_path_entries(self):
        """Generate the jar files' class path entries.

        The self.jar_module_paths is a dictionary.
        e.g.
            {'/abspath/to/the/file.jar': 'relpath/to/the/module'}
        This method will generate the <classpathentry> for each jar file.
        The format of <classpathentry> looks like:
        <classpathentry exported="true" kind="lib"
            path="/abspath/to/the/file.jar"
            sourcepath="dependencies/relpath/to/the/module"/>

        Returns: A list of jar files' class path entries.
        """
        jar_entries = []
        for jar_relpath, module_relpath in self.jar_module_paths.items():
            jar_abspath = os.path.join(common_util.get_android_root_dir(),
                                       jar_relpath)
            alias_module_path = os.path.join(constant.KEY_DEPENDENCIES,
                                             module_relpath)
            jar_entries.append(self._CLASSPATH_LIB_ENTRY.format(
                jar_abspath, alias_module_path))
        return jar_entries

    def _gen_bin_dir_entry(self):
        """Generate the class path entry of the bin folder.

        Returns: A list has a class path entry of the bin folder.
        """
        return [self._CLASSPATH_SRC_ENTRY.format(self._OUTPUT_BIN_SYMBOLIC_NAME)
               ]

    def _create_classpath_content(self):
        """Create the project file .classpath under the module."""
        src_entries = self._gen_src_path_entries()
        src_entries.extend(self._gen_r_path_entries())
        src_entries.extend(self._gen_bin_dir_entry())
        jar_entries = self._gen_jar_path_entries()
        self.classpath_content = templates.ECLIPSE_CLASSPATH_XML.format(
            SRC=''.join(sorted(src_entries)),
            LIB=''.join(sorted(jar_entries)))

    def generate_project_file(self):
        """Generate .project file of the target module."""
        self._create_project_content()
        common_util.file_generate(self.project_file, self.project_content)

    def generate_classpath_file(self):
        """Generate .classpath file of the target module."""
        self._create_classpath_content()
        common_util.file_generate(self.classpath_file, self.classpath_content)

    @classmethod
    def generate_ide_project_files(cls, projects):
        """Generate Eclipse project files by a list of ProjectInfo instances.

        Args:
            projects: A list of ProjectInfo instances.
        """
        for project in projects:
            eclipse_configure = EclipseConf(project)
            eclipse_configure.generate_project_file()
            eclipse_configure.generate_classpath_file()
