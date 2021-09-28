#!/usr/bin/python

#
# Copyright (C) 2019 The Android Open Source Project
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

"""
Parses a given bugreport for specific device Id.
If parsing is successful, then generate respective
JSON configuration.

"""
import json
import os
import re
import sys
import zipfile

"""
Store static camera characteristics in given
JSON configuration file.
"""
def storeJsonConfigration(filePath, chars):
    with open(filePath, 'w') as jsonFile:
        json.dump(chars, jsonFile, sort_keys=True, indent=1)

"""
Parse media.camera dump section and populate the camera
characteristics list of dictionaries.
"""
def parseCameraDump(deviceId, cameraDumpPath, tagList):
    deviceRegExp = "== Camera HAL device device@[0-9]+\.[0-9]+/{0} \(v3.".format(deviceId)
    physicalDeviceRegExp = "Physical camera [0-9] characteristics:"
    tagRegExp = " {4}android[a-zA-Z0-9\.]+ \([a-z0-9]+\): "
    tagValueRegExp = "[^a-zA-Z0-9-\._]"
    with open(cameraDumpPath, "r") as file:
        devices = re.split(deviceRegExp, file.read())
        if len(devices) != 3 and len(devices) != 2:
            print "Camera device id: {0} not found".format(deviceId)
            sys.exit()

        physicalDevices = re.split(physicalDeviceRegExp, devices[1])
        physicalIdx = 0
        for physicalDevice in physicalDevices:
            physicalIdx += 1
            tags = re.split(tagRegExp, physicalDevice)
            tagsContent = re.findall(tagRegExp, physicalDevice)
            i = 0;
            parseEnd = False
            deviceChars = dict()
            for tag in tags[1:]:
                if parseEnd:
                    break

                lines = tag.splitlines()
                if len(lines) < 2:
                    print "Empty tag entry, skipping!"
                    continue
                tagName = tagsContent[i].split()[0]

                if tagName is None or len(tagName) < 1:
                    print "Invalid tag found, skipping!"
                    continue

                i += 1
                for line in lines[1:]:
                    if line.startswith('== Camera HAL device device'):
                        parseEnd = True
                        break

                    values = re.split(r' {8}', line)
                    if len(values) == 2:
                        key = tagName
                        tagValues = filter(None, re.split(tagValueRegExp, values[1]))
                        if deviceChars.has_key(key):
                            deviceChars[key] = deviceChars[key] + tagValues
                        else:
                            deviceChars[key] = tagValues
                    else:
                        break
            tagList.append(deviceChars)
    os.remove(cameraDumpPath)

if __name__ == '__main__':
    argc = len(sys.argv)
    deviceId = ""
    bugreportPath = ""
    configPath = ""
    if argc >= 4:
        bugreportPath = str(sys.argv[1])
        deviceId = str(sys.argv[2])
        configPath = str(sys.argv[3])
    else:
        print "Usage: parse_bugreport.py PathToBugreport DeviceId JSONConfigurationPath"
        sys.exit();

    with zipfile.ZipFile(bugreportPath) as bugzip:
        cameraDumpFile = ""
        for name in bugzip.namelist():
            if re.match("bugreport", name) is not None:
                cameraDumpFile = name
                break

        if len(cameraDumpFile) == 0:
            print("Camera dump not found in bugreport!")
            sys.exit()

        tagList = list()
        parseCameraDump(deviceId, bugzip.extract(cameraDumpFile), tagList)
        storeJsonConfigration(configPath, tagList)

