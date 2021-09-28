#!/usr/bin/env python
#
# Copyright (C) 2020 The Android Open Source Project
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

"""
Extracts compat_config.xml from built jar files and merges them into a single
XML file.
"""

import argparse
import collections
import sys
import xml.etree.ElementTree as ET
from zipfile import ZipFile

XmlContent = collections.namedtuple('XmlContent', ['xml', 'source'])

def extract_compat_config(jarfile):
    """
    Reads all compat_config.xml files from a jarfile.

    Yields: XmlContent for each XML file found.
    """
    with ZipFile(jarfile, 'r') as jar:
        for info in jar.infolist():
            if info.filename.endswith("_compat_config.xml"):
                with jar.open(info.filename, 'r') as xml:
                    yield XmlContent(xml, info.filename)

def change_element_tostring(element):
    s = "%s(%s)" % (element.attrib['name'], element.attrib['id'])
    metadata = element.find('meta-data')
    if metadata is not None:
        s += " defined in class %s at %s" % (metadata.attrib['definedIn'], metadata.attrib['sourcePosition'])
    return s

class ChangeDefinition(collections.namedtuple('ChangeDefinition', ['source', 'element'])):
    def __str__(self):
        return "  From: %s:\n    %s" % (self.source, change_element_tostring(self.element))

class ConfigMerger(object):

    def __init__(self, detect_conflicts):
        self.tree = ET.ElementTree()
        self.tree._setroot(ET.Element("config"))
        self.detect_conflicts = detect_conflicts
        self.changes_by_id = dict()
        self.changes_by_name = dict()
        self.errors = 0
        self.write_errors_to = sys.stderr

    def merge(self, xmlFile, source):
        xml = ET.parse(xmlFile)
        for child in xml.getroot():
            if self.detect_conflicts:
                id = child.attrib['id']
                name = child.attrib['name']
                this_change = ChangeDefinition(source, child)
                if id in self.changes_by_id.keys():
                    duplicate = self.changes_by_id[id]
                    self.write_errors_to.write(
                        "ERROR: Duplicate definitions for compat change with ID %s:\n%s\n%s\n" % (
                        id, duplicate, this_change))
                    self.errors += 1
                if name in self.changes_by_name.keys():
                    duplicate = self.changes_by_name[name]
                    self.write_errors_to.write(
                        "ERROR: Duplicate definitions for compat change with name %s:\n%s\n%s\n" % (
                        name, duplicate, this_change))
                    self.errors += 1

                self.changes_by_id[id] = this_change
                self.changes_by_name[name] = this_change
            self.tree.getroot().append(child)

    def _check_error(self):
        if self.errors > 0:
            raise Exception("Failed due to %d earlier errors" % self.errors)

    def write(self, filename):
        self._check_error()
        self.tree.write(filename, encoding='utf-8', xml_declaration=True)

    def write_device_config(self, filename):
        self._check_error()
        self.strip_config_for_device().write(filename, encoding='utf-8', xml_declaration=True)

    def strip_config_for_device(self):
        new_tree = ET.ElementTree()
        new_tree._setroot(ET.Element("config"))
        for change in self.tree.getroot():
            new_change = ET.Element("compat-change")
            new_change.attrib = change.attrib.copy()
            new_tree.getroot().append(new_change)
        return new_tree

def main(argv):
    parser = argparse.ArgumentParser(
        description="Processes compat config XML files")
    parser.add_argument("--jar", type=argparse.FileType('rb'), action='append',
        help="Specifies a jar file to extract compat_config.xml from.")
    parser.add_argument("--xml", type=argparse.FileType('rb'), action='append',
        help="Specifies an xml file to read compat_config from.")
    parser.add_argument("--device-config", dest="device_config", type=argparse.FileType('wb'),
        help="Specify where to write config for embedding on the device to. "
        "Meta data not needed on the devivce is stripped from this.")
    parser.add_argument("--merged-config", dest="merged_config", type=argparse.FileType('wb'),
        help="Specify where to write merged config to. This will include metadata.")
    parser.add_argument("--allow-duplicates", dest="allow_duplicates", action='store_true',
        help="Allow duplicate changed IDs in the merged config.")

    args = parser.parse_args()

    config = ConfigMerger(detect_conflicts = not args.allow_duplicates)
    if args.jar:
        for jar in args.jar:
            for xml_content in extract_compat_config(jar):
                config.merge(xml_content.xml, "%s:%s" % (jar.name, xml_content.source))
    if args.xml:
        for xml in args.xml:
            config.merge(xml, xml.name)

    if args.device_config:
        config.write_device_config(args.device_config)

    if args.merged_config:
        config.write(args.merged_config)



if __name__ == "__main__":
    main(sys.argv)
