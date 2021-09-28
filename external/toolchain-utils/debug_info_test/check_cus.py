# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""check compile units."""

from __future__ import print_function

import os
import subprocess

import check_ngcc

cu_checks = [check_ngcc.not_by_gcc]


def check_compile_unit(dso_path, producer, comp_path):
  """check all compiler flags used to build the compile unit.

  Args:
    dso_path: path to the elf/dso.
    producer: DW_AT_producer contains the compiler command line.
    comp_path: DW_AT_comp_dir + DW_AT_name.

  Returns:
    A set of failed tests.
  """
  failed = set()
  for c in cu_checks:
    if not c(dso_path, producer, comp_path):
      failed.add(c.__module__)

  return failed


def check_compile_units(dso_path):
  """check all compile units in the given dso.

  Args:
    dso_path: path to the dso.

  Returns:
    True if everything looks fine otherwise False.
  """

  failed = set()
  producer = ''
  comp_path = ''

  readelf = subprocess.Popen(
      ['readelf', '--debug-dump=info', '--dwarf-depth=1', dso_path],
      stdout=subprocess.PIPE,
      stderr=open(os.devnull, 'w'),
      encoding='utf-8')
  for l in readelf.stdout:
    if 'DW_TAG_compile_unit' in l:
      if producer:
        failed = failed.union(check_compile_unit(dso_path, producer, comp_path))
      producer = ''
      comp_path = ''
    elif 'DW_AT_producer' in l:
      producer = l
    elif 'DW_AT_name' in l:
      comp_path = os.path.join(comp_path, l.split(':')[-1].strip())
    elif 'DW_AT_comp_dir' in l:
      comp_path = os.path.join(l.split(':')[-1].strip(), comp_path)
  if producer:
    failed = failed.union(check_compile_unit(dso_path, producer, comp_path))

  if failed:
    print('%s failed check: %s' % (dso_path, ' '.join(failed)))
    return False

  return True
