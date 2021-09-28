# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper module to determine whether a script is executed inside the chroot."""

from __future__ import print_function

import os


def VerifyOutsideChroot():
  """Checks whether the script invoked was executed in the chroot.

  Raises:
    AssertionError: The script was run inside the chroot.
  """

  chroot_only_path = '/mnt/host/depot_tools'

  in_chroot_err_message = 'Script should be run outside the chroot.'

  assert not os.path.isdir(chroot_only_path), in_chroot_err_message
