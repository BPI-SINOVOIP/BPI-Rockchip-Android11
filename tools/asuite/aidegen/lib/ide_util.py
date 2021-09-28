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

"""It is an AIDEGen sub task : IDE operation task!

Takes a project file path as input, after passing the needed check(file
existence, IDE type, etc.), launch the project in related IDE.

    Typical usage example:

    ide_util_obj = IdeUtil()
    if ide_util_obj.is_ide_installed():
        ide_util_obj.config_ide(project_file)
        ide_util_obj.launch_ide()

        # Get the configuration folders of IntelliJ or Android Studio.
        ide_util_obj.get_ide_config_folders()
"""

import glob
import logging
import os
import platform
import re
import subprocess

from xml.etree import ElementTree

from aidegen import constant
from aidegen import templates
from aidegen.lib import aidegen_metrics
from aidegen.lib import android_dev_os
from aidegen.lib import common_util
from aidegen.lib import config
from aidegen.lib import errors
from aidegen.lib import ide_common_util
from aidegen.lib import project_config
from aidegen.lib import project_file_gen
from aidegen.sdk import jdk_table
from aidegen.lib import xml_util

# Add 'nohup' to prevent IDE from being terminated when console is terminated.
_IDEA_FOLDER = '.idea'
_IML_EXTENSION = '.iml'
_JDK_PATH_TOKEN = '@JDKpath'
_COMPONENT_END_TAG = '  </component>'
_ECLIPSE_WS = '~/Documents/AIDEGen_Eclipse_workspace'
_ALERT_CREATE_WS = ('AIDEGen will create a workspace at %s for Eclipse, '
                    'Enter `y` to allow AIDEgen to automatically create the '
                    'workspace for you. Otherwise, you need to select the '
                    'workspace after Eclipse is launched.\nWould you like '
                    'AIDEgen to automatically create the workspace for you?'
                    '(y/n)' % constant.ECLIPSE_WS)
_NO_LAUNCH_IDE_CMD = """
Can not find IDE: {}, in path: {}, you can:
    - add IDE executable to your $PATH
or  - specify the exact IDE executable path by "aidegen -p"
or  - specify "aidegen -n" to generate project file only
"""
_INFO_IMPORT_CONFIG = ('{} needs to import the application configuration for '
                       'the new version!\nAfter the import is finished, rerun '
                       'the command if your project did not launch. Please '
                       'follow the showing dialog to finish the import action.'
                       '\n\n')
CONFIG_DIR = 'config'
LINUX_JDK_PATH = os.path.join(common_util.get_android_root_dir(),
                              'prebuilts/jdk/jdk8/linux-x86')
LINUX_JDK_TABLE_PATH = 'config/options/jdk.table.xml'
LINUX_FILE_TYPE_PATH = 'config/options/filetypes.xml'
LINUX_ANDROID_SDK_PATH = os.path.join(os.getenv('HOME'), 'Android/Sdk')
MAC_JDK_PATH = os.path.join(common_util.get_android_root_dir(),
                            'prebuilts/jdk/jdk8/darwin-x86')
MAC_JDK_TABLE_PATH = 'options/jdk.table.xml'
MAC_FILE_TYPE_XML_PATH = 'options/filetypes.xml'
MAC_ANDROID_SDK_PATH = os.path.join(os.getenv('HOME'), 'Library/Android/sdk')
PATTERN_KEY = 'pattern'
TYPE_KEY = 'type'
JSON_TYPE = 'JSON'
TEST_MAPPING_NAME = 'TEST_MAPPING'
_TEST_MAPPING_TYPE = '<mapping pattern="TEST_MAPPING" type="JSON" />'
_XPATH_EXTENSION_MAP = 'component/extensionMap'
_XPATH_MAPPING = _XPATH_EXTENSION_MAP + '/mapping'


# pylint: disable=too-many-lines
class IdeUtil:
    """Provide a set of IDE operations, e.g., launch and configuration.

    Attributes:
        _ide: IdeBase derived instance, the related IDE object.

    For example:
        1. Check if IDE is installed.
        2. Config IDE, e.g. config code style, SDK path, and etc.
        3. Launch an IDE.
    """

    def __init__(self,
                 installed_path=None,
                 ide='j',
                 config_reset=False,
                 is_mac=False):
        logging.debug('IdeUtil with OS name: %s%s', platform.system(),
                      '(Mac)' if is_mac else '')
        self._ide = _get_ide(installed_path, ide, config_reset, is_mac)

    def is_ide_installed(self):
        """Checks if the IDE is already installed.

        Returns:
            True if IDE is installed already, otherwise False.
        """
        return self._ide.is_ide_installed()

    def launch_ide(self):
        """Launches the relative IDE by opening the passed project file."""
        return self._ide.launch_ide()

    def config_ide(self, project_abspath):
        """To config the IDE, e.g., setup code style, init SDK, and etc.

        Args:
            project_abspath: An absolute path of the project.
        """
        self._ide.project_abspath = project_abspath
        if self.is_ide_installed() and self._ide:
            self._ide.apply_optional_config()

    def get_default_path(self):
        """Gets IDE default installed path."""
        return self._ide.default_installed_path

    def ide_name(self):
        """Gets IDE name."""
        return self._ide.ide_name

    def get_ide_config_folders(self):
        """Gets the config folders of IDE."""
        return self._ide.config_folders


