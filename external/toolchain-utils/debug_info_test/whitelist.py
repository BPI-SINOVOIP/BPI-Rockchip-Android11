# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Whitelist functions."""

from __future__ import print_function

import os
import glob
import re


# Matching a string of length m in an NFA of size n is O(mn^2), but the
# performance also depends largely on the implementation. It appears to be fast
# enough according to the tests.
#
# The performance bottleneck of this script is readelf. Unless this becomes
# slower than readelf, don't waste time here.
def is_whitelisted(list_name, pattern):
  """Check whether the given pattern is specified in the whitelist.

  Args:
    list_name: name of the whitelist.
    pattern: the target string.

  Returns:
    True if matched otherwise False.
  """
  return pattern and whitelists[list_name].match(pattern)


def prepare_whitelist(patterns):
  """Join and compile the re patterns.

  Args:
    patterns: regex patterns.

  Returns:
    A compiled re object.
  """
  return re.compile('|'.join(patterns))


def load_whitelists(dirname):
  """Load whitelists under dirname.

  A whitelist ends with .whitelist.

  Args:
    dirname: path to the dir.

  Returns:
    A dictionary of 'filename' -> whitelist matcher.
  """
  wlist = {}
  for fn in glob.glob(os.path.join(dirname, '*.whitelist')):
    key = os.path.splitext(os.path.basename(fn))[0]
    with open(fn, 'r', encoding='utf-8') as f:
      patterns = f.read().splitlines()
      patterns = [l for l in patterns if l != '']
      patterns = [l for l in patterns if l[0] != '#']
    wlist[key] = prepare_whitelist(patterns)
  return wlist


whitelists = load_whitelists(os.path.dirname(__file__))
