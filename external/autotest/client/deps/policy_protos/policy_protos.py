#!/usr/bin/python2

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compile policy protobuf Python files.

The policy protos are used by a bunch of policy_* and login_* autotests, as well
as policy_testserver.py in the Chromium code base. policy_descriptor.proto is
used by session_manager.py to store and retrieve policy from/to disk.
"""

import os
import pipes
from autotest_lib.client.bin import utils

version = 2

# cloud_policy.proto and chrome_extension_policy.proto are unreferenced here,
# but used by policy_testserver.py in the Chromium code base, so don't remote
# them!
PROTO_PATHS = [
        'usr/share/protofiles/chrome_device_policy.proto',
        'usr/share/protofiles/policy_common_definitions.proto',
        'usr/share/protofiles/device_management_backend.proto',
        'usr/share/protofiles/cloud_policy.proto',
        'usr/share/protofiles/chrome_extension_policy.proto',
        'usr/include/chromeos/dbus/login_manager/policy_descriptor.proto',
]

def setup(top_dir):
    sysroot = os.environ['SYSROOT']
    dirs = set(os.path.dirname(p) for p in PROTO_PATHS)
    include_args = ['--proto_path=' + os.path.join(sysroot, d) for d in dirs]
    proto_paths = [os.path.join(sysroot, p) for p in PROTO_PATHS]
    cmd = ['protoc', '--python_out=' + top_dir] + include_args + proto_paths
    utils.run(' '.join(pipes.quote(arg) for arg in cmd))
    return


pwd = os.getcwd()
utils.update_version(os.path.join(pwd, 'src'), False, version, setup, pwd)
