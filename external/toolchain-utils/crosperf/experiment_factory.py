# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to generate experiments."""

from __future__ import print_function
import os
import re
import socket
import sys

from benchmark import Benchmark
import config
from cros_utils import logger
from cros_utils import command_executer
from experiment import Experiment
from label import Label
from label import MockLabel
from results_cache import CacheConditions
import test_flag
import file_lock_machine

# Users may want to run Telemetry tests either individually, or in
# specified sets.  Here we define sets of tests that users may want
# to run together.

telemetry_perfv2_tests = [
    'kraken',
    'octane',
]

telemetry_pagecycler_tests = [
    'page_cycler_v2.intl_ar_fa_he',
    'page_cycler_v2.intl_es_fr_pt-BR',
    'page_cycler_v2.intl_hi_ru',
    'page_cycler_v2.intl_ja_zh',
    'page_cycler_v2.intl_ko_th_vi',
    'page_cycler_v2.typical_25',
]

telemetry_toolchain_old_perf_tests = [
    'page_cycler_v2.intl_es_fr_pt-BR',
    'page_cycler_v2.intl_hi_ru',
    'page_cycler_v2.intl_ja_zh',
    'page_cycler_v2.intl_ko_th_vi',
    'page_cycler_v2.netsim.top_10',
    'page_cycler_v2.typical_25',
    'spaceport',
    'tab_switching.top_10',
]
telemetry_toolchain_perf_tests = [
    'octane', 'kraken', 'speedometer', 'speedometer2', 'jetstream2'
]
graphics_perf_tests = [
    'graphics_GLBench',
    'graphics_GLMark2',
    'graphics_SanAngeles',
    'graphics_WebGLAquarium',
    'graphics_WebGLPerformance',
]
# TODO: disable rendering.desktop by default as the benchmark is
# currently in a bad state
# page_cycler_v2.typical_25 is deprecated and the recommend replacement is
# loading.desktop@@typical (crbug.com/916340)
telemetry_crosbolt_perf_tests = [
    'octane',
    'kraken',
    'speedometer2',
    'jetstream',
    'loading.desktop',
    # 'rendering.desktop',
]

crosbolt_perf_tests = [
    'graphics_WebGLAquarium',
    'tast.video.PlaybackPerfVP91080P30FPS',
]

#    'cheets_AntutuTest',
#    'cheets_PerfBootServer',
#    'cheets_CandyCrushTest',
#    'cheets_LinpackTest',
# ]

dso_list = [
    'all',
    'chrome',
    'kallsyms',
]


