#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for redact_profile.py."""

from __future__ import division, print_function

import io
import unittest

from afdo_redaction import redact_profile

_redact_limit = redact_profile.dedup_records.__defaults__[0]


def _redact(input_lines, summary_to=None):
  if isinstance(input_lines, str):
    input_lines = input_lines.splitlines()

  if summary_to is None:
    summary_to = io.StringIO()

  output_to = io.StringIO()
  redact_profile.run(
      profile_input_file=input_lines,
      summary_output_file=summary_to,
      profile_output_file=output_to)
  return output_to.getvalue()


def _redact_with_summary(input_lines):
  summary = io.StringIO()
  result = _redact(input_lines, summary_to=summary)
  return result, summary.getvalue()


def _generate_repeated_function_body(repeats, fn_name='_some_name'):
  # Arbitrary function body ripped from a textual AFDO profile.
  function_header = fn_name + ':1234:185'
  function_body = [
      ' 6: 83',
      ' 15: 126',
      ' 62832: 126',
      ' 6: _ZNK5blink10PaintLayer14GroupedMappingEv:2349',
      '  1: 206',
      '  1: _ZNK5blink10PaintLayer14GroupedMappersEv:2060',
      '   1: 206',
      ' 11: _ZNK5blink10PaintLayer25GetCompositedLayerMappingEv:800',
      '  2.1: 80',
  ]

  # Be sure to zfill this, so the functions are output in sorted order.
  num_width = len(str(repeats))

  lines = []
  for i in range(repeats):
    num = str(i).zfill(num_width)
    lines.append(num + function_header)
    lines.extend(function_body)
  return lines


class Tests(unittest.TestCase):
  """All of our tests for redact_profile."""

  def test_no_input_works(self):
    self.assertEqual(_redact(''), '')

  def test_single_function_works(self):
    lines = _generate_repeated_function_body(1)
    result_file = '\n'.join(lines) + '\n'
    self.assertEqual(_redact(lines), result_file)

  def test_duplicate_of_single_function_works(self):
    lines = _generate_repeated_function_body(2)
    result_file = '\n'.join(lines) + '\n'
    self.assertEqual(_redact(lines), result_file)

  def test_not_too_many_duplicates_of_single_function_redacts_none(self):
    lines = _generate_repeated_function_body(_redact_limit - 1)
    result_file = '\n'.join(lines) + '\n'
    self.assertEqual(_redact(lines), result_file)

  def test_many_duplicates_of_single_function_redacts_them_all(self):
    lines = _generate_repeated_function_body(_redact_limit)
    self.assertEqual(_redact(lines), '')

  def test_many_duplicates_of_single_function_leaves_other_functions(self):
    kept_lines = _generate_repeated_function_body(1, fn_name='_keep_me')
    # Something to distinguish us from the rest. Just bump a random counter.
    kept_lines[1] += '1'

    result_file = '\n'.join(kept_lines) + '\n'

    lines = _generate_repeated_function_body(
        _redact_limit, fn_name='_discard_me')
    self.assertEqual(_redact(kept_lines + lines), result_file)
    self.assertEqual(_redact(lines + kept_lines), result_file)

    more_lines = _generate_repeated_function_body(
        _redact_limit, fn_name='_and_discard_me')
    self.assertEqual(_redact(lines + kept_lines + more_lines), result_file)
    self.assertEqual(_redact(lines + more_lines), '')

  def test_correct_summary_is_printed_when_nothing_is_redacted(self):
    lines = _generate_repeated_function_body(1)
    _, summary = _redact_with_summary(lines)
    self.assertIn('Retained 1/1 functions', summary)
    self.assertIn('Retained 827/827 samples, total', summary)
    # Note that top-level samples == "samples without inlining taken into
    # account," not "sum(entry_counts)"
    self.assertIn('Retained 335/335 top-level samples', summary)

  def test_correct_summary_is_printed_when_everything_is_redacted(self):
    lines = _generate_repeated_function_body(_redact_limit)
    _, summary = _redact_with_summary(lines)
    self.assertIn('Retained 0/100 functions', summary)
    self.assertIn('Retained 0/82,700 samples, total', summary)
    self.assertIn('Retained 0/33,500 top-level samples', summary)

  def test_correct_summary_is_printed_when_most_everything_is_redacted(self):
    kept_lines = _generate_repeated_function_body(1, fn_name='_keep_me')
    kept_lines[1] += '1'

    lines = _generate_repeated_function_body(_redact_limit)
    _, summary = _redact_with_summary(kept_lines + lines)
    self.assertIn('Retained 1/101 functions', summary)
    self.assertIn('Retained 1,575/84,275 samples, total', summary)
    self.assertIn('Retained 1,083/34,583 top-level samples', summary)


if __name__ == '__main__':
  unittest.main()
