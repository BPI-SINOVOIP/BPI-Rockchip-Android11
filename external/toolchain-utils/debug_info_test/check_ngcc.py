# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check whether the compile unit is not built by gcc."""

from __future__ import print_function

from whitelist import is_whitelisted


def not_by_gcc(dso_path, producer, comp_path):
  """Check whether the compile unit is not built by gcc.

  Args:
    dso_path: path to the elf/dso.
    producer: DW_AT_producer contains the compiler command line.
    comp_path: DW_AT_comp_dir + DW_AT_name.

  Returns:
    False if compiled by gcc otherwise True.
  """
  if is_whitelisted('ngcc_comp_path', comp_path):
    return True

  if is_whitelisted('ngcc_dso_path', dso_path):
    return True

  return 'GNU C' not in producer
