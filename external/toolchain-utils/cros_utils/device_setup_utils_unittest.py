#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for device_setup_utils."""

from __future__ import print_function

import time

import unittest
from unittest import mock

from device_setup_utils import DutWrapper

from cros_utils import command_executer
from cros_utils import logger

BIG_LITTLE_CPUINFO = """processor       : 0
model name      : ARMv8 Processor rev 4 (v8l)
BogoMIPS        : 48.00
Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 1
model name      : ARMv8 Processor rev 4 (v8l)
BogoMIPS        : 48.00
Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 2
model name      : ARMv8 Processor rev 2 (v8l)
BogoMIPS        : 48.00
Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd08
CPU revision    : 2
"""
LITTLE_ONLY_CPUINFO = """processor       : 0
model name      : ARMv8 Processor rev 4 (v8l)
BogoMIPS        : 48.00
Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4

processor       : 1
model name      : ARMv8 Processor rev 4 (v8l)
BogoMIPS        : 48.00
Features        : half thumb fastmult vfp edsp neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 8
CPU variant     : 0x0
CPU part        : 0xd03
CPU revision    : 4
"""

NOT_BIG_LITTLE_CPUINFO = """processor       : 0
model name      : ARMv7 Processor rev 1 (v7l)
Features        : swp half thumb fastmult vfp edsp thumbee neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 7
CPU variant     : 0x0
CPU part        : 0xc0d
CPU revision    : 1

processor       : 1
model name      : ARMv7 Processor rev 1 (v7l)
Features        : swp half thumb fastmult vfp edsp thumbee neon vfpv3 tls vfpv4
CPU implementer : 0x41
CPU architecture: 7
CPU variant     : 0x0
CPU part        : 0xc0d
CPU revision    : 1

Hardware        : Rockchip (Device Tree)
Revision        : 0000
Serial          : 0000000000000000
"""


