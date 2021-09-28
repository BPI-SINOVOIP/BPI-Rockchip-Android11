# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to deal with result cache."""

from __future__ import division
from __future__ import print_function

import collections
import glob
import hashlib
import heapq
import json
import os
import pickle
import re
import tempfile

from cros_utils import command_executer
from cros_utils import misc

from image_checksummer import ImageChecksummer

import results_report
import test_flag

SCRATCH_DIR = os.path.expanduser('~/cros_scratch')
RESULTS_FILE = 'results.txt'
MACHINE_FILE = 'machine.txt'
AUTOTEST_TARBALL = 'autotest.tbz2'
PERF_RESULTS_FILE = 'perf-results.txt'
CACHE_KEYS_FILE = 'cache_keys.txt'


class Result(object):
  """Class for holding the results of a single test run.

  This class manages what exactly is stored inside the cache without knowing
  what the key of the cache is. For runs with perf, it stores perf.data,
  perf.report, etc. The key generation is handled by the ResultsCache class.
  """

  def __init__(self, logger, label, log_level, machine, cmd_exec=None):
    self.chromeos_root = label.chromeos_root
    self._logger = logger
    self.ce = cmd_exec or command_executer.GetCommandExecuter(
        self._logger, log_level=log_level)
    self.temp_dir = None
    self.label = label
    self.results_dir = None
    self.log_level = log_level
    self.machine = machine
    self.perf_data_files = []
    self.perf_report_files = []
    self.results_file = []
    self.turbostat_log_file = ''
    self.cpustats_log_file = ''
    self.top_log_file = ''
    self.wait_time_log_file = ''
    self.chrome_version = ''
    self.err = None
    self.chroot_results_dir = ''
    self.test_name = ''
    self.keyvals = None
    self.board = None
    self.suite = None
    self.cwp_dso = ''
    self.retval = None
    self.out = None
    self.top_cmds = []

  def GetTopCmds(self):
    """Get the list of top commands consuming CPU on the machine."""
    return self.top_cmds

  def FormatStringTop5(self):
    """Get formatted top5 string.

    Get the formatted string with top5 commands consuming CPU on DUT machine.
    """
    format_list = [
        'Top 5 commands with highest CPU usage:',
        # Header.
        '%20s %9s %6s   %s' % ('COMMAND', 'AVG CPU%', 'COUNT', 'HIGHEST 5'),
        '-' * 50,
    ]
    if self.top_cmds:
      for topcmd in self.top_cmds[:5]:
        print_line = '%20s %9.2f %6s   %s' % (topcmd['cmd'], topcmd['cpu_avg'],
                                              topcmd['count'], topcmd['top5'])
        format_list.append(print_line)
    else:
      format_list.append('[NO DATA FROM THE TOP LOG]')
    format_list.append('-' * 50)
    return '\n'.join(format_list)

  def CopyFilesTo(self, dest_dir, files_to_copy):
    file_index = 0
    for file_to_copy in files_to_copy:
      if not os.path.isdir(dest_dir):
        command = 'mkdir -p %s' % dest_dir
        self.ce.RunCommand(command)
      dest_file = os.path.join(
          dest_dir, ('%s.%s' % (os.path.basename(file_to_copy), file_index)))
      ret = self.ce.CopyFiles(file_to_copy, dest_file, recursive=False)
      if ret:
        raise IOError('Could not copy results file: %s' % file_to_copy)

  def CopyResultsTo(self, dest_dir):
    self.CopyFilesTo(dest_dir, self.perf_data_files)
    self.CopyFilesTo(dest_dir, self.perf_report_files)
    if self.perf_data_files or self.perf_report_files:
      self._logger.LogOutput('Perf results files stored in %s.' % dest_dir)

  def GetNewKeyvals(self, keyvals_dict):
    # Initialize 'units' dictionary.
    units_dict = {}
    for k in keyvals_dict:
      units_dict[k] = ''
    results_files = self.GetDataMeasurementsFiles()
    for f in results_files:
      # Make sure we can find the results file
      if os.path.exists(f):
        data_filename = f
      else:
        # Otherwise get the base filename and create the correct
        # path for it.
        _, f_base = misc.GetRoot(f)
        data_filename = os.path.join(self.chromeos_root, 'chroot/tmp',
                                     self.temp_dir, f_base)
      if data_filename.find('.json') > 0:
        raw_dict = dict()
        if os.path.exists(data_filename):
          with open(data_filename, 'r') as data_file:
            raw_dict = json.load(data_file)

        if 'charts' in raw_dict:
          raw_dict = raw_dict['charts']
        for k1 in raw_dict:
          field_dict = raw_dict[k1]
          for k2 in field_dict:
            result_dict = field_dict[k2]
            key = k1 + '__' + k2
            if 'value' in result_dict:
              keyvals_dict[key] = result_dict['value']
            elif 'values' in result_dict:
              values = result_dict['values']
              if ('type' in result_dict and
                  result_dict['type'] == 'list_of_scalar_values' and values and
                  values != 'null'):
                keyvals_dict[key] = sum(values) / float(len(values))
              else:
                keyvals_dict[key] = values
            units_dict[key] = result_dict['units']
      else:
        if os.path.exists(data_filename):
          with open(data_filename, 'r') as data_file:
            lines = data_file.readlines()
            for line in lines:
              tmp_dict = json.loads(line)
              graph_name = tmp_dict['graph']
              graph_str = (graph_name + '__') if graph_name else ''
              key = graph_str + tmp_dict['description']
              keyvals_dict[key] = tmp_dict['value']
              units_dict[key] = tmp_dict['units']

    return keyvals_dict, units_dict

  def AppendTelemetryUnits(self, keyvals_dict, units_dict):
    """keyvals_dict is the dict of key-value used to generate Crosperf reports.

    units_dict is a dictionary of the units for the return values in
    keyvals_dict.  We need to associate the units with the return values,
    for Telemetry tests, so that we can include the units in the reports.
    This function takes each value in keyvals_dict, finds the corresponding
    unit in the units_dict, and replaces the old value with a list of the
    old value and the units.  This later gets properly parsed in the
    ResultOrganizer class, for generating the reports.
    """

    results_dict = {}
    for k in keyvals_dict:
      # We don't want these lines in our reports; they add no useful data.
      if not k or k == 'telemetry_Crosperf':
        continue
      val = keyvals_dict[k]
      units = units_dict[k]
      new_val = [val, units]
      results_dict[k] = new_val
    return results_dict

  def GetKeyvals(self):
    results_in_chroot = os.path.join(self.chromeos_root, 'chroot', 'tmp')
    if not self.temp_dir:
      self.temp_dir = tempfile.mkdtemp(dir=results_in_chroot)
      command = 'cp -r {0}/* {1}'.format(self.results_dir, self.temp_dir)
      self.ce.RunCommand(command, print_to_console=False)

    command = ('./generate_test_report --no-color --csv %s' % (os.path.join(
        '/tmp', os.path.basename(self.temp_dir))))
    _, out, _ = self.ce.ChrootRunCommandWOutput(
        self.chromeos_root, command, print_to_console=False)
    keyvals_dict = {}
    tmp_dir_in_chroot = misc.GetInsideChrootPath(self.chromeos_root,
                                                 self.temp_dir)
    for line in out.splitlines():
      tokens = re.split('=|,', line)
      key = tokens[-2]
      if key.startswith(tmp_dir_in_chroot):
        key = key[len(tmp_dir_in_chroot) + 1:]
      value = tokens[-1]
      keyvals_dict[key] = value

    # Check to see if there is a perf_measurements file and get the
    # data from it if so.
    keyvals_dict, units_dict = self.GetNewKeyvals(keyvals_dict)
    if self.suite == 'telemetry_Crosperf':
      # For telemtry_Crosperf results, append the units to the return
      # results, for use in generating the reports.
      keyvals_dict = self.AppendTelemetryUnits(keyvals_dict, units_dict)
    return keyvals_dict

  def GetSamples(self):
    samples = 0
    for perf_data_file in self.perf_data_files:
      chroot_perf_data_file = misc.GetInsideChrootPath(self.chromeos_root,
                                                       perf_data_file)
      perf_path = os.path.join(self.chromeos_root, 'chroot', 'usr/bin/perf')
      perf_file = '/usr/sbin/perf'
      if os.path.exists(perf_path):
        perf_file = '/usr/bin/perf'

      # For each perf.data, we want to collect sample count for specific DSO.
      # We specify exact match for known DSO type, and every sample for `all`.
      exact_match = ''
      if self.cwp_dso == 'all':
        exact_match = '""'
      elif self.cwp_dso == 'chrome':
        exact_match = '" chrome "'
      elif self.cwp_dso == 'kallsyms':
        exact_match = '"[kernel.kallsyms]"'
      else:
        # This will need to be updated once there are more DSO types supported,
        # if user want an exact match for the field they want.
        exact_match = '"%s"' % self.cwp_dso

      command = ('%s report -n -s dso -i %s 2> /dev/null | grep %s' %
                 (perf_file, chroot_perf_data_file, exact_match))
      _, result, _ = self.ce.ChrootRunCommandWOutput(self.chromeos_root,
                                                     command)
      # Accumulate the sample count for all matched fields.
      # Each line looks like this:
      #     45.42%        237210  chrome
      # And we want the second number which is the sample count.
      sample = 0
      try:
        for line in result.split('\n'):
          attr = line.split()
          if len(attr) == 3 and '%' in attr[0]:
            sample += int(attr[1])
      except:
        raise RuntimeError('Cannot parse perf dso result')

      samples += sample
    return [samples, u'samples']

  def GetResultsDir(self):
    mo = re.search(r'Results placed in (\S+)', self.out)
    if mo:
      result = mo.group(1)
      return result
    raise RuntimeError('Could not find results directory.')

  def FindFilesInResultsDir(self, find_args):
    if not self.results_dir:
      return ''

    command = 'find %s %s' % (self.results_dir, find_args)
    ret, out, _ = self.ce.RunCommandWOutput(command, print_to_console=False)
    if ret:
      raise RuntimeError('Could not run find command!')
    return out

  def GetResultsFile(self):
    if self.suite == 'telemetry_Crosperf':
      return self.FindFilesInResultsDir('-name histograms.json').splitlines()
    return self.FindFilesInResultsDir('-name results-chart.json').splitlines()

  def GetPerfDataFiles(self):
    return self.FindFilesInResultsDir('-name perf.data').splitlines()

  def GetPerfReportFiles(self):
    return self.FindFilesInResultsDir('-name perf.data.report').splitlines()

  def GetDataMeasurementsFiles(self):
    result = self.FindFilesInResultsDir('-name perf_measurements').splitlines()
    if not result:
      if self.suite == 'telemetry_Crosperf':
        result = \
            self.FindFilesInResultsDir('-name histograms.json').splitlines()
      else:
        result = \
            self.FindFilesInResultsDir('-name results-chart.json').splitlines()
    return result

  def GetTurbostatFile(self):
    """Get turbostat log path string."""
    return self.FindFilesInResultsDir('-name turbostat.log').split('\n')[0]

  def GetCpustatsFile(self):
    """Get cpustats log path string."""
    return self.FindFilesInResultsDir('-name cpustats.log').split('\n')[0]

  def GetTopFile(self):
    """Get cpustats log path string."""
    return self.FindFilesInResultsDir('-name top.log').split('\n')[0]

  def GetWaitTimeFile(self):
    """Get wait time log path string."""
    return self.FindFilesInResultsDir('-name wait_time.log').split('\n')[0]

  def _CheckDebugPath(self, option, path):
    relative_path = path[1:]
    out_chroot_path = os.path.join(self.chromeos_root, 'chroot', relative_path)
    if os.path.exists(out_chroot_path):
      if option == 'kallsyms':
        path = os.path.join(path, 'System.map-*')
      return '--' + option + ' ' + path
    else:
      print('** WARNING **: --%s option not applied, %s does not exist' %
            (option, out_chroot_path))
      return ''

  def GeneratePerfReportFiles(self):
    perf_report_files = []
    for perf_data_file in self.perf_data_files:
      # Generate a perf.report and store it side-by-side with the perf.data
      # file.
      chroot_perf_data_file = misc.GetInsideChrootPath(self.chromeos_root,
                                                       perf_data_file)
      perf_report_file = '%s.report' % perf_data_file
      if os.path.exists(perf_report_file):
        raise RuntimeError(
            'Perf report file already exists: %s' % perf_report_file)
      chroot_perf_report_file = misc.GetInsideChrootPath(
          self.chromeos_root, perf_report_file)
      perf_path = os.path.join(self.chromeos_root, 'chroot', 'usr/bin/perf')

      perf_file = '/usr/sbin/perf'
      if os.path.exists(perf_path):
        perf_file = '/usr/bin/perf'

      debug_path = self.label.debug_path

      if debug_path:
        symfs = '--symfs ' + debug_path
        vmlinux = '--vmlinux ' + os.path.join(debug_path, 'boot', 'vmlinux')
        kallsyms = ''
        print('** WARNING **: --kallsyms option not applied, no System.map-* '
              'for downloaded image.')
      else:
        if self.label.image_type != 'local':
          print('** WARNING **: Using local debug info in /build, this may '
                'not match the downloaded image.')
        build_path = os.path.join('/build', self.board)
        symfs = self._CheckDebugPath('symfs', build_path)
        vmlinux_path = os.path.join(build_path, 'usr/lib/debug/boot/vmlinux')
        vmlinux = self._CheckDebugPath('vmlinux', vmlinux_path)
        kallsyms_path = os.path.join(build_path, 'boot')
        kallsyms = self._CheckDebugPath('kallsyms', kallsyms_path)

      command = ('%s report -n %s %s %s -i %s --stdio > %s' %
                 (perf_file, symfs, vmlinux, kallsyms, chroot_perf_data_file,
                  chroot_perf_report_file))
      if self.log_level != 'verbose':
        self._logger.LogOutput('Generating perf report...\nCMD: %s' % command)
      exit_code = self.ce.ChrootRunCommand(self.chromeos_root, command)
      if exit_code == 0:
        if self.log_level != 'verbose':
          self._logger.LogOutput('Perf report generated successfully.')
      else:
        raise RuntimeError(
            'Perf report not generated correctly. CMD: %s' % command)

      # Add a keyval to the dictionary for the events captured.
      perf_report_files.append(
          misc.GetOutsideChrootPath(self.chromeos_root,
                                    chroot_perf_report_file))
    return perf_report_files

  def GatherPerfResults(self):
    report_id = 0
    for perf_report_file in self.perf_report_files:
      with open(perf_report_file, 'r') as f:
        report_contents = f.read()
        for group in re.findall(r'Events: (\S+) (\S+)', report_contents):
          num_events = group[0]
          event_name = group[1]
          key = 'perf_%s_%s' % (report_id, event_name)
          value = str(misc.UnitToNumber(num_events))
          self.keyvals[key] = value

  def PopulateFromRun(self, out, err, retval, test, suite, cwp_dso):
    self.board = self.label.board
    self.out = out
    self.err = err
    self.retval = retval
    self.test_name = test
    self.suite = suite
    self.cwp_dso = cwp_dso
    self.chroot_results_dir = self.GetResultsDir()
    self.results_dir = misc.GetOutsideChrootPath(self.chromeos_root,
                                                 self.chroot_results_dir)
    self.results_file = self.GetResultsFile()
    self.perf_data_files = self.GetPerfDataFiles()
    # Include all perf.report data in table.
    self.perf_report_files = self.GeneratePerfReportFiles()
    self.turbostat_log_file = self.GetTurbostatFile()
    self.cpustats_log_file = self.GetCpustatsFile()
    self.top_log_file = self.GetTopFile()
    self.wait_time_log_file = self.GetWaitTimeFile()
    # TODO(asharif): Do something similar with perf stat.

    # Grab keyvals from the directory.
    self.ProcessResults()

  def ProcessChartResults(self):
    # Open and parse the json results file generated by telemetry/test_that.
    if not self.results_file:
      raise IOError('No results file found.')
    filename = self.results_file[0]
    if not filename.endswith('.json'):
      raise IOError('Attempt to call json on non-json file: %s' % filename)
    if not os.path.exists(filename):
      raise IOError('%s does not exist' % filename)

    keyvals = {}
    with open(filename, 'r') as f:
      raw_dict = json.load(f)
      if 'charts' in raw_dict:
        raw_dict = raw_dict['charts']
      for k, field_dict in raw_dict.items():
        for item in field_dict:
          keyname = k + '__' + item
          value_dict = field_dict[item]
          if 'value' in value_dict:
            result = value_dict['value']
          elif 'values' in value_dict:
            values = value_dict['values']
            if not values:
              continue
            if ('type' in value_dict and
                value_dict['type'] == 'list_of_scalar_values' and
                values != 'null'):
              result = sum(values) / float(len(values))
            else:
              result = values
          else:
            continue
          units = value_dict['units']
          new_value = [result, units]
          keyvals[keyname] = new_value
    return keyvals

  def ProcessTurbostatResults(self):
    """Given turbostat_log_file non-null parse cpu stats from file.

    Returns:
      Dictionary of 'cpufreq', 'cputemp' where each
      includes dictionary 'all': [list_of_values]

    Example of the output of turbostat_log.
    ----------------------
    CPU     Avg_MHz Busy%   Bzy_MHz TSC_MHz IRQ     CoreTmp
    -       329     12.13   2723    2393    10975   77
    0       336     12.41   2715    2393    6328    77
    2       323     11.86   2731    2393    4647    69
    CPU     Avg_MHz Busy%   Bzy_MHz TSC_MHz IRQ     CoreTmp
    -       1940    67.46   2884    2393    39920   83
    0       1827    63.70   2877    2393    21184   83
    """
    cpustats = {}
    read_data = ''
    with open(self.turbostat_log_file) as f:
      read_data = f.readlines()

    if not read_data:
      self._logger.LogOutput('WARNING: Turbostat output file is empty.')
      return {}

    # First line always contains the header.
    stats = read_data[0].split()

    # Mandatory parameters.
    if 'CPU' not in stats:
      self._logger.LogOutput(
          'WARNING: Missing data for CPU# in Turbostat output.')
      return {}
    if 'Bzy_MHz' not in stats:
      self._logger.LogOutput(
          'WARNING: Missing data for Bzy_MHz in Turbostat output.')
      return {}
    cpu_index = stats.index('CPU')
    cpufreq_index = stats.index('Bzy_MHz')
    cpufreq = cpustats.setdefault('cpufreq', {'all': []})

    # Optional parameters.
    cputemp_index = -1
    if 'CoreTmp' in stats:
      cputemp_index = stats.index('CoreTmp')
      cputemp = cpustats.setdefault('cputemp', {'all': []})

    # Parse data starting from the second line ignoring repeating headers.
    for st in read_data[1:]:
      # Data represented by int or float separated by spaces.
      numbers = st.split()
      if not all(word.replace('.', '', 1).isdigit() for word in numbers[1:]):
        # Skip the line if data mismatch.
        continue
      if numbers[cpu_index] != '-':
        # Ignore Core-specific statistics which starts with Core number.
        # Combined statistics for all core has "-" CPU identifier.
        continue

      cpufreq['all'].append(int(numbers[cpufreq_index]))
      if cputemp_index != -1:
        cputemp['all'].append(int(numbers[cputemp_index]))
    return cpustats

  def ProcessTopResults(self):
    """Given self.top_log_file process top log data.

    Returns:
      List of dictionaries with the following keyvals:
       'cmd': command name (string),
       'cpu_avg': average cpu usage (float),
       'count': number of occurrences (int),
       'top5': up to 5 highest cpu usages (descending list of floats)

    Example of the top log:
      PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
     4102 chronos   12  -8 3454472 238300 118188 R  41.8   6.1   0:08.37 chrome
      375 root       0 -20       0      0      0 S   5.9   0.0   0:00.17 kworker
      617 syslog    20   0   25332   8372   7888 S   5.9   0.2   0:00.77 systemd

      PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
     5745 chronos   20   0 5438580 139328  67988 R 122.8   3.6   0:04.26 chrome
      912 root     -51   0       0      0      0 S   2.0   0.0   0:01.04 irq/cro
      121 root      20   0       0      0      0 S   1.0   0.0   0:00.45 spi5
    """
    all_data = ''
    with open(self.top_log_file) as f:
      all_data = f.read()

    if not all_data:
      self._logger.LogOutput('WARNING: Top log file is empty.')
      return []

    top_line_regex = re.compile(
        r"""
        ^\s*(?P<pid>\d+)\s+         # Group 1: PID
        \S+\s+\S+\s+-?\d+\s+        # Ignore: user, prio, nice
        \d+\s+\d+\s+\d+\s+          # Ignore: virt/res/shared mem
        \S+\s+                      # Ignore: state
        (?P<cpu_use>\d+\.\d+)\s+    # Group 2: CPU usage
        \d+\.\d+\s+\d+:\d+\.\d+\s+  # Ignore: mem usage, time
        (?P<cmd>\S+)$               # Group 3: command
        """, re.VERBOSE)
    # Page represents top log data per one measurement within time interval
    # 'top_interval'.
    # Pages separated by empty line.
    pages = all_data.split('\n\n')
    # Snapshots are structured representation of the pages.
    snapshots = []
    for page in pages:
      if not page:
        continue

      # Snapshot list will contain all processes (command duplicates are
      # allowed).
      snapshot = []
      for line in page.splitlines():
        match = top_line_regex.match(line)
        if match:
          # Top line is valid, collect data.
          process = {
              # NOTE: One command may be represented by multiple processes.
              'cmd': match.group('cmd'),
              'pid': int(match.group('pid')),
              'cpu_use': float(match.group('cpu_use')),
          }

          # Filter out processes with 0 CPU usage and top command.
          if process['cpu_use'] > 0 and process['cmd'] != 'top':
            snapshot.append(process)

      # If page contained meaningful data add snapshot to the list.
      if snapshot:
        snapshots.append(snapshot)

    # Define threshold of CPU usage when Chrome is busy, i.e. benchmark is
    # running.
    # Ideally it should be 100% but it will be hardly reachable with 1 core.
    # Statistics on DUT with 2-6 cores shows that chrome load of 100%, 95% and
    # 90% equally occurs in 72-74% of all top log snapshots.
    # Further decreasing of load threshold leads to a shifting percent of
    # "high load" snapshots which might include snapshots when benchmark is
    # not running.
    # On 1-core DUT 90% chrome cpu load occurs in 55%, 95% in 33% and 100% in 2%
    # of snapshots accordingly.
    CHROME_HIGH_CPU_LOAD = 90
    # Number of snapshots where chrome is heavily used.
    high_load_snapshots = 0
    # Total CPU use per process in ALL active snapshots.
    cmd_total_cpu_use = collections.defaultdict(float)
    # Top CPU usages per command.
    cmd_top5_cpu_use = collections.defaultdict(list)
    # List of Top Commands to be returned.
    topcmds = []

    for snapshot_processes in snapshots:
      # CPU usage per command in one snapshot.
      cmd_cpu_use_per_snapshot = collections.defaultdict(float)
      for process in snapshot_processes:
        cmd = process['cmd']
        cpu_use = process['cpu_use']

        # Collect CPU usage per command.
        cmd_cpu_use_per_snapshot[cmd] += cpu_use

      if cmd_cpu_use_per_snapshot.setdefault('chrome',
                                             0.0) > CHROME_HIGH_CPU_LOAD:
        # Combined CPU usage of "chrome" command exceeds "High load" threshold
        # which means DUT is busy running a benchmark.
        high_load_snapshots += 1
        for cmd, cpu_use in cmd_cpu_use_per_snapshot.items():
          # Update total CPU usage.
          cmd_total_cpu_use[cmd] += cpu_use

          # Add cpu_use into command top cpu usages, sorted in descending order.
          heapq.heappush(cmd_top5_cpu_use[cmd], round(cpu_use, 1))

    for consumer, usage in sorted(
        cmd_total_cpu_use.items(), key=lambda x: x[1], reverse=True):
      # Iterate through commands by descending order of total CPU usage.
      topcmd = {
          'cmd': consumer,
          'cpu_avg': usage / high_load_snapshots,
          'count': len(cmd_top5_cpu_use[consumer]),
          'top5': heapq.nlargest(5, cmd_top5_cpu_use[consumer]),
      }
      topcmds.append(topcmd)

    return topcmds

  def ProcessCpustatsResults(self):
    """Given cpustats_log_file non-null parse cpu data from file.

    Returns:
      Dictionary of 'cpufreq', 'cputemp' where each
      includes dictionary of parameter: [list_of_values]

    Example of cpustats.log output.
    ----------------------
    /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq 1512000
    /sys/devices/system/cpu/cpu2/cpufreq/cpuinfo_cur_freq 2016000
    little-cpu 41234
    big-cpu 51234

    If cores share the same policy their frequencies may always match
    on some devices.
    To make report concise we should eliminate redundancy in the output.
    Function removes cpuN data if it duplicates data from other cores.
    """

    cpustats = {}
    read_data = ''
    with open(self.cpustats_log_file) as f:
      read_data = f.readlines()

    if not read_data:
      self._logger.LogOutput('WARNING: Cpustats output file is empty.')
      return {}

    cpufreq_regex = re.compile(r'^[/\S]+/(cpu\d+)/[/\S]+\s+(\d+)$')
    cputemp_regex = re.compile(r'^([^/\s]+)\s+(\d+)$')

    for st in read_data:
      match = cpufreq_regex.match(st)
      if match:
        cpu = match.group(1)
        # CPU frequency comes in kHz.
        freq_khz = int(match.group(2))
        freq_mhz = freq_khz / 1000
        # cpufreq represents a dictionary with CPU frequency-related
        # data from cpustats.log.
        cpufreq = cpustats.setdefault('cpufreq', {})
        cpu_n_freq = cpufreq.setdefault(cpu, [])
        cpu_n_freq.append(freq_mhz)
      else:
        match = cputemp_regex.match(st)
        if match:
          therm_type = match.group(1)
          # The value is int, uCelsius unit.
          temp_uc = float(match.group(2))
          # Round to XX.X float.
          temp_c = round(temp_uc / 1000, 1)
          # cputemp represents a dictionary with temperature measurements
          # from cpustats.log.
          cputemp = cpustats.setdefault('cputemp', {})
          therm_type = cputemp.setdefault(therm_type, [])
          therm_type.append(temp_c)

    # Remove duplicate statistics from cpustats.
    pruned_stats = {}
    for cpukey, cpuparam in cpustats.items():
      # Copy 'cpufreq' and 'cputemp'.
      pruned_params = pruned_stats.setdefault(cpukey, {})
      for paramkey, paramvalue in sorted(cpuparam.items()):
        # paramvalue is list of all measured data.
        if paramvalue not in pruned_params.values():
          pruned_params[paramkey] = paramvalue

    return pruned_stats

  def ProcessHistogramsResults(self):
    # Open and parse the json results file generated by telemetry/test_that.
    if not self.results_file:
      raise IOError('No results file found.')
    filename = self.results_file[0]
    if not filename.endswith('.json'):
      raise IOError('Attempt to call json on non-json file: %s' % filename)
    if not os.path.exists(filename):
      raise IOError('%s does not exist' % filename)

    keyvals = {}
    with open(filename) as f:
      histograms = json.load(f)
      value_map = {}
      # Gets generic set values.
      for obj in histograms:
        if 'type' in obj and obj['type'] == 'GenericSet':
          value_map[obj['guid']] = obj['values']

      for obj in histograms:
        if 'name' not in obj or 'sampleValues' not in obj:
          continue
        metric_name = obj['name']
        vals = obj['sampleValues']
        if isinstance(vals, list):
          # Remove None elements from the list
          vals = [val for val in vals if val is not None]
          if vals:
            result = float(sum(vals)) / len(vals)
          else:
            result = 0
        else:
          result = vals
        unit = obj['unit']
        diagnostics = obj['diagnostics']
        # for summaries of benchmarks
        key = metric_name
        if key not in keyvals:
          keyvals[key] = [[result], unit]
        else:
          keyvals[key][0].append(result)
        # TODO: do we need summaries of stories?
        # for summaries of story tags
        if 'storyTags' in diagnostics:
          guid = diagnostics['storyTags']
          if guid not in value_map:
            raise RuntimeError('Unrecognized storyTags in %s ' % (obj))
          for story_tag in value_map[guid]:
            key = metric_name + '__' + story_tag
            if key not in keyvals:
              keyvals[key] = [[result], unit]
            else:
              keyvals[key][0].append(result)
    # calculate summary
    for key in keyvals:
      vals = keyvals[key][0]
      unit = keyvals[key][1]
      result = float(sum(vals)) / len(vals)
      keyvals[key] = [result, unit]
    return keyvals

  def ProcessResults(self, use_cache=False):
    # Note that this function doesn't know anything about whether there is a
    # cache hit or miss. It should process results agnostic of the cache hit
    # state.
    if (self.results_file and self.suite == 'telemetry_Crosperf' and
        'histograms.json' in self.results_file[0]):
      self.keyvals = self.ProcessHistogramsResults()
    elif (self.results_file and self.suite != 'telemetry_Crosperf' and
          'results-chart.json' in self.results_file[0]):
      self.keyvals = self.ProcessChartResults()
    else:
      if not use_cache:
        print('\n ** WARNING **: Had to use deprecated output-method to '
              'collect results.\n')
      self.keyvals = self.GetKeyvals()
    self.keyvals['retval'] = self.retval
    # If we are in CWP approximation mode, we want to collect DSO samples
    # for each perf.data file
    if self.cwp_dso and self.retval == 0:
      self.keyvals['samples'] = self.GetSamples()
      # If the samples count collected from perf file is 0, we will treat
      # it as a failed run.
      if self.keyvals['samples'][0] == 0:
        del self.keyvals['samples']
        self.keyvals['retval'] = 1
    # Generate report from all perf.data files.
    # Now parse all perf report files and include them in keyvals.
    self.GatherPerfResults()

    cpustats = {}
    # Turbostat output has higher priority of processing.
    if self.turbostat_log_file:
      cpustats = self.ProcessTurbostatResults()
    # Process cpustats output only if turbostat has no data.
    if not cpustats and self.cpustats_log_file:
      cpustats = self.ProcessCpustatsResults()
    if self.top_log_file:
      self.top_cmds = self.ProcessTopResults()
    if self.wait_time_log_file:
      with open(self.wait_time_log_file) as f:
        wait_time = f.readline().strip()
        try:
          wait_time = float(wait_time)
        except ValueError:
          raise ValueError('Wait time in log file is not a number.')
      # This is for accumulating wait time for telemtry_Crosperf runs only,
      # for test_that runs, please refer to suite_runner.
      self.machine.AddCooldownWaitTime(wait_time)

    for param_key, param in cpustats.items():
      for param_type, param_values in param.items():
        val_avg = sum(param_values) / len(param_values)
        val_min = min(param_values)
        val_max = max(param_values)
        # Average data is always included.
        self.keyvals['_'.join([param_key, param_type, 'avg'])] = val_avg
        # Insert min/max results only if they deviate
        # from average.
        if val_min != val_avg:
          self.keyvals['_'.join([param_key, param_type, 'min'])] = val_min
        if val_max != val_avg:
          self.keyvals['_'.join([param_key, param_type, 'max'])] = val_max

  def GetChromeVersionFromCache(self, cache_dir):
    # Read chrome_version from keys file, if present.
    chrome_version = ''
    keys_file = os.path.join(cache_dir, CACHE_KEYS_FILE)
    if os.path.exists(keys_file):
      with open(keys_file, 'r') as f:
        lines = f.readlines()
        for l in lines:
          if l.startswith('Google Chrome '):
            chrome_version = l
            if chrome_version.endswith('\n'):
              chrome_version = chrome_version[:-1]
            break
    return chrome_version

  def PopulateFromCacheDir(self, cache_dir, test, suite, cwp_dso):
    self.test_name = test
    self.suite = suite
    self.cwp_dso = cwp_dso
    # Read in everything from the cache directory.
    with open(os.path.join(cache_dir, RESULTS_FILE), 'rb') as f:
      self.out = pickle.load(f)
      self.err = pickle.load(f)
      self.retval = pickle.load(f)

    # Untar the tarball to a temporary directory
    self.temp_dir = tempfile.mkdtemp(
        dir=os.path.join(self.chromeos_root, 'chroot', 'tmp'))

    command = ('cd %s && tar xf %s' %
               (self.temp_dir, os.path.join(cache_dir, AUTOTEST_TARBALL)))
    ret = self.ce.RunCommand(command, print_to_console=False)
    if ret:
      raise RuntimeError('Could not untar cached tarball')
    self.results_dir = self.temp_dir
    self.results_file = self.GetDataMeasurementsFiles()
    self.perf_data_files = self.GetPerfDataFiles()
    self.perf_report_files = self.GetPerfReportFiles()
    self.chrome_version = self.GetChromeVersionFromCache(cache_dir)
    self.ProcessResults(use_cache=True)

  def CleanUp(self, rm_chroot_tmp):
    if rm_chroot_tmp and self.results_dir:
      dirname, basename = misc.GetRoot(self.results_dir)
      if basename.find('test_that_results_') != -1:
        command = 'rm -rf %s' % self.results_dir
      else:
        command = 'rm -rf %s' % dirname
      self.ce.RunCommand(command)
    if self.temp_dir:
      command = 'rm -rf %s' % self.temp_dir
      self.ce.RunCommand(command)

  def StoreToCacheDir(self, cache_dir, machine_manager, key_list):
    # Create the dir if it doesn't exist.
    temp_dir = tempfile.mkdtemp()

    # Store to the temp directory.
    with open(os.path.join(temp_dir, RESULTS_FILE), 'wb') as f:
      pickle.dump(self.out, f)
      pickle.dump(self.err, f)
      pickle.dump(self.retval, f)

    if not test_flag.GetTestMode():
      with open(os.path.join(temp_dir, CACHE_KEYS_FILE), 'w') as f:
        f.write('%s\n' % self.label.name)
        f.write('%s\n' % self.label.chrome_version)
        f.write('%s\n' % self.machine.checksum_string)
        for k in key_list:
          f.write(k)
          f.write('\n')

    if self.results_dir:
      tarball = os.path.join(temp_dir, AUTOTEST_TARBALL)
      command = ('cd %s && '
                 'tar '
                 '--exclude=var/spool '
                 '--exclude=var/log '
                 '-cjf %s .' % (self.results_dir, tarball))
      ret = self.ce.RunCommand(command)
      if ret:
        raise RuntimeError("Couldn't store autotest output directory.")
    # Store machine info.
    # TODO(asharif): Make machine_manager a singleton, and don't pass it into
    # this function.
    with open(os.path.join(temp_dir, MACHINE_FILE), 'w') as f:
      f.write(machine_manager.machine_checksum_string[self.label.name])

    if os.path.exists(cache_dir):
      command = 'rm -rf {0}'.format(cache_dir)
      self.ce.RunCommand(command)

    command = 'mkdir -p {0} && '.format(os.path.dirname(cache_dir))
    command += 'chmod g+x {0} && '.format(temp_dir)
    command += 'mv {0} {1}'.format(temp_dir, cache_dir)
    ret = self.ce.RunCommand(command)
    if ret:
      command = 'rm -rf {0}'.format(temp_dir)
      self.ce.RunCommand(command)
      raise RuntimeError(
          'Could not move dir %s to dir %s' % (temp_dir, cache_dir))

  @classmethod
  def CreateFromRun(cls,
                    logger,
                    log_level,
                    label,
                    machine,
                    out,
                    err,
                    retval,
                    test,
                    suite='telemetry_Crosperf',
                    cwp_dso=''):
    if suite == 'telemetry':
      result = TelemetryResult(logger, label, log_level, machine)
    else:
      result = cls(logger, label, log_level, machine)
    result.PopulateFromRun(out, err, retval, test, suite, cwp_dso)
    return result

  @classmethod
  def CreateFromCacheHit(cls,
                         logger,
                         log_level,
                         label,
                         machine,
                         cache_dir,
                         test,
                         suite='telemetry_Crosperf',
                         cwp_dso=''):
    if suite == 'telemetry':
      result = TelemetryResult(logger, label, log_level, machine)
    else:
      result = cls(logger, label, log_level, machine)
    try:
      result.PopulateFromCacheDir(cache_dir, test, suite, cwp_dso)

    except RuntimeError as e:
      logger.LogError('Exception while using cache: %s' % e)
      return None
    return result


