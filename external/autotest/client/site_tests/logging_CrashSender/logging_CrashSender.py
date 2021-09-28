# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, os, re
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.crash import crash_test


_25_HOURS_AGO = -25 * 60 * 60
_CRASH_SENDER_CRON_PATH = '/etc/cron.hourly/crash_sender.hourly'
_DAILY_RATE_LIMIT = 32
_MIN_UNIQUE_TIMES = 4
_SECONDS_SEND_SPREAD = 3600

class logging_CrashSender(crash_test.CrashTest):
    """
      End-to-end test of crash_sender.
    """
    version = 1


    def _check_hardware_info(self, result):
        # Get board name
        lsb_release = utils.read_file('/etc/lsb-release')
        board_match = re.search(r'CHROMEOS_RELEASE_BOARD=(.*)', lsb_release)
        if not ('Board: %s' % board_match.group(1)) in result['output']:
            raise error.TestFail('Missing board name %s in output' %
                                 board_match.group(1))
        # Get hwid
        with os.popen("crossystem hwid 2>/dev/null", "r") as hwid_proc:
            hwclass = hwid_proc.read()
        if not hwclass:
            hwclass = 'undefined'
        if not ('HWClass: %s' % hwclass) in result['output']:
            raise error.TestFail('Expected hwclass %s in output' % hwclass)


    def _check_send_result(self, result, report_kind, payload_name,
                           exec_name):
        if result['report_exists']:
            raise error.TestFail('Test report was not deleted after sending')
        if result['rate_count'] != 1:
            raise error.TestFail('Rate limit was not updated properly: #%d' %
                                 result['rate_count'])
        if not result['send_attempt']:
            raise error.TestFail('Sender did not attempt the send')
        if not result['send_success']:
            raise error.TestFail('Send did not complete successfully')
        if (result['sleep_time'] < 0 or
            result['sleep_time'] >= _SECONDS_SEND_SPREAD):
            raise error.TestFail('Sender did not sleep for an appropriate '
                                 'amount of time: #%d' % result['sleep_time'])
        if result['report_kind'] != report_kind:
            raise error.TestFail('Incorrect report kind "%s", expected "%s"',
                                 result['report_kind'], report_kind)
        desired_payload = self.get_crash_dir_name(payload_name)
        if result['report_payload'] != desired_payload:
            raise error.TestFail('Payload filename was incorrect, got "%s", '
                                 'expected "%s"', result['report_payload'],
                                 desired_payload)
        if result['exec_name'] != exec_name:
            raise error.TestFail('ExecName was incorrect, expected "%s", '
                                 'got "%s"', exec_name, result['exec_name'])


    def _check_simple_minidump_send(self, report):
        result = self._call_sender_one_crash(report=report)
        self._check_send_result(result, 'minidump',
                                '%s.dmp' % self._FAKE_TEST_BASENAME, 'fake')
        if (not 'Version: my_ver' in result['output']):
            raise error.TestFail('Simple minidump send failed')
        self._check_hardware_info(result)
        # Also test "Image type" field.  Note that it will not be "dev" even
        # on a dev build because crash-test-in-progress will exist.
        if result['image_type']:
            raise error.TestFail('Image type "%s" should not exist' %
                                 result['image_type'])
        # Also test "Boot mode" field.  Note that it will not be "dev" even
        # when booting in dev mode because crash-test-in-progress will exist.
        if result['boot_mode']:
            raise error.TestFail('Boot mode "%s" should not exist' %
                                 result['boot_mode'])


    def _test_sender_simple_minidump(self):
        """Test sending a single minidump crash report."""
        self._check_simple_minidump_send(None)


    def _shift_file_mtime(self, path, delta):
        statinfo = os.stat(path)
        os.utime(path, (statinfo.st_atime,
                        statinfo.st_mtime + delta))


    def _test_sender_simple_old_minidump(self):
        """Test that old minidumps and metadata are sent."""
        dmp_path = self.write_crash_dir_entry(
            '%s.dmp' % self._FAKE_TEST_BASENAME, '')
        meta_path = self.write_fake_meta(
            '%s.meta' % self._FAKE_TEST_BASENAME, 'fake', dmp_path)
        self._shift_file_mtime(dmp_path, _25_HOURS_AGO)
        self._shift_file_mtime(meta_path, _25_HOURS_AGO)
        self._check_simple_minidump_send(meta_path)


    def _test_sender_simple_kernel_crash(self):
        """Test sending a single kcrash report."""
        kcrash_fake_report = self.write_crash_dir_entry(
            'kernel.today.kcrash', '')
        self.write_fake_meta('kernel.today.meta',
                             'kernel',
                             kcrash_fake_report)
        result = self._call_sender_one_crash(report=kcrash_fake_report)
        self._check_send_result(result, 'kcrash', 'kernel.today.kcrash',
                                'kernel')
        self._check_hardware_info(result)


    def _test_sender_pausing(self):
        """Test the sender returns immediately when the pause file is present.

        This is testing the sender's test functionality - if this regresses,
        other tests can become flaky because the cron-started sender may run
        asynchronously to these tests.  Disable child sending as normally
        this environment configuration allows our children to run in spite of
        the pause file."""
        self._set_system_sending(False)
        result = self._call_sender_one_crash(should_fail=True,
                                             ignore_pause=False)
        if (not result['report_exists'] or
            not 'Exiting early due to' in result['output'] or
            result['send_attempt']):
            raise error.TestFail('Sender did not pause')


    def _test_sender_reports_disabled(self):
        """Test that when reporting is disabled, we don't send."""
        result = self._call_sender_one_crash(reports_enabled=False)
        if (result['report_exists'] or
            not 'Crash reporting is disabled' in result['output'] or
            result['send_attempt']):
            raise error.TestFail('Sender did not handle reports disabled')


    def _test_sender_rate_limiting(self):
        """Test the sender properly rate limits and sends with delay."""
        sleep_times = []
        for i in range(1, _DAILY_RATE_LIMIT + 1):
            result = self._call_sender_one_crash()
            if not result['send_attempt'] or not result['send_success']:
                raise error.TestFail('Crash uploader did not send on #%d' % i)
            if result['rate_count'] != i:
                raise error.TestFail('Did not properly persist rate on #%d' % i)
            sleep_times.append(result['sleep_time'])
        logging.debug('Sleeps between sending crashes were: %s', sleep_times)
        unique_times = {}
        for i in range(0, _DAILY_RATE_LIMIT):
            unique_times[sleep_times[i]] = True
        if len(unique_times) < _MIN_UNIQUE_TIMES:
            raise error.TestFail('Expected at least %d unique times: %s' %
                                 _MIN_UNIQUE_TIMES, sleep_times)
        # Now the _DAILY_RATE_LIMIT ^ th send request should fail.
        result = self._call_sender_one_crash()
        if (not result['report_exists'] or
            not 'Cannot send more crashes' in result['output'] or
            result['rate_count'] != _DAILY_RATE_LIMIT):
            raise error.TestFail('Crash rate limiting did not take effect')

        # Set one rate file a day earlier and verify can send, be sure to skip
        # the 'state' directory.
        rate_files = [
            name for name in os.listdir(self._CRASH_SENDER_RATE_DIR)
            if os.path.isfile(os.path.join(self._CRASH_SENDER_RATE_DIR, name))
        ]
        rate_path = os.path.join(self._CRASH_SENDER_RATE_DIR, rate_files[0])
        self._shift_file_mtime(rate_path, _25_HOURS_AGO)
        utils.system('ls -la ' + self._CRASH_SENDER_RATE_DIR)
        result = self._call_sender_one_crash()
        if (not result['send_attempt'] or
            not result['send_success'] or
            result['rate_count'] != _DAILY_RATE_LIMIT):
            raise error.TestFail('Crash not sent even after 25hrs pass')


    def _test_sender_single_instance(self):
        """Test the sender fails to start when another instance is running."""
        with self.hold_crash_lock():
            result = self._call_sender_one_crash(should_fail=True)
            if (not 'Failed to acquire a lock' in result['output'] or
                result['send_attempt'] or not result['report_exists']):
                raise error.TestFail('Allowed instance to run while lock held')


    def _test_sender_send_fails(self):
        """Test that when the send fails we try again later."""
        result = self._call_sender_one_crash(send_success=False)
        if not result['send_attempt'] or result['send_success']:
            raise error.TestError('Did not properly cause a send failure')
        if result['rate_count'] != 1:
            raise error.TestFail('Did not count a failed send against rate '
                                 'limiting')
        if not result['report_exists']:
            raise error.TestFail('Expected minidump to be saved for later '
                                 'sending')

        # Also test "Image type" field.  For testing purposes, we set it upon
        # mock failure.  Note that it will not be "dev" even on a dev build
        # because crash-test-in-progress will exist.
        if not result['image_type']:
            raise error.TestFail('Missing image type on mock failure')
        if result['image_type'] != 'mock-fail':
            raise error.TestFail('Incorrect image type on mock failure '
                                 '("%s" != "mock-fail")' %
                                 result['image_type'])


    def run_once(self):
        self.run_crash_tests([
            'sender_simple_minidump',
            'sender_simple_old_minidump',
            'sender_simple_kernel_crash',
            'sender_pausing',
            'sender_reports_disabled',
            'sender_rate_limiting',
            'sender_single_instance',
            'sender_send_fails']);
