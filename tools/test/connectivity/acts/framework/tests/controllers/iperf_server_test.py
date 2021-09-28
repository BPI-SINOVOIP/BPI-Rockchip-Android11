#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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
import os

from acts.controllers import iperf_server
from acts.controllers.iperf_server import IPerfServer
from acts.controllers.iperf_server import IPerfServerOverAdb
from acts.controllers.iperf_server import IPerfServerOverSsh

# The position in the call tuple that represents the args array.
ARGS = 0

# The position in the call tuple that represents the kwargs dict.
KWARGS = 1

MOCK_LOGFILE_PATH = '/path/to/foo'


class IPerfServerModuleTest(unittest.TestCase):
    """Tests the acts.controllers.iperf_server module."""

    def test_create_creates_local_iperf_server_with_int(self):
        self.assertIsInstance(
            iperf_server.create([12345])[0],
            IPerfServer,
            'create() failed to create IPerfServer for integer input.'
        )

    def test_create_creates_local_iperf_server_with_str(self):
        self.assertIsInstance(
            iperf_server.create(['12345'])[0],
            IPerfServer,
            'create() failed to create IPerfServer for integer input.'
        )

    def test_create_cannot_create_local_iperf_server_with_bad_str(self):
        with self.assertRaises(ValueError):
            iperf_server.create(['12345BAD_STRING'])

    @mock.patch('acts.controllers.iperf_server.utils')
    def test_create_creates_server_over_ssh_with_ssh_config_and_port(self, _):
        self.assertIsInstance(
            iperf_server.create([{'ssh_config': {'user': '', 'host': ''},
                                  'port': ''}])[0],
            IPerfServerOverSsh,
            'create() failed to create IPerfServerOverSsh for a valid config.'
        )

    def test_create_creates_server_over_adb_with_proper_config(self):
        self.assertIsInstance(
            iperf_server.create([{'AndroidDevice': '53R147', 'port': 0}])[0],
            IPerfServerOverAdb,
            'create() failed to create IPerfServerOverAdb for a valid config.'
        )

    def test_create_raises_value_error_on_bad_config_dict(self):
        with self.assertRaises(ValueError):
            iperf_server.create([{'AndroidDevice': '53R147', 'ssh_config': {}}])

    def test_get_port_from_ss_output_returns_correct_port_ipv4(self):
        ss_output = ('tcp LISTEN  0 5 127.0.0.1:<PORT>  *:*'
                     ' users:(("cmd",pid=<PID>,fd=3))')
        self.assertEqual(
            iperf_server._get_port_from_ss_output(ss_output, '<PID>'), '<PORT>')

    def test_get_port_from_ss_output_returns_correct_port_ipv6(self):
        ss_output = ('tcp LISTEN  0 5 ff:ff:ff:ff:ff:ff:<PORT>  *:*'
                     ' users:(("cmd",pid=<PID>,fd=3))')
        self.assertEqual(
            iperf_server._get_port_from_ss_output(ss_output, '<PID>'), '<PORT>')


class IPerfServerBaseTest(unittest.TestCase):
    """Tests acts.controllers.iperf_server.IPerfServerBase."""

    @mock.patch('os.makedirs')
    def test_get_full_file_path_creates_parent_directory(self, mock_makedirs):
        # Will never actually be created/used.
        logging.log_path = '/tmp/unit_test_garbage'

        server = IPerfServer('port')

        full_file_path = server._get_full_file_path()

        self.assertTrue(
            mock_makedirs.called,
            'Did not attempt to create a directory.'
        )
        self.assertEqual(
            os.path.dirname(full_file_path),
            mock_makedirs.call_args[ARGS][0],
            'The parent directory of the full file path was not created.'
        )


class IPerfServerTest(unittest.TestCase):
    """Tests acts.controllers.iperf_server.IPerfServer."""

    PID = 123456

    def setUp(self):
        iperf_server._get_port_from_ss_output = lambda *_: IPerfServerTest.PID

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.subprocess')
    @mock.patch('acts.controllers.iperf_server.job')
    def test_start_makes_started_true(self, mock_job, __, ___):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server.start()

        self.assertTrue(server.started)

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.subprocess')
    @mock.patch('acts.controllers.iperf_server.job')
    def test_start_stop_makes_started_false(self, _, __, ___):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH

        server.start()
        server.stop()

        self.assertFalse(server.started)

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.subprocess')
    @mock.patch('acts.controllers.iperf_server.job')
    def test_start_sets_current_log_file(self, _, __, ___):
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH

        server.start()

        self.assertEqual(
            server._current_log_file,
            MOCK_LOGFILE_PATH,
            'The _current_log_file was not received from _get_full_file_path.'
        )

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.subprocess')
    def test_stop_returns_current_log_file(self, _, __):
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._current_log_file = MOCK_LOGFILE_PATH
        server._iperf_process = mock.Mock()

        log_file = server.stop()

        self.assertEqual(
            log_file,
            MOCK_LOGFILE_PATH,
            'The _current_log_file was not returned by stop().'
        )

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.subprocess')
    @mock.patch('acts.controllers.iperf_server.job')
    def test_start_does_not_run_two_concurrent_processes(self, start_proc, _, __):
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_process = mock.Mock()

        server.start()

        self.assertFalse(
            start_proc.called,
            'start() should not begin a second process if another is running.'
        )

    @mock.patch('acts.utils.stop_standing_subprocess')
    def test_stop_exits_early_if_no_process_has_started(self, stop_proc):
        server = IPerfServer('port')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_process = None

        server.stop()

        self.assertFalse(
            stop_proc.called,
            'stop() should not kill a process if no process is running.'
        )