class IdeBase:
    """The most base class of IDE, provides interface and partial path init.

    Class Attributes:
        _JDK_PATH: The path of JDK in android project.
        _IDE_JDK_TABLE_PATH: The path of JDK table which record JDK info in IDE.
        _IDE_FILE_TYPE_PATH: The path of filetypes.xml.
        _JDK_CONTENT: A string, the content of the JDK configuration.
        _DEFAULT_ANDROID_SDK_PATH: A string, the path of Android SDK.
        _CONFIG_DIR: A string of the config folder name.
        _SYMBOLIC_VERSIONS: A string list of the symbolic link paths of the
                            relevant IDE.

    Attributes:
        _installed_path: String for the IDE binary path.
        _config_reset: Boolean, True for reset configuration, else not reset.
        _bin_file_name: String for IDE executable file name.
        _bin_paths: A list of all possible IDE executable file absolute paths.
        _ide_name: String for IDE name.
        _bin_folders: A list of all possible IDE installed paths.
        config_folders: A list of all possible paths for the IntelliJ
                        configuration folder.
        project_abspath: The absolute path of the project.

    For example:
        1. Check if IDE is installed.
        2. Launch IDE.
        3. Config IDE.
    """

    _JDK_PATH = ''
    _IDE_JDK_TABLE_PATH = ''
    _IDE_FILE_TYPE_PATH = ''
    _JDK_CONTENT = ''
    _DEFAULT_ANDROID_SDK_PATH = ''
    _CONFIG_DIR = ''
    _SYMBOLIC_VERSIONS = []

    def __init__(self, installed_path=None, config_reset=False):
        self._installed_path = installed_path
        self._config_reset = config_reset
        self._ide_name = ''
        self._bin_file_name = ''
        self._bin_paths = []
        self._bin_folders = []
        self.config_folders = []
        self.project_abspath = ''

    def is_ide_installed(self):
        """Checks if IDE is already installed.

        Returns:
            True if IDE is installed already, otherwise False.
        """
        return bool(self._installed_path)

    def launch_ide(self):
        """Launches IDE by opening the passed project file."""
        ide_common_util.launch_ide(self.project_abspath, self._get_ide_cmd(),
                                   self._ide_name)

    def apply_optional_config(self):
        """Do IDEA global config action.

        Run code style config, SDK config.
        """
        if not self._installed_path:
            return
        # Skip config action if there's no config folder exists.
        _path_list = self._get_config_root_paths()
        if not _path_list:
            return
        self.config_folders = _path_list.copy()

        for _config_path in _path_list:
            jdk_file = os.path.join(_config_path, self._IDE_JDK_TABLE_PATH)
            jdk_xml = jdk_table.JDKTableXML(jdk_file, self._JDK_CONTENT,
                                            self._JDK_PATH,
                                            self._DEFAULT_ANDROID_SDK_PATH)
            if jdk_xml.config_jdk_table_xml():
                project_file_gen.gen_enable_debugger_module(
                    self.project_abspath, jdk_xml.android_sdk_version)

            # Set the max file size in the idea.properties.
            intellij_config_dir = os.path.join(_config_path, self._CONFIG_DIR)
            config.IdeaProperties(intellij_config_dir).set_max_file_size()

            self._add_test_mapping_file_type(_config_path)

    def _add_test_mapping_file_type(self, _config_path):
        """Adds TEST_MAPPING file type.

        IntelliJ can't recognize TEST_MAPPING files as the json file. It needs
        adding file type mapping in filetypes.xml to recognize TEST_MAPPING
        files.

        Args:
            _config_path: the path of IDE config.
        """
        file_type_path = os.path.join(_config_path, self._IDE_FILE_TYPE_PATH)
        if not os.path.isfile(file_type_path):
            logging.warning('Filetypes.xml is not found.')
            return

        file_type_xml = xml_util.parse_xml(file_type_path)
        if not file_type_xml:
            logging.warning('Can\'t parse filetypes.xml.')
            return

        root = file_type_xml.getroot()
        add_pattern = True
        for mapping in root.findall(_XPATH_MAPPING):
            attrib = mapping.attrib
            if PATTERN_KEY in attrib and TYPE_KEY in attrib:
                if attrib[PATTERN_KEY] == TEST_MAPPING_NAME:
                    if attrib[TYPE_KEY] != JSON_TYPE:
                        attrib[TYPE_KEY] = JSON_TYPE
                        file_type_xml.write(file_type_path)
                    add_pattern = False
                    break
        if add_pattern:
            root.find(_XPATH_EXTENSION_MAP).append(
                ElementTree.fromstring(_TEST_MAPPING_TYPE))
            pretty_xml = common_util.to_pretty_xml(root)
            common_util.file_generate(file_type_path, pretty_xml)

    def _get_config_root_paths(self):
        """Get the config root paths from derived class.

        Returns:
            A string list of IDE config paths, return multiple paths if more
            than one path are found, return an empty list when none is found.
        """
        raise NotImplementedError()

    @property
    def default_installed_path(self):
        """Gets IDE default installed path."""
        return ' '.join(self._bin_folders)

    @property
    def ide_name(self):
        """Gets IDE name."""
        return self._ide_name

    def _get_ide_cmd(self):
        """Compose launch IDE command to run a new process and redirect output.

        Returns:
            A string of launch IDE command.
        """
        return ide_common_util.get_run_ide_cmd(self._installed_path,
                                               self.project_abspath)

    def _init_installed_path(self, installed_path):
        """Initialize IDE installed path.

        Args:
            installed_path: the installed path to be checked.
        """
        if installed_path:
            path_list = ide_common_util.get_script_from_input_path(
                installed_path, self._bin_file_name)
            self._installed_path = path_list[0] if path_list else None
        else:
            self._installed_path = self._get_script_from_system()
        if not self._installed_path:
            logging.error('No %s installed.', self._ide_name)
            return

        self._set_installed_path()

    def _get_script_from_system(self):
        """Get one IDE installed path from internal path.

        Returns:
            The sh full path, or None if not found.
        """
        sh_list = self._get_existent_scripts_in_system()
        return sh_list[0] if sh_list else None

    def _get_possible_bin_paths(self):
        """Gets all possible IDE installed paths."""
        return [os.path.join(f, self._bin_file_name) for f in self._bin_folders]

    def _get_ide_from_environment_paths(self):
        """Get IDE executable binary file from environment paths.

        Returns:
            A string of IDE executable binary path if found, otherwise return
            None.
        """
        env_paths = os.environ['PATH'].split(':')
        for env_path in env_paths:
            path = ide_common_util.get_scripts_from_dir_path(
                env_path, self._bin_file_name)
            if path:
                return path
        return None

    def _setup_ide(self):
        """The callback used to run the necessary setup work of the IDE.

        When ide_util.config_ide is called to set up the JDK, SDK and some
        features, the main thread will callback the Idexxx._setup_ide
        to provide the chance for running the necessary setup of the specific
        IDE. Default is to do nothing.
        """

    def _get_existent_scripts_in_system(self):
        """Gets the relevant IDE run script path from system.

        First get correct IDE installed path from internal paths, if not found
        search it from environment paths.

        Returns:
            The list of script full path, or None if no found.
        """
        return (ide_common_util.get_script_from_internal_path(self._bin_paths,
                                                              self._ide_name) or
                self._get_ide_from_environment_paths())

    def _get_user_preference(self, versions):
        """Make sure the version is valid and update preference if needed.

        Args:
            versions: A list of the IDE script path, contains the symbolic path.

        Returns: An IDE script path, or None is not found.
        """
        if not versions:
            return None
        if len(versions) == 1:
            return versions[0]
        with config.AidegenConfig() as conf:
            if not self._config_reset and (conf.preferred_version(self.ide_name)
                                           in versions):
                return conf.preferred_version(self.ide_name)
            display_versions = self._merge_symbolic_version(versions)
            preferred = ide_common_util.ask_preference(display_versions,
                                                       self.ide_name)
            if preferred:
                conf.set_preferred_version(self._get_real_path(preferred),
                                           self.ide_name)

            return conf.preferred_version(self.ide_name)

    def _set_installed_path(self):
        """Write the user's input installed path into the config file.

        If users input an existent IDE installed path, we should keep it in
        the configuration.
        """
        if self._installed_path:
            with config.AidegenConfig() as aconf:
                aconf.set_preferred_version(self._installed_path, self.ide_name)

    def _merge_symbolic_version(self, versions):
        """Merge the duplicate version of symbolic links.

        Stable and beta versions are a symbolic link to an existing version.
        This function assemble symbolic and real to make it more clear to read.
        Ex:
        ['/opt/intellij-ce-stable/bin/idea.sh',
        '/opt/intellij-ce-2019.1/bin/idea.sh'] to
        ['/opt/intellij-ce-stable/bin/idea.sh ->
        /opt/intellij-ce-2019.1/bin/idea.sh',
        '/opt/intellij-ce-2019.1/bin/idea.sh']

        Args:
            versions: A list of all installed versions.

        Returns:
            A list of versions to show for user to select. It may contain
            'symbolic_path/idea.sh -> original_path/idea.sh'.
        """
        display_versions = versions[:]
        for symbolic in self._SYMBOLIC_VERSIONS:
            if symbolic in display_versions and (os.path.isfile(symbolic)):
                real_path = os.path.realpath(symbolic)
                for index, path in enumerate(display_versions):
                    if path == symbolic:
                        display_versions[index] = ' -> '.join(
                            [display_versions[index], real_path])
                        break
        return display_versions

    @staticmethod
    def _get_real_path(path):
        """ Get real path from merged path.

        Turn the path string "/opt/intellij-ce-stable/bin/idea.sh -> /opt/
        intellij-ce-2019.2/bin/idea.sh" into
        "/opt/intellij-ce-stable/bin/idea.sh"

        Args:
            path: A path string may be merged with symbolic path.
        Returns:
            The real IntelliJ installed path.
        """
        return path.split()[0]


