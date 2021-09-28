#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
import logging
import unittest

import mock
from acts.controllers.android_lib import logcat
from acts.controllers.android_lib.logcat import TimestampTracker

BASE_TIMESTAMP = '2000-01-01 12:34:56.789   123 75348 '


class LogcatTest(unittest.TestCase):
    """Tests acts.controllers.android_lib.logcat"""

    @staticmethod
    def patch(patched):
        return mock.patch('acts.controllers.android_lib.logcat.%s' % patched)

    def setUp(self):
        self._get_log_level = logcat._get_log_level

    def tearDown(self):
        logcat._get_log_level = self._get_log_level

    # TimestampTracker

    def test_read_output_sets_last_timestamp_if_found(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP + 'D message')

        self.assertEqual(tracker.last_timestamp, '2000-01-01 12:34:56.789')

    def test_read_output_keeps_last_timestamp_if_no_new_stamp_is_found(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP + 'D message')
        tracker.read_output('--------- beginning of main')

        self.assertEqual(tracker.last_timestamp, '2000-01-01 12:34:56.789')

    def test_read_output_updates_timestamp_to_first_in_results(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP + 'D 9999-99-99 12:34:56.789')

        self.assertEqual(tracker.last_timestamp, '2000-01-01 12:34:56.789')

    # _get_log_level

    def test_get_log_level_verbose(self):
        """Tests that Logcat's verbose logs make it to the debug level."""
        level = logcat._get_log_level(BASE_TIMESTAMP + 'V')

        self.assertEqual(level, logging.DEBUG)

    def test_get_log_level_debug(self):
        """Tests that Logcat's debug logs make it to the debug level."""
        level = logcat._get_log_level(BASE_TIMESTAMP + 'D')

        self.assertEqual(level, logging.DEBUG)

    def test_get_log_level_info(self):
        """Tests that Logcat's info logs make it to the info level."""
        level = logcat._get_log_level(BASE_TIMESTAMP + 'I')

        self.assertEqual(level, logging.INFO)

    def test_get_log_level_warning(self):
        """Tests that Logcat's warning logs make it to the warning level."""
        level = logcat._get_log_level(BASE_TIMESTAMP + 'W')

        self.assertEqual(level, logging.WARNING)

    def test_get_log_level_error(self):
        """Tests that Logcat's error logs make it to the error level."""
        level = logcat._get_log_level(BASE_TIMESTAMP + 'E')

        self.assertEqual(level, logging.ERROR)

    def test_get_log_level_markers(self):
        """Tests that Logcat's marker logs make it to the error level."""
        level = logcat._get_log_level('--------- beginning of main')

        self.assertEqual(level, logging.ERROR)

    # _log_line_func

    def test_log_line_func_returns_func_that_logs_to_given_logger(self):
        logcat._get_log_level = lambda message: logging.INFO
        tracker = mock.Mock()
        log = mock.Mock()
        message = 'MESSAGE'

        logcat._log_line_func(log, tracker)(message)

        self.assertEqual(log.log.called, True)
        log.log.assert_called_once_with(logging.INFO, message)

    def test_log_line_func_returns_func_that_updates_the_timestamp(self):
        logcat._get_log_level = lambda message: logging.INFO
        tracker = mock.Mock()
        log = mock.Mock()
        message = 'MESSAGE'

        logcat._log_line_func(log, tracker)(message)

        self.assertEqual(tracker.read_output.called, True)
        tracker.read_output.assert_called_once_with(message)

    # _on_retry

    def test_on_retry_returns_func_that_formats_with_last_timestamp(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP)
        new_command = logcat._on_retry('S3R14L', 'extra_params', tracker)(None)

        self.assertIn('-T "%s"' % tracker.last_timestamp, new_command)

    def test_on_retry_func_returns_string_that_contains_the_given_serial(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP)
        new_command = logcat._on_retry('S3R14L', 'extra_params', tracker)(None)

        self.assertTrue('-s S3R14L' in new_command)

    def test_on_retry_func_returns_string_that_contains_any_extra_params(self):
        tracker = TimestampTracker()
        tracker.read_output(BASE_TIMESTAMP)
        new_command = logcat._on_retry('S3R14L', 'extra_params', tracker)(None)

        self.assertTrue('extra_params' in new_command)

    # create_logcat_keepalive_process

    def test_create_logcat_keepalive_process_creates_a_new_logger(self):
        with self.patch('log_stream') as log_stream, self.patch('Process'):
            logcat.create_logcat_keepalive_process('S3R14L', 'dir')
        self.assertEqual(log_stream.create_logger.call_args[0][0],
                         'adblog_S3R14L')
        self.assertEqual(log_stream.create_logger.call_args[1]['subcontext'],
                         'dir')

    def test_create_logcat_keepalive_process_creates_a_new_process(self):
        with self.patch('log_stream'), self.patch('Process') as process:
            logcat.create_logcat_keepalive_process('S3R14L', 'dir')

        self.assertIn('S3R14L', process.call_args[0][0])

    def test_create_logcat_keepalive_process_sets_output_callback(self):
        with self.patch('log_stream'), self.patch('Process'):
            process = logcat.create_logcat_keepalive_process('S3R14L', 'dir')

        self.assertEqual(process.set_on_output_callback.called, True)

    def test_create_logcat_keepalive_process_sets_on_terminate_callback(self):
        with self.patch('log_stream'), self.patch('Process'):
            process = logcat.create_logcat_keepalive_process('S3R14L', 'dir')

        self.assertEqual(process.set_on_terminate_callback.called, True)


if __name__ == '__main__':
    unittest.main()
