#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for heatmap_generator.py."""

from __future__ import division, print_function

import unittest.mock as mock
import unittest

import os

from heatmaps import heatmap_generator


def _write_perf_mmap(pid, tid, addr, size, fp):
  print(
      '0 0 0 0 PERF_RECORD_MMAP2 %d/%d: '
      '[%x(%x) @ 0x0 0:0 0 0] '
      'r-xp /opt/google/chrome/chrome\n' % (pid, tid, addr, size),
      file=fp)


def _write_perf_fork(pid_from, tid_from, pid_to, tid_to, fp):
  print(
      '0 0 0 0 PERF_RECORD_FORK(%d:%d):(%d:%d)\n' % (pid_to, tid_to, pid_from,
                                                     tid_from),
      file=fp)


def _write_perf_exit(pid_from, tid_from, pid_to, tid_to, fp):
  print(
      '0 0 0 0 PERF_RECORD_EXIT(%d:%d):(%d:%d)\n' % (pid_to, tid_to, pid_from,
                                                     tid_from),
      file=fp)


def _write_perf_sample(pid, tid, addr, fp):
  print(
      '0 0 0 0 PERF_RECORD_SAMPLE(IP, 0x2): '
      '%d/%d: %x period: 100000 addr: 0' % (pid, tid, addr),
      file=fp)
  print(' ... thread: chrome:%d' % tid, file=fp)
  print(' ...... dso: /opt/google/chrome/chrome\n', file=fp)


def _heatmap(file_name, page_size=4096, hugepage=None, analyze=False, top_n=10):
  generator = heatmap_generator.HeatmapGenerator(
      file_name, page_size, hugepage, '',
      log_level='none')  # Don't log to stdout
  generator.draw()
  if analyze:
    generator.analyze('/path/to/chrome', top_n)


def _cleanup(file_name):
  files = [
      file_name, 'out.txt', 'inst-histo.txt', 'inst-histo-hp.txt',
      'inst-histo-sp.txt', 'heat_map.png', 'timeline.png', 'addr2symbol.txt'
  ]
  for f in files:
    if os.path.exists(f):
      os.remove(f)