class IdeIntelliJ(IdeBase):
    """Provide basic IntelliJ ops, e.g., launch IDEA, and config IntelliJ.

    For example:
        1. Check if IntelliJ is installed.
        2. Launch an IntelliJ.
        3. Config IntelliJ.
    """
    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._ide_name = constant.IDE_INTELLIJ
        self._ls_ce_path = ''
        self._ls_ue_path = ''

    def _get_config_root_paths(self):
        """Get the config root paths from derived class.

        Returns:
            A string list of IDE config paths, return multiple paths if more
            than one path are found, return an empty list when none is found.
        """
        raise NotImplementedError()

    def _get_preferred_version(self):
        """Get the user's preferred IntelliJ version.

        Locates the IntelliJ IDEA launch script path by following rule.

        1. If config file recorded user's preference version, load it.
        2. If config file didn't record, search them form default path if there
           are more than one version, ask user and record it.

        Returns:
            The sh full path, or None if no IntelliJ version is installed.
        """
        ce_paths = ide_common_util.get_intellij_version_path(self._ls_ce_path)
        ue_paths = ide_common_util.get_intellij_version_path(self._ls_ue_path)
        all_versions = self._get_all_versions(ce_paths, ue_paths)
        tmp_versions = all_versions.copy()
        for version in tmp_versions:
            real_version = os.path.realpath(version)
            if config.AidegenConfig.deprecated_intellij_version(real_version):
                all_versions.remove(version)
        return self._get_user_preference(all_versions)

    def _setup_ide(self):
        """The callback used to run the necessary setup work for the IDE.

        IntelliJ has a default flow to let the user import the configuration
        from the previous version, aidegen makes sure not to break the behavior
        by checking in this callback implementation.
        """
        run_script_path = os.path.realpath(self._installed_path)
        app_folder = self._get_application_path(run_script_path)
        if not app_folder:
            logging.warning('\nInvalid IDE installed path.')
            return

        show_hint = False
        folder_path = os.path.join(os.getenv('HOME'), app_folder,
                                   'config', 'plugins')
        import_process = None
        while not os.path.isdir(folder_path):
            # Guide the user to go through the IDE flow.
            if not show_hint:
                print('\n{} {}'.format(common_util.COLORED_INFO('INFO:'),
                                       _INFO_IMPORT_CONFIG.format(
                                           self.ide_name)))
                try:
                    import_process = subprocess.Popen(
                        ide_common_util.get_run_ide_cmd(run_script_path, ''),
                        shell=True)
                except (subprocess.SubprocessError, ValueError):
                    logging.warning('\nSubprocess call gets the invalid input.')
                finally:
                    show_hint = True
        try:
            import_process.wait(1)
        except subprocess.TimeoutExpired:
            import_process.terminate()
        return

    def _get_script_from_system(self):
        """Get correct IntelliJ installed path from internal path.

        Returns:
            The sh full path, or None if no IntelliJ version is installed.
        """
        found = self._get_preferred_version()
        if found:
            logging.debug('IDE internal installed path: %s.', found)
        return found

    @staticmethod
    def _get_all_versions(cefiles, uefiles):
        """Get all versions of launch script files.

        Args:
            cefiles: CE version launch script paths.
            uefiles: UE version launch script paths.

        Returns:
            A list contains all versions of launch script files.
        """
        all_versions = []
        if cefiles:
            all_versions.extend(cefiles)
        if uefiles:
            all_versions.extend(uefiles)
        return all_versions

    @staticmethod
    def _get_application_path(run_script_path):
        """Get the relevant configuration folder based on the run script path.

        Args:
            run_script_path: The string of the run script path for the IntelliJ.

        Returns:
            The string of the IntelliJ application folder name or None if the
            run_script_path is invalid. The returned folder format is as
            follows,
                1. .IdeaIC2019.3
                2. .IntelliJIdea2019.3
        """
        if not run_script_path or not os.path.isfile(run_script_path):
            return None
        index = str.find(run_script_path, 'intellij-')
        target_path = None if index == -1 else run_script_path[index:]
        if not target_path or '-' not in run_script_path:
            return None

        path_data = target_path.split('-')
        if not path_data or len(path_data) < 3:
            return None

        config_folder = None
        ide_version = path_data[2].split(os.sep)[0]

        if path_data[1] == 'ce':
            config_folder = ''.join(['.IdeaIC', ide_version])
        elif path_data[1] == 'ue':
            config_folder = ''.join(['.IntelliJIdea', ide_version])

        return config_folder