class TelemetryResult(Result):
  """Class to hold the results of a single Telemetry run."""

  def PopulateFromRun(self, out, err, retval, test, suite, cwp_dso):
    self.out = out
    self.err = err
    self.retval = retval

    self.ProcessResults()

  # pylint: disable=arguments-differ
  def ProcessResults(self):
    # The output is:
    # url,average_commit_time (ms),...
    # www.google.com,33.4,21.2,...
    # We need to convert to this format:
    # {"www.google.com:average_commit_time (ms)": "33.4",
    #  "www.google.com:...": "21.2"}
    # Added note:  Occasionally the output comes back
    # with "JSON.stringify(window.automation.GetResults())" on
    # the first line, and then the rest of the output as
    # described above.

    lines = self.out.splitlines()
    self.keyvals = {}

    if lines:
      if lines[0].startswith('JSON.stringify'):
        lines = lines[1:]

    if not lines:
      return
    labels = lines[0].split(',')
    for line in lines[1:]:
      fields = line.split(',')
      if len(fields) != len(labels):
        continue
      for i in range(1, len(labels)):
        key = '%s %s' % (fields[0], labels[i])
        value = fields[i]
        self.keyvals[key] = value
    self.keyvals['retval'] = self.retval

  def PopulateFromCacheDir(self, cache_dir, test, suite, cwp_dso):
    self.test_name = test
    self.suite = suite
    self.cwp_dso = cwp_dso
    with open(os.path.join(cache_dir, RESULTS_FILE), 'rb') as f:
      self.out = pickle.load(f)
      self.err = pickle.load(f)
      self.retval = pickle.load(f)

    self.chrome_version = \
        super(TelemetryResult, self).GetChromeVersionFromCache(cache_dir)
    self.ProcessResults()


