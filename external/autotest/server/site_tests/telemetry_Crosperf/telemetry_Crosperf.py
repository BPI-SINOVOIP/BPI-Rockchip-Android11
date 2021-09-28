# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import logging
import os
import re
import shlex
import shutil

from contextlib import contextmanager

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server import utils
from autotest_lib.server.cros import telemetry_runner
from autotest_lib.server.cros.crosperf import device_setup_utils
from autotest_lib.site_utils import test_runner_utils

WAIT_FOR_CMD_TIMEOUT_SECS = 60
DUT_COMMON_SSH_OPTIONS = [
        '-o StrictHostKeyChecking=no', '-o UserKnownHostsFile=/dev/null',
        '-o BatchMode=yes', '-o ConnectTimeout=30',
        '-o ServerAliveInterval=900', '-o ServerAliveCountMax=3',
        '-o ConnectionAttempts=4', '-o Protocol=2'
]
DUT_SCP_OPTIONS = ' '.join(DUT_COMMON_SSH_OPTIONS)

RSA_KEY = '-i %s' % test_runner_utils.TEST_KEY_PATH
DUT_CHROME_RESULTS_DIR = '/usr/local/telemetry/src'

TURBOSTAT_LOG = 'turbostat.log'
CPUSTATS_LOG = 'cpustats.log'
CPUINFO_LOG = 'cpuinfo.log'
TOP_LOG = 'top.log'
WAIT_TIME_LOG = 'wait_time.log'

# Result Statuses
SUCCESS_STATUS = 'SUCCESS'
WARNING_STATUS = 'WARNING'
FAILED_STATUS = 'FAILED'

# Regex for the RESULT output lines understood by chrome buildbot.
# Keep in sync with
# chromium/tools/build/scripts/slave/performance_log_processor.py.
RESULTS_REGEX = re.compile(r'(?P<IMPORTANT>\*)?RESULT '
                           r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                           r'(?P<VALUE>[\{\[]?[-\d\., ]+[\}\]]?)('
                           r' ?(?P<UNITS>.+))?')
HISTOGRAM_REGEX = re.compile(r'(?P<IMPORTANT>\*)?HISTOGRAM '
                             r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                             r'(?P<VALUE_JSON>{.*})(?P<UNITS>.+)?')