class IPerfServerOverSshTest(unittest.TestCase):
    """Tests acts.controllers.iperf_server.IPerfServerOverSsh."""

    INIT_ARGS = [{'host': 'TEST_HOST', 'user': 'test'}, 'PORT']

    @mock.patch('acts.controllers.iperf_server.connection')
    def test_start_makes_started_true(self, _):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServerOverSsh(*self.INIT_ARGS)
        server._ssh_session = mock.Mock()
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH

        server.start()

        self.assertTrue(server.started)

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.connection')
    def test_start_stop_makes_started_false(self, _, __):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServerOverSsh(*self.INIT_ARGS)
        server._ssh_session = mock.Mock()
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH

        server.start()
        server.stop()

        self.assertFalse(server.started)

    @mock.patch('builtins.open')
    @mock.patch('acts.controllers.iperf_server.connection')
    def test_stop_returns_expected_log_file(self, _, __):
        server = IPerfServerOverSsh(*self.INIT_ARGS)
        server._ssh_session = mock.Mock()
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_pid = mock.Mock()

        log_file = server.stop()

        self.assertEqual(
            log_file,
            MOCK_LOGFILE_PATH,
            'The expected log file was not returned by stop().'
        )

    @mock.patch('acts.controllers.iperf_server.connection')
    def test_start_does_not_run_two_concurrent_processes(self, _):
        server = IPerfServerOverSsh(*self.INIT_ARGS)
        server._ssh_session = mock.Mock()
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_pid = mock.Mock()

        server.start()

        self.assertFalse(
            server._ssh_session.run_async.called,
            'start() should not begin a second process if another is running.'
        )

    @mock.patch('acts.utils.stop_standing_subprocess')
    @mock.patch('acts.controllers.iperf_server.connection')
    def test_stop_exits_early_if_no_process_has_started(self, _, __):
        server = IPerfServerOverSsh(*self.INIT_ARGS)
        server._ssh_session = mock.Mock()
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_pid = None

        server.stop()

        self.assertFalse(
            server._ssh_session.run_async.called,
            'stop() should not kill a process if no process is running.'
        )


class IPerfServerOverAdbTest(unittest.TestCase):
    """Tests acts.controllers.iperf_server.IPerfServerOverSsh."""

    ANDROID_DEVICE_PROP = ('acts.controllers.iperf_server.'
                           'IPerfServerOverAdb._android_device')

    @mock.patch(ANDROID_DEVICE_PROP)
    def test_start_makes_started_true(self, mock_ad):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServerOverAdb('53R147', 'PORT')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        mock_ad.adb.shell.return_value = '<PID>'

        server.start()

        self.assertTrue(server.started)

    @mock.patch('acts.libs.proc.job.run')
    @mock.patch('builtins.open')
    @mock.patch(ANDROID_DEVICE_PROP)
    def test_start_stop_makes_started_false(self, mock_ad, _, __):
        """Tests calling start() without calling stop() makes started True."""
        server = IPerfServerOverAdb('53R147', 'PORT')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        mock_ad.adb.shell.side_effect = ['<PID>', '', '', '']

        server.start()
        server.stop()

        self.assertFalse(server.started)

    @mock.patch('acts.libs.proc.job.run')
    @mock.patch('builtins.open')
    @mock.patch(ANDROID_DEVICE_PROP)
    def test_stop_returns_expected_log_file(self, mock_ad, _, __):
        server = IPerfServerOverAdb('53R147', 'PORT')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_process = mock.Mock()
        server._iperf_process_adb_pid = '<PID>'
        mock_ad.adb.shell.side_effect = ['', '', '']

        log_file = server.stop()

        self.assertEqual(
            log_file,
            MOCK_LOGFILE_PATH,
            'The expected log file was not returned by stop().'
        )

    @mock.patch(ANDROID_DEVICE_PROP)
    def test_start_does_not_run_two_concurrent_processes(self, android_device):
        server = IPerfServerOverAdb('53R147', 'PORT')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_process = mock.Mock()

        server.start()

        self.assertFalse(
            android_device.adb.shell_nb.called,
            'start() should not begin a second process if another is running.'
        )

    @mock.patch('acts.libs.proc.job.run')
    @mock.patch('builtins.open')
    @mock.patch(ANDROID_DEVICE_PROP)
    def test_stop_exits_early_if_no_process_has_started(self, android_device, _,
                                                        __):
        server = IPerfServerOverAdb('53R147', 'PORT')
        server._get_full_file_path = lambda _: MOCK_LOGFILE_PATH
        server._iperf_pid = None

        server.stop()

        self.assertFalse(
            android_device.adb.shell_nb.called,
            'stop() should not kill a process if no process is running.'
        )


if __name__ == '__main__':
    unittest.main()
