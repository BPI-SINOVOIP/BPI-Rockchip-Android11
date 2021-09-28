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
#
import os
import sys
from distutils.util import strtobool

import ltp_test_cases
from common import filter_utils

def run(android_build_top, arch, n_bit, is_low_mem, is_hwasan, output_file):

    android_build_top = android_build_top
    ltp_tests = ltp_test_cases.LtpTestCases(
        android_build_top, None)

    test_filter = filter_utils.Filter()
    ltp_tests.GenConfig(
        arch,
        n_bit,
        test_filter,
        output_file=output_file,
        run_staging=False,
        is_low_mem=is_low_mem,
        is_hwasan=is_hwasan)

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("use: %s n_bit output_file" % sys.argv[0])
        sys.exit(1)
    arch = sys.argv[1]
    n_bit = sys.argv[2]
    is_low_mem = strtobool(sys.argv[3])
    is_hwasan = strtobool(sys.argv[4])
    output_path = sys.argv[5]
    run(os.environ['ANDROID_BUILD_TOP'], arch, n_bit, is_low_mem, is_hwasan, output_path)
