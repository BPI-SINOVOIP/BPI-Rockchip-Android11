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

"""Config class.

History:
    version 2: Record the user's each preferred ide version by the key name
               [ide_base.ide_name]_preferred_version. E.g., the key name of the
               preferred IntelliJ is IntelliJ_preferred_version and the example
               is as follows.
               "Android Studio_preferred_version": "/opt/android-studio-3.0/bin/
               studio.sh"
               "IntelliJ_preferred_version": "/opt/intellij-ce-stable/bin/
               idea.sh"

    version 1: Record the user's preferred IntelliJ version by the key name
               preferred_version and doesn't support any other IDEs. The example
               is "preferred_version": "/opt/intellij-ce-stable/bin/idea.sh".
"""

import copy
import json
import logging
import os
import re

from aidegen import constant
from aidegen import templates
from aidegen.lib import common_util

_DIR_LIB = 'lib'


class AidegenConfig:
    """Class manages AIDEGen's configurations.

    Attributes:
        _config: A dict contains the aidegen config.
        _config_backup: A dict contains the aidegen config.
    """

    # Constants of AIDEGen config
    _DEFAULT_CONFIG_FILE = 'aidegen.config'
    _CONFIG_DIR = os.path.join(
        os.path.expanduser('~'), '.config', 'asuite', 'aidegen')
    _CONFIG_FILE_PATH = os.path.join(_CONFIG_DIR, _DEFAULT_CONFIG_FILE)
    _KEY_APPEND = 'preferred_version'

    # Constants of enable debugger
    _ENABLE_DEBUG_CONFIG_DIR = 'enable_debugger'
    _ENABLE_DEBUG_CONFIG_FILE = 'enable_debugger.iml'
    _ENABLE_DEBUG_DIR = os.path.join(_CONFIG_DIR, _ENABLE_DEBUG_CONFIG_DIR)
    _DIR_SRC = 'src'
    _DIR_GEN = 'gen'
    DEBUG_ENABLED_FILE_PATH = os.path.join(_ENABLE_DEBUG_DIR,
                                           _ENABLE_DEBUG_CONFIG_FILE)

    # Constants of checking deprecated IntelliJ version.
    # The launch file idea.sh of IntelliJ is in ASCII encoding.
    ENCODE_TYPE = 'ISO-8859-1'
    ACTIVE_KEYWORD = '$JAVA_BIN'

    def __init__(self):
        self._config = {}
        self._config_backup = {}
        self._create_config_folder()

    def __enter__(self):
        self._load_aidegen_config()
        self._config_backup = copy.deepcopy(self._config)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._save_aidegen_config()

    def preferred_version(self, ide=None):
        """AIDEGen configuration getter.

        Args:
            ide: The string of the relevant IDE name, same as the data of
                 IdeBase._ide_name or IdeUtil.ide_name(). None represents the
                 usage of the version 1.

        Returns:
            The preferred version item of configuration data if exists and is
            not deprecated, otherwise None.
        """
        key = '_'.join([ide, self._KEY_APPEND]) if ide else self._KEY_APPEND
        preferred_version = self._config.get(key, '')
        # Backward compatible check.
        if not preferred_version:
            preferred_version = self._config.get(self._KEY_APPEND, '')

        if preferred_version:
            real_version = os.path.realpath(preferred_version)
            if ide and not self.deprecated_version(ide, real_version):
                return preferred_version
            # Backward compatible handling.
            if not ide and not self.deprecated_intellij_version(real_version):
                return preferred_version
        return None

    def set_preferred_version(self, preferred_version, ide=None):
        """AIDEGen configuration setter.

        Args:
            preferred_version: A string, user's preferred version to be set.
            ide: The string of the relevant IDE name, same as the data of
                 IdeBase._ide_name or IdeUtil.ide_name(). None presents the
                 usage of the version 1.
        """
        key = '_'.join([ide, self._KEY_APPEND]) if ide else self._KEY_APPEND
        self._config[key] = preferred_version

    def _load_aidegen_config(self):
        """Load data from configuration file."""
        if os.path.exists(self._CONFIG_FILE_PATH):
            try:
                with open(self._CONFIG_FILE_PATH) as cfg_file:
                    self._config = json.load(cfg_file)
            except ValueError as err:
                info = '{} format is incorrect, error: {}'.format(
                    self._CONFIG_FILE_PATH, err)
                logging.info(info)
            except IOError as err:
                logging.error(err)
                raise

    def _save_aidegen_config(self):
        """Save data to configuration file."""
        if self._is_config_modified():
            with open(self._CONFIG_FILE_PATH, 'w') as cfg_file:
                json.dump(self._config, cfg_file, indent=4)

    def _is_config_modified(self):
        """Check if configuration data is modified."""
        return any(key for key in self._config if not key in self._config_backup
                   or self._config[key] != self._config_backup[key])

    def _create_config_folder(self):
        """Create the config folder if it doesn't exist."""
        if not os.path.exists(self._CONFIG_DIR):
            os.makedirs(self._CONFIG_DIR)

    def _gen_enable_debug_sub_dir(self, dir_name):
        """Generate a dir under enable debug dir.

        Args:
            dir_name: A string of the folder name.
        """
        _dir = os.path.join(self._ENABLE_DEBUG_DIR, dir_name)
        if not os.path.exists(_dir):
            os.makedirs(_dir)

    def _gen_androidmanifest(self):
        """Generate an AndroidManifest.xml under enable debug dir.

        Once the AndroidManifest.xml does not exist or file size is zero,
        AIDEGen will generate it with default content to prevent the red
        underline error in IntelliJ.
        """
        _file = os.path.join(self._ENABLE_DEBUG_DIR, constant.ANDROID_MANIFEST)
        if not os.path.exists(_file) or os.stat(_file).st_size == 0:
            common_util.file_generate(_file, templates.ANDROID_MANIFEST_CONTENT)

    def _gen_enable_debugger_config(self, android_sdk_version):
        """Generate the enable_debugger.iml config file.

        Re-generate the enable_debugger.iml everytime for correcting the Android
        SDK version.

        Args:
            android_sdk_version: The version name of the Android Sdk in the
                                 jdk.table.xml.
        """
        content = templates.XML_ENABLE_DEBUGGER.format(
            ANDROID_SDK_VERSION=android_sdk_version)
        common_util.file_generate(self.DEBUG_ENABLED_FILE_PATH, content)

    def create_enable_debugger_module(self, android_sdk_version):
        """Create the enable_debugger module.

        1. Create two empty folders named src and gen.
        2. Create an empty file named AndroidManifest.xml
        3. Create the enable_denugger.iml.

        Args:
            android_sdk_version: The version name of the Android Sdk in the
                                 jdk.table.xml.

        Returns: True if successfully generate the enable debugger module,
                 otherwise False.
        """
        try:
            self._gen_enable_debug_sub_dir(self._DIR_SRC)
            self._gen_enable_debug_sub_dir(self._DIR_GEN)
            self._gen_androidmanifest()
            self._gen_enable_debugger_config(android_sdk_version)
            return True
        except (IOError, OSError) as err:
            logging.warning(('Can\'t create the enable_debugger module in %s.\n'
                             '%s'), self._CONFIG_DIR, err)
            return False

    @staticmethod
    def deprecated_version(ide, script_path):
        """Check if the script_path belongs to a deprecated IDE version.

        Args:
            ide: The string of the relevant IDE name, same as the data of
                 IdeBase._ide_name or IdeUtil.ide_name().
            script_path: The path string of the IDE script file.

        Returns: True if the preferred version is deprecated, otherwise False.
        """
        if ide == constant.IDE_ANDROID_STUDIO:
            return AidegenConfig.deprecated_studio_version(script_path)
        if ide == constant.IDE_INTELLIJ:
            return AidegenConfig.deprecated_intellij_version(script_path)
        return False

    @staticmethod
    def deprecated_intellij_version(idea_path):
        """Check if the preferred IntelliJ version is deprecated or not.

        The IntelliJ version is deprecated once the string "$JAVA_BIN" doesn't
        exist in the idea.sh.

        Args:
            idea_path: the absolute path to idea.sh.

        Returns: True if the preferred version was deprecated, otherwise False.
        """
        if os.path.isfile(idea_path):
            file_content = common_util.read_file_content(
                idea_path, AidegenConfig.ENCODE_TYPE)
            return AidegenConfig.ACTIVE_KEYWORD not in file_content
        return False

    @staticmethod
    def deprecated_studio_version(script_path):
        """Check if the preferred Studio version is deprecated or not.

        The Studio version is deprecated once the /android-studio-*/lib folder
        doesn't exist.

        Args:
            script_path: the absolute path to the ide script file.

        Returns: True if the preferred version is deprecated, otherwise False.
        """
        if not os.path.isfile(script_path):
            return True
        script_dir = os.path.dirname(script_path)
        if not os.path.isdir(script_dir):
            return True
        lib_path = os.path.join(os.path.dirname(script_dir), _DIR_LIB)
        return not os.path.isdir(lib_path)