class IdeLinuxIntelliJ(IdeIntelliJ):
    """Provide the IDEA behavior implementation for OS Linux.

    Class Attributes:
        _INTELLIJ_RE: Regular expression of IntelliJ installed name in GLinux.

    For example:
        1. Check if IntelliJ is installed.
        2. Launch an IntelliJ.
        3. Config IntelliJ.
    """

    _JDK_PATH = LINUX_JDK_PATH
    # TODO(b/127899277): Preserve a config for jdk version option case.
    _CONFIG_DIR = CONFIG_DIR
    _IDE_JDK_TABLE_PATH = LINUX_JDK_TABLE_PATH
    _IDE_FILE_TYPE_PATH = LINUX_FILE_TYPE_PATH
    _JDK_CONTENT = templates.LINUX_JDK_XML
    _DEFAULT_ANDROID_SDK_PATH = LINUX_ANDROID_SDK_PATH
    _SYMBOLIC_VERSIONS = ['/opt/intellij-ce-stable/bin/idea.sh',
                          '/opt/intellij-ue-stable/bin/idea.sh',
                          '/opt/intellij-ce-beta/bin/idea.sh',
                          '/opt/intellij-ue-beta/bin/idea.sh']
    _INTELLIJ_RE = re.compile(r'intellij-(ce|ue)-')

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'idea.sh'
        self._bin_folders = ['/opt/intellij-*/bin']
        self._ls_ce_path = os.path.join('/opt/intellij-ce-*/bin',
                                        self._bin_file_name)
        self._ls_ue_path = os.path.join('/opt/intellij-ue-*/bin',
                                        self._bin_file_name)
        self._init_installed_path(installed_path)

    def _get_config_root_paths(self):
        """To collect the global config folder paths of IDEA as a string list.

        The config folder of IntelliJ IDEA is under the user's home directory,
        .IdeaIC20xx.x and .IntelliJIdea20xx.x are folder names for different
        versions.

        Returns:
            A string list for IDE config root paths, and return an empty list
            when none is found.
        """
        if not self._installed_path:
            return None

        _config_folders = []
        _config_folder = ''
        if IdeLinuxIntelliJ._INTELLIJ_RE.search(self._installed_path):
            _path_data = os.path.realpath(self._installed_path)
            _config_folder = self._get_application_path(_path_data)
            if not _config_folder:
                return None

            if not os.path.isdir(os.path.join(os.getenv('HOME'),
                                              _config_folder)):
                logging.debug("\nThe config folder: %s doesn't exist",
                              _config_folder)
                self._setup_ide()

            _config_folders.append(
                os.path.join(os.getenv('HOME'), _config_folder))
        else:
            # TODO(b/123459239): For the case that the user provides the IDEA
            # binary path, we now collect all possible IDEA config root paths.
            _config_folders = glob.glob(
                os.path.join(os.getenv('HOME'), '.IdeaI?20*'))
            _config_folders.extend(
                glob.glob(os.path.join(os.getenv('HOME'), '.IntelliJIdea20*')))
            logging.debug('The config path list: %s.', _config_folders)

        return _config_folders


