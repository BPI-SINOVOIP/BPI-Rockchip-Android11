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

"""Unittests for xml_util."""

import unittest
from unittest import mock
from xml.etree import ElementTree

from aidegen.lib import aidegen_metrics
from aidegen.lib import common_util
from aidegen.lib import xml_util


# pylint: disable=protected-access
class XMLUtilUnittests(unittest.TestCase):
    """Unit tests for xml_util.py"""

    @mock.patch.object(common_util, 'read_file_content')
    @mock.patch.object(aidegen_metrics, 'send_exception_metrics')
    @mock.patch.object(ElementTree, 'parse')
    def test_parse_xml(self, mock_parse, mock_metrics, mock_read):
        """Test _parse_xml."""
        mock_parse.return_value = ElementTree.ElementTree()
        xml_util.parse_xml('a')
        self.assertFalse(mock_metrics.called)
        mock_parse.side_effect = ElementTree.ParseError()
        mock_read.return_value = ''
        xml_util.parse_xml('a')
        self.assertTrue(mock_metrics.called)


if __name__ == '__main__':
    unittest.main()