class ExperimentFactory(object):
  """Factory class for building an Experiment, given an ExperimentFile as input.

  This factory is currently hardcoded to produce an experiment for running
  ChromeOS benchmarks, but the idea is that in the future, other types
  of experiments could be produced.
  """

  def AppendBenchmarkSet(self, benchmarks, benchmark_list, test_args,
                         iterations, rm_chroot_tmp, perf_args, suite,
                         show_all_results, retries, run_local, cwp_dso, weight):
    """Add all the tests in a set to the benchmarks list."""
    for test_name in benchmark_list:
      telemetry_benchmark = Benchmark(
          test_name, test_name, test_args, iterations, rm_chroot_tmp, perf_args,
          suite, show_all_results, retries, run_local, cwp_dso, weight)
      benchmarks.append(telemetry_benchmark)

  def GetExperiment(self, experiment_file, working_directory, log_dir):
    """Construct an experiment from an experiment file."""
    global_settings = experiment_file.GetGlobalSettings()
    experiment_name = global_settings.GetField('name')
    board = global_settings.GetField('board')
    chromeos_root = global_settings.GetField('chromeos_root')
    log_level = global_settings.GetField('logging_level')
    if log_level not in ('quiet', 'average', 'verbose'):
      log_level = 'verbose'

    skylab = global_settings.GetField('skylab')
    # Check whether skylab tool is installed correctly for skylab mode.
    if skylab and not self.CheckSkylabTool(chromeos_root, log_level):
      sys.exit(0)

    remote = global_settings.GetField('remote')
    # This is used to remove the ",' from the remote if user
    # add them to the remote string.
    new_remote = []
    if remote:
      for i in remote:
        c = re.sub('["\']', '', i)
        new_remote.append(c)
    remote = new_remote
    rm_chroot_tmp = global_settings.GetField('rm_chroot_tmp')
    perf_args = global_settings.GetField('perf_args')
    download_debug = global_settings.GetField('download_debug')
    # Do not download debug symbols when perf_args is not specified.
    if not perf_args and download_debug:
      download_debug = False
    acquire_timeout = global_settings.GetField('acquire_timeout')
    cache_dir = global_settings.GetField('cache_dir')
    cache_only = global_settings.GetField('cache_only')
    config.AddConfig('no_email', global_settings.GetField('no_email'))
    share_cache = global_settings.GetField('share_cache')
    results_dir = global_settings.GetField('results_dir')
    # Warn user that option use_file_locks is deprecated.
    use_file_locks = global_settings.GetField('use_file_locks')
    if use_file_locks:
      l = logger.GetLogger()
      l.LogWarning('Option use_file_locks is deprecated, please remove it '
                   'from your experiment settings.')
    locks_dir = global_settings.GetField('locks_dir')
    # If not specified, set the locks dir to the default locks dir in
    # file_lock_machine.
    if not locks_dir:
      locks_dir = file_lock_machine.Machine.LOCKS_DIR
    if not os.path.exists(locks_dir):
      raise RuntimeError('Cannot access default lock directory. '
                         'Please run prodaccess or specify a local directory')
    chrome_src = global_settings.GetField('chrome_src')
    show_all_results = global_settings.GetField('show_all_results')
    cwp_dso = global_settings.GetField('cwp_dso')
    if cwp_dso and not cwp_dso in dso_list:
      raise RuntimeError('The DSO specified is not supported')
    ignore_min_max = global_settings.GetField('ignore_min_max')
    dut_config = {
        'enable_aslr': global_settings.GetField('enable_aslr'),
        'intel_pstate': global_settings.GetField('intel_pstate'),
        'cooldown_time': global_settings.GetField('cooldown_time'),
        'cooldown_temp': global_settings.GetField('cooldown_temp'),
        'governor': global_settings.GetField('governor'),
        'cpu_usage': global_settings.GetField('cpu_usage'),
        'cpu_freq_pct': global_settings.GetField('cpu_freq_pct'),
        'turbostat': global_settings.GetField('turbostat'),
        'top_interval': global_settings.GetField('top_interval'),
    }

    # Default cache hit conditions. The image checksum in the cache and the
    # computed checksum of the image must match. Also a cache file must exist.
    cache_conditions = [
        CacheConditions.CACHE_FILE_EXISTS, CacheConditions.CHECKSUMS_MATCH
    ]
    if global_settings.GetField('rerun_if_failed'):
      cache_conditions.append(CacheConditions.RUN_SUCCEEDED)
    if global_settings.GetField('rerun'):
      cache_conditions.append(CacheConditions.FALSE)
    if global_settings.GetField('same_machine'):
      cache_conditions.append(CacheConditions.SAME_MACHINE_MATCH)
    if global_settings.GetField('same_specs'):
      cache_conditions.append(CacheConditions.MACHINES_MATCH)

    # Construct benchmarks.
    # Some fields are common with global settings. The values are
    # inherited and/or merged with the global settings values.
    benchmarks = []
    all_benchmark_settings = experiment_file.GetSettings('benchmark')

    # Check if there is duplicated benchmark name
    benchmark_names = {}
    # Check if in cwp_dso mode, all benchmarks should have same iterations
    cwp_dso_iterations = 0

    for benchmark_settings in all_benchmark_settings:
      benchmark_name = benchmark_settings.name
      test_name = benchmark_settings.GetField('test_name')
      if not test_name:
        test_name = benchmark_name
      test_args = benchmark_settings.GetField('test_args')

      # Rename benchmark name if 'story-filter' or 'story-tag-filter' specified
      # in test_args. Make sure these two tags only appear once.
      story_count = 0
      for arg in test_args.split():
        if '--story-filter=' in arg or '--story-tag-filter=' in arg:
          story_count += 1
          if story_count > 1:
            raise RuntimeError('Only one story or story-tag filter allowed in '
                               'a single benchmark run')
          # Rename benchmark name with an extension of 'story'-option
          benchmark_name = '%s@@%s' % (benchmark_name, arg.split('=')[-1])

      # Check for duplicated benchmark name after renaming
      if not benchmark_name in benchmark_names:
        benchmark_names[benchmark_name] = True
      else:
        raise SyntaxError("Duplicate benchmark name: '%s'." % benchmark_name)

      iterations = benchmark_settings.GetField('iterations')
      if cwp_dso:
        if cwp_dso_iterations != 0 and iterations != cwp_dso_iterations:
          raise RuntimeError('Iterations of each benchmark run are not the ' \
                             'same')
        cwp_dso_iterations = iterations

      suite = benchmark_settings.GetField('suite')
      retries = benchmark_settings.GetField('retries')
      run_local = benchmark_settings.GetField('run_local')
      weight = benchmark_settings.GetField('weight')
      if weight:
        if not cwp_dso:
          raise RuntimeError('Weight can only be set when DSO specified')
        if suite != 'telemetry_Crosperf':
          raise RuntimeError('CWP approximation weight only works with '
                             'telemetry_Crosperf suite')
        if run_local:
          raise RuntimeError('run_local must be set to False to use CWP '
                             'approximation')
        if weight < 0:
          raise RuntimeError('Weight should be a float >=0')
      elif cwp_dso:
        raise RuntimeError('With DSO specified, each benchmark should have a '
                           'weight')

      if suite == 'telemetry_Crosperf':
        if test_name == 'all_perfv2':
          self.AppendBenchmarkSet(benchmarks, telemetry_perfv2_tests, test_args,
                                  iterations, rm_chroot_tmp, perf_args, suite,
                                  show_all_results, retries, run_local, cwp_dso,
                                  weight)
        elif test_name == 'all_pagecyclers':
          self.AppendBenchmarkSet(benchmarks, telemetry_pagecycler_tests,
                                  test_args, iterations, rm_chroot_tmp,
                                  perf_args, suite, show_all_results, retries,
                                  run_local, cwp_dso, weight)
        elif test_name == 'all_crosbolt_perf':
          self.AppendBenchmarkSet(
              benchmarks, telemetry_crosbolt_perf_tests, test_args, iterations,
              rm_chroot_tmp, perf_args, 'telemetry_Crosperf', show_all_results,
              retries, run_local, cwp_dso, weight)
          self.AppendBenchmarkSet(
              benchmarks,
              crosbolt_perf_tests,
              '',
              iterations,
              rm_chroot_tmp,
              perf_args,
              '',
              show_all_results,
              retries,
              run_local=False,
              cwp_dso=cwp_dso,
              weight=weight)
        elif test_name == 'all_toolchain_perf':
          self.AppendBenchmarkSet(benchmarks, telemetry_toolchain_perf_tests,
                                  test_args, iterations, rm_chroot_tmp,
                                  perf_args, suite, show_all_results, retries,
                                  run_local, cwp_dso, weight)
          # Add non-telemetry toolchain-perf benchmarks:
          benchmarks.append(
              Benchmark(
                  'graphics_WebGLAquarium',
                  'graphics_WebGLAquarium',
                  '',
                  iterations,
                  rm_chroot_tmp,
                  perf_args,
                  'crosperf_Wrapper',  # Use client wrapper in Autotest
                  show_all_results,
                  retries,
                  run_local=False,
                  cwp_dso=cwp_dso,
                  weight=weight))
        elif test_name == 'all_toolchain_perf_old':
          self.AppendBenchmarkSet(
              benchmarks, telemetry_toolchain_old_perf_tests, test_args,
              iterations, rm_chroot_tmp, perf_args, suite, show_all_results,
              retries, run_local, cwp_dso, weight)
        else:
          benchmark = Benchmark(benchmark_name, test_name, test_args,
                                iterations, rm_chroot_tmp, perf_args, suite,
                                show_all_results, retries, run_local, cwp_dso,
                                weight)
          benchmarks.append(benchmark)
      else:
        if test_name == 'all_graphics_perf':
          self.AppendBenchmarkSet(
              benchmarks,
              graphics_perf_tests,
              '',
              iterations,
              rm_chroot_tmp,
              perf_args,
              '',
              show_all_results,
              retries,
              run_local=False,
              cwp_dso=cwp_dso,
              weight=weight)
        else:
          # Add the single benchmark.
          benchmark = Benchmark(
              benchmark_name,
              test_name,
              test_args,
              iterations,
              rm_chroot_tmp,
              perf_args,
              suite,
              show_all_results,
              retries,
              run_local=False,
              cwp_dso=cwp_dso,
              weight=weight)
          benchmarks.append(benchmark)

    if not benchmarks:
      raise RuntimeError('No benchmarks specified')

    # Construct labels.
    # Some fields are common with global settings. The values are
    # inherited and/or merged with the global settings values.
    labels = []
    all_label_settings = experiment_file.GetSettings('label')
    all_remote = list(remote)
    for label_settings in all_label_settings:
      label_name = label_settings.name
      image = label_settings.GetField('chromeos_image')
      build = label_settings.GetField('build')
      autotest_path = label_settings.GetField('autotest_path')
      debug_path = label_settings.GetField('debug_path')
      chromeos_root = label_settings.GetField('chromeos_root')
      my_remote = label_settings.GetField('remote')
      compiler = label_settings.GetField('compiler')
      new_remote = []
      if my_remote:
        for i in my_remote:
          c = re.sub('["\']', '', i)
          new_remote.append(c)
      my_remote = new_remote

      if image:
        if skylab:
          raise RuntimeError('In skylab mode, local image should not be used.')
        if build:
          raise RuntimeError('Image path and build are provided at the same '
                             'time, please use only one of them.')
      else:
        if not build:
          raise RuntimeError("Can not have empty 'build' field!")
        image, autotest_path, debug_path = label_settings.GetXbuddyPath(
            build, autotest_path, debug_path, board, chromeos_root, log_level,
            download_debug)

      cache_dir = label_settings.GetField('cache_dir')
      chrome_src = label_settings.GetField('chrome_src')

      # TODO(yunlian): We should consolidate code in machine_manager.py
      # to derermine whether we are running from within google or not
      if ('corp.google.com' in socket.gethostname() and not my_remote and
          not skylab):
        my_remote = self.GetDefaultRemotes(board)
      if global_settings.GetField('same_machine') and len(my_remote) > 1:
        raise RuntimeError('Only one remote is allowed when same_machine '
                           'is turned on')
      all_remote += my_remote
      image_args = label_settings.GetField('image_args')
      if test_flag.GetTestMode():
        # pylint: disable=too-many-function-args
        label = MockLabel(label_name, build, image, autotest_path, debug_path,
                          chromeos_root, board, my_remote, image_args,
                          cache_dir, cache_only, log_level, compiler, skylab,
                          chrome_src)
      else:
        label = Label(label_name, build, image, autotest_path, debug_path,
                      chromeos_root, board, my_remote, image_args, cache_dir,
                      cache_only, log_level, compiler, skylab, chrome_src)
      labels.append(label)

    if not labels:
      raise RuntimeError('No labels specified')

    email = global_settings.GetField('email')
    all_remote += list(set(my_remote))
    all_remote = list(set(all_remote))
    if skylab:
      for remote in all_remote:
        self.CheckRemotesInSkylab(remote)
    experiment = Experiment(experiment_name, all_remote, working_directory,
                            chromeos_root, cache_conditions, labels, benchmarks,
                            experiment_file.Canonicalize(), email,
                            acquire_timeout, log_dir, log_level, share_cache,
                            results_dir, locks_dir, cwp_dso, ignore_min_max,
                            skylab, dut_config)

    return experiment

  def GetDefaultRemotes(self, board):
    default_remotes_file = os.path.join(
        os.path.dirname(__file__), 'default_remotes')
    try:
      with open(default_remotes_file) as f:
        for line in f:
          key, v = line.split(':')
          if key.strip() == board:
            remotes = v.strip().split()
            if remotes:
              return remotes
            else:
              raise RuntimeError('There is no remote for {0}'.format(board))
    except IOError:
      # TODO: rethrow instead of throwing different exception.
      raise RuntimeError(
          'IOError while reading file {0}'.format(default_remotes_file))
    else:
      raise RuntimeError('There is no remote for {0}'.format(board))

  def CheckRemotesInSkylab(self, remote):
    # TODO: (AI:zhizhouy) need to check whether a remote is a local or lab
    # machine. If not lab machine, raise an error.
    pass

  def CheckSkylabTool(self, chromeos_root, log_level):
    SKYLAB_PATH = '/usr/local/bin/skylab'
    if os.path.exists(SKYLAB_PATH):
      return True
    l = logger.GetLogger()
    l.LogOutput('Skylab tool not installed, trying to install it.')
    ce = command_executer.GetCommandExecuter(l, log_level=log_level)
    setup_lab_tools = os.path.join(chromeos_root, 'chromeos-admin', 'lab-tools',
                                   'setup_lab_tools')
    cmd = '%s' % setup_lab_tools
    status = ce.RunCommand(cmd)
    if status != 0:
      raise RuntimeError('Skylab tool not installed correctly, please try to '
                         'manually install it from %s' % setup_lab_tools)
    l.LogOutput('Skylab is installed at %s, please login before first use. '
                'Login by running "skylab login" and follow instructions.' %
                SKYLAB_PATH)
    return False
