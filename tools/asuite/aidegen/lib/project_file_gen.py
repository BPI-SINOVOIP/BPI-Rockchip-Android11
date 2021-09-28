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

"""It is an AIDEGen sub task : generate the project files.

    Usage example:
    projects: A list of ProjectInfo instances.
    ProjectFileGenerator.generate_ide_project_file(projects)
"""

import logging
import os
import shutil

from aidegen import constant
from aidegen import templates
from aidegen.idea import iml
from aidegen.idea import xml_gen
from aidegen.lib import common_util
from aidegen.lib import config
from aidegen.lib import project_config
from aidegen.project import source_splitter

# FACET_SECTION is a part of iml, which defines the framework of the project.
_MODULE_SECTION = ('            <module fileurl="file:///$PROJECT_DIR$/%s.iml"'
                   ' filepath="$PROJECT_DIR$/%s.iml" />')
_SUB_MODULES_SECTION = ('            <module fileurl="file:///{IML}" '
                        'filepath="{IML}" />')
_MODULE_TOKEN = '@MODULES@'
_ENABLE_DEBUGGER_MODULE_TOKEN = '@ENABLE_DEBUGGER_MODULE@'
_IDEA_FOLDER = '.idea'
_MODULES_XML = 'modules.xml'
_COPYRIGHT_FOLDER = 'copyright'
_CODE_STYLE_FOLDER = 'codeStyles'
_APACHE_2_XML = 'Apache_2.xml'
_PROFILES_SETTINGS_XML = 'profiles_settings.xml'
_CODE_STYLE_CONFIG_XML = 'codeStyleConfig.xml'
_JSON_SCHEMAS_CONFIG_XML = 'jsonSchemas.xml'
_PROJECT_XML = 'Project.xml'
_COMPILE_XML = 'compiler.xml'
_MISC_XML = 'misc.xml'
_CONFIG_JSON = 'config.json'
_GIT_FOLDER_NAME = '.git'
# Support gitignore by symbolic link to aidegen/data/gitignore_template.
_GITIGNORE_FILE_NAME = '.gitignore'
_GITIGNORE_REL_PATH = 'tools/asuite/aidegen/data/gitignore_template'
_GITIGNORE_ABS_PATH = os.path.join(common_util.get_android_root_dir(),
                                   _GITIGNORE_REL_PATH)
# Support code style by symbolic link to aidegen/data/AndroidStyle_aidegen.xml.
_CODE_STYLE_REL_PATH = 'tools/asuite/aidegen/data/AndroidStyle_aidegen.xml'
_CODE_STYLE_SRC_PATH = os.path.join(common_util.get_android_root_dir(),
                                    _CODE_STYLE_REL_PATH)
_TEST_MAPPING_CONFIG_PATH = ('tools/tradefederation/core/src/com/android/'
                             'tradefed/util/testmapping/TEST_MAPPING.config'
                             '.json')


