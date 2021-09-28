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

"""Unittests for XMLGenerator."""

import os
import shutil
import tempfile
import unittest
from unittest import mock

from xml.etree import ElementTree

from aidegen.lib import common_util
from aidegen.idea import xml_gen


# pylint: disable=protected-access
class XMLGenUnittests(unittest.TestCase):
    """Unit tests for XMLGenerator class."""

    _TEST_DIR = None
    _XML_NAME = 'test.xml'
    _DEFAULT_XML = """<?xml version="1.0" encoding="UTF-8"?>
<project version="4"></project>
"""
    _IGNORE_GIT_XML = """<?xml version="1.0" encoding="UTF-8"?>
<project version="4">
  <component name="VcsManagerConfiguration">
    <ignored-roots><path value="/b" /></ignored-roots>
  </component>
</project>
"""

    def setUp(self):
        """Prepare the testdata related path."""
        XMLGenUnittests._TEST_DIR = tempfile.mkdtemp()
        self.xml = xml_gen.XMLGenerator(self._TEST_DIR, self._XML_NAME)
        common_util.file_generate(self.xml.xml_path, self._DEFAULT_XML)
        self.xml.parse()

    def tearDown(self):
        """Clear the testdata related path."""
        shutil.rmtree(self._TEST_DIR)

    def test_find_elements_by_name(self):
        """Test find_elements_by_name."""
        node = self.xml.xml_obj.getroot()
        ElementTree.SubElement(node, 'a', attrib={'name': 'b'})
        elements = self.xml.find_elements_by_name('a', 'b')
        self.assertEqual(len(elements), 1)

    def test_append_node(self):
        """Test append_node."""
        node = self.xml.xml_obj.getroot()
        self.xml.append_node(node, '<a />')
        self.assertEqual(len(node.findall('a')), 1)

    @mock.patch.object(common_util, 'to_pretty_xml')
    @mock.patch.object(common_util, 'file_generate')
    def test_create_xml(self, mock_file_gen, mock_pretty_xml):
        """Test create_xml."""
        self.xml.create_xml()
        self.assertTrue(mock_file_gen.called)
        self.assertTrue(mock_pretty_xml.called)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(common_util, 'get_android_root_dir')
    @mock.patch.object(xml_gen, 'XMLGenerator')
    def test_gen_vcs_xml(self, mock_xml_gen, mock_root_dir, mock_file_gen):
        """Test gen_vcs_xml."""
        mock_gen_xml = mock.Mock()
        mock_xml_gen.return_value = mock_gen_xml
        mock_xml_gen.xml_obj = None
        mock_root_dir.return_value = self._TEST_DIR
        xml_gen.gen_vcs_xml(self._TEST_DIR, [])
        self.assertFalse(mock_file_gen.called)
        mock_root_dir.return_value = '/a'
        xml_gen.gen_vcs_xml(self._TEST_DIR, ['/a'])
        self.assertTrue(mock_file_gen.called)

    @mock.patch.object(os.path, 'exists')
    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(xml_gen.XMLGenerator, 'create_xml')
    @mock.patch.object(xml_gen, 'XMLGenerator')
    def test_write_ignore_git_dirs_file(self, mock_xml_gen, mock_create_xml,
                                        mock_file_gen, mock_exists):
        """Test write_ignore_git_dirs_file."""
        mock_gen_xml = mock.Mock()
        mock_xml_gen.return_value = mock_gen_xml
        mock_gen_xml.xml_obj = False
        mock_exists.return_value = False
        xml_gen.write_ignore_git_dirs_file(self._TEST_DIR, ['/a'])
        self.assertTrue(mock_file_gen.called)
        mock_exists.return_value = True
        mock_xml_gen.return_value = self.xml
        xml_gen.write_ignore_git_dirs_file(self._TEST_DIR, ['/a'])
        ignore_root = self.xml.xml_obj.find('component').find('ignored-roots')
        self.assertEqual(ignore_root.find('path').attrib['value'], '/a')
        common_util.file_generate(os.path.join(self._TEST_DIR, 'workspace.xml'),
                                  self._IGNORE_GIT_XML)
        self.xml = xml_gen.XMLGenerator(self._TEST_DIR, 'workspace.xml')
        mock_xml_gen.return_value = self.xml
        xml_gen.write_ignore_git_dirs_file(self._TEST_DIR, ['/a/b'])
        ignore_root = self.xml.xml_obj.find('component').find('ignored-roots')
        self.assertEqual(ignore_root.find('path').attrib['value'], '/a/b')
        self.assertTrue(mock_create_xml.called)


if __name__ == '__main__':
    unittest.main()
