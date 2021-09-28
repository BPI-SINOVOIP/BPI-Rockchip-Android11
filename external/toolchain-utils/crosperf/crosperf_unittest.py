#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for crosperf."""

from __future__ import division
from __future__ import print_function

import argparse
import io
import tempfile
import unittest
import unittest.mock as mock

import crosperf
import settings_factory
import experiment_file

EXPERIMENT_FILE_1 = """
  board: x86-alex
  remote: chromeos-alex3
  locks_dir: /tmp
  perf_args: record -a -e cycles
  benchmark: PageCycler {
    iterations: 3
  }

  image1 {
    chromeos_image: /usr/local/google/cros_image1.bin
  }

  image2 {
    remote: chromeos-lumpy1
    chromeos_image: /usr/local/google/cros_image2.bin
  }
  """


class CrosperfTest(unittest.TestCase):
  """Crosperf test class."""

  def setUp(self):
    input_file = io.StringIO(EXPERIMENT_FILE_1)
    self.exp_file = experiment_file.ExperimentFile(input_file)

  def testDryRun(self):
    with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
      f.write(EXPERIMENT_FILE_1)
      f.flush()
      crosperf.Main(['', f.name, '--dry_run'])

  def testConvertOptionsToSettings(self):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-l',
        '--log_dir',
        dest='log_dir',
        default='',
        help='The log_dir, default is under '
        '<crosperf_logs>/logs')
    crosperf.SetupParserOptions(parser)
    argv = ['crosperf/crosperf.py', 'temp.exp', '--rerun=True']
    options, _ = parser.parse_known_args(argv)
    settings = crosperf.ConvertOptionsToSettings(options)
    self.assertIsNotNone(settings)
    self.assertIsInstance(settings, settings_factory.GlobalSettings)
    self.assertEqual(len(settings.fields), 38)
    self.assertTrue(settings.GetField('rerun'))
    argv = ['crosperf/crosperf.py', 'temp.exp']
    options, _ = parser.parse_known_args(argv)
    settings = crosperf.ConvertOptionsToSettings(options)
    self.assertFalse(settings.GetField('rerun'))

  def testExceptionPrintTraceback(self):
    """Test the main function can print traceback in exception."""

    def mock_RunCrosperf(*_args, **_kwargs):
      return 10 / 0

    with mock.patch('crosperf.RunCrosperf', new=mock_RunCrosperf):
      with self.assertRaises(ZeroDivisionError) as context:
        crosperf.Main([])
      self.assertEqual('division by zero', str(context.exception))


if __name__ == '__main__':
  unittest.main()