class ProjectFileGenerator:
    """Project file generator.

    Attributes:
        project_info: A instance of ProjectInfo.
    """

    def __init__(self, project_info):
        """ProjectFileGenerator initialize.

        Args:
            project_info: A instance of ProjectInfo.
        """
        self.project_info = project_info

    def generate_intellij_project_file(self, iml_path_list=None):
        """Generates IntelliJ project file.

        # TODO(b/155346505): Move this method to idea folder.

        Args:
            iml_path_list: An optional list of submodule's iml paths, the
                           default value is None.
        """
        if self.project_info.is_main_project:
            self._generate_modules_xml(iml_path_list)
            self._copy_constant_project_files()

    @classmethod
    def generate_ide_project_files(cls, projects):
        """Generate IDE project files by a list of ProjectInfo instances.

        It deals with the sources by ProjectSplitter to create iml files for
        each project and generate_intellij_project_file only creates
        the other project files under .idea/.

        Args:
            projects: A list of ProjectInfo instances.
        """
        # Initialization
        iml.IMLGenerator.USED_NAME_CACHE.clear()
        proj_splitter = source_splitter.ProjectSplitter(projects)
        proj_splitter.get_dependencies()
        proj_splitter.revise_source_folders()
        iml_paths = [proj_splitter.gen_framework_srcjars_iml()]
        proj_splitter.gen_projects_iml()
        iml_paths += [project.iml_path for project in projects]
        ProjectFileGenerator(
            projects[0]).generate_intellij_project_file(iml_paths)
        _merge_project_vcs_xmls(projects)

    def _copy_constant_project_files(self):
        """Copy project files to target path with error handling.

        This function would copy compiler.xml, misc.xml, codeStyles folder and
        copyright folder to target folder. Since these files aren't mandatory in
        IntelliJ, it only logs when an IOError occurred.
        """
        target_path = self.project_info.project_absolute_path
        idea_dir = os.path.join(target_path, _IDEA_FOLDER)
        copyright_dir = os.path.join(idea_dir, _COPYRIGHT_FOLDER)
        code_style_dir = os.path.join(idea_dir, _CODE_STYLE_FOLDER)
        common_util.file_generate(
            os.path.join(idea_dir, _COMPILE_XML), templates.XML_COMPILER)
        common_util.file_generate(
            os.path.join(idea_dir, _MISC_XML), templates.XML_MISC)
        common_util.file_generate(
            os.path.join(copyright_dir, _APACHE_2_XML), templates.XML_APACHE_2)
        common_util.file_generate(
            os.path.join(copyright_dir, _PROFILES_SETTINGS_XML),
            templates.XML_PROFILES_SETTINGS)
        common_util.file_generate(
            os.path.join(code_style_dir, _CODE_STYLE_CONFIG_XML),
            templates.XML_CODE_STYLE_CONFIG)
        code_style_target_path = os.path.join(code_style_dir, _PROJECT_XML)
        if os.path.exists(code_style_target_path):
            os.remove(code_style_target_path)
        try:
            shutil.copy2(_CODE_STYLE_SRC_PATH, code_style_target_path)
        except (OSError, SystemError) as err:
            logging.warning('%s can\'t copy the project files\n %s',
                            code_style_target_path, err)
        # Create .gitignore if it doesn't exist.
        _generate_git_ignore(target_path)
        # Create jsonSchemas.xml for TEST_MAPPING.
        _generate_test_mapping_schema(idea_dir)
        # Create config.json for Asuite plugin
        lunch_target = common_util.get_lunch_target()
        if lunch_target:
            common_util.file_generate(
                os.path.join(idea_dir, _CONFIG_JSON), lunch_target)

    def _generate_modules_xml(self, iml_path_list=None):
        """Generate modules.xml file.

        IntelliJ uses modules.xml to import which modules should be loaded to
        project. In multiple modules case, we will pass iml_path_list of
        submodules' dependencies and their iml file paths to add them into main
        module's module.xml file. The dependencies.iml file contains all shared
        dependencies source folders and jar files.

        Args:
            iml_path_list: A list of submodule iml paths.
        """
        module_path = self.project_info.project_absolute_path

        # b/121256503: Prevent duplicated iml names from breaking IDEA.
        module_name = iml.IMLGenerator.get_unique_iml_name(module_path)

        if iml_path_list is not None:
            module_list = [
                _MODULE_SECTION % (module_name, module_name),
                _MODULE_SECTION % (constant.KEY_DEPENDENCIES,
                                   constant.KEY_DEPENDENCIES)
            ]
            for iml_path in iml_path_list:
                module_list.append(_SUB_MODULES_SECTION.format(IML=iml_path))
        else:
            module_list = [
                _MODULE_SECTION % (module_name, module_name)
            ]
        module = '\n'.join(module_list)
        content = self._remove_debugger_token(templates.XML_MODULES)
        content = content.replace(_MODULE_TOKEN, module)
        target_path = os.path.join(module_path, _IDEA_FOLDER, _MODULES_XML)
        common_util.file_generate(target_path, content)

    def _remove_debugger_token(self, content):
        """Remove the token _ENABLE_DEBUGGER_MODULE_TOKEN.

        Remove the token _ENABLE_DEBUGGER_MODULE_TOKEN in 2 cases:
        1. Sub projects don't need to be filled in the enable debugger module
           so we remove the token here. For the main project, the enable
           debugger module will be appended if it exists at the time launching
           IDE.
        2. When there is no need to launch IDE.

        Args:
            content: The content of module.xml.

        Returns:
            String: The content of module.xml.
        """
        if (not project_config.ProjectConfig.get_instance().is_launch_ide or
                not self.project_info.is_main_project):
            content = content.replace(_ENABLE_DEBUGGER_MODULE_TOKEN, '')
        return content


