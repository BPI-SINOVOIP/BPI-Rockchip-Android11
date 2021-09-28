#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command script without compiler support for pass level bisection.

This script generates a pseudo log which a workable compiler should print out.
It assumes that -opt-bisect-limit and -print-debug-counter are supported by the
compiler.
"""

from __future__ import print_function

import os
import sys

from binary_search_tool.test import common


def Main(argv):
  if not os.path.exists('./is_setup'):
    return 1

  if len(argv) != 3:
    return 1

  limit_flags = os.environ['LIMIT_FLAGS']
  opt_bisect_exist = False
  debug_counter_exist = False

  for option in limit_flags.split():
    if '-opt-bisect-limit' in option:
      opt_bisect_limit = int(option.split('=')[-1])
      opt_bisect_exist = True
    if '-debug-counter=' in option:
      debug_counter = int(option.split('=')[-1])
      debug_counter_exist = True

  if not opt_bisect_exist:
    return 1

  # Manually set total number and bad number
  total_pass = 10
  total_transform = 20
  bad_pass = int(argv[1])
  bad_transform = int(argv[2])

  if opt_bisect_limit == -1:
    opt_bisect_limit = total_pass

  for i in range(1, total_pass + 1):
    bisect_str = 'BISECT: %srunning pass (%d) Combine redundant ' \
                 'instructions on function (f1)' \
                 % ('NOT ' if i > opt_bisect_limit else '', i)
    print(bisect_str, file=sys.stderr)

  if debug_counter_exist:
    print('Counters and values:', file=sys.stderr)
    print(
        'instcombine-visit : {%d, 0, %d}' % (total_transform, debug_counter),
        file=sys.stderr)

  if opt_bisect_limit > bad_pass or \
    (debug_counter_exist and debug_counter > bad_transform):
    common.WriteWorkingSet([1])
  else:
    common.WriteWorkingSet([0])

  return 0


if __name__ == '__main__':
  retval = Main(sys.argv)
  sys.exit(retval)