class telemetry_Crosperf(test.test):
    """
    Run one or more telemetry benchmarks under the crosperf script.

    """
    version = 1

    def scp_telemetry_results(self, client_ip, dut, file, host_dir,
                              ignore_status=False):
        """
        Copy telemetry results from dut.

        @param client_ip: The ip address of the DUT
        @param dut: The autotest host object representing DUT.
        @param file: The file to copy from DUT.
        @param host_dir: The directory on host to put the file .

        @returns status code for scp command.

        """
        cmd = []
        src = ('root@%s:%s' % (dut.hostname if dut else client_ip, file))
        cmd.extend(['scp', DUT_SCP_OPTIONS, RSA_KEY, '-v', src, host_dir])
        command = ' '.join(cmd)

        logging.debug('Retrieving Results: %s', command)
        try:
            result = utils.run(
                    command,
                    timeout=WAIT_FOR_CMD_TIMEOUT_SECS,
                    ignore_status=ignore_status)
            exit_code = result.exit_status
        except Exception as e:
            logging.error('Failed to retrieve results: %s', e)
            raise

        logging.debug('command return value: %d', exit_code)
        return exit_code

    @contextmanager
    def no_background(self, *_args):
        """
        Background stub.

        """
        yield

    @contextmanager
    def run_in_background_with_log(self, cmd, dut, log_path):
        """
        Get cpustats periodically in background.

        NOTE: No error handling, exception or error report if command fails
        to run in background. Command failure is silenced.

        """
        logging.info('Running in background:\n%s', cmd)
        pid = dut.run_background(cmd)
        try:
            # TODO(denik http://crbug.com/966514): replace with more reliable
            # way to check run success/failure in background.
            # Wait some time before checking the process.
            check = dut.run('sleep 5; kill -0 %s' % pid, ignore_status=True)
            if check.exit_status != 0:
                # command wasn't started correctly
                logging.error(
                        "Background command wasn't started correctly.\n"
                        'Command:\n%s', cmd)
                pid = ''
                yield
                return

            logging.info('Background process started successfully, pid %s',
                         pid)
            yield

        finally:
            if pid:
                # Stop background processes.
                logging.info('Killing background process, pid %s', pid)
                # Kill the process blindly. OK if it's already gone.
                # There is an issue when underlying child processes stay alive
                # while the parent master process is killed.
                # The solution is to kill the chain of processes via process
                # group id.
                dut.run('pgid=$(cat /proc/%s/stat | cut -d")" -f2 | '
                        'cut -d" " -f4) && kill -- -$pgid 2>/dev/null' % pid,
                        ignore_status=True)

                # Copy the results to results directory with silenced failure.
                scp_res = self.scp_telemetry_results(
                        '', dut, log_path, self.resultsdir, ignore_status=True)
                if scp_res:
                    logging.error(
                            'scp of cpuinfo logs failed '
                            'with error %d.', scp_res)

    def run_cpustats_in_background(self, dut, log_name):
        """
        Run command to collect CPU stats in background.

        """
        log_path = '/tmp/%s' % log_name
        cpu_stats_cmd = (
                'cpulog=%s; '
                'rm -f ${cpulog}; '
                # Stop after 720*0.5min=6hours if not killed at clean-up phase.
                'for i in {1..720} ; do '
                # Collect current CPU frequency on all cores and thermal data.
                ' paste -d" " '
                '  <(ls /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_cur_freq)'
                '  <(cat /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_cur_freq)'
                '  >> ${cpulog} || break; '  # exit loop if fails.
                ' paste -d" "'
                '   <(cat /sys/class/thermal/thermal_zone*/type)'
                '   <(cat /sys/class/thermal/thermal_zone*/temp)'
                # Filter in thermal data from only CPU-related sources.
                '  | grep -iE "soc|cpu|pkg|big|little" >> ${cpulog} || break; '
                # Repeat every 30 sec.
                ' sleep 30; '
                'done;') % log_path

        return self.run_in_background_with_log(cpu_stats_cmd, dut, log_path)

    def run_top_in_background(self, dut, log_name, interval_in_sec):
        """
        Run top in background.

        """
        log_path = '/tmp/%s' % log_name
        top_cmd = (
                # Run top in batch mode with specified interval and filter out
                # top system summary and processes not consuming %CPU.
                # Output of each iteration is separated by a blank line.
                'HOME=/usr/local COLUMNS=128 top -bi -d%.1f'
                ' | grep -E "^[ 0-9]|^$" > %s;') % (interval_in_sec, log_path)

        return self.run_in_background_with_log(top_cmd, dut, log_path)

    def run_turbostat_in_background(self, dut, log_name):
        """
        Run turbostat in background.

        """
        log_path = '/tmp/%s' % log_name
        turbostat_cmd = (
                'nohup turbostat --quiet --interval 10 '
                '--show=CPU,Bzy_MHz,Avg_MHz,TSC_MHz,Busy%%,IRQ,CoreTmp '
                '1> %s') % log_path

        return self.run_in_background_with_log(turbostat_cmd, dut, log_path)

    def run_cpuinfo(self, dut, log_name):
        """
        Collect CPU info of "dut" into "log_name" file.

        """
        cpuinfo_cmd = (
                'for cpunum in '
                "   $(awk '/^processor/ { print $NF ; }' /proc/cpuinfo ) ; do "
                ' for i in'
                ' `ls -d /sys/devices/system/cpu/cpu"${cpunum}"/cpufreq/'
                '{cpuinfo_cur_freq,scaling_*_freq,scaling_governor} '
                '     2>/dev/null` ; do '
                '  paste -d" " <(echo "${i}") <(cat "${i}"); '
                ' done;'
                'done;'
                'for cpudata in'
                '  /sys/devices/system/cpu/intel_pstate/no_turbo'
                '  /sys/devices/system/cpu/online; do '
                ' if [[ -e "${cpudata}" ]] ; then '
                '  paste <(echo "${cpudata}") <(cat "${cpudata}"); '
                ' fi; '
                'done; '
                # Adding thermal data to the log.
                'paste -d" "'
                '  <(cat /sys/class/thermal/thermal_zone*/type)'
                '  <(cat /sys/class/thermal/thermal_zone*/temp)')

        # Get CPUInfo at the end of the test.
        logging.info('Get cpuinfo: %s', cpuinfo_cmd)
        with open(os.path.join(self.resultsdir, log_name),
                  'w') as cpu_log_file:
            # Handle command failure gracefully.
            res = dut.run(
                    cpuinfo_cmd, stdout_tee=cpu_log_file, ignore_status=True)
            if res.exit_status:
                logging.error('Get cpuinfo command failed with %d.',
                              res.exit_status)

    def run_once(self, args, client_ip='', dut=None):
        """
        Run a single telemetry test.

        @param args: A dictionary of the arguments that were passed
                to this test.
        @param client_ip: The ip address of the DUT
        @param dut: The autotest host object representing DUT.

        @returns A TelemetryResult instance with the results of this execution.
        """
        test_name = args.get('test', '')
        test_args = args.get('test_args', '')
        profiler_args = args.get('profiler_args', '')

        dut_config_str = args.get('dut_config', '{}')
        dut_config = json.loads(dut_config_str)
        # Setup device with dut_config arguments before running test
        if dut_config:
            wait_time = device_setup_utils.setup_device(dut, dut_config)
            # Wait time can be used to accumulate cooldown time in Crosperf.
            with open(os.path.join(self.resultsdir, WAIT_TIME_LOG), 'w') as f:
                f.write(str(wait_time))

        output_format = 'histograms'

        # For local runs, we set local=True and use local chrome source to run
        # tests; for lab runs, we use devserver instead.
        # By default to be True.
        local = args.get('local', 'true').lower() == 'true'

        # If run_local=true, telemetry benchmark will run on DUT, otherwise
        # run remotely from host.
        # By default to be False.
        # TODO(zhizhouy): It is better to change the field name from "run_local"
        # to "telemetry_on_dut" in crosperf experiment files for consistency.
        telemetry_on_dut = args.get('run_local', '').lower() == 'true'

        # Init TelemetryRunner.
        tr = telemetry_runner.TelemetryRunner(
                dut, local=local, telemetry_on_dut=telemetry_on_dut)

        # Run the test. And collect profile if needed.
        try:
            # If profiler_args specified, we want to add several more options
            # to the command so that run_benchmark will collect system wide
            # profiles.
            if profiler_args:
                profiler_opts = [
                        '--interval-profiling-period=story_run',
                        '--interval-profiling-target=system_wide',
                        '--interval-profiler-options="%s"' % profiler_args
                ]

            top_interval = dut_config.get('top_interval', 0)
            turbostat = dut_config.get('turbostat')

            run_cpuinfo = self.run_cpustats_in_background if dut \
                else self.no_background
            run_turbostat = self.run_turbostat_in_background if (
                    dut and turbostat) else self.no_background
            run_top = self.run_top_in_background if (
                    dut and top_interval > 0) else self.no_background

            # FIXME(denik): replace with ExitStack.
            with run_cpuinfo(dut, CPUSTATS_LOG) as _cpu_cm, \
                run_turbostat(dut, TURBOSTAT_LOG) as _turbo_cm, \
                run_top(dut, TOP_LOG, top_interval) as _top_cm:

                arguments = []
                if test_args:
                    arguments.extend(shlex.split(test_args))
                if profiler_args:
                    arguments.extend(profiler_opts)
                logging.debug('Telemetry Arguments: %s', arguments)
                perf_value_writer = self
                artifacts = True if profiler_args else False
                result = tr.run_telemetry_benchmark(
                        test_name,
                        perf_value_writer,
                        *arguments,
                        ex_output_format=output_format,
                        results_dir=self.resultsdir,
                        no_verbose=True,
                        artifacts=artifacts)
                logging.info('Telemetry completed with exit status: %s.',
                             result.status)
                logging.info('output: %s\n', result.output)

        except (error.TestFail, error.TestWarn):
            logging.debug(
                    'Test did not succeed while executing telemetry test.')
            raise
        except:
            logging.debug('Unexpected failure on telemetry_Crosperf.')
            raise
        finally:
            if dut:
                self.run_cpuinfo(dut, CPUINFO_LOG)

        # Checking whether result file exists.
        filepath = os.path.join(self.resultsdir, 'histograms.json')
        if not os.path.exists(filepath):
            raise RuntimeError('Missing results file: %s' % filepath)

        # Copy the perf data file into the test_that profiling directory,
        # if necessary. It always comes from DUT.
        if profiler_args:
            filepath = os.path.join(self.resultsdir, 'artifacts')
            if not os.path.isabs(filepath):
                raise RuntimeError('Expected absolute path of '
                                   'arfifacts: %s' % filepath)
            perf_exist = False
            for root, _dirs, files in os.walk(filepath):
                for f in files:
                    if f.endswith('.perf.data'):
                        perf_exist = True
                        src_file = os.path.join(root, f)
                        # results-cache.py in crosperf supports multiple
                        # perf.data files, but only if they are named exactly
                        # so. Therefore, create a subdir for each perf.data
                        # file.
                        dst_dir = os.path.join(self.profdir, ''.join(
                                f.split('.')[:-2]))
                        os.makedirs(dst_dir)
                        dst_file = os.path.join(dst_dir, 'perf.data')
                        shutil.copyfile(src_file, dst_file)
            if not perf_exist:
                raise error.TestFail('Error: No profiles collected, test may '
                                     'not run correctly.')

        return result