def _merge_project_vcs_xmls(projects):
    """Merge sub projects' git paths into main project's vcs.xml.

    After all projects' vcs.xml are generated, collect the git path of each
    projects and write them into main project's vcs.xml.

    Args:
        projects: A list of ProjectInfo instances.
    """
    main_project_absolute_path = projects[0].project_absolute_path
    if main_project_absolute_path != common_util.get_android_root_dir():
        git_paths = [common_util.find_git_root(project.project_relative_path)
                     for project in projects if project.project_relative_path]
        xml_gen.gen_vcs_xml(main_project_absolute_path, git_paths)
    else:
        ignore_gits = sorted(_get_all_git_path(main_project_absolute_path))
        xml_gen.write_ignore_git_dirs_file(main_project_absolute_path,
                                           ignore_gits)

def _get_all_git_path(root_path):
    """Traverse all subdirectories to get all git folder's path.

    Args:
        root_path: A string of path to traverse.

    Yields:
        A git folder's path.
    """
    for dir_path, dir_names, _ in os.walk(root_path):
        if _GIT_FOLDER_NAME in dir_names:
            yield dir_path


def _generate_git_ignore(target_folder):
    """Generate .gitignore file.

    In target_folder, if there's no .gitignore file, uses symlink() to generate
    one to hide project content files from git.

    Args:
        target_folder: An absolute path string of target folder.
    """
    # TODO(b/133639849): Provide a common method to create symbolic link.
    # TODO(b/133641803): Move out aidegen artifacts from Android repo.
    try:
        gitignore_abs_path = os.path.join(target_folder, _GITIGNORE_FILE_NAME)
        rel_target = os.path.relpath(gitignore_abs_path, os.getcwd())
        rel_source = os.path.relpath(_GITIGNORE_ABS_PATH, target_folder)
        logging.debug('Relative target symlink path: %s.', rel_target)
        logging.debug('Relative ignore_template source path: %s.', rel_source)
        if not os.path.exists(gitignore_abs_path):
            os.symlink(rel_source, rel_target)
    except OSError as err:
        logging.error('Not support to run aidegen on Windows.\n %s', err)


def _generate_test_mapping_schema(idea_dir):
    """Create jsonSchemas.xml for TEST_MAPPING.

    Args:
        idea_dir: An absolute path string of target .idea folder.
    """
    config_path = os.path.join(
        common_util.get_android_root_dir(), _TEST_MAPPING_CONFIG_PATH)
    if os.path.isfile(config_path):
        common_util.file_generate(
            os.path.join(idea_dir, _JSON_SCHEMAS_CONFIG_XML),
            templates.TEST_MAPPING_SCHEMAS_XML.format(SCHEMA_PATH=config_path))
    else:
        logging.warning('Can\'t find TEST_MAPPING.config.json')


def _filter_out_source_paths(source_paths, module_relpaths):
    """Filter out the source paths which belong to the target module.

    The source_paths is a union set of all source paths of all target modules.
    For generating the dependencies.iml, we only need the source paths outside
    the target modules.

    Args:
        source_paths: A set contains the source folder paths.
        module_relpaths: A list, contains the relative paths of target modules
                         except the main module.

    Returns: A set of source paths.
    """
    return {x for x in source_paths if not any(
        {common_util.is_source_under_relative_path(x, y)
         for y in module_relpaths})}


def update_enable_debugger(module_path, enable_debugger_module_abspath=None):
    """Append the enable_debugger module's info in modules.xml file.

    Args:
        module_path: A string of the folder path contains IDE project content,
                     e.g., the folder contains the .idea folder.
        enable_debugger_module_abspath: A string of the im file path of enable
                                        debugger module.
    """
    replace_string = ''
    if enable_debugger_module_abspath:
        replace_string = _SUB_MODULES_SECTION.format(
            IML=enable_debugger_module_abspath)
    target_path = os.path.join(module_path, _IDEA_FOLDER, _MODULES_XML)
    content = common_util.read_file_content(target_path)
    content = content.replace(_ENABLE_DEBUGGER_MODULE_TOKEN, replace_string)
    common_util.file_generate(target_path, content)


def gen_enable_debugger_module(module_abspath, android_sdk_version):
    """Generate the enable_debugger module under AIDEGen config folder.

    Skip generating the enable_debugger module in IntelliJ once the attemption
    of getting the Android SDK version is failed.

    Args:
        module_abspath: the absolute path of the main project.
        android_sdk_version: A string, the Android SDK version in jdk.table.xml.
    """
    if not android_sdk_version:
        return
    with config.AidegenConfig() as aconf:
        if aconf.create_enable_debugger_module(android_sdk_version):
            update_enable_debugger(module_abspath,
                                   config.AidegenConfig.DEBUG_ENABLED_FILE_PATH)
