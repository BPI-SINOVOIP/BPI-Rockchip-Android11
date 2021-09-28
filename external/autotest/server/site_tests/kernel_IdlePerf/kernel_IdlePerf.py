#!/usr/bin/python2
#
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import json
import math
import re

from autotest_lib.server import test
from autotest_lib.server.cros import telemetry_runner
from autotest_lib.client.common_lib import error

# This test detects issues with low-throughput latency-sensitive workloads
# caused by entering idle state.
#
# Such loads sleep regularly but also need to wake up and hit deadlines. We've
# observed on some systems that if idle-state is enabled, we miss a lot of
# deadlines (even though the compute capacity is sufficient).
#
# This test runs top_25_smooth with idle-state both enabled and disabled, and
# looks for a discrepancy in the results. This workload is quite noisy, so
# we run multiple times and take N * stdev as the threshold for flagging an
# issue.
#
# In testing, this approach seemed quite robust, if the parameters (repetitions
# and threshold) are set appropriately. Increasing page-set repetitions helped a
# lot (reduces noise), as did selecting a good value for N (which trades off
# false positives vs. false negatives).
#
# Based on testing, we found good results by using 5 indicative pages, setting
# pageset-repetitions to 7, and taking the mean - 2 * stddev as the estimate
# for "we can be confident that the true regression is not worse than this".
#
# This results in under-estimating the regression (typically by around 2 with
# a healthy system), so false alarms should be rare or non-existent. In testing
# 50 iterations with a good and bad system, this identified 100% of regressions
# and non-regressions correctly (in fact mean - 1 * stddev would also have done
# so, but this seems a bit marginal).

# Repeat each page given number of times
PAGESET_REPEAT = 7

# PAGES can be set to a subset of pages to run for a shorter test, or None to
# run all pages in top_25_smooth.
# Simpler pages emphasise the issue more, as the system is more likely to enter
# idle state.
#
# These were selected by running all pages many times (on a system which
# exhibits the issue), and choosing the 5 pages which have the highest values
# for mean_regression - 2 * stddev - i.e. give the clearest indication of a
# regression.
PAGES = ['games.yahoo', 'Blogger', 'LinkedIn', 'cats', 'booking']

# Path to sysfs control file for disabling idle state
DISABLE_PATH = '/sys/devices/system/cpu/cpu{}/cpuidle/state{}/disable'

