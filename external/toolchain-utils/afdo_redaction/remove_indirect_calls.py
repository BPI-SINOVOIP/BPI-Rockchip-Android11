#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to remove all indirect call targets from textual AFDO profiles.

Indirect call samples can cause code to appear 'live' when it otherwise
wouldn't be. This resurrection can happen either by the way of profile-based
speculative devirtualization, or because of imprecision in LLVM's liveness
calculations when performing LTO.

This generally isn't a problem when an AFDO profile is applied to the binary it
was collected on. However, because we e.g., build NaCl from the same set of
objects as Chrome, this can become problematic, and lead to NaCl doubling in
size (or worse). See crbug.com/1005023 and crbug.com/916130.
"""

from __future__ import division, print_function

import argparse
import re


def _remove_indirect_call_targets(lines):
  # Lines with indirect call targets look like:
  #   1.1: 1234 foo:111 bar:122
  #
  # Where 1.1 is the line info/discriminator, 1234 is the total number of
  # samples seen for that line/discriminator, foo:111 is "111 of the calls here
  # went to foo," and bar:122 is "122 of the calls here went to bar."
  call_target_re = re.compile(
      r"""
      ^\s+                    # Top-level lines are function records.
      \d+(?:\.\d+)?:          # Line info/discriminator
      \s+
      \d+                     # Total sample count
      \s+
      ((?:[^\s:]+:\d+\s*)+)   # Indirect call target(s)
      $
  """, re.VERBOSE)
  for line in lines:
    line = line.rstrip()

    match = call_target_re.match(line)
    if not match:
      yield line + '\n'
      continue

    group_start, group_end = match.span(1)
    assert group_end == len(line)
    yield line[:group_start].rstrip() + '\n'


def run(input_stream, output_stream):
  for line in _remove_indirect_call_targets(input_stream):
    output_stream.write(line)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--input',
      default='/dev/stdin',
      help='File to read from. Defaults to stdin.')
  parser.add_argument(
      '--output',
      default='/dev/stdout',
      help='File to write to. Defaults to stdout.')
  args = parser.parse_args()

  with open(args.input) as stdin:
    with open(args.output, 'w') as stdout:
      run(stdin, stdout)


if __name__ == '__main__':
  main()