class IdeMacIntelliJ(IdeIntelliJ):
    """Provide the IDEA behavior implementation for OS Mac.

    For example:
        1. Check if IntelliJ is installed.
        2. Launch an IntelliJ.
        3. Config IntelliJ.
    """

    _JDK_PATH = MAC_JDK_PATH
    _IDE_JDK_TABLE_PATH = MAC_JDK_TABLE_PATH
    _IDE_FILE_TYPE_PATH = MAC_FILE_TYPE_XML_PATH
    _JDK_CONTENT = templates.MAC_JDK_XML
    _DEFAULT_ANDROID_SDK_PATH = MAC_ANDROID_SDK_PATH

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'idea'
        self._bin_folders = ['/Applications/IntelliJ IDEA.app/Contents/MacOS']
        self._bin_paths = self._get_possible_bin_paths()
        self._ls_ce_path = os.path.join(
            '/Applications/IntelliJ IDEA CE.app/Contents/MacOS',
            self._bin_file_name)
        self._ls_ue_path = os.path.join(
            '/Applications/IntelliJ IDEA.app/Contents/MacOS',
            self._bin_file_name)
        self._init_installed_path(installed_path)

    def _get_config_root_paths(self):
        """To collect the global config folder paths of IDEA as a string list.

        Returns:
            A string list for IDE config root paths, and return an empty list
            when none is found.
        """
        if not self._installed_path:
            return None

        _config_folders = []
        if 'IntelliJ' in self._installed_path:
            _config_folders = glob.glob(
                os.path.join(
                    os.getenv('HOME'), 'Library/Preferences/IdeaI?20*'))
            _config_folders.extend(
                glob.glob(
                    os.path.join(
                        os.getenv('HOME'),
                        'Library/Preferences/IntelliJIdea20*')))
        return _config_folders


