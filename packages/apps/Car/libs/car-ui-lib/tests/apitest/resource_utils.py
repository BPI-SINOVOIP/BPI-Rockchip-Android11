#!/usr/bin/env python3
#
# Copyright 2019, The Android Open Source Project
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

import os
import re

class ResourceLocation:
    def __init__(self, file, line=None):
        self.file = file
        self.line = line

    def __str__(self):
        if self.line is not None:
            return self.file + ':' + str(self.line)
        else:
            return self.file

class Resource:
    def __init__(self, name, type, location=None):
        self.name = name
        self.type = type
        self.locations = []
        if location is not None:
            self.locations.append(location)

    def __eq__(self, other):
        if isinstance(other, _Grab):
            return other == self
        return self.name == other.name and self.type == other.type

    def __ne__(self, other):
        if isinstance(other, _Grab):
            return other != self
        return self.name != other.name or self.type != other.type

    def __hash__(self):
        return hash((self.name, self.type))

    def __str__(self):
        result = ''
        for location in self.locations:
            result += str(location) + ': '
        result += '<'+self.type+' name="'+self.name+'"'

        return result + '>'

    def __repr__(self):
        return str(self)

def get_all_resources(resDir):
    allResDirs = [f for f in os.listdir(resDir) if os.path.isdir(os.path.join(resDir, f))]
    valuesDirs = [f for f in allResDirs if f.startswith('values')]
    fileDirs = [f for f in allResDirs if not f.startswith('values')]

    resources = set()

    # Get the filenames of the all the files in all the fileDirs
    for dir in fileDirs:
        type = dir.split('-')[0]
        for file in os.listdir(os.path.join(resDir, dir)):
            if file.endswith('.xml'):
                add_resource_to_set(resources,
                                    Resource(file[:-4], type,
                                             ResourceLocation(os.path.join(resDir, dir, file))))
                if dir.startswith("layout"):
                    for resource in get_ids_from_layout_file(os.path.join(resDir, dir, file)):
                        add_resource_to_set(resources, resource)

    for dir in valuesDirs:
        for file in os.listdir(os.path.join(resDir, dir)):
            if file.endswith('.xml'):
                for resource in get_resources_from_single_file(os.path.join(resDir, dir, file)):
                    add_resource_to_set(resources, resource)

    return resources

def get_ids_from_layout_file(filename):
    result = set()
    with open(filename, 'r') as file:
        r = re.compile("@\+id/([a-zA-Z0-9_]+)")
        for i in r.findall(file.read()):
            add_resource_to_set(result, Resource(i, 'id', ResourceLocation(filename)))
    return result

def get_resources_from_single_file(filename):
    # defer importing lxml to here so that people who aren't editing chassis don't have to have
    # lxml installed
    import lxml.etree as etree
    doc = etree.parse(filename)
    resourceTag = doc.getroot()
    result = set()
    for resource in resourceTag:
        if resource.tag == 'declare-styleable' or resource.tag is etree.Comment:
            continue

        resName = resource.get('name')
        resType = resource.tag
        if resType == "string-array":
            resType = "array"
        if resource.tag == 'item' or resource.tag == 'public':
            resType = resource.get('type')

        if resType != 'overlayable':
            add_resource_to_set(result, Resource(resName, resType,
                                                 ResourceLocation(filename, resource.sourceline)))

    return result

# Used to get objects out of sets
class _Grab:
    def __init__(self, value):
        self.search_value = value
    def __hash__(self):
        return hash(self.search_value)
    def __eq__(self, other):
        if self.search_value == other:
            self.actual_value = other
            return True
        return False

def add_resource_to_set(resourceset, resource):
    grabber = _Grab(resource)
    if grabber in resourceset:
        grabber.actual_value.locations.extend(resource.locations)
    else:
        resourceset.update([resource])

def merge_resources(set1, set2):
    for resource in set2:
        add_resource_to_set(set1, resource)
