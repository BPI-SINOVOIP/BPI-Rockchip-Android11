#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to translate the xbuddy config."""

from __future__ import print_function

import os
import sys

if '/mnt/host/source/src/third_party/toolchain-utils/crosperf' in sys.path:
  dev_path = os.path.expanduser('~/trunk/chromite/lib/xbuddy')
  sys.path.append(dev_path)
else:
  print('This script can only be run from inside a ChromeOS chroot.  Please '
        'enter your chroot, go to ~/src/third_party/toolchain-utils/crosperf'
        ' and try again.')
  sys.exit(0)

# pylint: disable=import-error,wrong-import-position
import xbuddy


def Main(xbuddy_string):
  if not os.path.exists('./xbuddy_config.ini'):
    config_path = os.path.expanduser(
        '~/trunk/chromite/lib/xbuddy/xbuddy_config.ini')
    os.symlink(config_path, './xbuddy_config.ini')
  x = xbuddy.XBuddy(manage_builds=False, static_dir='/tmp/devserver/static')
  build_id = x.Translate(os.path.split(xbuddy_string))
  return build_id


if __name__ == '__main__':
  print(Main(sys.argv[1]))
  sys.exit(0)
