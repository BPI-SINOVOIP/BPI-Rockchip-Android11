#!/usr/bin/env python3
#
# Copyright 2020, The Android Open Source Project
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

"""Unittests for IML class."""

import os
import shutil
import tempfile
import unittest
from unittest import mock

from aidegen import templates
from aidegen.lib import common_util
from aidegen.idea import iml
from atest import module_info


# pylint: disable=protected-access
class IMLGenUnittests(unittest.TestCase):
    """Unit tests for IMLGenerator class."""

    _TEST_DIR = None

    def setUp(self):
        """Prepare the testdata related path."""
        IMLGenUnittests._TEST_DIR = tempfile.mkdtemp()
        module = {
            'module_name': 'test',
            'iml_name': 'test_iml',
            'path': ['a/b'],
            'srcs': ['a/b/src'],
            'tests': ['a/b/tests'],
            'srcjars': ['x/y.srcjar'],
            'jars': ['s.jar'],
            'dependencies': ['m1']
        }
        with mock.patch.object(common_util, 'get_android_root_dir') as obj:
            obj.return_value = IMLGenUnittests._TEST_DIR
            self.iml = iml.IMLGenerator(module)

    def tearDown(self):
        """Clear the testdata related path."""
        self.iml = None
        shutil.rmtree(IMLGenUnittests._TEST_DIR)

    def test_init(self):
        """Test initialize the attributes."""
        self.assertEqual(self.iml._mod_info['module_name'], 'test')

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_iml_path(self, mock_root_path):
        """Test iml_path."""
        mock_root_path.return_value = IMLGenUnittests._TEST_DIR
        iml_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b/test_iml.iml')
        self.assertEqual(self.iml.iml_path, iml_path)

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_create(self, mock_root_path):
        """Test create."""
        mock_root_path.return_value = IMLGenUnittests._TEST_DIR
        module_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b')
        src_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b/src')
        test_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b/tests')
        srcjar_path = os.path.join(IMLGenUnittests._TEST_DIR, 'x/y.srcjar')
        jar_path = os.path.join(IMLGenUnittests._TEST_DIR, 's.jar')
        expected = """<?xml version="1.0" encoding="UTF-8"?>
<module type="JAVA_MODULE" version="4">
    <component name="NewModuleRootManager" inherit-compiler-output="true">
        <exclude-output />
        <content url="file://{MODULE_PATH}">
            <sourceFolder url="file://{SRC_PATH}" isTestSource="false" />
            <sourceFolder url="file://{TEST_PATH}" isTestSource="true" />
        </content>
        <orderEntry type="sourceFolder" forTests="false" />
        <content url="jar://{SRCJAR}!/">
            <sourceFolder url="jar://{SRCJAR}!/" isTestSource="False" />
        </content>
        <orderEntry type="module" module-name="m1" />
        <orderEntry type="module-library" exported="">
          <library>
            <CLASSES>
              <root url="jar://{JAR}!/" />
            </CLASSES>
            <JAVADOC />
            <SOURCES />
          </library>
        </orderEntry>
        <orderEntry type="inheritedJdk" />
    </component>
</module>
""".format(MODULE_PATH=module_path,
           SRC_PATH=src_path,
           TEST_PATH=test_path,
           SRCJAR=srcjar_path,
           JAR=jar_path)
        self.iml.create({'srcs': True, 'srcjars': True, 'dependencies': True,
                         'jars': True})
        gen_iml = os.path.join(IMLGenUnittests._TEST_DIR,
                               self.iml._mod_info['path'][0],
                               self.iml._mod_info['iml_name'] + '.iml')
        result = common_util.read_file_content(gen_iml)
        self.assertEqual(result, expected)

    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_gen_dep_sources(self, mock_root_path):
        """Test _generate_dep_srcs."""
        mock_root_path.return_value = IMLGenUnittests._TEST_DIR
        src_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b/src')
        test_path = os.path.join(IMLGenUnittests._TEST_DIR, 'a/b/tests')
        expected = """<?xml version="1.0" encoding="UTF-8"?>
<module type="JAVA_MODULE" version="4">
    <component name="NewModuleRootManager" inherit-compiler-output="true">
        <exclude-output />
        <content url="file://{SRC_PATH}">
            <sourceFolder url="file://{SRC_PATH}" isTestSource="false" />
        </content>
        <content url="file://{TEST_PATH}">
            <sourceFolder url="file://{TEST_PATH}" isTestSource="true" />
        </content>
        <orderEntry type="sourceFolder" forTests="false" />
        <orderEntry type="inheritedJdk" />
    </component>
</module>
""".format(SRC_PATH=src_path,
           TEST_PATH=test_path)
        self.iml.create({'dep_srcs': True})
        gen_iml = os.path.join(IMLGenUnittests._TEST_DIR,
                               self.iml._mod_info['path'][0],
                               self.iml._mod_info['iml_name'] + '.iml')
        result = common_util.read_file_content(gen_iml)
        self.assertEqual(result, expected)

    @mock.patch.object(iml.IMLGenerator, '_create_iml')
    @mock.patch.object(iml.IMLGenerator, '_generate_dependencies')
    @mock.patch.object(iml.IMLGenerator, '_generate_srcjars')
    def test_skip_create_iml(self, mock_gen_srcjars, mock_gen_dep,
                             mock_create_iml):
        """Test skipping create_iml."""
        self.iml.create({'srcjars': False, 'dependencies': False})
        self.assertFalse(mock_gen_srcjars.called)
        self.assertFalse(mock_gen_dep.called)
        self.assertFalse(mock_create_iml.called)

    @mock.patch('os.path.exists')
    def test_generate_facet(self, mock_exists):
        """Test _generate_facet."""
        mock_exists.return_value = False
        self.iml._generate_facet()
        self.assertEqual(self.iml._facet, '')
        mock_exists.return_value = True
        self.iml._generate_facet()
        self.assertEqual(self.iml._facet, templates.FACET)

    def test_get_uniq_iml_name(self):
        """Test the unique name cache mechanism.

        By using the path data in module info json as input, if the count of
        name data set is the same as sub folder path count, then it means
        there's no duplicated name, the test PASS.
        """
        # Add following test path
        test_paths = {
            'cts/tests/tests/app',
            'cts/tests/app',
            'cts/tests/app/app1/../app',
            'cts/tests/app/app2/../app',
            'cts/tests/app/app3/../app',
            'frameworks/base/tests/xxxxxxxxxxxx/base',
            'frameworks/base',
            'external/xxxxx-xxx/robolectric',
            'external/robolectric',
        }
        mod_info = module_info.ModuleInfo()
        test_paths.update(mod_info._get_path_to_module_info(
            mod_info.name_to_module_info).keys())
        print('\n{} {}.'.format('Test_paths length:', len(test_paths)))

        path_list = []
        for path in test_paths:
            path_list.append(path)
        print('{} {}.'.format('path list with length:', len(path_list)))

        names = [iml.IMLGenerator.get_unique_iml_name(f)
                 for f in path_list]
        print('{} {}.'.format('Names list with length:', len(names)))
        self.assertEqual(len(names), len(path_list))
        dic = {}
        for i, path in enumerate(path_list):
            dic[names[i]] = path
        print('{} {}.'.format('The size of name set is:', len(dic)))
        self.assertEqual(len(dic), len(path_list))


if __name__ == '__main__':
    unittest.main()