class HeatmapGeneratorDrawTests(unittest.TestCase):
  """All of our tests for heatmap_generator.draw() and related."""

  def test_with_one_mmap_one_sample(self):
    """Tests one perf record and one sample."""
    fname = 'test.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      _write_perf_sample(101, 101, 0xABCD101, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname)
    self.assertIn('out.txt', os.listdir('.'))
    with open('out.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 1)
      self.assertIn('101/101: 1 0', lines[0])

  def test_with_one_mmap_multiple_samples(self):
    """Tests one perf record and three samples."""
    fname = 'test.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      _write_perf_sample(101, 101, 0xABCD101, f)
      _write_perf_sample(101, 101, 0xABCD102, f)
      _write_perf_sample(101, 101, 0xABCE102, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname)
    self.assertIn('out.txt', os.listdir('.'))
    with open('out.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 3)
      self.assertIn('101/101: 1 0', lines[0])
      self.assertIn('101/101: 2 0', lines[1])
      self.assertIn('101/101: 3 4096', lines[2])

  def test_with_fork_and_exit(self):
    """Tests perf fork and perf exit."""
    fname = 'test_fork.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      _write_perf_fork(101, 101, 202, 202, f)
      _write_perf_sample(101, 101, 0xABCD101, f)
      _write_perf_sample(202, 202, 0xABCE101, f)
      _write_perf_exit(202, 202, 202, 202, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname)
    self.assertIn('out.txt', os.listdir('.'))
    with open('out.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 2)
      self.assertIn('101/101: 1 0', lines[0])
      self.assertIn('202/202: 2 4096', lines[1])

  def test_hugepage_creates_two_chrome_mmaps(self):
    """Test two chrome mmaps for the same process."""
    fname = 'test_hugepage.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x1000, f)
      _write_perf_fork(101, 101, 202, 202, f)
      _write_perf_mmap(202, 202, 0xABCD000, 0x100, f)
      _write_perf_mmap(202, 202, 0xABCD300, 0xD00, f)
      _write_perf_sample(101, 101, 0xABCD102, f)
      _write_perf_sample(202, 202, 0xABCD102, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname)
    self.assertIn('out.txt', os.listdir('.'))
    with open('out.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 2)
      self.assertIn('101/101: 1 0', lines[0])
      self.assertIn('202/202: 2 0', lines[1])

  def test_hugepage_creates_two_chrome_mmaps_fail(self):
    """Test two chrome mmaps for the same process."""
    fname = 'test_hugepage.txt'
    # Cases where first_mmap.size < second_mmap.size
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x1000, f)
      _write_perf_fork(101, 101, 202, 202, f)
      _write_perf_mmap(202, 202, 0xABCD000, 0x10000, f)
    self.addCleanup(_cleanup, fname)
    with self.assertRaises(AssertionError) as msg:
      _heatmap(fname)
    self.assertIn('Original MMAP size', str(msg.exception))

    # Cases where first_mmap.address > second_mmap.address
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x1000, f)
      _write_perf_fork(101, 101, 202, 202, f)
      _write_perf_mmap(202, 202, 0xABCC000, 0x10000, f)
    with self.assertRaises(AssertionError) as msg:
      _heatmap(fname)
    self.assertIn('Original MMAP starting address', str(msg.exception))

    # Cases where first_mmap.address + size <
    # second_mmap.address + second_mmap.size
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x1000, f)
      _write_perf_fork(101, 101, 202, 202, f)
      _write_perf_mmap(202, 202, 0xABCD100, 0x10000, f)
    with self.assertRaises(AssertionError) as msg:
      _heatmap(fname)
    self.assertIn('exceeds the end of original MMAP', str(msg.exception))

  def test_histogram(self):
    """Tests if the tool can generate correct histogram.

    In the tool, histogram is generated from statistics
    of perf samples (saved to out.txt). The histogram is
    generated by perf-to-inst-page.sh and saved to
    inst-histo.txt. It will be used to draw heat maps.
    """
    fname = 'test_histo.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      for i in range(100):
        _write_perf_sample(101, 101, 0xABCD000 + i, f)
        _write_perf_sample(101, 101, 0xABCE000 + i, f)
        _write_perf_sample(101, 101, 0xABFD000 + i, f)
        _write_perf_sample(101, 101, 0xAFCD000 + i, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname)
    self.assertIn('inst-histo.txt', os.listdir('.'))
    with open('inst-histo.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 4)
      self.assertIn('100 0', lines[0])
      self.assertIn('100 4096', lines[1])
      self.assertIn('100 196608', lines[2])
      self.assertIn('100 4194304', lines[3])

  def test_histogram_two_mb_page(self):
    """Tests handling of 2MB page."""
    fname = 'test_histo.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      for i in range(100):
        _write_perf_sample(101, 101, 0xABCD000 + i, f)
        _write_perf_sample(101, 101, 0xABCE000 + i, f)
        _write_perf_sample(101, 101, 0xABFD000 + i, f)
        _write_perf_sample(101, 101, 0xAFCD000 + i, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname, page_size=2 * 1024 * 1024)
    self.assertIn('inst-histo.txt', os.listdir('.'))
    with open('inst-histo.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 2)
      self.assertIn('300 0', lines[0])
      self.assertIn('100 4194304', lines[1])

  def test_histogram_in_and_out_hugepage(self):
    """Tests handling the case of separating samples in and out huge page."""
    fname = 'test_histo.txt'
    with open(fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      for i in range(100):
        _write_perf_sample(101, 101, 0xABCD000 + i, f)
        _write_perf_sample(101, 101, 0xABCE000 + i, f)
        _write_perf_sample(101, 101, 0xABFD000 + i, f)
        _write_perf_sample(101, 101, 0xAFCD000 + i, f)
    self.addCleanup(_cleanup, fname)
    _heatmap(fname, hugepage=[0, 8192])
    file_list = os.listdir('.')
    self.assertNotIn('inst-histo.txt', file_list)
    self.assertIn('inst-histo-hp.txt', file_list)
    self.assertIn('inst-histo-sp.txt', file_list)
    with open('inst-histo-hp.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 2)
      self.assertIn('100 0', lines[0])
      self.assertIn('100 4096', lines[1])
    with open('inst-histo-sp.txt') as f:
      lines = f.readlines()
      self.assertEqual(len(lines), 2)
      self.assertIn('100 196608', lines[0])
      self.assertIn('100 4194304', lines[1])


class HeatmapGeneratorAnalyzeTests(unittest.TestCase):
  """All of our tests for heatmap_generator.analyze() and related."""

  def setUp(self):
    # Use the same perf report for testing
    self.fname = 'test_histo.txt'
    with open(self.fname, 'w') as f:
      _write_perf_mmap(101, 101, 0xABCD000, 0x100, f)
      for i in range(10):
        _write_perf_sample(101, 101, 0xABCD000 + i, f)
        _write_perf_sample(101, 101, 0xABCE000 + i, f)
        _write_perf_sample(101, 101, 0xABFD000 + i, f)
    self.nm = ('000000000abcd000 t Func1@Page1\n'
               '000000000abcd001 t Func2@Page1\n'
               '000000000abcd0a0 t Func3@Page1andFunc1@Page2\n'
               '000000000abce010 t Func2@Page2\n'
               '000000000abfd000 t Func1@Page3\n')

  def tearDown(self):
    _cleanup(self.fname)

  @mock.patch('subprocess.check_output')
  def test_analyze_hot_pages_with_hp_top(self, mock_nm):
    """Test if the analyze() can print the top page with hugepage."""
    mock_nm.return_value = self.nm
    _heatmap(self.fname, hugepage=[0, 8192], analyze=True, top_n=1)
    file_list = os.listdir('.')
    self.assertIn('addr2symbol.txt', file_list)
    with open('addr2symbol.txt') as f:
      contents = f.read()
      self.assertIn('Func2@Page1 : 9', contents)
      self.assertIn('Func1@Page1 : 1', contents)
      self.assertIn('Func1@Page3 : 10', contents)
      # Only displaying one page in hugepage
      self.assertNotIn('Func3@Page1andFunc1@Page2 : 10', contents)

  @mock.patch('subprocess.check_output')
  def test_analyze_hot_pages_without_hp_top(self, mock_nm):
    """Test if the analyze() can print the top page without hugepage."""
    mock_nm.return_value = self.nm
    _heatmap(self.fname, analyze=True, top_n=1)
    file_list = os.listdir('.')
    self.assertIn('addr2symbol.txt', file_list)
    with open('addr2symbol.txt') as f:
      contents = f.read()
      self.assertIn('Func2@Page1 : 9', contents)
      self.assertIn('Func1@Page1 : 1', contents)
      # Only displaying one page
      self.assertNotIn('Func3@Page1andFunc1@Page2 : 10', contents)
      self.assertNotIn('Func1@Page3 : 10', contents)

  @mock.patch('subprocess.check_output')
  def test_analyze_hot_pages_with_hp_top10(self, mock_nm):
    """Test if the analyze() can print with default top 10."""
    mock_nm.return_value = self.nm
    _heatmap(self.fname, analyze=True)
    # Make sure nm command is called correctly.
    mock_nm.assert_called_with(['nm', '-n', '/path/to/chrome'])
    file_list = os.listdir('.')
    self.assertIn('addr2symbol.txt', file_list)
    with open('addr2symbol.txt') as f:
      contents = f.read()
      self.assertIn('Func2@Page1 : 9', contents)
      self.assertIn('Func1@Page1 : 1', contents)
      self.assertIn('Func3@Page1andFunc1@Page2 : 10', contents)
      self.assertIn('Func1@Page3 : 10', contents)


if __name__ == '__main__':
  unittest.main()
