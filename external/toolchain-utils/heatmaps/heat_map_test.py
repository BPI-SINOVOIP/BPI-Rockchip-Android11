#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for heat_map.py."""

from __future__ import print_function

import unittest.mock as mock
import unittest

import os

from cros_utils import command_executer

from heatmaps import heat_map
from heatmaps import heatmap_generator


def make_heatmap(chromeos_root='/path/to/fake/chromeos_root/',
                 perf_data='/any_path/perf.data'):
  return heat_map.HeatMapProducer(chromeos_root, perf_data, None, None, '')


def fake_mkdtemp(prefix):
  """Mock tempfile.mkdtemp() by just create a pathname."""
  return prefix + 'random_dir'


def fake_parser_error(_, msg):
  """Redirect parser.error() to exception."""
  raise Exception(msg)


def fake_generate_perf_report_exception(_):
  raise Exception


class HeatmapTest(unittest.TestCase):
  """All of our tests for heat_map."""

  # pylint: disable=protected-access
  @mock.patch('shutil.copy2')
  @mock.patch('tempfile.mkdtemp')
  def test_EnsureFileInChrootAlreadyInside(self, mock_mkdtemp, mock_copy):
    perf_data_inchroot = (
        '/path/to/fake/chromeos_root/chroot/inchroot_path/perf.data')
    heatmap = make_heatmap(perf_data=perf_data_inchroot)
    heatmap._EnsureFileInChroot()
    self.assertFalse(heatmap.temp_dir_created)
    self.assertEqual(heatmap.temp_dir,
                     '/path/to/fake/chromeos_root/chroot/inchroot_path/')
    self.assertEqual(heatmap.temp_perf_inchroot, '/inchroot_path/')
    mock_mkdtemp.assert_not_called()
    mock_copy.assert_not_called()

  @mock.patch('shutil.copy2')
  @mock.patch('tempfile.mkdtemp', fake_mkdtemp)
  def test_EnsureFileInChrootOutsideNeedCopy(self, mock_copy):
    heatmap = make_heatmap()
    heatmap._EnsureFileInChroot()
    self.assertTrue(heatmap.temp_dir_created)
    self.assertEqual(mock_copy.call_count, 1)
    self.assertEqual(heatmap.temp_dir,
                     '/path/to/fake/chromeos_root/src/random_dir')
    self.assertEqual(heatmap.temp_perf_inchroot, '~/trunk/src/random_dir')

  @mock.patch.object(command_executer.CommandExecuter, 'ChrootRunCommand')
  def test_GeneratePerfReport(self, mock_ChrootRunCommand):
    heatmap = make_heatmap()
    heatmap.temp_dir = '/fake/chroot/inchroot_path/'
    heatmap.temp_perf_inchroot = '/inchroot_path/'
    mock_ChrootRunCommand.return_value = 0
    heatmap._GeneratePerfReport()
    cmd = ('cd %s && perf report -D -i perf.data > perf_report.txt' %
           heatmap.temp_perf_inchroot)
    mock_ChrootRunCommand.assert_called_with(heatmap.chromeos_root, cmd)
    self.assertEqual(mock_ChrootRunCommand.call_count, 1)
    self.assertEqual(heatmap.perf_report,
                     '/fake/chroot/inchroot_path/perf_report.txt')

  @mock.patch.object(heatmap_generator, 'HeatmapGenerator')
  def test_GetHeatMap(self, mock_heatmap_generator):
    heatmap = make_heatmap()
    heatmap._GetHeatMap(10)
    self.assertTrue(mock_heatmap_generator.called)

  @mock.patch.object(heat_map.HeatMapProducer, '_EnsureFileInChroot')
  @mock.patch.object(heat_map.HeatMapProducer, '_GeneratePerfReport')
  @mock.patch.object(heat_map.HeatMapProducer, '_GetHeatMap')
  @mock.patch.object(heat_map.HeatMapProducer, '_RemoveFiles')
  def test_Run(self, mock_remove_files, mock_get_heatmap,
               mock_generate_perf_report, mock_ensure_file_in_chroot):
    heatmap = make_heatmap()
    heatmap.Run(10)
    mock_ensure_file_in_chroot.assert_called_once_with()
    mock_generate_perf_report.assert_called_once_with()
    mock_get_heatmap.assert_called_once_with(10)
    mock_remove_files.assert_called_once_with()

  @mock.patch.object(heat_map.HeatMapProducer, '_EnsureFileInChroot')
  @mock.patch.object(
      heat_map.HeatMapProducer,
      '_GeneratePerfReport',
      new=fake_generate_perf_report_exception)
  @mock.patch.object(heat_map.HeatMapProducer, '_GetHeatMap')
  @mock.patch.object(heat_map.HeatMapProducer, '_RemoveFiles')
  @mock.patch('builtins.print')
  def test_Run_with_exception(self, mock_print, mock_remove_files,
                              mock_get_heatmap, mock_ensure_file_in_chroot):
    heatmap = make_heatmap()
    with self.assertRaises(Exception):
      heatmap.Run(10)
    mock_ensure_file_in_chroot.assert_called_once_with()
    mock_get_heatmap.assert_not_called()
    mock_remove_files.assert_called_once_with()
    mock_print.assert_not_called()

  @mock.patch('argparse.ArgumentParser.error', fake_parser_error)
  @mock.patch.object(os.path, 'isfile')
  @mock.patch.object(heat_map, 'IsARepoRoot')
  def test_main_arg_format(self, mock_IsARepoRoot, mock_isfile):
    """Test wrong arg format are detected."""
    args = ['--chromeos_root=/fake/chroot/', '--perf_data=/path/to/perf.data']

    # Test --chromeos_root format
    mock_IsARepoRoot.return_value = False
    with self.assertRaises(Exception) as msg:
      heat_map.main(args)
    self.assertIn('does not contain .repo dir.', str(msg.exception))

    # Test --perf_data format
    mock_IsARepoRoot.return_value = True
    mock_isfile.return_value = False
    with self.assertRaises(Exception) as msg:
      heat_map.main(args)
    self.assertIn('Cannot find perf_data', str(msg.exception))

    # Test --hugepage format
    mock_isfile.return_value = True
    args.append('--hugepage=0')
    with self.assertRaises(Exception) as msg:
      heat_map.main(args)
    self.assertIn('Wrong format of hugepage range', str(msg.exception))

    # Test --hugepage parse
    args[-1] = '--hugepage=0,4096'
    heat_map.HeatMapProducer = mock.MagicMock()
    heat_map.main(args)
    heat_map.HeatMapProducer.assert_called_with(
        '/fake/chroot/', '/path/to/perf.data', [0, 4096], None, '')


if __name__ == '__main__':
  unittest.main()
