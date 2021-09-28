#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unittest of experiment_file."""
from __future__ import print_function

import io
import unittest

from experiment_file import ExperimentFile

EXPERIMENT_FILE_1 = """
  board: x86-alex
  remote: chromeos-alex3
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

EXPERIMENT_FILE_2 = """
  board: x86-alex
  remote: chromeos-alex3
  iterations: 3

  benchmark: PageCycler {
  }

  benchmark: AndroidBench {
    iterations: 2
  }

  image1 {
    chromeos_image:/usr/local/google/cros_image1.bin
  }

  image2 {
    chromeos_image: /usr/local/google/cros_image2.bin
  }
  """

EXPERIMENT_FILE_3 = """
  board: x86-alex
  remote: chromeos-alex3
  iterations: 3

  benchmark: PageCycler {
  }

  image1 {
    chromeos_image:/usr/local/google/cros_image1.bin
  }

  image1 {
    chromeos_image: /usr/local/google/cros_image2.bin
  }
  """

EXPERIMENT_FILE_4 = """
  board: x86-alex
  remote: chromeos-alex3
  iterations: 3

  benchmark: webrtc {
    test_args: --story-filter=datachannel
  }

  benchmark: webrtc {
    test_args: --story-tag-filter=smoothness
  }

  image1 {
    chromeos_image:/usr/local/google/cros_image1.bin
  }
  """

DUT_CONFIG_EXPERIMENT_FILE_GOOD = """
  board: kevin64
  remote: chromeos-kevin.cros
  turbostat: False
  intel_pstate: no_hwp
  cooldown_temp: 38
  cooldown_time: 5
  governor: powersave
  cpu_usage: exclusive_cores
  cpu_freq_pct: 50
  top_interval: 5

  benchmark: speedometer {
    iterations: 3
    suite: telemetry_Crosperf
  }

  image1 {
    chromeos_image:/usr/local/google/cros_image1.bin
  }
  """

DUT_CONFIG_EXPERIMENT_FILE_BAD_GOV = """
  board: kevin64
  remote: chromeos-kevin.cros
  intel_pstate: active
  governor: misspelled_governor

  benchmark: speedometer2 {
    iterations: 3
    suite: telemetry_Crosperf
  }
  """

DUT_CONFIG_EXPERIMENT_FILE_BAD_CPUUSE = """
  board: kevin64
  remote: chromeos-kevin.cros
  turbostat: False
  governor: ondemand
  cpu_usage: unknown

  benchmark: speedometer2 {
    iterations: 3
    suite: telemetry_Crosperf
  }

  image1 {
    chromeos_image:/usr/local/google/cros_image1.bin
  }
  """

OUTPUT_FILE = """board: x86-alex
remote: chromeos-alex3
perf_args: record -a -e cycles

benchmark: PageCycler {
\titerations: 3
}

label: image1 {
\tchromeos_image: /usr/local/google/cros_image1.bin
\tremote: chromeos-alex3
}

