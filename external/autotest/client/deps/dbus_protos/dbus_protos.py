#!/usr/bin/python2

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compile D-Bus protobuf Python files.

The protos are used by a bunch of cryptohome autotests.
"""

import os
import pipes
from autotest_lib.client.common_lib import utils

version = 1

# These proto definitions are installed by system_api.
PROTO_DEFS = {
    'cryptohome': ['key.proto', 'rpc.proto'],
}


def setup(top_dir):
    sysroot = os.environ['SYSROOT']
    parent_path = os.path.join(sysroot, 'usr/include/chromeos/dbus/')
    for subdir, protos in PROTO_DEFS.items():
        proto_path = os.path.join(parent_path, subdir)
        cmd = (['protoc', '--proto_path=' + proto_path,
                '--python_out=' + top_dir] +
               [os.path.join(proto_path, proto_def) for proto_def in protos])
    utils.run(' '.join(pipes.quote(arg) for arg in cmd))
    return


pwd = os.getcwd()
utils.update_version(os.path.join(pwd, 'src'), False, version, setup, pwd)