class kernel_IdlePerf(test.test):
    """
    Server side regression test for performance impact of idle-state.

    This test runs some smoothness tests with and without sleep enabled, to
    check that the impact of enabling sleep is not significant.

    """
    version = 1
    _cleanup_required = False

    def _check_sysfs(self, host):
        # First check that we are on a suitable DUT which offers the ability to
        # disable the idle state
        arch = host.run_output('uname -m')
        if arch != 'aarch64':
            # Idle states differ between CPU architectures, so this test would
            # need further development to support other platforms.
            raise error.TestNAError('Test only supports Arm aarch64 CPUs')
        if not host.path_exists(DISABLE_PATH.format(0, 1)):
            logging.error('sysfs path absent: cannot disable idle state')
            raise error.TestError('Cannot disable idle state')

        # Identify available idle states. state0 is running state; other states
        # should be disabled when disabling idle.
        self.states = []
        state_dirs = host.run_output(
            'ls -1 /sys/devices/system/cpu/cpu0/cpuidle/')
        for state in state_dirs.split('\n'):
            if re.match('state[1-9][0-9]*$', state):
                # Look for dirnames like 'state1' (but exclude 'state0')
                self.states.append(int(state[5:]))
        logging.info('Found idle states: {}'.format(self.states))

        self.cpu_count = int(host.run_output('nproc --all'))
        logging.info('Found {} cpus'.format(self.cpu_count))
        logging.info('Idle enabled = {}'.format(self._is_idle_enabled(host)))

        # From this point on we expect the test to be able to run, so we will
        # need to ensure that the idle state is restored when the test exits
        self._cleanup_required = True
        self._enable_idle(host, False)
        if self._is_idle_enabled(host):
            logging.error('Failed to disable idle state')
            raise error.TestError('Cannot disable idle state')
        self._enable_idle(host, True)
        if not self._is_idle_enabled(host):
            logging.error('Failed to re-enable idle state')
            raise error.TestError('Cannot disable idle state')

    def _is_idle_enabled(self, host):
        return host.run_output('cat ' + DISABLE_PATH.format(0, 1)) == '0'

    def _enable_idle(self, host, enable):
        logging.info('Setting idle enabled to {}'.format(enable))
        x = '0' if enable else '1'
        for cpu in range(0, self.cpu_count):
            for state in self.states:
                path = DISABLE_PATH.format(cpu, state)
                host.run_output('echo {} > {}'.format(x, path))

    def _parse_results_file(self, path):
        def _mean(values):
            return sum(values) / float(len(values))

        with open(path) as fp:
            histogram_json = json.load(fp)

        scores = {}
        # list of % smooth scores for each page and for each pageset-repetition
        for page in histogram_json['charts']['percentage_smooth']:
            if page == 'summary':
                continue
            page_result = histogram_json['charts']['percentage_smooth'][page]
            scores[page] = {'percentage_smooth': _mean(page_result['values']),
                            'std': page_result['std']
                           }
        return scores

    def _compare_results(self, idle_enabled, idle_disabled):
        results = {
            'passed': True
        }
        for page in idle_enabled:
            diff = (idle_disabled[page]['percentage_smooth']
                   - idle_enabled[page]['percentage_smooth'])
            diff_std = (math.sqrt(idle_enabled[page]['std'] ** 2
                       + idle_disabled[page]['std'] ** 2))
            passed = (idle_enabled[page]['percentage_smooth'] >
                     (idle_disabled[page]['percentage_smooth'] - diff_std * 2))
            key = re.sub('\W', '_', page)
            results[key] = {
                'idle_enabled': idle_enabled[page],
                'idle_disabled': idle_disabled[page],
                'difference': diff,
                'difference_std': diff_std,
                'passed': passed
                }
            results['passed'] = results['passed'] and passed
        return results

    def _run_telemetry(self, host, telemetry, enable):
        logging.info('Running telemetry with idle enabled = {}'.format(enable))
        self._enable_idle(host, enable)

        args = ['--pageset-repeat={}'.format(PAGESET_REPEAT)]
        if PAGES:
            stories = r'\|'.join(r'\(' + p + r'\)' for p in PAGES)
            story_filter = '--story-filter={}'.format(stories)
            args.append(story_filter)

        logging.info('Running telemetry with args: {}'.format(args))
        result = telemetry.run_telemetry_benchmark(
            'smoothness.top_25_smooth', self, *args)
        if result.status != telemetry_runner.SUCCESS_STATUS:
            raise error.TestFail('Failed to run benchmark')

        # ensure first run doesn't get overwritten by second run
        default_path = os.path.join(self.resultsdir, 'results-chart.json')
        if enable:
            unique_path = os.path.join(self.resultsdir,
                                       'results-chart-idle-enabled.json')
        else:
            unique_path = os.path.join(self.resultsdir,
                                       'results-chart-idle-disabled.json')
        os.rename(default_path, unique_path)

        return self._parse_results_file(unique_path)

    def run_once(self, host=None, args={}):
        """Run the telemetry scrolling benchmark.

        @param host: host we are running telemetry on.

        """

        logging.info('Checking sysfs')
        self._check_sysfs(host)

        local = args.get('local') == 'True'
        telemetry = telemetry_runner.TelemetryRunner(
                        host, local, telemetry_on_dut=False)

        logging.info('Starting test')
        results_idle   = self._run_telemetry(host, telemetry, True)
        results_noidle = self._run_telemetry(host, telemetry, False)

        # Score is the regression in percentage of smooth frames caused by
        # enabling CPU idle.
        logging.info('Processing results')
        results = self._compare_results(results_idle, results_noidle)

        self.write_perf_keyval(results)

        if not results['passed']:
            raise error.TestFail('enabling CPU idle significantly '
                                 'regresses scrolling performance')

    def cleanup(self, host):
        """Cleanup of the test.

        @param host: host we are running telemetry on.

        """
        if self._cleanup_required:
            logging.info('Restoring idle to enabled')
            self._enable_idle(host, True)
