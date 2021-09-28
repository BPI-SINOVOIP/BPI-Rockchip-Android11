# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""check whether intended components exists in the given dso."""

from __future__ import print_function

import os
import subprocess

from whitelist import is_whitelisted


def check_debug_info(dso_path, readelf_content):
  """Check whether debug info section exists in the elf file.

  Args:
    dso_path: path to the dso.
    readelf_content: debug info dumped by command readelf.

  Returns:
    True if debug info section exists, otherwise False.
  """

  # Return True if it is whitelisted
  if is_whitelisted('exist_debug_info', dso_path):
    return True

  for l in readelf_content:
    if 'debug_info' in l:
      return True
  return False


def check_producer(dso_path, readelf_content):
  """Check whether DW_AT_producer exists in each compile unit.

  Args:
    dso_path: path to the dso.
    readelf_content: debug info dumped by command readelf.

  Returns:
    True if DW_AT_producer exists in each compile unit, otherwise False.
    Notice: If no compile unit in DSO, also return True.
  """

  # Return True if it is whitelisted
  if is_whitelisted('exist_producer', dso_path):
    return True

  # Indicate if there is a producer under each cu
  cur_producer = False

  first_cu = True
  producer_exist = True

  for l in readelf_content:
    if 'DW_TAG_compile_unit' in l:
      if not first_cu and not cur_producer:
        producer_exist = False
        break
      first_cu = False
      cur_producer = False
    elif 'DW_AT_producer' in l:
      cur_producer = True

  # Check whether last producer of compile unit exists in the elf,
  # also return True if no cu in the DSO.
  if not first_cu and not cur_producer:
    producer_exist = False

  return producer_exist


def check_exist_all(dso_path):
  """check whether intended components exists in the given dso.

  Args:
    dso_path: path to the dso.

  Returns:
    True if everything looks fine otherwise False.
  """

  readelf = subprocess.Popen(
      ['readelf', '--debug-dump=info', '--dwarf-depth=1', dso_path],
      stdout=subprocess.PIPE,
      stderr=open(os.devnull, 'w'),
      encoding='utf-8')
  readelf_content = list(readelf.stdout)

  exist_checks = [check_debug_info, check_producer]

  for e in exist_checks:
    if not e(dso_path, readelf_content):
      check_failed = e.__module__ + ': ' + e.__name__
      print('%s failed check: %s' % (dso_path, check_failed))
      return False

  return True
