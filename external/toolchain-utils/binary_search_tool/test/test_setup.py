#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Emulate running of test setup script, is_good.py should fail without this."""

from __future__ import print_function

import sys


def Main():
  # create ./is_setup
  with open('./is_setup', 'w', encoding='utf-8'):
    pass

  return 0


if __name__ == '__main__':
  retval = Main()
  sys.exit(retval)