class IdeStudio(IdeBase):
    """Class offers a set of Android Studio launching utilities.

    For example:
        1. Check if Android Studio is installed.
        2. Launch an Android Studio.
        3. Config Android Studio.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._ide_name = constant.IDE_ANDROID_STUDIO

    def _get_config_root_paths(self):
        """Get the config root paths from derived class.

        Returns:
            A string list of IDE config paths, return multiple paths if more
            than one path are found, return an empty list when none is found.
        """
        raise NotImplementedError()

    def _get_script_from_system(self):
        """Get correct Studio installed path from internal path.

        Returns:
            The sh full path, or None if no Studio version is installed.
        """
        found = self._get_preferred_version()
        if found:
            logging.debug('IDE internal installed path: %s.', found)
        return found

    def _get_preferred_version(self):
        """Get the user's preferred Studio version.

        Locates the Studio launch script path by following rule.

        1. If config file recorded user's preference version, load it.
        2. If config file didn't record, search them form default path if there
           are more than one version, ask user and record it.

        Returns:
            The sh full path, or None if no Studio version is installed.
        """
        versions = self._get_existent_scripts_in_system()
        if not versions:
            return None
        for version in versions:
            real_version = os.path.realpath(version)
            if config.AidegenConfig.deprecated_studio_version(real_version):
                versions.remove(version)
        return self._get_user_preference(versions)

    def apply_optional_config(self):
        """Do the configuration of Android Studio.

        Configures code style and SDK for Java project and do nothing for
        others.
        """
        if not self.project_abspath:
            return
        # TODO(b/150662865): The following workaround should be replaced.
        # Since the path of the artifact for Java is the .idea directory but
        # native is a CMakeLists.txt file using this to workaround first.
        if os.path.isfile(self.project_abspath):
            return
        if os.path.isdir(self.project_abspath):
            IdeBase.apply_optional_config(self)


class IdeLinuxStudio(IdeStudio):
    """Class offers a set of Android Studio launching utilities for OS Linux.

    For example:
        1. Check if Android Studio is installed.
        2. Launch an Android Studio.
        3. Config Android Studio.
    """

    _JDK_PATH = LINUX_JDK_PATH
    _CONFIG_DIR = CONFIG_DIR
    _IDE_JDK_TABLE_PATH = LINUX_JDK_TABLE_PATH
    _JDK_CONTENT = templates.LINUX_JDK_XML
    _DEFAULT_ANDROID_SDK_PATH = LINUX_ANDROID_SDK_PATH
    _SYMBOLIC_VERSIONS = [
        '/opt/android-studio-with-blaze-stable/bin/studio.sh',
        '/opt/android-studio-stable/bin/studio.sh',
        '/opt/android-studio-with-blaze-beta/bin/studio.sh',
        '/opt/android-studio-beta/bin/studio.sh']

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'studio.sh'
        self._bin_folders = ['/opt/android-studio*/bin']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)

    def _get_config_root_paths(self):
        """Collect the global config folder paths as a string list.

        Returns:
            A string list for IDE config root paths, and return an empty list
            when none is found.
        """
        return glob.glob(os.path.join(os.getenv('HOME'), '.AndroidStudio*'))


class IdeMacStudio(IdeStudio):
    """Class offers a set of Android Studio launching utilities for OS Mac.

    For example:
        1. Check if Android Studio is installed.
        2. Launch an Android Studio.
        3. Config Android Studio.
    """

    _JDK_PATH = MAC_JDK_PATH
    _IDE_JDK_TABLE_PATH = MAC_JDK_TABLE_PATH
    _JDK_CONTENT = templates.MAC_JDK_XML
    _DEFAULT_ANDROID_SDK_PATH = MAC_ANDROID_SDK_PATH

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'studio'
        self._bin_folders = ['/Applications/Android Studio.app/Contents/MacOS']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)

    def _get_config_root_paths(self):
        """Collect the global config folder paths as a string list.

        Returns:
            A string list for IDE config root paths, and return an empty list
            when none is found.
        """
        return glob.glob(
            os.path.join(
                os.getenv('HOME'), 'Library/Preferences/AndroidStudio*'))


class IdeEclipse(IdeBase):
    """Class offers a set of Eclipse launching utilities.

    Attributes:
        cmd: A list of the build command.

    For example:
        1. Check if Eclipse is installed.
        2. Launch an Eclipse.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._ide_name = constant.IDE_ECLIPSE
        self._bin_file_name = 'eclipse'
        self.cmd = []

    def _get_script_from_system(self):
        """Get correct IDE installed path from internal path.

        Remove any file with extension, the filename should be like, 'eclipse',
        'eclipse47' and so on, check if the file is executable and filter out
        file such as 'eclipse.ini'.

        Returns:
            The sh full path, or None if no IntelliJ version is installed.
        """
        for ide_path in self._bin_paths:
            # The binary name of Eclipse could be eclipse47, eclipse49,
            # eclipse47_testing or eclipse49_testing. So finding the matched
            # binary by /path/to/ide/eclipse*.
            ls_output = glob.glob(ide_path + '*', recursive=True)
            if ls_output:
                ls_output = sorted(ls_output)
                match_eclipses = []
                for path in ls_output:
                    if os.access(path, os.X_OK):
                        match_eclipses.append(path)
                if match_eclipses:
                    match_eclipses = sorted(match_eclipses)
                    logging.debug('Result for checking %s after sort: %s.',
                                  self._ide_name, match_eclipses[0])
                    return match_eclipses[0]
        return None

    def _get_ide_cmd(self):
        """Compose launch IDE command to run a new process and redirect output.

        AIDEGen will create a default workspace
        ~/Documents/AIDEGen_Eclipse_workspace for users if they agree to do
        that. Also, we could not import the default project through the command
        line so remove the project path argument.

        Returns:
            A string of launch IDE command.
        """
        if (os.path.exists(os.path.expanduser(constant.ECLIPSE_WS))
                or str(input(_ALERT_CREATE_WS)).lower() == 'y'):
            self.cmd.extend(['-data', constant.ECLIPSE_WS])
        self.cmd.extend([constant.IGNORE_STD_OUT_ERR_CMD])
        return ' '.join(self.cmd)

    def apply_optional_config(self):
        """Override to do nothing."""

    def _get_config_root_paths(self):
        """Override to do nothing."""


