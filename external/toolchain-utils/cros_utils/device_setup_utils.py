# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utils for setting devices

This script provides utils to set device specs.
"""

from __future__ import division
from __future__ import print_function

__author__ = 'zhizhouy@google.com (Zhizhou Yang)'

import re
import time

from contextlib import contextmanager

from cros_utils import command_executer


class DutWrapper(object):
  """Wrap DUT parameters inside."""

  def __init__(self,
               chromeos_root,
               remote,
               log_level='verbose',
               logger=None,
               ce=None,
               dut_config=None):
    self.chromeos_root = chromeos_root
    self.remote = remote
    self.log_level = log_level
    self.logger = logger
    self.ce = ce or command_executer.GetCommandExecuter(log_level=log_level)
    self.dut_config = dut_config

  def RunCommandOnDut(self, command, ignore_status=False):
    """Helper function to run command on DUT."""
    ret, msg, err_msg = self.ce.CrosRunCommandWOutput(
        command, machine=self.remote, chromeos_root=self.chromeos_root)

    if ret:
      err_msg = ('Command execution on DUT %s failed.\n'
                 'Failing command: %s\n'
                 'returned %d\n'
                 'Error message: %s' % (self.remote, command, ret, err_msg))
      if ignore_status:
        self.logger.LogError(err_msg +
                             '\n(Failure is considered non-fatal. Continue.)')
      else:
        self.logger.LogFatal(err_msg)

    return ret, msg, err_msg

  def DisableASLR(self):
    """Disable ASLR on DUT."""
    disable_aslr = ('set -e; '
                    'if [[ -e /proc/sys/kernel/randomize_va_space ]]; then '
                    '  echo 0 > /proc/sys/kernel/randomize_va_space; '
                    'fi')
    if self.log_level == 'average':
      self.logger.LogOutput('Disable ASLR.')
    self.RunCommandOnDut(disable_aslr)

  def SetCpuGovernor(self, governor, ignore_status=False):
    """Setup CPU Governor on DUT."""
    set_gov_cmd = (
        'for f in `ls -d /sys/devices/system/cpu/cpu*/cpufreq 2>/dev/null`; do '
        # Skip writing scaling_governor if cpu is offline.
        ' [[ -e ${f/cpufreq/online} ]] && grep -q 0 ${f/cpufreq/online} '
        '   && continue; '
        ' cd $f; '
        ' if [[ -e scaling_governor ]]; then '
        '  echo %s > scaling_governor; fi; '
        'done; ')
    if self.log_level == 'average':
      self.logger.LogOutput('Setup CPU Governor: %s.' % governor)
    ret, _, _ = self.RunCommandOnDut(
        set_gov_cmd % governor, ignore_status=ignore_status)
    return ret

  def DisableTurbo(self):
    """Disable Turbo on DUT."""
    dis_turbo_cmd = (
        'if [[ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then '
        '  if grep -q 0 /sys/devices/system/cpu/intel_pstate/no_turbo;  then '
        '    echo -n 1 > /sys/devices/system/cpu/intel_pstate/no_turbo; '
        '  fi; '
        'fi; ')
    if self.log_level == 'average':
      self.logger.LogOutput('Disable Turbo.')
    self.RunCommandOnDut(dis_turbo_cmd)

  def SetupCpuUsage(self):
    """Setup CPU usage.

    Based on self.dut_config['cpu_usage'] configure CPU cores
    utilization.
    """

    if (self.dut_config['cpu_usage'] == 'big_only' or
        self.dut_config['cpu_usage'] == 'little_only'):
      _, arch, _ = self.RunCommandOnDut('uname -m')

      if arch.lower().startswith('arm') or arch.lower().startswith('aarch64'):
        self.SetupArmCores()

  def SetupArmCores(self):
    """Setup ARM big/little cores."""

    # CPU implemeters/part numbers of big/LITTLE CPU.
    # Format: dict(CPU implementer: set(CPU part numbers))
    LITTLE_CORES = {
        '0x41': {
            '0xd01',  # Cortex A32
            '0xd03',  # Cortex A53
            '0xd04',  # Cortex A35
            '0xd05',  # Cortex A55
        },
    }
    BIG_CORES = {
        '0x41': {
            '0xd07',  # Cortex A57
            '0xd08',  # Cortex A72
            '0xd09',  # Cortex A73
            '0xd0a',  # Cortex A75
            '0xd0b',  # Cortex A76
        },
    }

    # Values of CPU Implementer and CPU part number are exposed by cpuinfo.
    # Format:
    # =================
    # processor       : 0
    # model name      : ARMv8 Processor rev 4 (v8l)
    # BogoMIPS        : 48.00
    # Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
    # CPU implementer : 0x41
    # CPU architecture: 8
    # CPU variant     : 0x0
    # CPU part        : 0xd03
    # CPU revision    : 4

    _, cpuinfo, _ = self.RunCommandOnDut('cat /proc/cpuinfo')

    # List of all CPU cores: 0, 1, ..
    proc_matches = re.findall(r'^processor\s*: (\d+)$', cpuinfo, re.MULTILINE)
    # List of all corresponding CPU implementers
    impl_matches = re.findall(r'^CPU implementer\s*: (0x[\da-f]+)$', cpuinfo,
                              re.MULTILINE)
    # List of all corresponding CPU part numbers
    part_matches = re.findall(r'^CPU part\s*: (0x[\da-f]+)$', cpuinfo,
                              re.MULTILINE)
    assert len(proc_matches) == len(impl_matches)
    assert len(part_matches) == len(impl_matches)

    all_cores = set(proc_matches)
    dut_big_cores = {
        core
        for core, impl, part in zip(proc_matches, impl_matches, part_matches)
        if impl in BIG_CORES and part in BIG_CORES[impl]
    }
    dut_lit_cores = {
        core
        for core, impl, part in zip(proc_matches, impl_matches, part_matches)
        if impl in LITTLE_CORES and part in LITTLE_CORES[impl]
    }

    if self.dut_config['cpu_usage'] == 'big_only':
      cores_to_enable = dut_big_cores
      cores_to_disable = all_cores - dut_big_cores
    elif self.dut_config['cpu_usage'] == 'little_only':
      cores_to_enable = dut_lit_cores
      cores_to_disable = all_cores - dut_lit_cores
    else:
      self.logger.LogError(
          'cpu_usage=%s is not supported on ARM.\n'
          'Ignore ARM CPU setup and continue.' % self.dut_config['cpu_usage'])
      return

    if cores_to_enable:
      cmd_enable_cores = ('echo 1 | tee /sys/devices/system/cpu/cpu{%s}/online'
                          % ','.join(sorted(cores_to_enable)))

      cmd_disable_cores = ''
      if cores_to_disable:
        cmd_disable_cores = (
            'echo 0 | tee /sys/devices/system/cpu/cpu{%s}/online' % ','.join(
                sorted(cores_to_disable)))

      self.RunCommandOnDut('; '.join([cmd_enable_cores, cmd_disable_cores]))
    else:
      # If there are no cores enabled by dut_config then configuration
      # is invalid for current platform and should be ignored.
      self.logger.LogError(
          '"cpu_usage" is invalid for targeted platform.\n'
          'dut_config[cpu_usage]=%s\n'
          'dut big cores: %s\n'
          'dut little cores: %s\n'
          'Ignore ARM CPU setup and continue.' % (self.dut_config['cpu_usage'],
                                                  dut_big_cores, dut_lit_cores))

  def GetCpuOnline(self):
    """Get online status of CPU cores.

    Return dict of {int(cpu_num): <0|1>}.
    """
    get_cpu_online_cmd = ('paste -d" "'
                          ' <(ls /sys/devices/system/cpu/cpu*/online)'
                          ' <(cat /sys/devices/system/cpu/cpu*/online)')
    _, online_output_str, _ = self.RunCommandOnDut(get_cpu_online_cmd)

    # Here is the output we expect to see:
    # -----------------
    # /sys/devices/system/cpu/cpu0/online 0
    # /sys/devices/system/cpu/cpu1/online 1

    cpu_online = {}
    cpu_online_match = re.compile(r'^[/\S]+/cpu(\d+)/[/\S]+\s+(\d+)$')
    for line in online_output_str.splitlines():
      match = cpu_online_match.match(line)
      if match:
        cpu = int(match.group(1))
        status = int(match.group(2))
        cpu_online[cpu] = status
    # At least one CPU has to be online.
    assert cpu_online

    return cpu_online

  def SetupCpuFreq(self, online_cores):
    """Setup CPU frequency.

    Based on self.dut_config['cpu_freq_pct'] setup frequency of online CPU cores
    to a supported value which is less or equal to (freq_pct * max_freq / 100)
    limited by min_freq.

    NOTE: scaling_available_frequencies support is required.
    Otherwise the function has no effect.
    """
    freq_percent = self.dut_config['cpu_freq_pct']
    list_all_avail_freq_cmd = ('ls /sys/devices/system/cpu/cpu{%s}/cpufreq/'
                               'scaling_available_frequencies')
    # Ignore error to support general usage of frequency setup.
    # Not all platforms support scaling_available_frequencies.
    ret, all_avail_freq_str, _ = self.RunCommandOnDut(
        list_all_avail_freq_cmd % ','.join(str(core) for core in online_cores),
        ignore_status=True)
    if ret or not all_avail_freq_str:
      # No scalable frequencies available for the core.
      return ret
    for avail_freq_path in all_avail_freq_str.split():
      # Get available freq from every scaling_available_frequency path.
      # Error is considered fatal in self.RunCommandOnDut().
      _, avail_freq_str, _ = self.RunCommandOnDut('cat ' + avail_freq_path)
      assert avail_freq_str

      all_avail_freq = sorted(
          int(freq_str) for freq_str in avail_freq_str.split())
      min_freq = all_avail_freq[0]
      max_freq = all_avail_freq[-1]
      # Calculate the frequency we are targeting.
      target_freq = round(max_freq * freq_percent / 100)
      # More likely it's not in the list of supported frequencies
      # and our goal is to find the one which is less or equal.
      # Default is min and we will try to maximize it.
      avail_ngt_target = min_freq
      # Find the largest not greater than the target.
      for next_largest in reversed(all_avail_freq):
        if next_largest <= target_freq:
          avail_ngt_target = next_largest
          break

      max_freq_path = avail_freq_path.replace('scaling_available_frequencies',
                                              'scaling_max_freq')
      min_freq_path = avail_freq_path.replace('scaling_available_frequencies',
                                              'scaling_min_freq')
      # With default ignore_status=False we expect 0 status or Fatal error.
      self.RunCommandOnDut('echo %s | tee %s %s' %
                           (avail_ngt_target, max_freq_path, min_freq_path))

  def WaitCooldown(self):
    """Wait for DUT to cool down to certain temperature."""
    waittime = 0
    timeout_in_sec = int(self.dut_config['cooldown_time']) * 60
    # Temperature from sensors come in uCelsius units.
    temp_in_ucels = int(self.dut_config['cooldown_temp']) * 1000
    sleep_interval = 30

    # Wait until any of two events occurs:
    # 1. CPU cools down to a specified temperature.
    # 2. Timeout cooldown_time expires.
    # For the case when targeted temperature is not reached within specified
    # timeout the benchmark is going to start with higher initial CPU temp.
    # In the worst case it may affect test results but at the same time we
    # guarantee the upper bound of waiting time.
    # TODO(denik): Report (or highlight) "high" CPU temperature in test results.
    # "high" should be calculated based on empirical data per platform.
    # Based on such reports we can adjust CPU configuration or
    # cooldown limits accordingly.
    while waittime < timeout_in_sec:
      _, temp_output, _ = self.RunCommandOnDut(
          'cat /sys/class/thermal/thermal_zone*/temp', ignore_status=True)
      if any(int(temp) > temp_in_ucels for temp in temp_output.split()):
        time.sleep(sleep_interval)
        waittime += sleep_interval
      else:
        # Exit the loop when:
        # 1. Reported temp numbers from all thermal sensors do not exceed
        # 'cooldown_temp' or
        # 2. No data from the sensors.
        break

    self.logger.LogOutput('Cooldown wait time: %.1f min' % (waittime / 60))
    return waittime

  def DecreaseWaitTime(self):
    """Change the ten seconds wait time for pagecycler to two seconds."""
    FILE = '/usr/local/telemetry/src/tools/perf/page_sets/page_cycler_story.py'
    ret = self.RunCommandOnDut('ls ' + FILE)

    if not ret:
      sed_command = 'sed -i "s/_TTI_WAIT_TIME = 10/_TTI_WAIT_TIME = 2/g" '
      self.RunCommandOnDut(sed_command + FILE)

  def StopUI(self):
    """Stop UI on DUT."""
    # Added "ignore_status" for the case when crosperf stops ui service which
    # was already stopped. Command is going to fail with 1.
    self.RunCommandOnDut('stop ui', ignore_status=True)

  def StartUI(self):
    """Start UI on DUT."""
    # Similar to StopUI, `start ui` fails if the service is already started.
    self.RunCommandOnDut('start ui', ignore_status=True)

  def KerncmdUpdateNeeded(self, intel_pstate):
    """Check whether kernel cmdline update is needed.

    Args:
      intel_pstate: kernel command line argument (active, passive, no_hwp)

    Returns:
      True if update is needed.
    """

    good = 0

    # Check that dut platform supports hwp
    cmd = "grep -q '^flags.*hwp' /proc/cpuinfo"
    ret_code, _, _ = self.RunCommandOnDut(cmd, ignore_status=True)
    if ret_code != good:
      # Intel hwp is not supported, update is not needed.
      return False

    kern_cmdline_cmd = 'grep -q "intel_pstate=%s" /proc/cmdline' % intel_pstate
    ret_code, _, _ = self.RunCommandOnDut(kern_cmdline_cmd, ignore_status=True)
    self.logger.LogOutput('grep /proc/cmdline returned %d' % ret_code)
    if (intel_pstate and ret_code == good or
        not intel_pstate and ret_code != good):
      # No need to updated cmdline if:
      # 1. We are setting intel_pstate and we found it is already set.
      # 2. Not using intel_pstate and it is not in cmdline.
      return False

    # Otherwise we need to update intel_pstate.
    return True

  def UpdateKerncmdIntelPstate(self, intel_pstate):
    """Update kernel command line.

    Args:
      intel_pstate: kernel command line argument (active, passive, no_hwp)
    """

    good = 0

    # First phase is to remove rootfs verification to allow cmdline change.
    remove_verif_cmd = ' '.join([
        '/usr/share/vboot/bin/make_dev_ssd.sh',
        '--remove_rootfs_verification',
        '--partition %d',
    ])
    # Command for partition 2.
    verif_part2_failed, _, _ = self.RunCommandOnDut(
        remove_verif_cmd % 2, ignore_status=True)
    # Command for partition 4
    # Some machines in the lab use partition 4 to boot from,
    # so cmdline should be update for both partitions.
    verif_part4_failed, _, _ = self.RunCommandOnDut(
        remove_verif_cmd % 4, ignore_status=True)
    if verif_part2_failed or verif_part4_failed:
      self.logger.LogFatal(
          'ERROR. Failed to update kernel cmdline on partition %d.\n'
          'Remove verification failed with status %d' %
          (2 if verif_part2_failed else 4, verif_part2_failed or
           verif_part4_failed))

    self.RunCommandOnDut('reboot && exit')
    # Give enough time for dut to complete reboot
    # TODO(denik): Replace with the function checking machine availability.
    time.sleep(30)

    # Second phase to update intel_pstate in kernel cmdline.
    kern_cmdline = '\n'.join([
        'tmpfile=$(mktemp)',
        'partnumb=%d',
        'pstate=%s',
        # Store kernel cmdline in a temp file.
        '/usr/share/vboot/bin/make_dev_ssd.sh --partition ${partnumb}'
        ' --save_config ${tmpfile}',
        # Remove intel_pstate argument if present.
        "sed -i -r 's/ intel_pstate=[A-Za-z_]+//g' ${tmpfile}.${partnumb}",
        # Insert intel_pstate with a new value if it is set.
        '[[ -n ${pstate} ]] &&'
        ' sed -i -e \"s/ *$/ intel_pstate=${pstate}/\" ${tmpfile}.${partnumb}',
        # Save the change in kernel cmdline.
        # After completion we have to reboot.
        '/usr/share/vboot/bin/make_dev_ssd.sh --partition ${partnumb}'
        ' --set_config ${tmpfile}'
    ])
    kern_part2_cmdline_cmd = kern_cmdline % (2, intel_pstate)
    self.logger.LogOutput(
        'Command to change kernel command line: %s' % kern_part2_cmdline_cmd)
    upd_part2_failed, _, _ = self.RunCommandOnDut(
        kern_part2_cmdline_cmd, ignore_status=True)
    # Again here we are updating cmdline for partition 4
    # in addition to partition 2. Without this some machines
    # in the lab might fail.
    kern_part4_cmdline_cmd = kern_cmdline % (4, intel_pstate)
    self.logger.LogOutput(
        'Command to change kernel command line: %s' % kern_part4_cmdline_cmd)
    upd_part4_failed, _, _ = self.RunCommandOnDut(
        kern_part4_cmdline_cmd, ignore_status=True)
    if upd_part2_failed or upd_part4_failed:
      self.logger.LogFatal(
          'ERROR. Failed to update kernel cmdline on partition %d.\n'
          'intel_pstate update failed with status %d' %
          (2 if upd_part2_failed else 4, upd_part2_failed or upd_part4_failed))

    self.RunCommandOnDut('reboot && exit')
    # Wait 30s after reboot.
    time.sleep(30)

    # Verification phase.
    # Check that cmdline was updated.
    # Throw an exception if not.
    kern_cmdline_cmd = 'grep -q "intel_pstate=%s" /proc/cmdline' % intel_pstate
    ret_code, _, _ = self.RunCommandOnDut(kern_cmdline_cmd, ignore_status=True)
    if (intel_pstate and ret_code != good or
        not intel_pstate and ret_code == good):
      # Kernel cmdline doesn't match input intel_pstate.
      self.logger.LogFatal(
          'ERROR. Failed to update kernel cmdline. '
          'Final verification failed with status %d' % ret_code)

    self.logger.LogOutput('Kernel cmdline updated successfully.')

  @contextmanager
  def PauseUI(self):
    """Stop UI before and Start UI after the context block.

    Context manager will make sure UI is always resumed at the end.
    """
    self.StopUI()
    try:
      yield

    finally:
      self.StartUI()

  def SetupDevice(self):
    """Setup device to get it ready for testing.

    @Returns Wait time of cool down for this benchmark run.
    """
    self.logger.LogOutput('Update kernel cmdline if necessary and reboot')
    intel_pstate = self.dut_config['intel_pstate']
    if intel_pstate and self.KerncmdUpdateNeeded(intel_pstate):
      self.UpdateKerncmdIntelPstate(intel_pstate)

    wait_time = 0
    # Pause UI while configuring the DUT.
    # This will accelerate setup (waiting for cooldown has x10 drop)
    # and help to reset a Chrome state left after the previous test.
    with self.PauseUI():
      # Unless the user turns on ASLR in the flag, we first disable ASLR
      # before running the benchmarks
      if not self.dut_config['enable_aslr']:
        self.DisableASLR()

      # CPU usage setup comes first where we enable/disable cores.
      self.SetupCpuUsage()
      cpu_online_status = self.GetCpuOnline()
      # List of online cores of type int (core number).
      online_cores = [
          core for core, status in cpu_online_status.items() if status
      ]
      if self.dut_config['cooldown_time']:
        # Setup power conservative mode for effective cool down.
        # Set ignore status since powersave may no be available
        # on all platforms and we are going to handle it.
        ret = self.SetCpuGovernor('powersave', ignore_status=True)
        if ret:
          # "powersave" is not available, use "ondemand".
          # Still not a fatal error if it fails.
          ret = self.SetCpuGovernor('ondemand', ignore_status=True)
        # TODO(denik): Run comparison test for 'powersave' and 'ondemand'
        # on scarlet and kevin64.
        # We might have to consider reducing freq manually to the min
        # if it helps to reduce waiting time.
        wait_time = self.WaitCooldown()

      # Setup CPU governor for the benchmark run.
      # It overwrites the previous governor settings.
      governor = self.dut_config['governor']
      # FIXME(denik): Pass online cores to governor setup.
      self.SetCpuGovernor(governor)

      # Disable Turbo and Setup CPU freq should ALWAYS proceed governor setup
      # since governor may change:
      # - frequency;
      # - turbo/boost.
      self.DisableTurbo()
      self.SetupCpuFreq(online_cores)

      self.DecreaseWaitTime()
      # FIXME(denik): Currently we are not recovering the previous cpufreq
      # settings since we do reboot/setup every time anyway.
      # But it may change in the future and then we have to recover the
      # settings.
    return wait_time