class IdeaProperties:
    """Class manages IntelliJ's idea.properties attribute.

    Class Attributes:
        _PROPERTIES_FILE: The property file name of IntelliJ.
        _KEY_FILESIZE: The key name of the maximun file size.
        _FILESIZE_LIMIT: The value to be set as the max file size.
        _RE_SEARCH_FILESIZE: A regular expression to find the current max file
                             size.
        _PROPERTIES_CONTENT: The default content of idea.properties to be
                             generated.

    Attributes:
        idea_file: The absolute path of the idea.properties.
                   For example:
                   In Linux, it is ~/.IdeaIC2019.1/config/idea.properties.
                   In Mac, it is ~/Library/Preferences/IdeaIC2019.1/
                   idea.properties.
    """

    # Constants of idea.properties
    _PROPERTIES_FILE = 'idea.properties'
    _KEY_FILESIZE = 'idea.max.intellisense.filesize'
    _FILESIZE_LIMIT = 100000
    _RE_SEARCH_FILESIZE = r'%s\s?=\s?(?P<value>\d+)' % _KEY_FILESIZE
    _PROPERTIES_CONTENT = """# custom IntelliJ IDEA properties

#-------------------------------------------------------------------------------
# Maximum size of files (in kilobytes) for which IntelliJ IDEA provides coding
# assistance. Coding assistance for large files can affect editor performance
# and increase memory consumption.
# The default value is 2500.
#-------------------------------------------------------------------------------
idea.max.intellisense.filesize=100000
"""

    def __init__(self, config_dir):
        """IdeaProperties initialize.

        Args:
            config_dir: The absolute dir of the idea.properties.
        """
        self.idea_file = os.path.join(config_dir, self._PROPERTIES_FILE)

    def _set_default_idea_properties(self):
        """Create the file idea.properties."""
        common_util.file_generate(self.idea_file, self._PROPERTIES_CONTENT)

    def _reset_max_file_size(self):
        """Reset the max file size value in the idea.properties."""
        updated_flag = False
        properties = common_util.read_file_content(self.idea_file).splitlines()
        for index, line in enumerate(properties):
            res = re.search(self._RE_SEARCH_FILESIZE, line)
            if res and int(res.group('value')) < self._FILESIZE_LIMIT:
                updated_flag = True
                properties[index] = '%s=%s' % (self._KEY_FILESIZE,
                                               str(self._FILESIZE_LIMIT))
        if updated_flag:
            common_util.file_generate(self.idea_file, '\n'.join(properties))

    def set_max_file_size(self):
        """Set the max file size parameter in the idea.properties."""
        if not os.path.exists(self.idea_file):
            self._set_default_idea_properties()
        else:
            self._reset_max_file_size()
