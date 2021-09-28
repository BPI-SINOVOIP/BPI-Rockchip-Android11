#!/usr/bin/env python
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
"""Downloads adb prebuilt from the build server."""
import os
import sys

THIS_DIR = os.path.realpath(os.path.dirname(__file__))

sys.path.append(THIS_DIR + '/common/python')

import update_prebuilts as update

adb_install_list = [
    update.InstallEntry('sdk_arm64-sdk', 'adb', 'adb', need_exec=True),
]

adb_extracted_list = [
]

if __name__ == '__main__':
    update.main(THIS_DIR, 'adb', adb_install_list, adb_extracted_list)