class DutWrapperTest(unittest.TestCase):
  """Class of DutWrapper test."""
  real_logger = logger.GetLogger()

  mock_cmd_exec = mock.Mock(spec=command_executer.CommandExecuter)
  mock_logger = mock.Mock(spec=logger.Logger)

  def __init__(self, *args, **kwargs):
    super(DutWrapperTest, self).__init__(*args, **kwargs)

  def setUp(self):
    self.dw = DutWrapper(
        '/tmp/chromeos',
        'lumpy.cros2',
        log_level='verbose',
        logger=self.mock_logger,
        ce=self.mock_cmd_exec,
        dut_config={})

  @mock.patch.object(command_executer.CommandExecuter, 'CrosRunCommandWOutput')
  def test_run_command_on_dut(self, mock_cros_runcmd):
    self.mock_cmd_exec.CrosRunCommandWOutput = mock_cros_runcmd
    mock_cros_runcmd.return_value = (0, '', '')
    mock_cros_runcmd.assert_not_called()
    self.dw.RunCommandOnDut('run command;')
    mock_cros_runcmd.assert_called_once_with(
        'run command;', chromeos_root='/tmp/chromeos', machine='lumpy.cros2')

  @mock.patch.object(command_executer.CommandExecuter, 'CrosRunCommandWOutput')
  def test_dut_wrapper_fatal_error(self, mock_cros_runcmd):
    self.mock_cmd_exec.CrosRunCommandWOutput = mock_cros_runcmd
    # Command returns error 1.
    mock_cros_runcmd.return_value = (1, '', 'Error!')
    mock_cros_runcmd.assert_not_called()
    self.dw.RunCommandOnDut('run command;')
    mock_cros_runcmd.assert_called_once_with(
        'run command;', chromeos_root='/tmp/chromeos', machine='lumpy.cros2')
    # Error status causes log fatal.
    self.assertEqual(
        self.mock_logger.method_calls[-1],
        mock.call.LogFatal('Command execution on DUT lumpy.cros2 failed.\n'
                           'Failing command: run command;\nreturned 1\n'
                           'Error message: Error!'))

  @mock.patch.object(command_executer.CommandExecuter, 'CrosRunCommandWOutput')
  def test_dut_wrapper_ignore_error(self, mock_cros_runcmd):
    self.mock_cmd_exec.CrosRunCommandWOutput = mock_cros_runcmd
    # Command returns error 1.
    mock_cros_runcmd.return_value = (1, '', 'Error!')
    self.dw.RunCommandOnDut('run command;', ignore_status=True)
    mock_cros_runcmd.assert_called_once_with(
        'run command;', chromeos_root='/tmp/chromeos', machine='lumpy.cros2')
    # Error status is not fatal. LogError records the error message.
    self.assertEqual(
        self.mock_logger.method_calls[-1],
        mock.call.LogError('Command execution on DUT lumpy.cros2 failed.\n'
                           'Failing command: run command;\nreturned 1\n'
                           'Error message: Error!\n'
                           '(Failure is considered non-fatal. Continue.)'))

  def test_disable_aslr(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    self.dw.DisableASLR()
    # pyformat: disable
    set_cpu_cmd = ('set -e; '
                   'if [[ -e /proc/sys/kernel/randomize_va_space ]]; then '
                   '  echo 0 > /proc/sys/kernel/randomize_va_space; '
                   'fi')
    self.dw.RunCommandOnDut.assert_called_once_with(set_cpu_cmd)

  def test_set_cpu_governor(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    self.dw.SetCpuGovernor('new_governor', ignore_status=False)
    set_cpu_cmd = (
        'for f in `ls -d /sys/devices/system/cpu/cpu*/cpufreq 2>/dev/null`; do '
        # Skip writing scaling_governor if cpu is offline.
        ' [[ -e ${f/cpufreq/online} ]] && grep -q 0 ${f/cpufreq/online} '
        '   && continue; '
        ' cd $f; '
        ' if [[ -e scaling_governor ]]; then '
        '  echo %s > scaling_governor; fi; '
        'done; ')
    self.dw.RunCommandOnDut.assert_called_once_with(
        set_cpu_cmd % 'new_governor', ignore_status=False)

  def test_set_cpu_governor_propagate_error(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(1, '', 'Error.'))
    self.dw.SetCpuGovernor('non-exist_governor')
    set_cpu_cmd = (
        'for f in `ls -d /sys/devices/system/cpu/cpu*/cpufreq 2>/dev/null`; do '
        # Skip writing scaling_governor if cpu is not online.
        ' [[ -e ${f/cpufreq/online} ]] && grep -q 0 ${f/cpufreq/online} '
        '   && continue; '
        ' cd $f; '
        ' if [[ -e scaling_governor ]]; then '
        '  echo %s > scaling_governor; fi; '
        'done; ')
    # By default error status is fatal.
    self.dw.RunCommandOnDut.assert_called_once_with(
        set_cpu_cmd % 'non-exist_governor', ignore_status=False)

  def test_set_cpu_governor_ignore_status(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(1, '', 'Error.'))
    ret_code = self.dw.SetCpuGovernor('non-exist_governor', ignore_status=True)
    set_cpu_cmd = (
        'for f in `ls -d /sys/devices/system/cpu/cpu*/cpufreq 2>/dev/null`; do '
        # Skip writing scaling_governor if cpu is not online.
        ' [[ -e ${f/cpufreq/online} ]] && grep -q 0 ${f/cpufreq/online} '
        '   && continue; '
        ' cd $f; '
        ' if [[ -e scaling_governor ]]; then '
        '  echo %s > scaling_governor; fi; '
        'done; ')
    self.dw.RunCommandOnDut.assert_called_once_with(
        set_cpu_cmd % 'non-exist_governor', ignore_status=True)
    self.assertEqual(ret_code, 1)

  def test_disable_turbo(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    self.dw.DisableTurbo()
    set_cpu_cmd = (
        # Disable Turbo in Intel pstate driver
        'if [[ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then '
        '  if grep -q 0 /sys/devices/system/cpu/intel_pstate/no_turbo;  then '
        '    echo -n 1 > /sys/devices/system/cpu/intel_pstate/no_turbo; '
        '  fi; '
        'fi; ')
    self.dw.RunCommandOnDut.assert_called_once_with(set_cpu_cmd)

  def test_get_cpu_online_two(self):
    """Test one digit CPU #."""
    self.dw.RunCommandOnDut = mock.Mock(
        return_value=(0, '/sys/devices/system/cpu/cpu0/online 0\n'
                      '/sys/devices/system/cpu/cpu1/online 1\n', ''))
    cpu_online = self.dw.GetCpuOnline()
    self.assertEqual(cpu_online, {0: 0, 1: 1})

  def test_get_cpu_online_twelve(self):
    """Test two digit CPU #."""
    self.dw.RunCommandOnDut = mock.Mock(
        return_value=(0, '/sys/devices/system/cpu/cpu0/online 1\n'
                      '/sys/devices/system/cpu/cpu1/online 0\n'
                      '/sys/devices/system/cpu/cpu10/online 1\n'
                      '/sys/devices/system/cpu/cpu11/online 1\n'
                      '/sys/devices/system/cpu/cpu2/online 1\n'
                      '/sys/devices/system/cpu/cpu3/online 0\n'
                      '/sys/devices/system/cpu/cpu4/online 1\n'
                      '/sys/devices/system/cpu/cpu5/online 0\n'
                      '/sys/devices/system/cpu/cpu6/online 1\n'
                      '/sys/devices/system/cpu/cpu7/online 0\n'
                      '/sys/devices/system/cpu/cpu8/online 1\n'
                      '/sys/devices/system/cpu/cpu9/online 0\n', ''))
    cpu_online = self.dw.GetCpuOnline()
    self.assertEqual(cpu_online, {
        0: 1,
        1: 0,
        2: 1,
        3: 0,
        4: 1,
        5: 0,
        6: 1,
        7: 0,
        8: 1,
        9: 0,
        10: 1,
        11: 1
    })

  def test_get_cpu_online_no_output(self):
    """Test error case, no output."""
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    with self.assertRaises(AssertionError):
      self.dw.GetCpuOnline()

  def test_get_cpu_online_command_error(self):
    """Test error case, command error."""
    self.dw.RunCommandOnDut = mock.Mock(side_effect=AssertionError)
    with self.assertRaises(AssertionError):
      self.dw.GetCpuOnline()

  @mock.patch.object(DutWrapper, 'SetupArmCores')
  def test_setup_cpu_usage_little_on_arm(self, mock_setup_arm):
    self.dw.SetupArmCores = mock_setup_arm
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, 'armv7l', ''))
    self.dw.dut_config['cpu_usage'] = 'little_only'
    self.dw.SetupCpuUsage()
    self.dw.SetupArmCores.assert_called_once_with()

  @mock.patch.object(DutWrapper, 'SetupArmCores')
  def test_setup_cpu_usage_big_on_aarch64(self, mock_setup_arm):
    self.dw.SetupArmCores = mock_setup_arm
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, 'aarch64', ''))
    self.dw.dut_config['cpu_usage'] = 'big_only'
    self.dw.SetupCpuUsage()
    self.dw.SetupArmCores.assert_called_once_with()

  @mock.patch.object(DutWrapper, 'SetupArmCores')
  def test_setup_cpu_usage_big_on_intel(self, mock_setup_arm):
    self.dw.SetupArmCores = mock_setup_arm
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, 'x86_64', ''))
    self.dw.dut_config['cpu_usage'] = 'big_only'
    self.dw.SetupCpuUsage()
    # Check that SetupArmCores not called with invalid setup.
    self.dw.SetupArmCores.assert_not_called()

  @mock.patch.object(DutWrapper, 'SetupArmCores')
  def test_setup_cpu_usage_all_on_intel(self, mock_setup_arm):
    self.dw.SetupArmCores = mock_setup_arm
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, 'x86_64', ''))
    self.dw.dut_config['cpu_usage'] = 'all'
    self.dw.SetupCpuUsage()
    # Check that SetupArmCores not called in general case.
    self.dw.SetupArmCores.assert_not_called()

  def test_setup_arm_cores_big_on_big_little(self):
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, BIG_LITTLE_CPUINFO, ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_usage'] = 'big_only'
    self.dw.SetupArmCores()
    self.dw.RunCommandOnDut.assert_called_with(
        'echo 1 | tee /sys/devices/system/cpu/cpu{2}/online; '
        'echo 0 | tee /sys/devices/system/cpu/cpu{0,1}/online')

  def test_setup_arm_cores_little_on_big_little(self):
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, BIG_LITTLE_CPUINFO, ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_usage'] = 'little_only'
    self.dw.SetupArmCores()
    self.dw.RunCommandOnDut.assert_called_with(
        'echo 1 | tee /sys/devices/system/cpu/cpu{0,1}/online; '
        'echo 0 | tee /sys/devices/system/cpu/cpu{2}/online')

  def test_setup_arm_cores_invalid_config(self):
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, LITTLE_ONLY_CPUINFO, ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_usage'] = 'big_only'
    self.dw.SetupArmCores()
    # Check that setup command is not sent when trying
    # to use 'big_only' on a platform with all little cores.
    self.dw.RunCommandOnDut.assert_called_once_with('cat /proc/cpuinfo')

  def test_setup_arm_cores_not_big_little(self):
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, NOT_BIG_LITTLE_CPUINFO, ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_usage'] = 'big_only'
    self.dw.SetupArmCores()
    # Check that setup command is not sent when trying
    # to use 'big_only' on a platform w/o support of big/little.
    self.dw.RunCommandOnDut.assert_called_once_with('cat /proc/cpuinfo')

  def test_setup_arm_cores_unsupported_cpu_usage(self):
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, BIG_LITTLE_CPUINFO, ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_usage'] = 'exclusive_cores'
    self.dw.SetupArmCores()
    # Check that setup command is not sent when trying to use
    # 'exclusive_cores' on ARM CPU setup.
    self.dw.RunCommandOnDut.assert_called_once_with('cat /proc/cpuinfo')

  def test_setup_cpu_freq_single_full(self):
    online = [0]
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0,
         '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies\n',
         ''),
        (0, '1 2 3 4 5 6 7 8 9 10', ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_freq_pct'] = 100
    self.dw.SetupCpuFreq(online)
    self.assertGreaterEqual(self.dw.RunCommandOnDut.call_count, 3)
    self.assertEqual(
        self.dw.RunCommandOnDut.call_args,
        mock.call('echo 10 | tee '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq'))

  def test_setup_cpu_freq_middle(self):
    online = [0]
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0,
         '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies\n',
         ''),
        (0, '1 2 3 4 5 6 7 8 9 10', ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_freq_pct'] = 60
    self.dw.SetupCpuFreq(online)
    self.assertGreaterEqual(self.dw.RunCommandOnDut.call_count, 2)
    self.assertEqual(
        self.dw.RunCommandOnDut.call_args,
        mock.call('echo 6 | tee '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq'))

  def test_setup_cpu_freq_lowest(self):
    online = [0]
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0,
         '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies\n',
         ''),
        (0, '1 2 3 4 5 6 7 8 9 10', ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_freq_pct'] = 0
    self.dw.SetupCpuFreq(online)
    self.assertGreaterEqual(self.dw.RunCommandOnDut.call_count, 2)
    self.assertEqual(
        self.dw.RunCommandOnDut.call_args,
        mock.call('echo 1 | tee '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq'))

  def test_setup_cpu_freq_multiple_middle(self):
    online = [0, 1]
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0,
         '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies\n'
         '/sys/devices/system/cpu/cpu1/cpufreq/scaling_available_frequencies\n',
         ''),
        (0, '1 2 3 4 5 6 7 8 9 10', ''),
        (0, '', ''),
        (0, '1 4 6 8 10 12 14 16 18 20', ''),
        (0, '', ''),
    ])
    self.dw.dut_config['cpu_freq_pct'] = 70
    self.dw.SetupCpuFreq(online)
    self.assertEqual(self.dw.RunCommandOnDut.call_count, 5)
    self.assertEqual(
        self.dw.RunCommandOnDut.call_args_list[2],
        mock.call('echo 7 | tee '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq '
                  '/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq'))
    self.assertEqual(
        self.dw.RunCommandOnDut.call_args_list[4],
        mock.call('echo 14 | tee '
                  '/sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq '
                  '/sys/devices/system/cpu/cpu1/cpufreq/scaling_min_freq'))

  def test_setup_cpu_freq_no_scaling_available(self):
    online = [0, 1]
    self.dw.RunCommandOnDut = mock.Mock(
        return_value=(2, '', 'No such file or directory'))
    self.dw.dut_config['cpu_freq_pct'] = 50
    self.dw.SetupCpuFreq(online)
    self.dw.RunCommandOnDut.assert_called_once()
    self.assertNotRegex(self.dw.RunCommandOnDut.call_args_list[0][0][0],
                        '^echo.*scaling_max_freq$')

  def test_setup_cpu_freq_multiple_no_access(self):
    online = [0, 1]
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0,
         '/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies\n'
         '/sys/devices/system/cpu/cpu1/cpufreq/scaling_available_frequencies\n',
         ''),
        (0, '1 4 6 8 10 12 14 16 18 20', ''),
        AssertionError(),
    ])
    self.dw.dut_config['cpu_freq_pct'] = 30
    # Error status causes log fatal.
    with self.assertRaises(AssertionError):
      self.dw.SetupCpuFreq(online)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_nowait(self, mock_sleep):
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '39000', ''))
    self.dw.dut_config['cooldown_time'] = 10
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    # Send command to DUT only once to check temperature
    # and make sure it does not exceed the threshold.
    self.dw.RunCommandOnDut.assert_called_once()
    mock_sleep.assert_not_called()
    self.assertEqual(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_needwait_once(self, mock_sleep):
    """Wait one iteration for cooldown.

    Set large enough timeout and changing temperature
    output. Make sure it exits when expected value
    received.
    Expect that WaitCooldown check temp twice.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[(0, '41000',
                                                      ''), (0, '39999', '')])
    self.dw.dut_config['cooldown_time'] = 100
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    self.dw.RunCommandOnDut.assert_called()
    self.assertEqual(self.dw.RunCommandOnDut.call_count, 2)
    mock_sleep.assert_called()
    self.assertGreater(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_needwait(self, mock_sleep):
    """Test exit by timeout.

    Send command to DUT checking the temperature and
    check repeatedly until timeout goes off.
    Output from temperature sensor never changes.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '41000', ''))
    self.dw.dut_config['cooldown_time'] = 60
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    self.dw.RunCommandOnDut.assert_called()
    self.assertGreater(self.dw.RunCommandOnDut.call_count, 2)
    mock_sleep.assert_called()
    self.assertGreater(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_needwait_multtemp(self, mock_sleep):
    """Wait until all temps go down.

    Set large enough timeout and changing temperature
    output. Make sure it exits when expected value
    for all temperatures received.
    Expect 3 checks.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (0, '41000\n20000\n30000\n45000', ''),
        (0, '39000\n20000\n30000\n41000', ''),
        (0, '39000\n20000\n30000\n31000', ''),
    ])
    self.dw.dut_config['cooldown_time'] = 100
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    self.dw.RunCommandOnDut.assert_called()
    self.assertEqual(self.dw.RunCommandOnDut.call_count, 3)
    mock_sleep.assert_called()
    self.assertGreater(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_thermal_error(self, mock_sleep):
    """Handle error status.

    Any error should be considered non-fatal.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[
        (1, '39000\n20000\n30000\n41000', 'Thermal error'),
        (1, '39000\n20000\n30000\n31000', 'Thermal error'),
    ])
    self.dw.dut_config['cooldown_time'] = 10
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    # Check that errors are ignored.
    self.dw.RunCommandOnDut.assert_called_with(
        'cat /sys/class/thermal/thermal_zone*/temp', ignore_status=True)
    self.assertEqual(self.dw.RunCommandOnDut.call_count, 2)
    # Check that we are waiting even when an error is returned
    # as soon as data is coming.
    mock_sleep.assert_called()
    self.assertGreater(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_thermal_no_output(self, mock_sleep):
    """Handle no output.

    Check handling of empty stdout.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[(1, '', 'Thermal error')])
    self.dw.dut_config['cooldown_time'] = 10
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    # Check that errors are ignored.
    self.dw.RunCommandOnDut.assert_called_once_with(
        'cat /sys/class/thermal/thermal_zone*/temp', ignore_status=True)
    # No wait.
    mock_sleep.assert_not_called()
    self.assertEqual(wait_time, 0)

  @mock.patch.object(time, 'sleep')
  def test_wait_cooldown_thermal_ws_output(self, mock_sleep):
    """Handle whitespace output.

    Check handling of whitespace only.
    """
    mock_sleep.return_value = 0
    self.dw.RunCommandOnDut = mock.Mock(side_effect=[(1, '\n',
                                                      'Thermal error')])
    self.dw.dut_config['cooldown_time'] = 10
    self.dw.dut_config['cooldown_temp'] = 40
    wait_time = self.dw.WaitCooldown()
    # Check that errors are ignored.
    self.dw.RunCommandOnDut.assert_called_once_with(
        'cat /sys/class/thermal/thermal_zone*/temp', ignore_status=True)
    # No wait.
    mock_sleep.assert_not_called()
    self.assertEqual(wait_time, 0)

  def test_stop_ui(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    self.dw.StopUI()
    self.dw.RunCommandOnDut.assert_called_once_with(
        'stop ui', ignore_status=True)

  def test_start_ui(self):
    self.dw.RunCommandOnDut = mock.Mock(return_value=(0, '', ''))
    self.dw.StartUI()
    self.dw.RunCommandOnDut.assert_called_once_with(
        'start ui', ignore_status=True)

  def test_setup_device(self):

    def FakeRunner(command, ignore_status=False):
      # pylint fix for unused variable.
      del command, ignore_status
      return 0, '', ''

    def SetupMockFunctions():
      self.dw.RunCommandOnDut = mock.Mock(return_value=FakeRunner)
      self.dw.KerncmdUpdateNeeded = mock.Mock(return_value=True)
      self.dw.UpdateKerncmdIntelPstate = mock.Mock(return_value=0)
      self.dw.DisableASLR = mock.Mock(return_value=0)
      self.dw.SetupCpuUsage = mock.Mock(return_value=0)
      self.dw.SetupCpuFreq = mock.Mock(return_value=0)
      self.dw.GetCpuOnline = mock.Mock(return_value={0: 1, 1: 1, 2: 0})
      self.dw.SetCpuGovernor = mock.Mock(return_value=0)
      self.dw.DisableTurbo = mock.Mock(return_value=0)
      self.dw.StopUI = mock.Mock(return_value=0)
      self.dw.StartUI = mock.Mock(return_value=0)
      self.dw.WaitCooldown = mock.Mock(return_value=0)
      self.dw.DecreaseWaitTime = mock.Mock(return_value=0)

    self.dw.dut_config['enable_aslr'] = False
    self.dw.dut_config['cooldown_time'] = 0
    self.dw.dut_config['governor'] = 'fake_governor'
    self.dw.dut_config['cpu_freq_pct'] = 65
    self.dw.dut_config['intel_pstate'] = 'no_hwp'

    SetupMockFunctions()
    self.dw.SetupDevice()

    self.dw.KerncmdUpdateNeeded.assert_called_once()
    self.dw.UpdateKerncmdIntelPstate.assert_called_once()
    self.dw.DisableASLR.assert_called_once()
    self.dw.SetupCpuUsage.assert_called_once_with()
    self.dw.SetupCpuFreq.assert_called_once_with([0, 1])
    self.dw.GetCpuOnline.assert_called_once_with()
    self.dw.SetCpuGovernor.assert_called_once_with('fake_governor')
    self.dw.DisableTurbo.assert_called_once_with()
    self.dw.DecreaseWaitTime.assert_called_once_with()
    self.dw.StopUI.assert_called_once_with()
    self.dw.StartUI.assert_called_once_with()
    self.dw.WaitCooldown.assert_not_called()

    # Test SetupDevice with cooldown
    self.dw.dut_config['cooldown_time'] = 10

    SetupMockFunctions()
    self.dw.GetCpuOnline = mock.Mock(return_value={0: 0, 1: 1})

    self.dw.SetupDevice()

    self.dw.WaitCooldown.assert_called_once_with()
    self.dw.DisableASLR.assert_called_once()
    self.dw.DisableTurbo.assert_called_once_with()
    self.dw.SetupCpuUsage.assert_called_once_with()
    self.dw.SetupCpuFreq.assert_called_once_with([1])
    self.dw.SetCpuGovernor.assert_called()
    self.dw.GetCpuOnline.assert_called_once_with()
    self.dw.StopUI.assert_called_once_with()
    self.dw.StartUI.assert_called_once_with()
    self.assertGreater(self.dw.SetCpuGovernor.call_count, 1)
    self.assertEqual(self.dw.SetCpuGovernor.call_args,
                     mock.call('fake_governor'))

    # Test SetupDevice with cooldown
    SetupMockFunctions()
    self.dw.SetupCpuUsage = mock.Mock(side_effect=RuntimeError())

    with self.assertRaises(RuntimeError):
      self.dw.SetupDevice()

    # This call injected an exception.
    self.dw.SetupCpuUsage.assert_called_once_with()
    # Calls following the expeption are skipped.
    self.dw.WaitCooldown.assert_not_called()
    self.dw.DisableTurbo.assert_not_called()
    self.dw.SetupCpuFreq.assert_not_called()
    self.dw.SetCpuGovernor.assert_not_called()
    self.dw.GetCpuOnline.assert_not_called()
    # Check that Stop/Start UI are always called.
    self.dw.StopUI.assert_called_once_with()
    self.dw.StartUI.assert_called_once_with()


if __name__ == '__main__':
  unittest.main()
