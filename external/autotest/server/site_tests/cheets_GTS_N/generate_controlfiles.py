#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a trampoline script to invoke the actual generator script.

import os
import sys

target_script_name = 'generate_controlfiles_GTS_N.py'
target_script_path = os.path.abspath(os.path.join(os.path.dirname(__file__),
    '..', '..', 'cros', 'tradefed', target_script_name))
os.execv(target_script_path, sys.argv)
