#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for remove_indirect_calls"""

from __future__ import print_function

import io
import unittest

from afdo_redaction import remove_indirect_calls


def _run_test(input_lines):
  input_buf = io.StringIO('\n'.join(input_lines))
  output_buf = io.StringIO()
  remove_indirect_calls.run(input_buf, output_buf)
  return output_buf.getvalue().splitlines()


class Test(unittest.TestCase):
  """Tests"""

  def test_empty_profile(self):
    self.assertEqual(_run_test([]), [])

  def test_removal_on_real_world_code(self):
    # These are copied from an actual textual AFDO profile, but the names made
    # lints unhappy due to their length, so I had to be creative.
    profile_lines = """_ZLongSymbolName:52862:1766
 14: 2483
 8.1: _SomeInlinedSym:45413
  11: _AndAnother:35481
   2: 2483
   2.1: _YetAnother:25549
    3: 2483
    3.1: 351
    3.3: 2526 IndirectTarg1:675 Targ2:397 Targ3:77
  13.2: Whee:9932
   1.1: Whoo:9932
    0: BleepBloop:9932
     0: 2483
  """.strip().splitlines()

    expected_lines = """_ZLongSymbolName:52862:1766
 14: 2483
 8.1: _SomeInlinedSym:45413
  11: _AndAnother:35481
   2: 2483
   2.1: _YetAnother:25549
    3: 2483
    3.1: 351
    3.3: 2526
  13.2: Whee:9932
   1.1: Whoo:9932
    0: BleepBloop:9932
     0: 2483
  """.strip().splitlines()

    self.assertEqual(_run_test(profile_lines), expected_lines)


if __name__ == '__main__':
  unittest.main()
