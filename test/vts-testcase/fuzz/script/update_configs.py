#
# Copyright (C) 2017 The Android Open Source Project
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
"""Updates .bp, .mk, .xml files under test/vts-testcase/fuzz.

Among files affected are:
Config Files:
1. files matching: test/vts-testcase/fuzz/<hal_name>/<hal_version>/func_fuzzer/Android.mk
2. files matching: test/vts-testcase/fuzz/<hal_name>/<hal_version>/func_fuzzer/AndroidTest.xml
3. files matching: test/vts-testcase/fuzz/<hal_name>/<hal_version>/iface_fuzzer/Android.mk
4. files matching: test/vts-testcase/fuzz/<hal_name>/<hal_version>/iface_fuzzer/AndroidTest.xml


Usage:
    python test/vts-testcase/fuzz/script/update_configs.py
"""

import os
import sys

from config.config_gen import ConfigGen

ANDROID_BUILD_TOP = os.environ.get('ANDROID_BUILD_TOP')
if not ANDROID_BUILD_TOP:
    print 'Run "lunch" command first.'
    sys.exit(1)

if __name__ == '__main__':
    print 'Updating config files.'
    HAL_SCRIPT_DIR = os.path.join(ANDROID_BUILD_TOP, 'test', 'vts-testcase',
                                  'hal', 'script')
    sys.path.append(HAL_SCRIPT_DIR)
    config_gen = ConfigGen()
    config_gen.UpdateFuzzerConfigs()
