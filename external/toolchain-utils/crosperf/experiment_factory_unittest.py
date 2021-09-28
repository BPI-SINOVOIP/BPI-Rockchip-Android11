#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit test for experiment_factory.py"""

from __future__ import print_function

import io
import os
import socket
import unittest
import unittest.mock as mock

from cros_utils import command_executer
from cros_utils.file_utils import FileUtils

from experiment_file import ExperimentFile
import test_flag
import benchmark
import experiment_factory
from experiment_factory import ExperimentFactory
import settings_factory

EXPERIMENT_FILE_1 = """
  board: x86-alex
  remote: chromeos-alex3
  locks_dir: /tmp

  benchmark: PageCycler {
    iterations: 3
  }

  benchmark: webrtc {
    iterations: 1
    test_args: --story-filter=datachannel
  }

  image1 {
    chromeos_image: /usr/local/google/cros_image1.bin
  }

  image2 {
    chromeos_image: /usr/local/google/cros_image2.bin
  }
  """

EXPERIMENT_FILE_2 = """
  board: x86-alex
  remote: chromeos-alex3
  locks_dir: /tmp

  cwp_dso: kallsyms

  benchmark: Octane {
    iterations: 1
    suite: telemetry_Crosperf
    run_local: False
    weight: 0.8
  }

  benchmark: Kraken {
    iterations: 1
    suite: telemetry_Crosperf
    run_local: False
    weight: 0.2
  }

  image1 {
    chromeos_image: /usr/local/google/cros_image1.bin
  }
  """

# pylint: disable=too-many-function-args


