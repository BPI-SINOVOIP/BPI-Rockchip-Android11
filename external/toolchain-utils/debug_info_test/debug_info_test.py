#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test for debug info."""

from __future__ import print_function

import os
import subprocess
import sys

import check_icf
import check_cus
import check_exist

elf_checks = [
    check_exist.check_exist_all, check_cus.check_compile_units,
    check_icf.check_identical_code_folding
]


def scanelf(root):
  """Find ELFs in root.

  Args:
    root: root dir to start with the search.

  Returns:
    Filenames of ELFs in root.
  """
  p = subprocess.Popen(['scanelf', '-y', '-B', '-F', '%F', '-R', root],
                       stdout=subprocess.PIPE,
                       encoding='utf-8')
  return [l.strip() for l in p.stdout]


def Main(argv):
  if len(argv) < 2:
    print('usage: %s [file|dir]')
    return 1

  files = []
  cand = argv[1]
  if os.path.isfile(cand):
    files = [cand]
  elif os.path.isdir(cand):
    files = scanelf(cand)
  else:
    print('usage: %s [file|dir]')
    return 1

  failed = False
  for f in files:
    for c in elf_checks:
      if not c(f):
        failed = True

  if failed:
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
