#!/usr/bin/python2

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common, os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    # The copy the gtest files from the SYSROOT to the client
    for libdir in ('/usr/local/lib64', '/usr/lib64', '/usr/local/lib',
                   '/usr/lib'):
        libdir_path = os.path.join(os.environ['SYSROOT'], libdir)
        if os.path.exists(os.path.join(libdir_path, 'libgtest.so')):
            os.chdir(libdir_path)
            utils.run('cp libgtest* ' + top_dir)
            break

pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, pwd)