class ExperimentFactoryTest(unittest.TestCase):
  """Class for running experiment factory unittests."""

  def setUp(self):
    self.append_benchmark_call_args = []

  def testLoadExperimentFile1(self):
    experiment_file = ExperimentFile(io.StringIO(EXPERIMENT_FILE_1))
    exp = ExperimentFactory().GetExperiment(
        experiment_file, working_directory='', log_dir='')
    self.assertEqual(exp.remote, ['chromeos-alex3'])

    self.assertEqual(len(exp.benchmarks), 2)
    self.assertEqual(exp.benchmarks[0].name, 'PageCycler')
    self.assertEqual(exp.benchmarks[0].test_name, 'PageCycler')
    self.assertEqual(exp.benchmarks[0].iterations, 3)
    self.assertEqual(exp.benchmarks[1].name, 'webrtc@@datachannel')
    self.assertEqual(exp.benchmarks[1].test_name, 'webrtc')
    self.assertEqual(exp.benchmarks[1].iterations, 1)

    self.assertEqual(len(exp.labels), 2)
    self.assertEqual(exp.labels[0].chromeos_image,
                     '/usr/local/google/cros_image1.bin')
    self.assertEqual(exp.labels[0].board, 'x86-alex')

  def testLoadExperimentFile2CWP(self):
    experiment_file = ExperimentFile(io.StringIO(EXPERIMENT_FILE_2))
    exp = ExperimentFactory().GetExperiment(
        experiment_file, working_directory='', log_dir='')
    self.assertEqual(exp.cwp_dso, 'kallsyms')
    self.assertEqual(len(exp.benchmarks), 2)
    self.assertEqual(exp.benchmarks[0].weight, 0.8)
    self.assertEqual(exp.benchmarks[1].weight, 0.2)

  def testDuplecateBenchmark(self):
    mock_experiment_file = ExperimentFile(io.StringIO(EXPERIMENT_FILE_1))
    mock_experiment_file.all_settings = []
    benchmark_settings1 = settings_factory.BenchmarkSettings('name')
    mock_experiment_file.all_settings.append(benchmark_settings1)
    benchmark_settings2 = settings_factory.BenchmarkSettings('name')
    mock_experiment_file.all_settings.append(benchmark_settings2)

    with self.assertRaises(SyntaxError):
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')

  def testCWPExceptions(self):
    mock_experiment_file = ExperimentFile(io.StringIO(''))
    mock_experiment_file.all_settings = []
    global_settings = settings_factory.GlobalSettings('test_name')
    global_settings.SetField('locks_dir', '/tmp')

    # Test 1: DSO type not supported
    global_settings.SetField('cwp_dso', 'test')
    self.assertEqual(global_settings.GetField('cwp_dso'), 'test')
    mock_experiment_file.global_settings = global_settings
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('The DSO specified is not supported', str(msg.exception))

    # Test 2: No weight after DSO specified
    global_settings.SetField('cwp_dso', 'kallsyms')
    mock_experiment_file.global_settings = global_settings
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('With DSO specified, each benchmark should have a weight',
                     str(msg.exception))

    # Test 3: Weight is set, but no dso specified
    global_settings.SetField('cwp_dso', '')
    mock_experiment_file.global_settings = global_settings
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    benchmark_settings.SetField('weight', '0.8')
    mock_experiment_file.all_settings = []
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('Weight can only be set when DSO specified',
                     str(msg.exception))

    # Test 4: cwp_dso only works for telemetry_Crosperf benchmarks
    global_settings.SetField('cwp_dso', 'kallsyms')
    mock_experiment_file.global_settings = global_settings
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    benchmark_settings.SetField('weight', '0.8')
    mock_experiment_file.all_settings = []
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual(
        'CWP approximation weight only works with '
        'telemetry_Crosperf suite', str(msg.exception))

    # Test 5: cwp_dso does not work for local run
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    benchmark_settings.SetField('weight', '0.8')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    benchmark_settings.SetField('run_local', 'True')
    mock_experiment_file.all_settings = []
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('run_local must be set to False to use CWP approximation',
                     str(msg.exception))

    # Test 6: weight should be float >=0
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    benchmark_settings.SetField('weight', '-1.2')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    benchmark_settings.SetField('run_local', 'False')
    mock_experiment_file.all_settings = []
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('Weight should be a float >=0', str(msg.exception))

    # Test 7: more than one story tag in test_args
    benchmark_settings = settings_factory.BenchmarkSettings('name')
    benchmark_settings.SetField('test_args',
                                '--story-filter=a --story-tag-filter=b')
    benchmark_settings.SetField('weight', '1.2')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    mock_experiment_file.all_settings = []
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual(
        'Only one story or story-tag filter allowed in a single '
        'benchmark run', str(msg.exception))

    # Test 8: Iterations of each benchmark run are not same in cwp mode
    mock_experiment_file.all_settings = []
    benchmark_settings = settings_factory.BenchmarkSettings('name1')
    benchmark_settings.SetField('iterations', '4')
    benchmark_settings.SetField('weight', '1.2')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    benchmark_settings.SetField('run_local', 'False')
    mock_experiment_file.all_settings.append(benchmark_settings)
    benchmark_settings = settings_factory.BenchmarkSettings('name2')
    benchmark_settings.SetField('iterations', '3')
    benchmark_settings.SetField('weight', '1.2')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    benchmark_settings.SetField('run_local', 'False')
    mock_experiment_file.all_settings.append(benchmark_settings)
    with self.assertRaises(RuntimeError) as msg:
      ef = ExperimentFactory()
      ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual('Iterations of each benchmark run are not the same',
                     str(msg.exception))

  def test_append_benchmark_set(self):
    ef = ExperimentFactory()

    bench_list = []
    ef.AppendBenchmarkSet(bench_list, experiment_factory.telemetry_perfv2_tests,
                          '', 1, False, '', 'telemetry_Crosperf', False, 0,
                          False, '', 0)
    self.assertEqual(
        len(bench_list), len(experiment_factory.telemetry_perfv2_tests))
    self.assertTrue(isinstance(bench_list[0], benchmark.Benchmark))

    bench_list = []
    ef.AppendBenchmarkSet(
        bench_list, experiment_factory.telemetry_pagecycler_tests, '', 1, False,
        '', 'telemetry_Crosperf', False, 0, False, '', 0)
    self.assertEqual(
        len(bench_list), len(experiment_factory.telemetry_pagecycler_tests))
    self.assertTrue(isinstance(bench_list[0], benchmark.Benchmark))

    bench_list = []
    ef.AppendBenchmarkSet(
        bench_list, experiment_factory.telemetry_toolchain_perf_tests, '', 1,
        False, '', 'telemetry_Crosperf', False, 0, False, '', 0)
    self.assertEqual(
        len(bench_list), len(experiment_factory.telemetry_toolchain_perf_tests))
    self.assertTrue(isinstance(bench_list[0], benchmark.Benchmark))

  @mock.patch.object(socket, 'gethostname')
  def test_get_experiment(self, mock_socket):

    test_flag.SetTestMode(False)
    self.append_benchmark_call_args = []

    def FakeAppendBenchmarkSet(bench_list, set_list, args, iters, rm_ch,
                               perf_args, suite, show_all):
      'Helper function for test_get_experiment'
      arg_list = [
          bench_list, set_list, args, iters, rm_ch, perf_args, suite, show_all
      ]
      self.append_benchmark_call_args.append(arg_list)

    def FakeGetDefaultRemotes(board):
      if not board:
        return []
      return ['fake_chromeos_machine1.cros', 'fake_chromeos_machine2.cros']

    def FakeGetXbuddyPath(build, autotest_dir, debug_dir, board, chroot,
                          log_level, perf_args):
      autotest_path = autotest_dir
      if not autotest_path:
        autotest_path = 'fake_autotest_path'
      debug_path = debug_dir
      if not debug_path and perf_args:
        debug_path = 'fake_debug_path'
      if not build or not board or not chroot or not log_level:
        return '', autotest_path, debug_path
      return 'fake_image_path', autotest_path, debug_path

    ef = ExperimentFactory()
    ef.AppendBenchmarkSet = FakeAppendBenchmarkSet
    ef.GetDefaultRemotes = FakeGetDefaultRemotes

    label_settings = settings_factory.LabelSettings('image_label')
    benchmark_settings = settings_factory.BenchmarkSettings('bench_test')
    global_settings = settings_factory.GlobalSettings('test_name')

    label_settings.GetXbuddyPath = FakeGetXbuddyPath

    mock_experiment_file = ExperimentFile(io.StringIO(''))
    mock_experiment_file.all_settings = []

    test_flag.SetTestMode(True)
    # Basic test.
    global_settings.SetField('name', 'unittest_test')
    global_settings.SetField('board', 'lumpy')
    global_settings.SetField('locks_dir', '/tmp')
    global_settings.SetField('remote', '123.45.67.89 123.45.76.80')
    benchmark_settings.SetField('test_name', 'kraken')
    benchmark_settings.SetField('suite', 'telemetry_Crosperf')
    benchmark_settings.SetField('iterations', 1)
    label_settings.SetField(
        'chromeos_image',
        'chromeos/src/build/images/lumpy/latest/chromiumos_test_image.bin')
    label_settings.SetField('chrome_src', '/usr/local/google/home/chrome-top')
    label_settings.SetField('autotest_path', '/tmp/autotest')

    mock_experiment_file.global_settings = global_settings
    mock_experiment_file.all_settings.append(label_settings)
    mock_experiment_file.all_settings.append(benchmark_settings)
    mock_experiment_file.all_settings.append(global_settings)

    mock_socket.return_value = ''

    # First test. General test.
    exp = ef.GetExperiment(mock_experiment_file, '', '')
    self.assertCountEqual(exp.remote, ['123.45.67.89', '123.45.76.80'])
    self.assertEqual(exp.cache_conditions, [0, 2, 1])
    self.assertEqual(exp.log_level, 'average')

    self.assertEqual(len(exp.benchmarks), 1)
    self.assertEqual(exp.benchmarks[0].name, 'bench_test')
    self.assertEqual(exp.benchmarks[0].test_name, 'kraken')
    self.assertEqual(exp.benchmarks[0].iterations, 1)
    self.assertEqual(exp.benchmarks[0].suite, 'telemetry_Crosperf')
    self.assertFalse(exp.benchmarks[0].show_all_results)

    self.assertEqual(len(exp.labels), 1)
    self.assertEqual(
        exp.labels[0].chromeos_image, 'chromeos/src/build/images/lumpy/latest/'
        'chromiumos_test_image.bin')
    self.assertEqual(exp.labels[0].autotest_path, '/tmp/autotest')
    self.assertEqual(exp.labels[0].board, 'lumpy')

    # Second test: Remotes listed in labels.
    test_flag.SetTestMode(True)
    label_settings.SetField('remote', 'chromeos1.cros chromeos2.cros')
    exp = ef.GetExperiment(mock_experiment_file, '', '')
    self.assertCountEqual(
        exp.remote,
        ['123.45.67.89', '123.45.76.80', 'chromeos1.cros', 'chromeos2.cros'])

    # Third test: Automatic fixing of bad  logging_level param:
    global_settings.SetField('logging_level', 'really loud!')
    exp = ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual(exp.log_level, 'verbose')

    # Fourth test: Setting cache conditions; only 1 remote with "same_machine"
    global_settings.SetField('rerun_if_failed', 'true')
    global_settings.SetField('rerun', 'true')
    global_settings.SetField('same_machine', 'true')
    global_settings.SetField('same_specs', 'true')

    self.assertRaises(Exception, ef.GetExperiment, mock_experiment_file, '', '')
    label_settings.SetField('remote', '')
    global_settings.SetField('remote', '123.45.67.89')
    exp = ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual(exp.cache_conditions, [0, 2, 3, 4, 6, 1])

    # Fifth Test: Adding a second label; calling GetXbuddyPath; omitting all
    # remotes (Call GetDefaultRemotes).
    mock_socket.return_value = 'test.corp.google.com'
    global_settings.SetField('remote', '')
    global_settings.SetField('same_machine', 'false')

    label_settings_2 = settings_factory.LabelSettings('official_image_label')
    label_settings_2.SetField('chromeos_root', 'chromeos')
    label_settings_2.SetField('build', 'official-dev')
    label_settings_2.SetField('autotest_path', '')
    label_settings_2.GetXbuddyPath = FakeGetXbuddyPath

    mock_experiment_file.all_settings.append(label_settings_2)
    exp = ef.GetExperiment(mock_experiment_file, '', '')
    self.assertEqual(len(exp.labels), 2)
    self.assertEqual(exp.labels[1].chromeos_image, 'fake_image_path')
    self.assertEqual(exp.labels[1].autotest_path, 'fake_autotest_path')
    self.assertCountEqual(
        exp.remote,
        ['fake_chromeos_machine1.cros', 'fake_chromeos_machine2.cros'])

  def test_get_default_remotes(self):
    board_list = [
        'elm', 'bob', 'chell', 'kefka', 'lulu', 'nautilus', 'snappy',
        'veyron_minnie'
    ]

    ef = ExperimentFactory()
    self.assertRaises(Exception, ef.GetDefaultRemotes, 'bad-board')

    # Verify that we have entries for every board, and that we get at least
    # two machines for each board.
    for b in board_list:
      remotes = ef.GetDefaultRemotes(b)
      if b == 'daisy':
        self.assertEqual(len(remotes), 1)
      else:
        self.assertGreaterEqual(len(remotes), 2)

  @mock.patch.object(command_executer.CommandExecuter, 'RunCommand')
  @mock.patch.object(os.path, 'exists')
  def test_check_skylab_tool(self, mock_exists, mock_runcmd):
    ef = ExperimentFactory()
    chromeos_root = '/tmp/chromeos'
    log_level = 'average'

    mock_exists.return_value = True
    ret = ef.CheckSkylabTool(chromeos_root, log_level)
    self.assertTrue(ret)

    mock_exists.return_value = False
    mock_runcmd.return_value = 1
    with self.assertRaises(RuntimeError) as err:
      ef.CheckSkylabTool(chromeos_root, log_level)
    self.assertEqual(mock_runcmd.call_count, 1)
    self.assertEqual(
        str(err.exception), 'Skylab tool not installed '
        'correctly, please try to manually install it from '
        '/tmp/chromeos/chromeos-admin/lab-tools/setup_lab_tools')

    mock_runcmd.return_value = 0
    mock_runcmd.call_count = 0
    ret = ef.CheckSkylabTool(chromeos_root, log_level)
    self.assertEqual(mock_runcmd.call_count, 1)
    self.assertFalse(ret)


if __name__ == '__main__':
  FileUtils.Configure(True)
  test_flag.SetTestMode(True)
  unittest.main()
