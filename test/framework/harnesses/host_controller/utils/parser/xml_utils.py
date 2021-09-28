#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging

from xml.etree import ElementTree


def GetAttributes(xml_file, tag, attrs):
    """Parses attributes in the given tag from xml file.

    Args:
        xml_file: The xml file object.
        tag: a string, tag to parse from tag xml_file.
        attrs: a list of attributes to look up inside the tag.

    Returns:
        A dict containing values of the attributes in tag from the xml file.
    """
    result = {}
    for _, elem in ElementTree.iterparse(xml_file, ("start", )):
        if elem.tag == tag:
            for attr in attrs:
                if attr in elem.attrib:
                    result[attr] = elem.attrib[attr]
                else:
                    logging.warning(
                        "Cannot find an attribute {} in <{}>".format(
                            attr, tag))
    return result
