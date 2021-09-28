#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for post_process_orderfile.py."""

from __future__ import division, print_function

import os
import shutil
import tempfile
import unittest

import post_process_orderfile


def _write_nm_file(name):
  with open(name, 'w') as out:
    out.write('000001 s NotAValidSymbol1\n')
    out.write('000002 S NotAValidSymbol2\n')
    out.write('000010 t FirstValidSymbol\n')
    out.write('000012 t \n')
    out.write('000020 T Builtins_SecondValidSymbol\n')
    out.write('000030 T $SymbolToIgnore\n')
    out.write('000036 T Builtins_LastValidSymbol\n')


def _write_orderfile(name):
  with open(name, 'w') as out:
    out.write('SymbolOrdered1\n')
    out.write('SymbolOrdered2\n')


def _cleanup(files):
  for f in files:
    shutil.rmtree(f, ignore_errors=True)


class Tests(unittest.TestCase):
  """All of our tests for post_process_orderfile."""

  # pylint: disable=protected-access
  def test__parse_nm_output(self):
    temp_dir = tempfile.mkdtemp()
    self.addCleanup(_cleanup, [temp_dir])
    chrome_nm_file = os.path.join(temp_dir, 'chrome_nm.txt')
    _write_nm_file(chrome_nm_file)
    with open(chrome_nm_file) as f:
      results = list(post_process_orderfile._parse_nm_output(f))
      self.assertEqual(len(results), 3)
      self.assertIn('FirstValidSymbol', results)
      self.assertIn('Builtins_SecondValidSymbol', results)
      self.assertIn('Builtins_LastValidSymbol', results)

  def test__remove_duplicates(self):
    duplicates = ['marker1', 'marker2', 'marker3', 'marker2', 'marker1']
    results = list(post_process_orderfile._remove_duplicates(duplicates))
    self.assertEqual(results, ['marker1', 'marker2', 'marker3'])

  def test_run(self):
    temp_dir = tempfile.mkdtemp()
    self.addCleanup(_cleanup, [temp_dir])
    orderfile_input = os.path.join(temp_dir, 'orderfile.in.txt')
    orderfile_output = os.path.join(temp_dir, 'orderfile.out.txt')
    chrome_nm_file = os.path.join(temp_dir, 'chrome_nm.txt')
    _write_nm_file(chrome_nm_file)
    _write_orderfile(orderfile_input)
    with open(orderfile_input) as in_stream, \
    open(orderfile_output, 'w') as out_stream, \
    open(chrome_nm_file) as chrome_nm_stream:
      post_process_orderfile.run(in_stream, chrome_nm_stream, out_stream)

    with open(orderfile_output) as check:
      results = [x.strip() for x in check.readlines()]
      self.assertEqual(
          results,
          [
              # Start marker should be put first.
              'chrome_begin_ordered_code',
              # Symbols in orderfile come next.
              'SymbolOrdered1',
              'SymbolOrdered2',
              # Builtin functions in chrome_nm come next, and sorted.
              'Builtins_LastValidSymbol',
              'Builtins_SecondValidSymbol',
              # Last symbol should be end marker.
              'chrome_end_ordered_code'
          ])


if __name__ == '__main__':
  unittest.main()