label: image2 {
\tchromeos_image: /usr/local/google/cros_image2.bin
\tremote: chromeos-lumpy1
}\n\n"""


class ExperimentFileTest(unittest.TestCase):
  """The main class for Experiment File test."""

  def testLoadExperimentFile1(self):
    input_file = io.StringIO(EXPERIMENT_FILE_1)
    experiment_file = ExperimentFile(input_file)
    global_settings = experiment_file.GetGlobalSettings()
    self.assertEqual(global_settings.GetField('remote'), ['chromeos-alex3'])
    self.assertEqual(
        global_settings.GetField('perf_args'), 'record -a -e cycles')
    benchmark_settings = experiment_file.GetSettings('benchmark')
    self.assertEqual(len(benchmark_settings), 1)
    self.assertEqual(benchmark_settings[0].name, 'PageCycler')
    self.assertEqual(benchmark_settings[0].GetField('iterations'), 3)

    label_settings = experiment_file.GetSettings('label')
    self.assertEqual(len(label_settings), 2)
    self.assertEqual(label_settings[0].name, 'image1')
    self.assertEqual(label_settings[0].GetField('chromeos_image'),
                     '/usr/local/google/cros_image1.bin')
    self.assertEqual(label_settings[1].GetField('remote'), ['chromeos-lumpy1'])
    self.assertEqual(label_settings[0].GetField('remote'), ['chromeos-alex3'])

  def testOverrideSetting(self):
    input_file = io.StringIO(EXPERIMENT_FILE_2)
    experiment_file = ExperimentFile(input_file)
    global_settings = experiment_file.GetGlobalSettings()
    self.assertEqual(global_settings.GetField('remote'), ['chromeos-alex3'])

    benchmark_settings = experiment_file.GetSettings('benchmark')
    self.assertEqual(len(benchmark_settings), 2)
    self.assertEqual(benchmark_settings[0].name, 'PageCycler')
    self.assertEqual(benchmark_settings[0].GetField('iterations'), 3)
    self.assertEqual(benchmark_settings[1].name, 'AndroidBench')
    self.assertEqual(benchmark_settings[1].GetField('iterations'), 2)

  def testDuplicateLabel(self):
    input_file = io.StringIO(EXPERIMENT_FILE_3)
    self.assertRaises(Exception, ExperimentFile, input_file)

  def testDuplicateBenchmark(self):
    input_file = io.StringIO(EXPERIMENT_FILE_4)
    experiment_file = ExperimentFile(input_file)
    benchmark_settings = experiment_file.GetSettings('benchmark')
    self.assertEqual(benchmark_settings[0].name, 'webrtc')
    self.assertEqual(benchmark_settings[0].GetField('test_args'),
                     '--story-filter=datachannel')
    self.assertEqual(benchmark_settings[1].name, 'webrtc')
    self.assertEqual(benchmark_settings[1].GetField('test_args'),
                     '--story-tag-filter=smoothness')

  def testCanonicalize(self):
    input_file = io.StringIO(EXPERIMENT_FILE_1)
    experiment_file = ExperimentFile(input_file)
    res = experiment_file.Canonicalize()
    self.assertEqual(res, OUTPUT_FILE)

  def testLoadDutConfigExperimentFile_Good(self):
    input_file = io.StringIO(DUT_CONFIG_EXPERIMENT_FILE_GOOD)
    experiment_file = ExperimentFile(input_file)
    global_settings = experiment_file.GetGlobalSettings()
    self.assertEqual(global_settings.GetField('turbostat'), False)
    self.assertEqual(global_settings.GetField('intel_pstate'), 'no_hwp')
    self.assertEqual(global_settings.GetField('governor'), 'powersave')
    self.assertEqual(global_settings.GetField('cpu_usage'), 'exclusive_cores')
    self.assertEqual(global_settings.GetField('cpu_freq_pct'), 50)
    self.assertEqual(global_settings.GetField('cooldown_time'), 5)
    self.assertEqual(global_settings.GetField('cooldown_temp'), 38)
    self.assertEqual(global_settings.GetField('top_interval'), 5)

  def testLoadDutConfigExperimentFile_WrongGovernor(self):
    input_file = io.StringIO(DUT_CONFIG_EXPERIMENT_FILE_BAD_GOV)
    with self.assertRaises(RuntimeError) as msg:
      ExperimentFile(input_file)
    self.assertRegex(str(msg.exception), 'governor: misspelled_governor')
    self.assertRegex(
        str(msg.exception), "Invalid enum value for field 'governor'."
        r' Must be one of \(performance, powersave, userspace, ondemand,'
        r' conservative, schedutils, sched, interactive\)')

  def testLoadDutConfigExperimentFile_WrongCpuUsage(self):
    input_file = io.StringIO(DUT_CONFIG_EXPERIMENT_FILE_BAD_CPUUSE)
    with self.assertRaises(RuntimeError) as msg:
      ExperimentFile(input_file)
    self.assertRegex(str(msg.exception), 'cpu_usage: unknown')
    self.assertRegex(
        str(msg.exception), "Invalid enum value for field 'cpu_usage'."
        r' Must be one of \(all, big_only, little_only, exclusive_cores\)')


if __name__ == '__main__':
  unittest.main()
