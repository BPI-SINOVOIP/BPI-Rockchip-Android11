# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros import moblab_test
from autotest_lib.server.hosts import moblab_host
from autotest_lib.utils import labellib


_CLEANUP_TIME_M = 5
_MOBLAB_IMAGE_STORAGE = '/mnt/moblab/static'

class moblab_StorageQual(moblab_test.MoblabTest):
    """
    Moblab storage qual suite test. Ensures that moblab can run the storage
    qual tests on the correct DUTs in the correct order. This test does not
    perform any destructive disk operations.

    The test requires 2 duts, labeled 'storage_qual_cq_1', 'storage_qual_cq_2'.
    Each DUT will run a sequence of tests, and the test will then verify
    that the correct tests ran on the correctly labeled DUT, in the correct
    order.
    """
    version = 1

    # Moblab expects to have 1 dut with each of these labels
    REQUIRED_LABELS = {'storage_qual_cq_1', 'storage_qual_cq_2'}

    EXPECTED_RESULTS = {
        'storage_qual_cq_1': [
            'hardware_StorageQualBase_before',
            'hardware_StorageStress_soak',
            'hardware_StorageStress_soak',
            'hardware_StorageStress_suspend',
            'hardware_StorageQualBase_after'
        ],
        'storage_qual_cq_2': [
            'hardware_StorageQualBase_before',
            'hardware_StorageStress_soak',
            'hardware_StorageStress_soak',
            'hardware_StorageQualTrimStress',
            'hardware_StorageQualTrimStress',
            'hardware_StorageQualBase_after'
        ]
    }

    def run_once(self, host, moblab_suite_max_retries,
                 target_build='', clear_devserver_cache=True,
                 test_timeout_hint_m=None):
        """Runs a suite on a Moblab Host against its test DUTS.

        @param host: Moblab Host that will run the suite.
        @param moblab_suite_max_retries: The maximum number of test retries
                allowed within the suite launched on moblab.
        @param target_build: Optional build to be use in the run_suite
                call on moblab. This argument is passed as is to run_suite. It
                must be a sensible build target for the board of the sub-DUTs
                attached to the moblab.
        @param clear_devserver_cache: If True, image cache of the devserver
                running on moblab is cleared before running the test to validate
                devserver imaging staging flow.
        @param test_timeout_hint_m: (int) Optional overall timeout for the test.
                For this test, it is very important to collect post failure data
                from the moblab device. If the overall timeout is provided, the
                test will try to fail early to save some time for log collection
                from the DUT.

        @raises AutoservRunError if the suite does not complete successfully.
        """
        self._host = host
        self._maybe_clear_devserver_cache(clear_devserver_cache)

        duts = host.afe.get_hosts()
        if len(duts) == 0:
            raise error.TestFail('All hosts for this MobLab are down. Please '
                                 'request the lab admins to take a look.')

        board = None
        dut_to_label = {}
        for dut in duts:
            # Fetch the board of the DUT's assigned to this Moblab. There should
            # only be one type.
            board = labellib.LabelsMapping(dut.labels)['board']
            for label in dut.labels:
                if label in self.REQUIRED_LABELS:
                    dut_to_label[dut.hostname] = label

        if not set(dut_to_label.values()) == self.REQUIRED_LABELS:
            raise error.TestFail(
                'Missing required labels on hosts %s, are some hosts down?'
                    % self.REQUIRED_LABELS - set(dut_to_label.values()))

        if not board:
            raise error.TestFail('Could not determine board from hosts.')

        if not target_build:
            stable_version_map = host.afe.get_stable_version_map(
                    host.afe.CROS_IMAGE_TYPE)
            target_build = stable_version_map.get_image_name(board)

        logging.info('Running suite: hardware_storagequal_cq')
        cmd = ("%s/site_utils/run_suite.py --pool='' --board=%s --build=%s "
               "--suite_name=hardware_storagequal_cq --retry=True "
               "--max_retries=%d" %
               (moblab_host.AUTOTEST_INSTALL_DIR, board, target_build,
               moblab_suite_max_retries))
        cmd, run_suite_timeout_s = self._append_run_suite_timeout(
                cmd,
                test_timeout_hint_m,
        )

        logging.debug('Run suite command: %s', cmd)
        try:
            result = host.run_as_moblab(cmd, timeout=run_suite_timeout_s)
        except error.AutoservRunError as e:
            if _is_run_suite_error_critical(e.result_obj.exit_status):
                raise

        logging.debug('Suite Run Output:\n%s', result.stderr)

        job_ids = self._get_job_ids_from_suite_output(result.stderr)

        logging.debug('Suite job ids %s', job_ids)

        keyvals_per_host = self._get_keyval_files_per_host(host, job_ids)

        logging.debug('Keyvals grouped by host %s', keyvals_per_host)

        failed_test = False
        for hostname in keyvals_per_host:
            label = dut_to_label[hostname]
            expected = self.EXPECTED_RESULTS[label]
            actual = self._get_test_execution_order(
                host, keyvals_per_host[hostname])

            logging.info('Comparing test order for %s from host %s',
                label, hostname)
            logging.info('%-37s %s', 'Expected', 'Actual')
            for i in range(max(len(expected), len(actual))):
                expected_i = expected[i] if i < len(expected) else None
                actual_i = actual[i] if i < len(actual) else None
                check_fail = expected_i != actual_i
                check_text = 'X' if check_fail else ' '
                logging.info('%s %-35s %s', check_text, expected_i, actual_i)
                failed_test = failed_test or check_fail

        # Cache directory can contain large binaries like CTS/CTS zip files
        # no need to offload those in the results.
        # The cache is owned by root user
        host.run('rm -fR /mnt/moblab/results/shared/cache',
                    timeout=600)

        if failed_test:
            raise error.TestFail(
                'Actual test execution order did not match expected')

    def _append_run_suite_timeout(self, cmd, test_timeout_hint_m):
        """Modify given run_suite command with timeout.

        @param cmd: run_suite command str.
        @param test_timeout_hint_m: (int) timeout for the test, or None.
        @return cmd, run_suite_timeout_s: cmd is the updated command str,
                run_suite_timeout_s is the timeout to use for the run_suite
                call, in seconds.
        """
        if test_timeout_hint_m is None:
            return cmd, 10800

        # Arguments passed in via test_args may be all str, depending on how
        # they're passed in.
        test_timeout_hint_m = int(test_timeout_hint_m)
        elasped_m = self.elapsed.total_seconds() / 60
        run_suite_timeout_m = (
                test_timeout_hint_m - elasped_m - _CLEANUP_TIME_M)
        logging.info('Overall test timeout hint provided (%d minutes)',
                     test_timeout_hint_m)
        logging.info('%d minutes have already elasped', elasped_m)
        logging.info(
                'Keeping %d minutes for cleanup, will allow %d minutes for '
                'the suite to run.', _CLEANUP_TIME_M, run_suite_timeout_m)
        cmd += ' --timeout_mins %d' % run_suite_timeout_m
        return cmd, run_suite_timeout_m * 60

    def _maybe_clear_devserver_cache(self, clear_devserver_cache):
        # When passed in via test_args, all arguments are str
        if not isinstance(clear_devserver_cache, bool):
            clear_devserver_cache = (clear_devserver_cache.lower() == 'true')
        if clear_devserver_cache:
            self._host.run('rm -rf %s/*' % _MOBLAB_IMAGE_STORAGE)

    def _get_job_ids_from_suite_output(self, suite_output):
        """Parse the set of job ids from run_suite output

        @param suite_output (str) output from run_suite command
        @return (set<int>) job ids contained in the suite
        """
        job_ids = set()
        job_id_pattern = re.compile('(\d+)-moblab')
        for line in suite_output.splitlines():
            match = job_id_pattern.search(line)
            logging.debug('suite line %s match %s', line, match)
            if match is None:
                continue
            job_ids.add(int(match.groups()[0]))
        return job_ids

    def _get_keyval_files_per_host(self, host, job_ids):
        """Find the result keyval files for the given job ids and
        group them by host

        @param host (moblab_host)
        @param job_ids (set<int>) set of job ids to find keyvals for
        @return (dict<str, list<str>>) map of hosts and the keyval
            file locations
        @throws AutoservRunError if the command fails to run on moblab
        """
        keyvals_per_host = {}
        keyvals = host.run_as_moblab(
            'find /mnt/moblab/results '
            '-wholename *-moblab/192.168*/hardware_Storage*/keyval')
        pattern = re.compile('(\d+)-moblab/(192.168.\d+.\d+)')
        for line in keyvals.stdout.splitlines():
            match = pattern.search(line)
            if match is None:
                continue
            job_id, dut = match.groups()
            if int(job_id) not in job_ids:
                continue
            if dut not in keyvals_per_host:
                keyvals_per_host[dut] = []
            keyvals_per_host[dut].append(line)

        return keyvals_per_host

    def _get_test_execution_order(self, host, keyvals):
        """Determines the test execution order for the given list
        of storage qual test result keyvals

        @param host (moblab_host)
        @param keyvals (list<str>) location of keyval files to order
        @return (list<str>) test names in the order they executed
        @throws AutoservRunError if the command fails to run on moblab
        """
        tests = host.run_as_moblab(
            'FILES=(%s); for FILE in ${FILES[@]}; do cat $FILE '
            '| grep storage_qual_cq; done '
            '| sort | cut -d " " -f 2'
            % ' '.join(keyvals)
        )
        test_execution_order = []
        pattern = re.compile('hardware_\w+')
        logging.debug(tests.stdout)
        for line in tests.stdout.splitlines():
            match = pattern.search(line)
            if match:
                test_execution_order.append(match.group(0))
        return test_execution_order

def _is_run_suite_error_critical(return_code):
    # We can't actually import run_suite here because importing run_suite pulls
    # in certain MySQLdb dependencies that fail to load in the context of a
    # test.
    # OTOH, these return codes are unlikely to change because external users /
    # builders depend on them.
    return return_code not in (
            0,  # run_suite.RETURN_CODES.OK
            2,  # run_suite.RETURN_CODES.WARNING
    )
