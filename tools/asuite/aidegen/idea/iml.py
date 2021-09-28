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

"""Creates the iml file for each module.

This class is used to create the iml file for each module. So far, only generate
the create_srcjar() for the framework-all module.

Usage example:
    modules_info = project_info.ProjectInfo.modules_info
    mod_info = modules_info.name_to_module_info['module']
    iml = IMLGenerator(mod_info)
    iml.create()
"""

from __future__ import absolute_import

import logging
import os

from aidegen import constant
from aidegen import templates
from aidegen.lib import common_util


class IMLGenerator:
    """Creates the iml file for each module.

    Class attributes:
        _USED_NAME_CACHE: A dict to cache already used iml project file names
                          and prevent duplicated iml names from breaking IDEA.

    Attributes:
        _mod_info: A dictionary of the module's data from module-info.json.
        _android_root: A string ot the Android root's absolute path.
        _mod_path: A string of the module's absolute path.
        _iml_path: A string of the module's iml absolute path.
        _facet: A string of the facet setting.
        _excludes: A string of the exclude relative paths.
        _srcs: A string of the source urls.
        _jars: A list of the jar urls.
        _srcjars: A list of srcjar urls.
        _deps: A list of the dependency module urls.
    """
    # b/121256503: Prevent duplicated iml names from breaking IDEA.
    # Use a map to cache in-using(already used) iml project file names.
    USED_NAME_CACHE = dict()

    def __init__(self, mod_info):
        """Initializes IMLGenerator.

        Args:
            mod_info: A dictionary of the module's data from module-info.json.
        """
        self._mod_info = mod_info
        self._android_root = common_util.get_android_root_dir()
        self._mod_path = os.path.join(self._android_root,
                                      mod_info[constant.KEY_PATH][0])
        self._iml_path = os.path.join(self._mod_path,
                                      mod_info[constant.KEY_IML_NAME] + '.iml')
        self._facet = ''
        self._excludes = ''
        self._srcs = ''
        self._jars = []
        self._srcjars = []
        self._deps = []

    @classmethod
    def get_unique_iml_name(cls, abs_module_path):
        """Create a unique iml name if needed.

        If the name of last sub folder is used already, prefixing it with prior
        sub folder names as a candidate name. If finally, it's unique, storing
        in USED_NAME_CACHE as: { abs_module_path:unique_name }. The cts case
        and UX of IDE view are the main reasons why using module path strategy
        but not name of module directly. Following is the detailed strategy:
        1. While loop composes a sensible and shorter name, by checking unique
           to finish the loop and finally add to cache.
           Take ['cts', 'tests', 'app', 'ui'] an example, if 'ui' isn't
           occupied, use it, else try 'cts_ui', then 'cts_app_ui', the worst
           case is whole three candidate names are occupied already.
        2. 'Else' for that while stands for no suitable name generated, so
           trying 'cts_tests_app_ui' directly. If it's still non unique, e.g.,
           module path cts/xxx/tests/app/ui occupied that name already,
           appending increasing sequence number to get a unique name.

        Args:
            abs_module_path: The absolute module path string.

        Return:
            String: A unique iml name.
        """
        if abs_module_path in cls.USED_NAME_CACHE:
            return cls.USED_NAME_CACHE[abs_module_path]

        uniq_name = abs_module_path.strip(os.sep).split(os.sep)[-1]
        if any(uniq_name == name for name in cls.USED_NAME_CACHE.values()):
            parent_path = os.path.relpath(abs_module_path,
                                          common_util.get_android_root_dir())
            sub_folders = parent_path.split(os.sep)
            zero_base_index = len(sub_folders) - 1
            # Start compose a sensible, shorter and unique name.
            while zero_base_index > 0:
                uniq_name = '_'.join(
                    [sub_folders[0], '_'.join(sub_folders[zero_base_index:])])
                zero_base_index = zero_base_index - 1
                if uniq_name not in cls.USED_NAME_CACHE.values():
                    break
            else:
                # TODO(b/133393638): To handle several corner cases.
                uniq_name_base = parent_path.strip(os.sep).replace(os.sep, '_')
                i = 0
                uniq_name = uniq_name_base
                while uniq_name in cls.USED_NAME_CACHE.values():
                    i = i + 1
                    uniq_name = '_'.join([uniq_name_base, str(i)])
        cls.USED_NAME_CACHE[abs_module_path] = uniq_name
        logging.debug('Unique name for module path of %s is %s.',
                      abs_module_path, uniq_name)
        return uniq_name

    @property
    def iml_path(self):
        """Gets the iml path."""
        return self._iml_path

    def create(self, content_type):
        """Creates the iml file.

        Create the iml file with specific part of sources.
        e.g.
        {
            'srcs': True,
            'dependencies': True,
        }

        Args:
            content_type: A dict to set which part of sources will be created.
        """
        if content_type.get(constant.KEY_SRCS, None):
            self._generate_srcs()
        if content_type.get(constant.KEY_DEP_SRCS, None):
            self._generate_dep_srcs()
        if content_type.get(constant.KEY_JARS, None):
            self._generate_jars()
        if content_type.get(constant.KEY_SRCJARS, None):
            self._generate_srcjars()
        if content_type.get(constant.KEY_DEPENDENCIES, None):
            self._generate_dependencies()

        if self._srcs or self._jars or self._srcjars or self._deps:
            self._create_iml()

    def _generate_facet(self):
        """Generates the facet when the AndroidManifest.xml exists."""
        if os.path.exists(os.path.join(self._mod_path,
                                       constant.ANDROID_MANIFEST)):
            self._facet = templates.FACET

    def _generate_srcs(self):
        """Generates the source urls of the project's iml file."""
        srcs = []
        for src in self._mod_info[constant.KEY_SRCS]:
            srcs.append(templates.SOURCE.format(
                SRC=os.path.join(self._android_root, src),
                IS_TEST='false'))
        for test in self._mod_info[constant.KEY_TESTS]:
            srcs.append(templates.SOURCE.format(
                SRC=os.path.join(self._android_root, test),
                IS_TEST='true'))
        self._excludes = self._mod_info.get(constant.KEY_EXCLUDES, '')
        self._srcs = templates.CONTENT.format(MODULE_PATH=self._mod_path,
                                              EXCLUDES=self._excludes,
                                              SOURCES=''.join(sorted(srcs)))

    def _generate_dep_srcs(self):
        """Generates the source urls of the dependencies.iml."""
        srcs = []
        for src in self._mod_info[constant.KEY_SRCS]:
            srcs.append(templates.OTHER_SOURCE.format(
                SRC=os.path.join(self._android_root, src),
                IS_TEST='false'))
        for test in self._mod_info[constant.KEY_TESTS]:
            srcs.append(templates.OTHER_SOURCE.format(
                SRC=os.path.join(self._android_root, test),
                IS_TEST='true'))
        self._srcs = ''.join(sorted(srcs))

    def _generate_jars(self):
        """Generates the jar urls."""
        for jar in self._mod_info[constant.KEY_JARS]:
            self._jars.append(templates.JAR.format(
                JAR=os.path.join(self._android_root, jar)))

    def _generate_srcjars(self):
        """Generates the srcjar urls."""
        for srcjar in self._mod_info[constant.KEY_SRCJARS]:
            self._srcjars.append(templates.SRCJAR.format(
                SRCJAR=os.path.join(self._android_root, srcjar)))

    def _generate_dependencies(self):
        """Generates the dependency module urls."""
        for dep in self._mod_info[constant.KEY_DEPENDENCIES]:
            self._deps.append(templates.DEPENDENCIES.format(MODULE=dep))

    def _create_iml(self):
        """Creates the iml file."""
        content = templates.IML.format(FACET=self._facet,
                                       SOURCES=self._srcs,
                                       JARS=''.join(self._jars),
                                       SRCJARS=''.join(self._srcjars),
                                       DEPENDENCIES=''.join(self._deps))
        common_util.file_generate(self._iml_path, content)
