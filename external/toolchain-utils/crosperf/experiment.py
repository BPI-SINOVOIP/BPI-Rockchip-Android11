# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The experiment setting module."""

from __future__ import print_function

import os
import time

from threading import Lock

from cros_utils import logger
from cros_utils import misc

import benchmark_run
from machine_manager import BadChecksum
from machine_manager import MachineManager
from machine_manager import MockMachineManager
import test_flag


class Experiment(object):
  """Class representing an Experiment to be run."""

  def __init__(self, name, remote, working_directory, chromeos_root,
               cache_conditions, labels, benchmarks, experiment_file, email_to,
               acquire_timeout, log_dir, log_level, share_cache,
               results_directory, locks_directory, cwp_dso, ignore_min_max,
               skylab, dut_config):
    self.name = name
    self.working_directory = working_directory
    self.remote = remote
    self.chromeos_root = chromeos_root
    self.cache_conditions = cache_conditions
    self.experiment_file = experiment_file
    self.email_to = email_to
    if not results_directory:
      self.results_directory = os.path.join(self.working_directory,
                                            self.name + '_results')
    else:
      self.results_directory = misc.CanonicalizePath(results_directory)
    self.log_dir = log_dir
    self.log_level = log_level
    self.labels = labels
    self.benchmarks = benchmarks
    self.num_complete = 0
    self.num_run_complete = 0
    self.share_cache = share_cache
    self.active_threads = []
    self.locks_dir = locks_directory
    self.locked_machines = []
    self.lock_mgr = None
    self.cwp_dso = cwp_dso
    self.ignore_min_max = ignore_min_max
    self.skylab = skylab
    self.l = logger.GetLogger(log_dir)

    if not self.benchmarks:
      raise RuntimeError('No benchmarks specified')
    if not self.labels:
      raise RuntimeError('No labels specified')
    if not remote and not self.skylab:
      raise RuntimeError('No remote hosts specified')

    # We need one chromeos_root to run the benchmarks in, but it doesn't
    # matter where it is, unless the ABIs are different.
    if not chromeos_root:
      for label in self.labels:
        if label.chromeos_root:
          chromeos_root = label.chromeos_root
          break
    if not chromeos_root:
      raise RuntimeError('No chromeos_root given and could not determine '
                         'one from the image path.')

    machine_manager_fn = MachineManager
    if test_flag.GetTestMode():
      machine_manager_fn = MockMachineManager
    self.machine_manager = machine_manager_fn(chromeos_root, acquire_timeout,
                                              log_level, locks_directory)
    self.l = logger.GetLogger(log_dir)

    for machine in self.remote:
      # machine_manager.AddMachine only adds reachable machines.
      self.machine_manager.AddMachine(machine)
    # Now machine_manager._all_machines contains a list of reachable
    # machines. This is a subset of self.remote. We make both lists the same.
    self.remote = [m.name for m in self.machine_manager.GetAllMachines()]
    if not self.remote:
      raise RuntimeError('No machine available for running experiment.')

    # Initialize checksums for all machines, ignore errors at this time.
    # The checksum will be double checked, and image will be flashed after
    # duts are locked/leased.
    self.SetCheckSums()

    self.start_time = None
    self.benchmark_runs = self._GenerateBenchmarkRuns(dut_config)

    self._schedv2 = None
    self._internal_counter_lock = Lock()

  def set_schedv2(self, schedv2):
    self._schedv2 = schedv2

  def schedv2(self):
    return self._schedv2

  def _GenerateBenchmarkRuns(self, dut_config):
    """Generate benchmark runs from labels and benchmark defintions."""
    benchmark_runs = []
    for label in self.labels:
      for benchmark in self.benchmarks:
        for iteration in range(1, benchmark.iterations + 1):

          benchmark_run_name = '%s: %s (%s)' % (label.name, benchmark.name,
                                                iteration)
          full_name = '%s_%s_%s' % (label.name, benchmark.name, iteration)
          logger_to_use = logger.Logger(self.log_dir, 'run.%s' % (full_name),
                                        True)
          benchmark_runs.append(
              benchmark_run.BenchmarkRun(
                  benchmark_run_name, benchmark, label, iteration,
                  self.cache_conditions, self.machine_manager, logger_to_use,
                  self.log_level, self.share_cache, dut_config))

    return benchmark_runs

  def SetCheckSums(self, forceSameImage=False):
    for label in self.labels:
      # We filter out label remotes that are not reachable (not in
      # self.remote). So each label.remote is a sublist of experiment.remote.
      label.remote = [r for r in label.remote if r in self.remote]
      try:
        self.machine_manager.ComputeCommonCheckSum(label)
      except BadChecksum:
        # Force same image on all machines, then we do checksum again. No
        # bailout if checksums still do not match.
        # TODO (zhizhouy): Need to figure out how flashing image will influence
        # the new checksum.
        if forceSameImage:
          self.machine_manager.ForceSameImageToAllMachines(label)
          self.machine_manager.ComputeCommonCheckSum(label)

      self.machine_manager.ComputeCommonCheckSumString(label)

  def Build(self):
    pass

  def Terminate(self):
    if self._schedv2 is not None:
      self._schedv2.terminate()
    else:
      for t in self.benchmark_runs:
        if t.isAlive():
          self.l.LogError("Terminating run: '%s'." % t.name)
          t.Terminate()

  def IsComplete(self):
    if self._schedv2:
      return self._schedv2.is_complete()
    if self.active_threads:
      for t in self.active_threads:
        if t.isAlive():
          t.join(0)
        if not t.isAlive():
          self.num_complete += 1
          if not t.cache_hit:
            self.num_run_complete += 1
          self.active_threads.remove(t)
      return False
    return True

  def BenchmarkRunFinished(self, br):
    """Update internal counters after br finishes.

    Note this is only used by schedv2 and is called by multiple threads.
    Never throw any exception here.
    """

    assert self._schedv2 is not None
    with self._internal_counter_lock:
      self.num_complete += 1
      if not br.cache_hit:
        self.num_run_complete += 1

  def Run(self):
    self.start_time = time.time()
    if self._schedv2 is not None:
      self._schedv2.run_sched()
    else:
      self.active_threads = []
      for run in self.benchmark_runs:
        # Set threads to daemon so program exits when ctrl-c is pressed.
        run.daemon = True
        run.start()
        self.active_threads.append(run)

  def SetCacheConditions(self, cache_conditions):
    for run in self.benchmark_runs:
      run.SetCacheConditions(cache_conditions)

  def Cleanup(self):
    """Make sure all machines are unlocked."""
    if self.locks_dir:
      # We are using the file locks mechanism, so call machine_manager.Cleanup
      # to unlock everything.
      self.machine_manager.Cleanup()

    if test_flag.GetTestMode() or not self.locked_machines:
      return

    # If we locked any machines earlier, make sure we unlock them now.
    if self.lock_mgr:
      machine_states = self.lock_mgr.GetMachineStates('unlock')
      self.lock_mgr.CheckMachineLocks(machine_states, 'unlock')
      unlocked_machines = self.lock_mgr.UpdateMachines(False)
      failed_machines = [
          m for m in self.locked_machines if m not in unlocked_machines
      ]
      if failed_machines:
        raise RuntimeError(
            'These machines are not unlocked correctly: %s' % failed_machines)
      self.lock_mgr = None
