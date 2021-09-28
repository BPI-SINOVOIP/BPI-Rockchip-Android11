# -*- coding: utf-8 -*-
#
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Utils for Crosperf to setup devices.

This script provides utils to set device specs. It is only used by Crosperf
to setup device before running tests.

"""

from __future__ import division

import logging
import re
import time

from contextlib import contextmanager


def run_command_on_dut(dut, command, ignore_status=False):
    """
    Helper function to run command on DUT.

    @param dut: The autotest host object representing DUT.
    @param command: The command to run on DUT.
    @ignore_status: Whether to ignore failure executing command.

    @returns Return code, stdout, and stderr.

    """
    result = dut.run(command, ignore_status=ignore_status)

    ret, msg, err_msg = result.exit_status, result.stdout, result.stderr

    if ret:
        err_msg = ('Command execution on DUT %s failed.\n'
                   'Failing command: %s\n'
                   'returned %d\n'
                   'Error message: %s' % (dut.hostname, command, ret, err_msg))
        if ignore_status:
            logging.warning(err_msg +
                            '\n(Failure is considered non-fatal. Continue.)')
        else:
            logging.error(err_msg)

    return ret, msg, err_msg


def disable_aslr(dut):
    """
    Disable ASLR on DUT.

    @param dut: The autotest host object representing DUT.

    """
    disable_aslr = ('set -e; '
                    'if [[ -e /proc/sys/kernel/randomize_va_space ]]; then '
                    '  echo 0 > /proc/sys/kernel/randomize_va_space; '
                    'fi')
    logging.info('Disable ASLR.')
    run_command_on_dut(dut, disable_aslr)


def set_cpu_governor(dut, governor, ignore_status=False):
    """
    Setup CPU Governor on DUT.

    @param dut: The autotest host object representing DUT.
    @param governor: CPU governor for DUT.
    @ignore_status: Whether to ignore failure executing command.

    @returns Return code of the command.

    """
    set_gov_cmd = (
        'for f in `ls -d /sys/devices/system/cpu/cpu*/cpufreq 2>/dev/null`; do '
        # Skip writing scaling_governor if cpu is offline.
        ' [[ -e ${f/cpufreq/online} ]] && grep -q 0 ${f/cpufreq/online} '
        '   && continue; '
        ' cd $f; '
        ' if [[ -e scaling_governor ]]; then '
        '  echo %s > scaling_governor; fi; '
        'done; ')
    logging.info('Setup CPU Governor: %s.', governor)
    ret, _, _ = run_command_on_dut(
        dut, set_gov_cmd % governor, ignore_status=ignore_status)
    return ret


def disable_turbo(dut):
    """
    Disable Turbo on DUT.

    @param dut: The autotest host object representing DUT.

    """
    dis_turbo_cmd = (
        'if [[ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then '
        '  if grep -q 0 /sys/devices/system/cpu/intel_pstate/no_turbo;  then '
        '    echo -n 1 > /sys/devices/system/cpu/intel_pstate/no_turbo; '
        '  fi; '
        'fi; ')
    logging.info('Disable Turbo.')
    run_command_on_dut(dut, dis_turbo_cmd)


def setup_cpu_usage(dut, cpu_usage):
    """
    Setup CPU usage.

    Based on dut_config['cpu_usage'] configure CPU cores utilization.

    @param dut: The autotest host object representing DUT.
    @param cpu_usage: Big/little core usage for CPU.

    """

    if cpu_usage in ('big_only', 'little_only'):
        _, arch, _ = run_command_on_dut(dut, 'uname -m')

        if arch.lower().startswith('arm') or arch.lower().startswith('aarch64'):
            setup_arm_cores(dut, cpu_usage)


def setup_arm_cores(dut, cpu_usage):
    """
    Setup ARM big/little cores.

    @param dut: The autotest host object representing DUT.
    @param cpu_usage: Big/little core usage for CPU.

    """

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

    _, cpuinfo, _ = run_command_on_dut(dut, 'cat /proc/cpuinfo')

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

    if cpu_usage == 'big_only':
        cores_to_enable = dut_big_cores
        cores_to_disable = all_cores - dut_big_cores
    elif cpu_usage == 'little_only':
        cores_to_enable = dut_lit_cores
        cores_to_disable = all_cores - dut_lit_cores
    else:
        logging.warning(
            'cpu_usage=%s is not supported on ARM.\n'
            'Ignore ARM CPU setup and continue.', cpu_usage)
        return

    if cores_to_enable:
        cmd_enable_cores = (
            'echo 1 | tee /sys/devices/system/cpu/cpu{%s}/online' %
            ','.join(sorted(cores_to_enable)))

        cmd_disable_cores = ''
        if cores_to_disable:
            cmd_disable_cores = (
                'echo 0 | tee /sys/devices/system/cpu/cpu{%s}/online' %
                ','.join(sorted(cores_to_disable)))

        run_command_on_dut(dut, '; '.join([cmd_enable_cores,
                                           cmd_disable_cores]))
    else:
        # If there are no cores enabled by dut_config then configuration
        # is invalid for current platform and should be ignored.
        logging.warning(
            '"cpu_usage" is invalid for targeted platform.\n'
            'dut_config[cpu_usage]=%s\n'
            'dut big cores: %s\n'
            'dut little cores: %s\n'
            'Ignore ARM CPU setup and continue.',
            cpu_usage, dut_big_cores, dut_lit_cores)


def get_cpu_online(dut):
    """
    Get online status of CPU cores.

    @param dut: The autotest host object representing DUT.

    @returns dict of {int(cpu_num): <0|1>}.

    """
    get_cpu_online_cmd = ('paste -d" "'
                          ' <(ls /sys/devices/system/cpu/cpu*/online)'
                          ' <(cat /sys/devices/system/cpu/cpu*/online)')
    _, online_output_str, _ = run_command_on_dut(dut, get_cpu_online_cmd)

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


def setup_cpu_freq(dut, freq_percent, online_cores):
    """Setup CPU frequency.

    Based on dut_config['cpu_freq_pct'] setup frequency of online CPU cores
    to a supported value which is less or equal to (freq_pct * max_freq / 100)
    limited by min_freq.

    NOTE: scaling_available_frequencies support is required.
    Otherwise the function has no effect.

    @param dut: The autotest host object representing DUT.
    @param freq_percent: Frequency of online CPU cores to set.
    @param online_cores: List of online cores.

    """
    list_all_avail_freq_cmd = ('ls /sys/devices/system/cpu/cpu{%s}/cpufreq/'
                               'scaling_available_frequencies')
    # Ignore error to support general usage of frequency setup.
    # Not all platforms support scaling_available_frequencies.
    ret, all_avail_freq_str, _ = run_command_on_dut(
        dut,
        list_all_avail_freq_cmd % ','.join(str(core) for core in online_cores),
        ignore_status=True)
    if ret or not all_avail_freq_str:
        # No scalable frequencies available for the core.
        return ret
    for avail_freq_path in all_avail_freq_str.split():
        # Get available freq from every scaling_available_frequency path.
        # Error is considered fatal in run_command_on_dut().
        _, avail_freq_str, _ = run_command_on_dut(dut, 'cat ' + avail_freq_path)
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
        run_command_on_dut(
            dut, 'echo %s | tee %s %s' %
            (avail_ngt_target, max_freq_path, min_freq_path))


def wait_cooldown(dut, cooldown_time, cooldown_temp):
    """
    Wait for DUT to cool down to certain temperature.

    @param cooldown_time: Cooldown timeout.
    @param cooldown_temp: Temperature to cooldown to.

    @returns Cooldown wait time.

    """
    waittime = 0
    timeout_in_sec = int(cooldown_time) * 60
    # Temperature from sensors come in uCelsius units.
    temp_in_ucels = int(cooldown_temp) * 1000
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
        _, temp_output, _ = run_command_on_dut(
            dut,
            'cat /sys/class/thermal/thermal_zone*/temp',
            ignore_status=True)
        if any(int(temp) > temp_in_ucels for temp in temp_output.split()):
            time.sleep(sleep_interval)
            waittime += sleep_interval
        else:
            # Exit the loop when:
            # 1. Reported temp numbers from all thermal sensors do not exceed
            # 'cooldown_temp' or
            # 2. No data from the sensors.
            break

    logging.info('Cooldown wait time: %.1f min', (waittime / 60))
    return waittime


def decrease_wait_time(dut):
    """
    Change the ten seconds wait time for pagecycler to two seconds.

    @param dut: The autotest host object representing DUT.

    """
    FILE = '/usr/local/telemetry/src/tools/perf/page_sets/page_cycler_story.py'
    ret = run_command_on_dut(dut, 'ls ' + FILE)

    if not ret:
        sed_command = 'sed -i "s/_TTI_WAIT_TIME = 10/_TTI_WAIT_TIME = 2/g" '
        run_command_on_dut(dut, sed_command + FILE)


def stop_ui(dut):
    """
    Stop UI on DUT.

    @param dut: The autotest host object representing DUT.

    """
    run_command_on_dut(dut, 'stop ui')


def start_ui(dut):
    """
    Start UI on DUT.

    @param dut: The autotest host object representing DUT.

    """
    run_command_on_dut(dut, 'start ui')


def kern_cmd_update_needed(dut, intel_pstate):
    """
    Check whether kernel cmdline update is needed.

    @param dut: The autotest host object representing DUT.
    @param intel_pstate: kernel command line argument (active, passive,
                         no_hwp).

    @returns True if update is needed.

    """
    good = 0

    # Check that dut platform supports hwp
    cmd = "grep -q '^flags.*hwp' /proc/cpuinfo"
    ret_code, _, _ = run_command_on_dut(dut, cmd, ignore_status=True)
    if ret_code != good:
        # Intel hwp is not supported, update is not needed.
        return False

    kern_cmdline_cmd = 'grep -q "intel_pstate=%s" /proc/cmdline' % intel_pstate
    ret_code, _, _ = run_command_on_dut(
        dut, kern_cmdline_cmd, ignore_status=True)
    logging.info('grep /proc/cmdline returned %d', ret_code)
    if (intel_pstate and ret_code == good or
            not intel_pstate and ret_code != good):
        # No need to updated cmdline if:
        # 1. We are setting intel_pstate and we found it is already set.
        # 2. Not using intel_pstate and it is not in cmdline.
        return False

    # Otherwise we need to update intel_pstate.
    return True


def update_kern_cmd_intel_pstate(dut, intel_pstate):
    """Update kernel command line.

    @param dut: The autotest host object representing DUT.
    @param intel_pstate: kernel command line argument(active, passive,
                         no_hwp).

    """
    good = 0

    # First phase is to remove rootfs verification to allow cmdline change.
    remove_verif_cmd = ' '.join([
        '/usr/share/vboot/bin/make_dev_ssd.sh',
        '--remove_rootfs_verification',
        '--partition %d',
    ])
    # Command for partition 2.
    verif_part2_failed, _, _ = run_command_on_dut(
        dut, remove_verif_cmd % 2, ignore_status=True)
    # Command for partition 4
    # Some machines in the lab use partition 4 to boot from,
    # so cmdline should be update for both partitions.
    verif_part4_failed, _, _ = run_command_on_dut(
        dut, remove_verif_cmd % 4, ignore_status=True)
    if verif_part2_failed or verif_part4_failed:
        logging.error(
            'ERROR. Failed to update kernel cmdline on partition %d.\n'
            'Remove verification failed with status %d',
            2 if verif_part2_failed else 4, verif_part2_failed or
            verif_part4_failed)

    run_command_on_dut(dut, 'reboot && exit')
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
    logging.info('Command to change kernel command line: %s',
                 kern_part2_cmdline_cmd)
    upd_part2_failed, _, _ = run_command_on_dut(
        dut, kern_part2_cmdline_cmd, ignore_status=True)
    # Again here we are updating cmdline for partition 4
    # in addition to partition 2. Without this some machines
    # in the lab might fail.
    kern_part4_cmdline_cmd = kern_cmdline % (4, intel_pstate)
    logging.info('Command to change kernel command line: %s',
                 kern_part4_cmdline_cmd)
    upd_part4_failed, _, _ = run_command_on_dut(
        dut, kern_part4_cmdline_cmd, ignore_status=True)
    if upd_part2_failed or upd_part4_failed:
        logging.error(
            'ERROR. Failed to update kernel cmdline on partition %d.\n'
            'intel_pstate update failed with status %d',
            2 if upd_part2_failed else 4, upd_part2_failed or upd_part4_failed)

    run_command_on_dut(dut, 'reboot && exit')
    # Wait 30s after reboot.
    time.sleep(30)

    # Verification phase.
    # Check that cmdline was updated.
    # Throw an exception if not.
    kern_cmdline_cmd = 'grep -q "intel_pstate=%s" /proc/cmdline' % intel_pstate
    ret_code, _, _ = run_command_on_dut(
        dut, kern_cmdline_cmd, ignore_status=True)
    if (intel_pstate and ret_code != good or
            not intel_pstate and ret_code == good):
        # Kernel cmdline doesn't match input intel_pstate.
        logging.error(
            'ERROR. Failed to update kernel cmdline. '
            'Final verification failed with status %d', ret_code)

    logging.info('Kernel cmdline updated successfully.')


@contextmanager
def pause_ui(dut):
    """
    Stop UI before and Start UI after the context block.

    Context manager will make sure UI is always resumed at the end.

    @param dut: The autotest host object representing DUT.

    """
    stop_ui(dut)
    try:
        yield

    finally:
        start_ui(dut)


def setup_device(dut, dut_config):
    """
    Setup device to get it ready for testing.

    @param dut: The autotest host object representing DUT.
    @param dut_config: A dictionary of DUT configurations.

    @returns Wait time of cool down for this benchmark run.

    """
    logging.info('Update kernel cmdline if necessary and reboot')
    intel_pstate = dut_config.get('intel_pstate')
    if intel_pstate and kern_cmd_update_needed(dut, intel_pstate):
        update_kern_cmd_intel_pstate(dut, intel_pstate)

    wait_time = 0
    # Pause UI while configuring the DUT.
    # This will accelerate setup (waiting for cooldown has x10 drop)
    # and help to reset a Chrome state left after the previous test.
    with pause_ui(dut):
        # Unless the user turns on ASLR in the flag, we first disable ASLR
        # before running the benchmarks
        if not dut_config.get('enable_aslr'):
            disable_aslr(dut)

        # CPU usage setup comes first where we enable/disable cores.
        setup_cpu_usage(dut, dut_config.get('cpu_usage'))
        cpu_online_status = get_cpu_online(dut)
        # List of online cores of type int (core number).
        online_cores = [
            core for core, status in cpu_online_status.items() if status
        ]
        if dut_config.get('cooldown_time'):
            # Setup power conservative mode for effective cool down.
            # Set ignore status since powersave may no be available
            # on all platforms and we are going to handle it.
            ret = set_cpu_governor(dut, 'powersave', ignore_status=True)
            if ret:
                # "powersave" is not available, use "ondemand".
                # Still not a fatal error if it fails.
                ret = set_cpu_governor(dut, 'ondemand', ignore_status=True)
            # TODO(denik): Run comparison test for 'powersave' and 'ondemand'
            # on scarlet and kevin64.
            # We might have to consider reducing freq manually to the min
            # if it helps to reduce waiting time.
            wait_time = wait_cooldown(
                dut, dut_config['cooldown_time'], dut_config['cooldown_temp'])

        # Setup CPU governor for the benchmark run.
        # It overwrites the previous governor settings.
        governor = dut_config.get('governor')
        # FIXME(denik): Pass online cores to governor setup.
        if governor:
            set_cpu_governor(dut, governor)

        # Disable Turbo and Setup CPU freq should ALWAYS proceed governor setup
        # since governor may change:
        # - frequency;
        # - turbo/boost.
        disable_turbo(dut)
        if dut_config.get('cpu_freq_pct'):
            setup_cpu_freq(dut, dut_config['cpu_freq_pct'], online_cores)

        decrease_wait_time(dut)
        # FIXME(denik): Currently we are not recovering the previous cpufreq
        # settings since we do reboot/setup every time anyway.
        # But it may change in the future and then we have to recover the
        # settings.
    return wait_time