class IdeLinuxEclipse(IdeEclipse):
    """Class offers a set of Eclipse launching utilities for OS Linux.

    For example:
        1. Check if Eclipse is installed.
        2. Launch an Eclipse.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_folders = ['/opt/eclipse*', '/usr/bin/']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)
        self.cmd = [constant.NOHUP, self._installed_path.replace(' ', r'\ ')]


class IdeMacEclipse(IdeEclipse):
    """Class offers a set of Eclipse launching utilities for OS Mac.

    For example:
        1. Check if Eclipse is installed.
        2. Launch an Eclipse.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'eclipse'
        self._bin_folders = [os.path.expanduser('~/eclipse/**')]
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)
        self.cmd = [self._installed_path.replace(' ', r'\ ')]


class IdeCLion(IdeBase):
    """Class offers a set of CLion launching utilities.

    For example:
        1. Check if CLion is installed.
        2. Launch an CLion.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._ide_name = constant.IDE_CLION

    def apply_optional_config(self):
        """Override to do nothing."""

    def _get_config_root_paths(self):
        """Override to do nothing."""


class IdeLinuxCLion(IdeCLion):
    """Class offers a set of CLion launching utilities for OS Linux.

    For example:
        1. Check if CLion is installed.
        2. Launch an CLion.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'clion.sh'
        # TODO(b/141288011): Handle /opt/clion-*/bin to let users choose a
        # preferred version of CLion in the future.
        self._bin_folders = ['/opt/clion-stable/bin']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)