class CacheConditions(object):
  """Various Cache condition values, for export."""

  # Cache hit only if the result file exists.
  CACHE_FILE_EXISTS = 0

  # Cache hit if the checksum of cpuinfo and totalmem of
  # the cached result and the new run match.
  MACHINES_MATCH = 1

  # Cache hit if the image checksum of the cached result and the new run match.
  CHECKSUMS_MATCH = 2

  # Cache hit only if the cached result was successful
  RUN_SUCCEEDED = 3

  # Never a cache hit.
  FALSE = 4

  # Cache hit if the image path matches the cached image path.
  IMAGE_PATH_MATCH = 5

  # Cache hit if the uuid of hard disk mataches the cached one

  SAME_MACHINE_MATCH = 6


class ResultsCache(object):
  """Class to handle the cache for storing/retrieving test run results.

  This class manages the key of the cached runs without worrying about what
  is exactly stored (value). The value generation is handled by the Results
  class.
  """
  CACHE_VERSION = 6

  def __init__(self):
    # Proper initialization happens in the Init function below.
    self.chromeos_image = None
    self.chromeos_root = None
    self.test_name = None
    self.iteration = None
    self.test_args = None
    self.profiler_args = None
    self.board = None
    self.cache_conditions = None
    self.machine_manager = None
    self.machine = None
    self._logger = None
    self.ce = None
    self.label = None
    self.share_cache = None
    self.suite = None
    self.log_level = None
    self.show_all = None
    self.run_local = None
    self.cwp_dso = None

  def Init(self, chromeos_image, chromeos_root, test_name, iteration, test_args,
           profiler_args, machine_manager, machine, board, cache_conditions,
           logger_to_use, log_level, label, share_cache, suite,
           show_all_results, run_local, cwp_dso):
    self.chromeos_image = chromeos_image
    self.chromeos_root = chromeos_root
    self.test_name = test_name
    self.iteration = iteration
    self.test_args = test_args
    self.profiler_args = profiler_args
    self.board = board
    self.cache_conditions = cache_conditions
    self.machine_manager = machine_manager
    self.machine = machine
    self._logger = logger_to_use
    self.ce = command_executer.GetCommandExecuter(
        self._logger, log_level=log_level)
    self.label = label
    self.share_cache = share_cache
    self.suite = suite
    self.log_level = log_level
    self.show_all = show_all_results
    self.run_local = run_local
    self.cwp_dso = cwp_dso

  def GetCacheDirForRead(self):
    matching_dirs = []
    for glob_path in self.FormCacheDir(self.GetCacheKeyList(True)):
      matching_dirs += glob.glob(glob_path)

    if matching_dirs:
      # Cache file found.
      return matching_dirs[0]
    return None

  def GetCacheDirForWrite(self, get_keylist=False):
    cache_path = self.FormCacheDir(self.GetCacheKeyList(False))[0]
    if get_keylist:
      args_str = '%s_%s_%s' % (self.test_args, self.profiler_args,
                               self.run_local)
      version, image = results_report.ParseChromeosImage(
          self.label.chromeos_image)
      keylist = [
          version, image, self.label.board, self.machine.name, self.test_name,
          str(self.iteration), args_str
      ]
      return cache_path, keylist
    return cache_path

  def FormCacheDir(self, list_of_strings):
    cache_key = ' '.join(list_of_strings)
    cache_dir = misc.GetFilenameFromString(cache_key)
    if self.label.cache_dir:
      cache_home = os.path.abspath(os.path.expanduser(self.label.cache_dir))
      cache_path = [os.path.join(cache_home, cache_dir)]
    else:
      cache_path = [os.path.join(SCRATCH_DIR, cache_dir)]

    if self.share_cache:
      for path in [x.strip() for x in self.share_cache.split(',')]:
        if os.path.exists(path):
          cache_path.append(os.path.join(path, cache_dir))
        else:
          self._logger.LogFatal('Unable to find shared cache: %s' % path)

    return cache_path

  def GetCacheKeyList(self, read):
    if read and CacheConditions.MACHINES_MATCH not in self.cache_conditions:
      machine_checksum = '*'
    else:
      machine_checksum = self.machine_manager.machine_checksum[self.label.name]
    if read and CacheConditions.CHECKSUMS_MATCH not in self.cache_conditions:
      checksum = '*'
    elif self.label.image_type == 'trybot':
      checksum = hashlib.md5(
          self.label.chromeos_image.encode('utf-8')).hexdigest()
    elif self.label.image_type == 'official':
      checksum = '*'
    else:
      checksum = ImageChecksummer().Checksum(self.label, self.log_level)

    if read and CacheConditions.IMAGE_PATH_MATCH not in self.cache_conditions:
      image_path_checksum = '*'
    else:
      image_path_checksum = hashlib.md5(
          self.chromeos_image.encode('utf-8')).hexdigest()

    machine_id_checksum = ''
    if read and CacheConditions.SAME_MACHINE_MATCH not in self.cache_conditions:
      machine_id_checksum = '*'
    else:
      if self.machine and self.machine.name in self.label.remote:
        machine_id_checksum = self.machine.machine_id_checksum
      else:
        for machine in self.machine_manager.GetMachines(self.label):
          if machine.name == self.label.remote[0]:
            machine_id_checksum = machine.machine_id_checksum
            break

    temp_test_args = '%s %s %s' % (self.test_args, self.profiler_args,
                                   self.run_local)
    test_args_checksum = hashlib.md5(temp_test_args.encode('utf-8')).hexdigest()
    return (image_path_checksum, self.test_name, str(self.iteration),
            test_args_checksum, checksum, machine_checksum, machine_id_checksum,
            str(self.CACHE_VERSION))

  def ReadResult(self):
    if CacheConditions.FALSE in self.cache_conditions:
      cache_dir = self.GetCacheDirForWrite()
      command = 'rm -rf %s' % (cache_dir,)
      self.ce.RunCommand(command)
      return None
    cache_dir = self.GetCacheDirForRead()

    if not cache_dir:
      return None

    if not os.path.isdir(cache_dir):
      return None

    if self.log_level == 'verbose':
      self._logger.LogOutput('Trying to read from cache dir: %s' % cache_dir)
    result = Result.CreateFromCacheHit(self._logger, self.log_level, self.label,
                                       self.machine, cache_dir, self.test_name,
                                       self.suite, self.cwp_dso)
    if not result:
      return None

    if (result.retval == 0 or
        CacheConditions.RUN_SUCCEEDED not in self.cache_conditions):
      return result

    return None

  def StoreResult(self, result):
    cache_dir, keylist = self.GetCacheDirForWrite(get_keylist=True)
    result.StoreToCacheDir(cache_dir, self.machine_manager, keylist)


class MockResultsCache(ResultsCache):
  """Class for mock testing, corresponding to ResultsCache class."""

  # FIXME: pylint complains about this mock init method, we should probably
  # replace all Mock classes in Crosperf with simple Mock.mock().
  # pylint: disable=arguments-differ
  def Init(self, *args):
    pass

  def ReadResult(self):
    return None

  def StoreResult(self, result):
    pass


class MockResult(Result):
  """Class for mock testing, corresponding to Result class."""

  def PopulateFromRun(self, out, err, retval, test, suite, cwp_dso):
    self.out = out
    self.err = err
    self.retval = retval
