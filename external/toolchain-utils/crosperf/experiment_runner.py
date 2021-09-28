# -*- coding: utf-8 -*-
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The experiment runner module."""
from __future__ import print_function

import getpass
import os
import shutil
import time

import lock_machine
import test_flag

from cros_utils import command_executer
from cros_utils import logger
from cros_utils.email_sender import EmailSender
from cros_utils.file_utils import FileUtils

import config
from experiment_status import ExperimentStatus
from results_cache import CacheConditions
from results_cache import ResultsCache
from results_report import HTMLResultsReport
from results_report import TextResultsReport
from results_report import JSONResultsReport
from schedv2 import Schedv2


def _WriteJSONReportToFile(experiment, results_dir, json_report):
  """Writes a JSON report to a file in results_dir."""
  has_llvm = any('llvm' in l.compiler for l in experiment.labels)
  compiler_string = 'llvm' if has_llvm else 'gcc'
  board = experiment.labels[0].board
  filename = 'report_%s_%s_%s.%s.json' % (board, json_report.date,
                                          json_report.time.replace(':', '.'),
                                          compiler_string)
  fullname = os.path.join(results_dir, filename)
  report_text = json_report.GetReport()
  with open(fullname, 'w') as out_file:
    out_file.write(report_text)


class ExperimentRunner(object):
  """ExperimentRunner Class."""

  STATUS_TIME_DELAY = 30
  THREAD_MONITOR_DELAY = 2

  def __init__(self,
               experiment,
               json_report,
               using_schedv2=False,
               log=None,
               cmd_exec=None):
    self._experiment = experiment
    self.l = log or logger.GetLogger(experiment.log_dir)
    self._ce = cmd_exec or command_executer.GetCommandExecuter(self.l)
    self._terminated = False
    self.json_report = json_report
    self.locked_machines = []
    if experiment.log_level != 'verbose':
      self.STATUS_TIME_DELAY = 10

    # Setting this to True will use crosperf sched v2 (feature in progress).
    self._using_schedv2 = using_schedv2

  def _GetMachineList(self):
    """Return a list of all requested machines.

    Create a list of all the requested machines, both global requests and
    label-specific requests, and return the list.
    """
    machines = self._experiment.remote
    # All Label.remote is a sublist of experiment.remote.
    for l in self._experiment.labels:
      for r in l.remote:
        assert r in machines
    return machines

  def _UpdateMachineList(self, locked_machines):
    """Update machines lists to contain only locked machines.

    Go through all the lists of requested machines, both global and
    label-specific requests, and remove any machine that we were not
    able to lock.

    Args:
      locked_machines: A list of the machines we successfully locked.
    """
    for m in self._experiment.remote:
      if m not in locked_machines:
        self._experiment.remote.remove(m)

    for l in self._experiment.labels:
      for m in l.remote:
        if m not in locked_machines:
          l.remote.remove(m)

  def _GetMachineType(self, lock_mgr, machine):
    """Get where is the machine from.

    Returns:
      The location of the machine: local or skylab
    """
    # We assume that lab machine always starts with chromeos*, and local
    # machines are ip address.
    if 'chromeos' in machine:
      if lock_mgr.CheckMachineInSkylab(machine):
        return 'skylab'
      else:
        raise RuntimeError('Lab machine not in Skylab.')
    return 'local'

  def _LockAllMachines(self, experiment):
    """Attempt to globally lock all of the machines requested for run.

    This method tries to lock all machines requested for this crosperf run
    in three different modes automatically, to prevent any other crosperf runs
    from being able to update/use the machines while this experiment is
    running:
      - Skylab machines: Use skylab lease-dut mechanism to lease
      - Local machines: Use file lock mechanism to lock
    """
    if test_flag.GetTestMode():
      self.locked_machines = self._GetMachineList()
      experiment.locked_machines = self.locked_machines
    else:
      experiment.lock_mgr = lock_machine.LockManager(
          self._GetMachineList(),
          '',
          experiment.labels[0].chromeos_root,
          experiment.locks_dir,
          log=self.l,
      )
      for m in experiment.lock_mgr.machines:
        machine_type = self._GetMachineType(experiment.lock_mgr, m)
        if machine_type == 'local':
          experiment.lock_mgr.AddMachineToLocal(m)
        elif machine_type == 'skylab':
          experiment.lock_mgr.AddMachineToSkylab(m)
      machine_states = experiment.lock_mgr.GetMachineStates('lock')
      experiment.lock_mgr.CheckMachineLocks(machine_states, 'lock')
      self.locked_machines = experiment.lock_mgr.UpdateMachines(True)
      experiment.locked_machines = self.locked_machines
      self._UpdateMachineList(self.locked_machines)
      experiment.machine_manager.RemoveNonLockedMachines(self.locked_machines)
      if not self.locked_machines:
        raise RuntimeError('Unable to lock any machines.')

  def _ClearCacheEntries(self, experiment):
    for br in experiment.benchmark_runs:
      cache = ResultsCache()
      cache.Init(
          br.label.chromeos_image, br.label.chromeos_root,
          br.benchmark.test_name, br.iteration, br.test_args, br.profiler_args,
          br.machine_manager, br.machine, br.label.board, br.cache_conditions,
          br.logger(), br.log_level, br.label, br.share_cache,
          br.benchmark.suite, br.benchmark.show_all_results,
          br.benchmark.run_local, br.benchmark.cwp_dso)
      cache_dir = cache.GetCacheDirForWrite()
      if os.path.exists(cache_dir):
        self.l.LogOutput('Removing cache dir: %s' % cache_dir)
        shutil.rmtree(cache_dir)

  def _Run(self, experiment):
    try:
      # We should not lease machines if tests are launched via `skylab
      # create-test`. This is because leasing DUT in skylab will create a
      # dummy task on the DUT and new test created will be hanging there.
      # TODO(zhizhouy): Need to check whether machine is ready or not before
      # assigning a test to it.
      if not experiment.skylab:
        self._LockAllMachines(experiment)
      # Calculate all checksums of avaiable/locked machines, to ensure same
      # label has same machines for testing
      experiment.SetCheckSums(forceSameImage=True)
      if self._using_schedv2:
        schedv2 = Schedv2(experiment)
        experiment.set_schedv2(schedv2)
      if CacheConditions.FALSE in experiment.cache_conditions:
        self._ClearCacheEntries(experiment)
      status = ExperimentStatus(experiment)
      experiment.Run()
      last_status_time = 0
      last_status_string = ''
      try:
        if experiment.log_level != 'verbose':
          self.l.LogStartDots()
        while not experiment.IsComplete():
          if last_status_time + self.STATUS_TIME_DELAY < time.time():
            last_status_time = time.time()
            border = '=============================='
            if experiment.log_level == 'verbose':
              self.l.LogOutput(border)
              self.l.LogOutput(status.GetProgressString())
              self.l.LogOutput(status.GetStatusString())
              self.l.LogOutput(border)
            else:
              current_status_string = status.GetStatusString()
              if current_status_string != last_status_string:
                self.l.LogEndDots()
                self.l.LogOutput(border)
                self.l.LogOutput(current_status_string)
                self.l.LogOutput(border)
                last_status_string = current_status_string
              else:
                self.l.LogAppendDot()
          time.sleep(self.THREAD_MONITOR_DELAY)
      except KeyboardInterrupt:
        self._terminated = True
        self.l.LogError('Ctrl-c pressed. Cleaning up...')
        experiment.Terminate()
        raise
      except SystemExit:
        self._terminated = True
        self.l.LogError('Unexpected exit. Cleaning up...')
        experiment.Terminate()
        raise
    finally:
      experiment.Cleanup()

  def _PrintTable(self, experiment):
    self.l.LogOutput(TextResultsReport.FromExperiment(experiment).GetReport())

  def _Email(self, experiment):
    # Only email by default if a new run was completed.
    send_mail = False
    for benchmark_run in experiment.benchmark_runs:
      if not benchmark_run.cache_hit:
        send_mail = True
        break
    if (not send_mail and not experiment.email_to or
        config.GetConfig('no_email')):
      return

    label_names = []
    for label in experiment.labels:
      label_names.append(label.name)
    subject = '%s: %s' % (experiment.name, ' vs. '.join(label_names))

    text_report = TextResultsReport.FromExperiment(experiment, True).GetReport()
    text_report += (
        '\nResults are stored in %s.\n' % experiment.results_directory)
    text_report = "<pre style='font-size: 13px'>%s</pre>" % text_report
    html_report = HTMLResultsReport.FromExperiment(experiment).GetReport()
    attachment = EmailSender.Attachment('report.html', html_report)
    email_to = experiment.email_to or []
    email_to.append(getpass.getuser())
    EmailSender().SendEmail(
        email_to,
        subject,
        text_report,
        attachments=[attachment],
        msg_type='html')

  def _StoreResults(self, experiment):
    if self._terminated:
      return
    results_directory = experiment.results_directory
    FileUtils().RmDir(results_directory)
    FileUtils().MkDirP(results_directory)
    self.l.LogOutput('Storing experiment file in %s.' % results_directory)
    experiment_file_path = os.path.join(results_directory, 'experiment.exp')
    FileUtils().WriteFile(experiment_file_path, experiment.experiment_file)

    self.l.LogOutput('Storing results report in %s.' % results_directory)
    results_table_path = os.path.join(results_directory, 'results.html')
    report = HTMLResultsReport.FromExperiment(experiment).GetReport()
    if self.json_report:
      json_report = JSONResultsReport.FromExperiment(
          experiment, json_args={'indent': 2})
      _WriteJSONReportToFile(experiment, results_directory, json_report)

    FileUtils().WriteFile(results_table_path, report)

    self.l.LogOutput('Storing email message body in %s.' % results_directory)
    msg_file_path = os.path.join(results_directory, 'msg_body.html')
    text_report = TextResultsReport.FromExperiment(experiment, True).GetReport()
    text_report += (
        '\nResults are stored in %s.\n' % experiment.results_directory)
    msg_body = "<pre style='font-size: 13px'>%s</pre>" % text_report
    FileUtils().WriteFile(msg_file_path, msg_body)

    self.l.LogOutput('Storing results of each benchmark run.')
    for benchmark_run in experiment.benchmark_runs:
      if benchmark_run.result:
        benchmark_run_name = ''.join(
            ch for ch in benchmark_run.name if ch.isalnum())
        benchmark_run_path = os.path.join(results_directory, benchmark_run_name)
        benchmark_run.result.CopyResultsTo(benchmark_run_path)
        benchmark_run.result.CleanUp(benchmark_run.benchmark.rm_chroot_tmp)

    topstats_file = os.path.join(results_directory, 'topstats.log')
    self.l.LogOutput('Storing top5 statistics of each benchmark run into %s.' %
                     topstats_file)
    with open(topstats_file, 'w') as top_fd:
      for benchmark_run in experiment.benchmark_runs:
        if benchmark_run.result:
          # Header with benchmark run name.
          top_fd.write('%s\n' % str(benchmark_run))
          # Formatted string with top statistics.
          top_fd.write(benchmark_run.result.FormatStringTop5())
          top_fd.write('\n\n')

  def Run(self):
    try:
      self._Run(self._experiment)
    finally:
      # Always print the report at the end of the run.
      self._PrintTable(self._experiment)
      if not self._terminated:
        self._StoreResults(self._experiment)
        self._Email(self._experiment)


class MockExperimentRunner(ExperimentRunner):
  """Mocked ExperimentRunner for testing."""

  def __init__(self, experiment, json_report):
    super(MockExperimentRunner, self).__init__(experiment, json_report)

  def _Run(self, experiment):
    self.l.LogOutput(
        "Would run the following experiment: '%s'." % experiment.name)

  def _PrintTable(self, experiment):
    self.l.LogOutput('Would print the experiment table.')

  def _Email(self, experiment):
    self.l.LogOutput('Would send result email.')

  def _StoreResults(self, experiment):
    self.l.LogOutput('Would store the results.')