class IdeMacCLion(IdeCLion):
    """Class offers a set of Android Studio launching utilities for OS Mac.

    For example:
        1. Check if Android Studio is installed.
        2. Launch an Android Studio.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'clion'
        self._bin_folders = ['/Applications/CLion.app/Contents/MacOS/CLion']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)


class IdeVSCode(IdeBase):
    """Class offers a set of VSCode launching utilities.

    For example:
        1. Check if VSCode is installed.
        2. Launch an VSCode.
    """

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._ide_name = constant.IDE_VSCODE

    def apply_optional_config(self):
        """Override to do nothing."""

    def _get_config_root_paths(self):
        """Override to do nothing."""


class IdeLinuxVSCode(IdeVSCode):
    """Class offers a set of VSCode launching utilities for OS Linux."""

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'code'
        self._bin_folders = ['/usr/bin']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)


class IdeMacVSCode(IdeVSCode):
    """Class offers a set of VSCode launching utilities for OS Mac."""

    def __init__(self, installed_path=None, config_reset=False):
        super().__init__(installed_path, config_reset)
        self._bin_file_name = 'code'
        self._bin_folders = ['/usr/local/bin']
        self._bin_paths = self._get_possible_bin_paths()
        self._init_installed_path(installed_path)


def get_ide_util_instance(ide='j'):
    """Get an IdeUtil class instance for launching IDE.

    Args:
        ide: A key character of IDE to be launched. Default ide='j' is to
            launch IntelliJ.

    Returns:
        An IdeUtil class instance.
    """
    conf = project_config.ProjectConfig.get_instance()
    if not conf.is_launch_ide:
        return None
    is_mac = (android_dev_os.AndroidDevOS.MAC == android_dev_os.AndroidDevOS.
              get_os_type())
    tool = IdeUtil(conf.ide_installed_path, ide, conf.config_reset, is_mac)
    if not tool.is_ide_installed():
        ipath = conf.ide_installed_path or tool.get_default_path()
        err = _NO_LAUNCH_IDE_CMD.format(constant.IDE_NAME_DICT[ide], ipath)
        logging.error(err)
        stack_trace = common_util.remove_user_home_path(err)
        logs = '%s exists? %s' % (common_util.remove_user_home_path(ipath),
                                  os.path.exists(ipath))
        aidegen_metrics.ends_asuite_metrics(constant.IDE_LAUNCH_FAILURE,
                                            stack_trace,
                                            logs)
        raise errors.IDENotExistError(err)
    return tool


def _get_ide(installed_path=None, ide='j', config_reset=False, is_mac=False):
    """Get IDE to be launched according to the ide input and OS type.

    Args:
        installed_path: The IDE installed path to be checked.
        ide: A key character of IDE to be launched. Default ide='j' is to
            launch IntelliJ.
        config_reset: A boolean, if true reset configuration data.

    Returns:
        A corresponding IDE instance.
    """
    if is_mac:
        return _get_mac_ide(installed_path, ide, config_reset)
    return _get_linux_ide(installed_path, ide, config_reset)


def _get_mac_ide(installed_path=None, ide='j', config_reset=False):
    """Get IDE to be launched according to the ide input for OS Mac.

    Args:
        installed_path: The IDE installed path to be checked.
        ide: A key character of IDE to be launched. Default ide='j' is to
            launch IntelliJ.
        config_reset: A boolean, if true reset configuration data.

    Returns:
        A corresponding IDE instance.
    """
    if ide == 'e':
        return IdeMacEclipse(installed_path, config_reset)
    if ide == 's':
        return IdeMacStudio(installed_path, config_reset)
    if ide == 'c':
        return IdeMacCLion(installed_path, config_reset)
    if ide == 'v':
        return IdeMacVSCode(installed_path, config_reset)
    return IdeMacIntelliJ(installed_path, config_reset)


def _get_linux_ide(installed_path=None, ide='j', config_reset=False):
    """Get IDE to be launched according to the ide input for OS Linux.

    Args:
        installed_path: The IDE installed path to be checked.
        ide: A key character of IDE to be launched. Default ide='j' is to
            launch IntelliJ.
        config_reset: A boolean, if true reset configuration data.

    Returns:
        A corresponding IDE instance.
    """
    if ide == 'e':
        return IdeLinuxEclipse(installed_path, config_reset)
    if ide == 's':
        return IdeLinuxStudio(installed_path, config_reset)
    if ide == 'c':
        return IdeLinuxCLion(installed_path, config_reset)
    if ide == 'v':
        return IdeLinuxVSCode(installed_path, config_reset)
    return IdeLinuxIntelliJ(installed_path, config_reset)
